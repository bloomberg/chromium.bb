// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/listener/subscribe_task.h"

#include <string>

#include "base/logging.h"
#include "chrome/browser/sync/notifier/listener/notification_constants.h"
#include "chrome/browser/sync/notifier/listener/xml_element_util.h"
#include "talk/base/task.h"
#include "talk/xmllite/qname.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmppconstants.h"
#include "talk/xmpp/xmppengine.h"

namespace browser_sync {

SubscribeTask::SubscribeTask(Task* parent,
                             NotificationMethod notification_method)
    : XmppTask(parent, buzz::XmppEngine::HL_SINGLE),
      notification_method_(notification_method) {
}

SubscribeTask::~SubscribeTask() {
}

bool SubscribeTask::HandleStanza(const buzz::XmlElement* stanza) {
  if (!MatchResponseIq(stanza, GetClient()->jid().BareJid(), task_id()))
    return false;
  QueueStanza(stanza);
  return true;
}

int SubscribeTask::ProcessStart() {
  LOG(INFO) << "P2P: Subscription task started.";
  scoped_ptr<buzz::XmlElement> iq_stanza(
      MakeSubscriptionMessage(notification_method_,
                              GetClient()->jid().BareJid(), task_id()));
  LOG(INFO) << "P2P: Subscription stanza: "
            << XmlElementToString(*iq_stanza.get());

  if (SendStanza(iq_stanza.get()) != buzz::XMPP_RETURN_OK) {
    SignalStatusUpdate(false);
    // TODO(akalin): This should be STATE_ERROR.
    return STATE_DONE;
  }
  return STATE_RESPONSE;
}

int SubscribeTask::ProcessResponse() {
  LOG(INFO) << "P2P: Subscription response received.";
  const buzz::XmlElement* stanza = NextStanza();
  if (stanza == NULL) {
    return STATE_BLOCKED;
  }
  LOG(INFO) << "P2P: Subscription response: " << XmlElementToString(*stanza);
  // We've receieved a response to our subscription request.
  if (stanza->HasAttr(buzz::QN_TYPE) &&
    stanza->Attr(buzz::QN_TYPE) == buzz::STR_RESULT) {
    SignalStatusUpdate(true);
    return STATE_DONE;
  }
  // An error response was received.
  // TODO(brg) : Error handling.
  SignalStatusUpdate(false);
  // TODO(akalin): This should be STATE_ERROR.
  return STATE_DONE;
}

buzz::XmlElement* SubscribeTask::MakeSubscriptionMessage(
    NotificationMethod notification_method,
    const buzz::Jid& to_jid_bare, const std::string& task_id) {
  switch (notification_method) {
    case NOTIFICATION_LEGACY:
      return MakeLegacySubscriptionMessage(to_jid_bare, task_id);
    case NOTIFICATION_TRANSITIONAL:
      return MakeNonLegacySubscriptionMessage(true, to_jid_bare, task_id);
    case NOTIFICATION_NEW:
      return MakeNonLegacySubscriptionMessage(false, to_jid_bare, task_id);
  }
  NOTREACHED();
  return NULL;
}

// TODO(akalin): Remove this once we get all clients on at least
// NOTIFICATION_TRANSITIONAL.

buzz::XmlElement* SubscribeTask::MakeLegacySubscriptionMessage(
    const buzz::Jid& to_jid_bare, const std::string& task_id) {
  DCHECK(to_jid_bare.IsBare());
  static const buzz::QName kQnNotifierGetAll(
      true, kNotifierNamespace, "getAll");
  static const buzz::QName kQnNotifierClientActive(true,
                                                   buzz::STR_EMPTY,
                                                   "ClientActive");
  static const buzz::QName kQnBool(true, buzz::STR_EMPTY, "bool");
  static const std::string kTrueString("true");

  // Create the subscription stanza using the notifications protocol.
  // <iq type='get' from='{fullJid}' to='{bareJid}' id='{#}'>
  //   <gn:getAll xmlns:gn='google:notifier' xmlns=''>
  //     <ClientActive bool='true'/>
  //   </gn:getAll>
  // </iq>
  buzz::XmlElement* get_all_request =
      MakeIq(buzz::STR_GET, to_jid_bare, task_id);

  buzz::XmlElement* notifier_get =
      new buzz::XmlElement(kQnNotifierGetAll, true);
  get_all_request->AddElement(notifier_get);

  buzz::XmlElement* client_active =
      new buzz::XmlElement(kQnNotifierClientActive, true);
  client_active->AddAttr(kQnBool, kTrueString);
  notifier_get->AddElement(client_active);

  return get_all_request;
}

// TODO(akalin): Remove the is_transitional switch once we get all
// clients on at least NOTIFICATION_NEW.

buzz::XmlElement* SubscribeTask::MakeNonLegacySubscriptionMessage(
    bool is_transitional, const buzz::Jid& to_jid_bare,
    const std::string& task_id) {
  DCHECK(to_jid_bare.IsBare());
  static const buzz::QName kQnNotifierGetAll(
      true, kNotifierNamespace, "getAll");

  // Create the subscription stanza using the notifications protocol.
  // <iq type='get' from='{fullJid}' to='{bareJid}' id='{#}'>
  //   <gn:getAll xmlns:gn="google:notifier" xmlns="">
  //     <ClientActive bool="true" />
  //     <!-- present only if is_transitional is set -->
  //     <SubscribedServiceUrl data="google:notifier">
  //     <SubscribedServiceUrl data="http://www.google.com/chrome/sync">
  //     <FilterNonSubscribed bool="true" />
  //   </gn:getAll>
  // </iq>

  buzz::XmlElement* iq = MakeIq(buzz::STR_GET, to_jid_bare, task_id);
  buzz::XmlElement* get_all = new buzz::XmlElement(kQnNotifierGetAll, true);
  iq->AddElement(get_all);

  get_all->AddElement(MakeBoolXmlElement("ClientActive", true));
  if (is_transitional) {
    get_all->AddElement(
        MakeStringXmlElement("SubscribedServiceUrl", kSyncLegacyServiceUrl));
  }
  get_all->AddElement(
      MakeStringXmlElement("SubscribedServiceUrl", kSyncServiceUrl));
  get_all->AddElement(MakeBoolXmlElement("FilterNonSubscribed", true));

  return iq;
}

}  // namespace browser_sync
