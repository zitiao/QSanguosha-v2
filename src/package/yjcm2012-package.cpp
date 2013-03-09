#include "yjcm2012-package.h"
#include "skill.h"
#include "standard.h"
#include "client.h"
#include "clientplayer.h"
#include "engine.h"
#include "god.h"
#include "maneuvering.h"

class Zhenlie: public TriggerSkill {
public:
    Zhenlie(): TriggerSkill("zhenlie") {
        events << TargetConfirmed << CardEffected << SlashEffected;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const{
        if (TriggerSkill::triggerable(player)) {
            if (event == TargetConfirmed) {
                CardUseStruct use = data.value<CardUseStruct>();
                if (use.to.contains(player) && use.from != player) {
                    if (use.card->isKindOf("Slash") || use.card->isNDTrick()) {
                        if (room->askForSkillInvoke(player, objectName(), data)) {
                            room->loseHp(player);
                            if (player->isAlive()) {
                                room->setCardFlag(use.card, "ZhenlieNullify");
                                player->setFlags("ZhenlieNullify");
                                if (!use.from->isNude()) {
                                    int id = room->askForCardChosen(player, use.from, "he", objectName());
                                    room->throwCard(id, use.from, player);
                                }
                            }
                        }
                    }
                }
            }
        } else if (event == CardEffected) {
            CardEffectStruct effect = data.value<CardEffectStruct>();
            if (!effect.card->isKindOf("Slash") && effect.card->hasFlag("ZhenlieNullify")
                && player->hasFlag("-ZhenlieNullify")) {
                LogMessage log;
                log.type = "#DanlaoAvoid";
                log.from = player;
                log.arg = effect.card->objectName();
                log.arg2 = objectName();
                room->sendLog(log);

                player->setFlags("-ZhenlieNullify");
                return true;
            }
        } else if (event == SlashEffected) {
            SlashEffectStruct effect = data.value<SlashEffectStruct>();
            if (effect.slash->hasFlag("ZhenlieNullify") && player->hasFlag("ZhenlieNullify")) {
                LogMessage log;
                log.type = "#DanlaoAvoid";
                log.from = player;
                log.arg = effect.slash->objectName();
                log.arg2 = objectName();
                room->sendLog(log);

                player->setFlags("-ZhenlieNullify");
                return true;
            }
        }
        return false;
    }
};

class Miji: public PhaseChangeSkill {
public:
    Miji(): PhaseChangeSkill("miji") {
    }

    virtual bool onPhaseChange(ServerPlayer *target) const{
        Room *room = target->getRoom();
        if (target->getPhase() == Player::Finish && target->isWounded() && target->askForSkillInvoke(objectName())) {
            QStringList draw_num;
            for (int i = 1; i <= target->getLostHp(); draw_num << QString::number(i++)) {}
            int num = room->askForChoice(target, "miji_draw", draw_num.join("+")).toInt();
            target->drawCards(num, true, objectName());

            if (!target->isKongcheng()) {
                int n = 0;
                forever {
                    int original_handcardnum = target->getHandcardNum();
                    if (n < num && !target->isKongcheng()) {
                        if (!room->askForYiji(target, target->handCards(), objectName(), false, false, false, num - n))
                            break;
                        n += original_handcardnum - target->getHandcardNum();
                    } else {
                        break;
                    }
                }
                // give the rest cards randomly
                if (n < num && !target->isKongcheng()) {
                    int rest_num = num - n;
                    forever {
                        QList<int> handcard_list = target->handCards();
                        qShuffle(handcard_list);
                        int give = qrand() % rest_num + 1;
                        rest_num -= give;
                        QList<int> to_give = handcard_list.length() < give ? handcard_list : handcard_list.mid(0, give);
                        ServerPlayer *receiver = room->getOtherPlayers(target).at(qrand() % (target->aliveCount() - 1));
                        DummyCard *dummy = new DummyCard;
                        foreach (int id, to_give)
                            dummy->addSubcard(id);
                        room->obtainCard(receiver, dummy, false);
                        delete dummy;
                        if (rest_num == 0 || target->isKongcheng())
                            break;
                    }
                }
            }
        }
        return false;
    }
};

QiceCard::QiceCard(){
    will_throw = false;
    handling_method = Card::MethodNone;
}

bool QiceCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const{
    CardStar card = Self->tag.value("qice").value<CardStar>();
    return card && card->targetFilter(targets, to_select, Self) && !Self->isProhibited(to_select, card);
}

bool QiceCard::targetFixed() const{
    CardStar card = Self->tag.value("qice").value<CardStar>();
    return card && card->targetFixed();
}

bool QiceCard::targetsFeasible(const QList<const Player *> &targets, const Player *Self) const{
    CardStar card = Self->tag.value("qice").value<CardStar>();
    return card && card->targetsFeasible(targets, Self);
}

const Card *QiceCard::validate(const CardUseStruct *card_use) const{
    Card *use_card = Sanguosha->cloneCard(user_string, SuitToBeDecided, -1);
    use_card->setSkillName("qice");
    foreach(int id, this->getSubcards())
        use_card->addSubcard(id);
    bool available = true;
    foreach (ServerPlayer *to, card_use->to)
        if (card_use->from->isProhibited(to, use_card)) {
            available = false;
            break;
        }
    available = available && use_card->isAvailable(card_use->from);
    use_card->deleteLater();
    if (!available) return NULL;
    return use_card;
}

class Qice: public ViewAsSkill{
public:
    Qice():ViewAsSkill("qice"){
    }

    virtual QDialog *getDialog() const{
        return GuhuoDialog::getInstance("qice", false);
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const{
        return !to_select->isEquipped();
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const{
        if (cards.length() < Self->getHandcardNum())
            return NULL;

        CardStar c = Self->tag.value("qice").value<CardStar>();
        if(c){
            QiceCard *card = new QiceCard;
            card->setUserString(c->objectName());
            card->addSubcards(cards);
            return card;
        }else
            return NULL;
    }

protected:
    virtual bool isEnabledAtPlay(const Player *player) const{
        if(player->isKongcheng())
            return false;
        else
            return !player->hasUsed("QiceCard");
    }
};

class Zhiyu: public MasochismSkill {
public:
    Zhiyu():MasochismSkill("zhiyu"){

    }

    virtual void onDamaged(ServerPlayer *target, const DamageStruct &damage) const{
        if(damage.from && damage.from->isAlive()
            && target->askForSkillInvoke(objectName(), QVariant::fromValue(damage))){
            target->drawCards(1);
            if (target->isKongcheng())
                return;

            Room *room = target->getRoom();
            room->broadcastSkillInvoke(objectName());
            room->showAllCards(target);

            QList<const Card *> cards = target->getHandcards();
            Card::Color color = cards.first()->getColor();
            bool same_color = true;
            foreach(const Card *card, cards){
                if(card->getColor() != color){
                    same_color = false;
                    break;
                }
            }

            if(same_color && !damage.from->isKongcheng())
                room->askForDiscard(damage.from, objectName(), 1, 1);
        }
    }
};

class Jiangchi:public DrawCardsSkill{
public:
    Jiangchi():DrawCardsSkill("jiangchi"){
    }

    virtual int getDrawNum(ServerPlayer *caozhang, int n) const{
        Room *room = caozhang->getRoom();
        QString choice = room->askForChoice(caozhang, objectName(), "jiang+chi+cancel");
        if(choice == "cancel")
            return n;
        LogMessage log;
        log.from = caozhang;
        log.arg = objectName();
        if(choice == "jiang"){
            log.type = "#Jiangchi1";
            room->sendLog(log);
            room->broadcastSkillInvoke(objectName(), 1);
            room->setPlayerCardLimitation(caozhang, "use,response", "Slash", true);
            return n + 1;
        }else{
            log.type = "#Jiangchi2";
            room->sendLog(log);
            room->broadcastSkillInvoke(objectName(), 2);
            room->setPlayerFlag(caozhang, "jiangchi_invoke");
            return n - 1;
        }
    }
};

class JiangchiTargetMod: public TargetModSkill {
public:
    JiangchiTargetMod(): TargetModSkill("#jiangchi-target") {
        frequency = NotFrequent;
    }

    virtual int getResidueNum(const Player *from, const Card *) const{
        if (from->hasSkill("jiangchi") && from->hasFlag("jiangchi_invoke"))
            return 1;
        else
            return 0;
    }

    virtual int getDistanceLimit(const Player *from, const Card *) const{
        if (from->hasSkill("jiangchi") && from->hasFlag("jiangchi_invoke"))
            return 1000;
        else
            return 0;
    }
};

class Qianxi: public PhaseChangeSkill {
public:
    Qianxi(): PhaseChangeSkill("qianxi") {
    }

    virtual bool onPhaseChange(ServerPlayer *target) const{
        Room *room = target->getRoom();
        if (target->getPhase() == Player::Start) {
            if (room->askForSkillInvoke(target, objectName())) {
                room->broadcastSkillInvoke(objectName());

                JudgeStruct judge;
                judge.pattern = QRegExp("(.*):(.*):(.*)");
                judge.reason = objectName();
                judge.play_animation = false;
                judge.who = target;

                room->judge(judge);

                if (!target->isAlive())
                    return false;

                QString color = judge.card->isRed() ? "red" : "black";
                target->tag[objectName()] = QVariant::fromValue(color);

                QList<ServerPlayer *> to_choose;
                foreach (ServerPlayer *p, room->getOtherPlayers(target)) {
                    if (target->distanceTo(p) == 1)
                        to_choose << p;
                }
                if (to_choose.isEmpty())
                    return false;

                ServerPlayer *victim = room->askForPlayerChosen(target, to_choose, objectName());
                QString pattern = QString(".|.|.|hand|%1$0").arg(color);

                room->setPlayerFlag(victim, "QianxiTarget");
                room->setPlayerMark(victim, QString("@qianxi_%1").arg(color), 1);
                room->setPlayerCardLimitation(victim, "use,response", pattern, false);

                LogMessage log;
                log.type = "#Qianxi";
                log.from = victim;
                log.arg = QString("no_suit_%1").arg(color);
                room->sendLog(log);
            }
        }
        return false;
    }
};

class QianxiClear: public TriggerSkill {
public:
    QianxiClear(): TriggerSkill("#qianxi-clear") {
        events << EventPhaseChanging << Death;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return !target->tag["qianxi"].toString().isNull();
    }

    virtual bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const{
        if (event == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to != Player::NotActive)
                return false;
        } else if (event == Death) {
            DeathStruct death = data.value<DeathStruct>();
            if (death.who != player)
                return false;
        }

        QString color = player->tag["qianxi"].toString();
        foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
            if (p->hasFlag("QianxiTarget")) {
                room->removePlayerCardLimitation(p, "use,response", QString(".|.|.|hand|%1$0").arg(color));
                room->setPlayerMark(p, QString("@qianxi_%1").arg(color), 0);
            }
        }
        return false;
    }
};

class Dangxian: public TriggerSkill{
public:
    Dangxian():TriggerSkill("dangxian"){
        frequency = Compulsory;
        events << EventPhaseStart;
    }

    virtual bool trigger(TriggerEvent , Room *room, ServerPlayer *liaohua, QVariant &) const{
        if (liaohua->getPhase() == Player::RoundStart) {
            room->broadcastSkillInvoke(objectName());
            LogMessage log;
            log.type = "#TriggerSkill";
            log.from = liaohua;
            log.arg = objectName();
            room->sendLog(log);

            liaohua->setPhase(Player::Play);
            room->broadcastProperty(liaohua, "phase");
            RoomThread *thread = room->getThread();
            if (!thread->trigger(EventPhaseStart, room, liaohua))
                thread->trigger(EventPhaseProceeding, room, liaohua);
            thread->trigger(EventPhaseEnd, room, liaohua);

            liaohua->setPhase(Player::RoundStart);
            room->broadcastProperty(liaohua, "phase");
        }
        return false;
    }
};

class Fuli: public TriggerSkill{
public:
    Fuli():TriggerSkill("fuli"){
        events << AskForPeaches;
        frequency = Limited;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return TriggerSkill::triggerable(target) && target->getMark("@laoji") > 0;
    }

    int getKingdoms(Room *room) const{
        QSet<QString> kingdom_set;
        foreach(ServerPlayer *p, room->getAlivePlayers()){
            kingdom_set << p->getKingdom();
        }
        return kingdom_set.size();
    }

    virtual bool trigger(TriggerEvent, Room* room, ServerPlayer *liaohua, QVariant &data) const{
        DyingStruct dying_data = data.value<DyingStruct>();
        if(dying_data.who != liaohua)
            return false;
        if(liaohua->askForSkillInvoke(objectName(), data)){
            room->broadcastInvoke("animate", "lightbox:$FuliAnimate");
            room->broadcastSkillInvoke(objectName());

            liaohua->loseMark("@laoji");

            RecoverStruct recover;
            recover.recover = qMin(getKingdoms(room), liaohua->getMaxHp()) - liaohua->getHp();
            room->recover(liaohua, recover);

            liaohua->turnOver();
        }
        return false;
    }
};

class Zishou:public DrawCardsSkill{
public:
    Zishou():DrawCardsSkill("zishou"){
    }

    virtual int getDrawNum(ServerPlayer *liubiao, int n) const{
        Room *room = liubiao->getRoom();
        if(liubiao->isWounded() && room->askForSkillInvoke(liubiao, objectName())){
            int losthp = liubiao->getLostHp();
            room->broadcastSkillInvoke(objectName(), qMin(3, losthp));
            liubiao->clearHistory();
            liubiao->skip(Player::Play);
            return n + losthp;
        }else
            return n;
    }
};

class Zongshi: public MaxCardsSkill{
public:
    Zongshi():MaxCardsSkill("zongshi"){
    }
    virtual int getExtra(const Player *target) const{
        int extra = 0;
        QSet<QString> kingdom_set;
        if(target->parent()){
            foreach(const Player *player, target->parent()->findChildren<const Player *>()){
                if(player->isAlive()){
                    kingdom_set << player->getKingdom();
                }
            }
        }
        extra = kingdom_set.size();
        if(target->hasSkill(objectName()))
            return extra;
        else
            return 0;
    }
};

class Shiyong: public TriggerSkill{
public:
    Shiyong():TriggerSkill("shiyong"){
        events << Damaged;
        frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        DamageStruct damage = data.value<DamageStruct>();
        if(damage.card && damage.card->isKindOf("Slash") &&
                (damage.card->isRed() || damage.card->hasFlag("drank"))){

            int index = 1;
            if (damage.from->getGeneralName().contains("guanyu"))
                index = 3;
            else if (damage.card->hasFlag("drank"))
                index = 2;
            room->broadcastSkillInvoke(objectName(), index);
            
            LogMessage log;
            log.type = "#TriggerSkill";
            log.from = player;
            log.arg = objectName();
            room->sendLog(log);

            room->loseMaxHp(player);
        }
        return false;
    }
};

GongqiCard::GongqiCard() {
    target_fixed = true;
}

void GongqiCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const{
    room->setPlayerFlag(source, "InfinityAttackRange");
    const Card *cd = Sanguosha->getCard(subcards.first());
    if (cd->isKindOf("EquipCard")) {
        QList<ServerPlayer *> targets;
        foreach (ServerPlayer *p, room->getOtherPlayers(source))
            if (!p->isNude()) targets << p;
        if (!targets.isEmpty() && source->askForSkillInvoke("gongqi", QVariant::fromValue(QString("discard")))) {
            ServerPlayer *to_discard = room->askForPlayerChosen(source, targets, "gongqi");
            room->throwCard(room->askForCardChosen(source, to_discard, "he", "gongqi"), to_discard, source);
        }
    }
}

class FuhunViewAsSkill: public ViewAsSkill {
public:
    FuhunViewAsSkill(): ViewAsSkill("fuhun") {
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return player->getHandcardNum() >= 2 && Slash::IsAvailable(player);
    }

    virtual bool isEnabledAtResponse(const Player *player, const QString &pattern) const{
        return Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE
               && player->getHandcardNum() >= 2 && pattern == "slash";
    }

    virtual bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const{
        return selected.length() < 2 && !to_select->isEquipped();
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const{
        if (cards.length() != 2)
            return NULL;

        Slash *slash = new Slash(Card::SuitToBeDecided, 0);
        slash->setSkillName(objectName());
        slash->addSubcards(cards);

        return slash;
    }
};

class Fuhun: public TriggerSkill {
public:
    Fuhun(): TriggerSkill("fuhun") {
        events << Damage << EventPhaseChanging;
        view_as_skill = new FuhunViewAsSkill;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const{
        if (event == Damage && TriggerSkill::triggerable(player)) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.card && damage.card->isKindOf("Slash") && damage.card->getSkillName() == objectName()
                && player->getPhase() == Player::Play) {
                room->acquireSkill(player, "wusheng");
                room->acquireSkill(player, "paoxiao");

                room->broadcastSkillInvoke(objectName(), qrand() % 2 + 1);
                player->setFlags(objectName());
            }
        } else if (event == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to == Player::NotActive && player->hasFlag(objectName())) {
                room->detachSkillFromPlayer(player, "wusheng");
                room->detachSkillFromPlayer(player, "paoxiao");
            }
        }

        return false;
    }
};

class Gongqi: public OneCardViewAsSkill {
public:
    Gongqi(): OneCardViewAsSkill("gongqi") {
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return !player->hasUsed("GongqiCard");
    }

    virtual bool viewFilter(const Card *to_select) const{
        return !Self->isJilei(to_select);
    }

    virtual const Card *viewAs(const Card *originalcard) const{
        GongqiCard *card = new GongqiCard;
        card->addSubcard(originalcard->getId());
        card->setSkillName(objectName());
        return card;
    }
};

JiefanCard::JiefanCard() {
    mute = true;
}

bool JiefanCard::targetFilter(const QList<const Player *> &targets, const Player *, const Player *) const{
    return targets.isEmpty();
}

void JiefanCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const{
    source->loseMark("@rescue");
    ServerPlayer *target = targets.first();
    source->tag["JiefanTarget"] = QVariant::fromValue((PlayerStar)target);
    int index = 1;
    if (target->isLord()
        && (target->getGeneralName().contains("sunquan")
            || target->getGeneralName().contains("sunjian")
            || target->getGeneralName().contains("sunce")))
        index = 2;
    room->broadcastSkillInvoke("jiefan", index);
    room->broadcastInvoke("animate", "lightbox:$JiefanAnimate" + QString::number(index));
    room->getThread()->delay(2000);

    foreach (ServerPlayer *player, room->getAllPlayers()) {
        if (player->isAlive() && player->inMyAttackRange(target))
            room->cardEffect(this, source, player);
    }
    source->tag.remove("JiefanTarget");
}

void JiefanCard::onEffect(const CardEffectStruct &effect) const{
    Room *room = effect.to->getRoom();

    PlayerStar target = effect.from->tag["JiefanTarget"].value<PlayerStar>();
    QVariant data = effect.from->tag["JiefanTarget"];
    if (target && !room->askForCard(effect.to, ".Weapon", "@jiefan-discard", data))
        target->drawCards(1);
}

class Jiefan: public ZeroCardViewAsSkill {
public:
    Jiefan(): ZeroCardViewAsSkill("jiefan") {
        frequency = Limited;
    }

    virtual const Card *viewAs() const{
        return new JiefanCard;
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return player->getMark("@rescue") >= 1;
    }
};

AnxuCard::AnxuCard(){
    mute = true;
}

bool AnxuCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const{
    if(to_select == Self)
        return false;
    if(targets.isEmpty())
        return true;
    else if(targets.length() == 1)
        return to_select->getHandcardNum() != targets.first()->getHandcardNum();
    else
        return false;
}

bool AnxuCard::targetsFeasible(const QList<const Player *> &targets, const Player *Self) const{
    return targets.length() == 2;
}

void AnxuCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const{
    QList<ServerPlayer *> selecteds = targets;
    ServerPlayer *from = selecteds.first()->getHandcardNum() < selecteds.last()->getHandcardNum() ? selecteds.takeFirst() : selecteds.takeLast();
    ServerPlayer *to = selecteds.takeFirst();
    if (from->getGeneralName().contains("sunquan"))
        room->broadcastSkillInvoke("anxu", 2);
    else
        room->broadcastSkillInvoke("anxu", 1);
    int id = room->askForCardChosen(from, to, "h", "anxu");
    const Card *cd = Sanguosha->getCard(id);
    CardMoveReason reason(CardMoveReason::S_REASON_GIVE, source->objectName());
    room->obtainCard(from, cd, reason);
    room->showCard(from, id);
    if (cd->getSuit() != Card::Spade)
        source->drawCards(1);
}

class Anxu: public ZeroCardViewAsSkill{
public:
    Anxu():ZeroCardViewAsSkill("anxu"){
    }

    virtual const Card *viewAs() const{
        return new AnxuCard;
    }

protected:
    virtual bool isEnabledAtPlay(const Player *player) const{
        return !player->hasUsed("AnxuCard");
    }

    virtual bool isEnabledAtResponse(const Player *player, const QString &pattern) const{
        return false;
    }
};

class Zhuiyi: public TriggerSkill{
public:
    Zhuiyi():TriggerSkill("zhuiyi"){
        events << Death;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL && target->hasSkill(objectName());
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        DeathStruct death = data.value<DeathStruct>();
        if (death.who != player)
            return false;
        QList<ServerPlayer *> targets = (death.damage && death.damage->from) ? room->getOtherPlayers(death.damage->from) :
                                                                               room->getAlivePlayers();

        QVariant data_for_ai = QVariant::fromValue(death.damage);
        if (targets.isEmpty() || !player->askForSkillInvoke(objectName(), data_for_ai))
            return false;

        ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName());

        if (target->getGeneralName().contains("sunquan"))
            room->broadcastSkillInvoke(objectName(), 2);
        else
            room->broadcastSkillInvoke(objectName(), 1);
        target->drawCards(3);
        RecoverStruct recover;
        recover.who = player;
        recover.recover = 1;
        room->recover(target, recover, true);
        return false;
    }
};

class LihuoViewAsSkill:public OneCardViewAsSkill{
public:
    LihuoViewAsSkill():OneCardViewAsSkill("lihuo"){
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return Slash::IsAvailable(player);
    }

    virtual bool isEnabledAtResponse(const Player *, const QString &pattern) const{
        return Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE
               && pattern == "slash";
    }

    virtual bool viewFilter(const Card* to_select) const{
        return to_select->objectName() == "slash";
    }

    virtual const Card *viewAs(const Card *originalCard) const{
        
        Card *acard = new FireSlash(originalCard->getSuit(), originalCard->getNumber());
        acard->addSubcard(originalCard->getId());
        acard->setSkillName(objectName());
        return acard;
    }
};

class Lihuo: public TriggerSkill{
public:
    Lihuo():TriggerSkill("lihuo"){
        events << DamageDone << CardFinished;
        view_as_skill = new LihuoViewAsSkill;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent triggerEvent, Room* room, ServerPlayer *player, QVariant &data) const{
        if (triggerEvent == DamageDone) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.card && damage.card->isKindOf("Slash") && damage.card->getSkillName() == objectName())
                damage.from->tag["LihuoSlash"] = QVariant::fromValue(damage.card);
        } else if (TriggerSkill::triggerable(player) && !player->tag.value("LihuoSlash").isNull()
                   && data.value<CardUseStruct>().card == player->tag.value("LihuoSlash").value<CardStar>()) {
            LogMessage log;
            log.type = "#TriggerSkill";
            log.from = player;
            log.arg = objectName();
            room->sendLog(log);

            player->tag.remove("LihuoSlash");
            room->broadcastSkillInvoke("lihuo", 2);
            room->loseHp(player, 1);
        }
        return false;
    }

    virtual int getEffectIndex(const ServerPlayer *, const Card *) const {
        return 1;
    }
};

class LihuoTargetMod: public TargetModSkill {
public:
    LihuoTargetMod(): TargetModSkill("#lihuo-target") {
        frequency = NotFrequent;
    }

    virtual int getExtraTargetNum(const Player *from, const Card *card) const{
        if (from->hasSkill("lihuo") && card->isKindOf("FireSlash"))
            return 1;
        else
            return 0;
    }
};

ChunlaoCard::ChunlaoCard() {
    will_throw = false;
    target_fixed = true;
    handling_method = Card::MethodNone;
}

void ChunlaoCard::use(Room *, ServerPlayer *source, QList<ServerPlayer *> &) const{
    source->addToPile("wine", this);
}

class ChunlaoViewAsSkill:public ViewAsSkill{
public:
    ChunlaoViewAsSkill():ViewAsSkill("chunlao"){
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return false;
    }

    virtual bool isEnabledAtResponse(const Player *, const QString &pattern) const{
        return pattern == "@@chunlao";
    }

    virtual bool viewFilter(const QList<const Card *> &, const Card* to_select) const{
        return to_select->isKindOf("Slash");
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const{
        if(cards.length() == 0)
            return NULL;

        Card *acard = new ChunlaoCard;
        acard->addSubcards(cards);
        acard->setSkillName(objectName());
        return acard;
    }
};

class Chunlao: public TriggerSkill{
public:
    Chunlao():TriggerSkill("chunlao"){
        events << EventPhaseStart << AskForPeaches ;
        view_as_skill = new ChunlaoViewAsSkill;
    }


    virtual bool trigger(TriggerEvent triggerEvent, Room* room, ServerPlayer *chengpu, QVariant &data) const{

        if(triggerEvent == EventPhaseStart &&
                chengpu->getPhase() == Player::Finish &&
                !chengpu->isKongcheng() &&
                chengpu->getPile("wine").isEmpty()){
            room->askForUseCard(chengpu, "@@chunlao", "@chunlao");
        }else if(triggerEvent == AskForPeaches && !chengpu->getPile("wine").isEmpty()){
            DyingStruct dying = data.value<DyingStruct>();
            while(dying.who->getHp() < 1 && chengpu->askForSkillInvoke(objectName(), data)){
                QList<int> cards = chengpu->getPile("wine");
                room->fillAG(cards, chengpu);
                int card_id = room->askForAG(chengpu, cards, true, objectName());
                room->broadcastInvoke("clearAG");
                if(card_id != -1){
                    CardMoveReason reason(CardMoveReason::S_REASON_REMOVE_FROM_PILE, QString(), "chunlao", QString());
                    room->throwCard(Sanguosha->getCard(card_id), reason, NULL);
                    Analeptic *analeptic = new Analeptic(Card::NoSuit, 0);
                    analeptic->setSkillName(objectName());
                    CardUseStruct use;
                    use.card = analeptic;
                    use.from = dying.who;
                    use.to << dying.who;
                    room->useCard(use);
                }
            }
        }
        return false;
    }

    virtual int getEffectIndex(const ServerPlayer *player, const Card *card) const {
        if (card->isKindOf("Analeptic")) {
            if (player->getGeneralName().contains("zhouyu"))
                return 3;
            else
                return 2;
        } else
            return 1;
    }
};

class ChunlaoClear: public TriggerSkill {
public:
    ChunlaoClear(): TriggerSkill("#chunlao-clear") {
        events << EventLoseSkill;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target && !target->hasSkill("chunlao") && target->getPile("wine").length() > 0;
    }

    virtual bool trigger(TriggerEvent, Room *, ServerPlayer *player, QVariant &) const{
        player->removePileByName("wine");
        return false;
    }
};

YJCM2012Package::YJCM2012Package()
    : Package("YJCM2012")
{
    General *bulianshi = new General(this, "bulianshi", "wu", 3, false);
    bulianshi->addSkill(new Anxu);
    bulianshi->addSkill(new Zhuiyi);

    General *caozhang = new General(this, "caozhang", "wei");
    caozhang->addSkill(new Jiangchi);
    caozhang->addSkill(new JiangchiTargetMod);
    related_skills.insertMulti("jiangchi", "#jiangchi-target");

    General *chengpu = new General(this, "chengpu", "wu");
    chengpu->addSkill(new Lihuo);
    chengpu->addSkill(new LihuoTargetMod);
    chengpu->addSkill(new Chunlao);
    chengpu->addSkill(new ChunlaoClear);
    related_skills.insertMulti("lihuo", "#lihuo-target");
    related_skills.insertMulti("chunlao", "#chunlao-clear");

    General *guanxingzhangbao = new General(this, "guanxingzhangbao", "shu");
    guanxingzhangbao->addSkill(new Fuhun);

    General *handang = new General(this, "handang", "wu");
    handang->addSkill(new Gongqi);
    handang->addSkill(new Jiefan);
    handang->addSkill(new MarkAssignSkill("@rescue", 1));

    General *huaxiong = new General(this, "huaxiong", "qun", 6);
    huaxiong->addSkill(new Shiyong);

    General *liaohua = new General(this, "liaohua", "shu");
    liaohua->addSkill(new Dangxian);
    liaohua->addSkill(new MarkAssignSkill("@laoji", 1));
    liaohua->addSkill(new Fuli);

    General *liubiao = new General(this, "liubiao", "qun", 4);
    liubiao->addSkill(new Zishou);
    liubiao->addSkill(new Zongshi);

    General *madai = new General(this, "madai", "shu");
    madai->addSkill(new Qianxi);
    madai->addSkill(new QianxiClear);
    madai->addSkill("mashu");
    related_skills.insertMulti("qianxi", "#qianxi-clear");

    General *wangyi = new General(this, "wangyi", "wei", 3, false);
    wangyi->addSkill(new Zhenlie);
    wangyi->addSkill(new Miji);

    General *xunyou = new General(this, "xunyou", "wei", 3);
    xunyou->addSkill(new Qice);
    xunyou->addSkill(new Zhiyu);

    addMetaObject<QiceCard>();
    addMetaObject<ChunlaoCard>();
    addMetaObject<GongqiCard>();
    addMetaObject<JiefanCard>();
    addMetaObject<AnxuCard>();
}

ADD_PACKAGE(YJCM2012)
