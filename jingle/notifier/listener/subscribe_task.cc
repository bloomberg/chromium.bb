// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/subscribe_task.h"

#include <string>

#include "base/logging.h"
#include "jingle/notifier/listener/notification_constants.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "talk/base/task.h"
#include "talk/xmllite/qname.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/xmppengine.h"

namespace notifier {

SubscribeTask::SubscribeTask(
    TaskParent* parent,
    const std::vector<std::string>& subscribed_services_list)
    : XmppTask(parent, buzz::XmppEngine::HL_SINGLE),
      subscribed_services_list_(subscribed_services_list) {
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
  VLOG(1) << "P2P: Subscription task started.";
  scoped_ptr<buzz::XmlElement> iq_stanza(
      MakeSubscriptionMessage(subscribed_services_list_,
                              GetClient()->jid().BareJid(), task_id()));
  VLOG(1) << "P2P: Subscription stanza: "
          << XmlElementToString(*iq_stanza.get());

  if (SendStanza(iq_stanza.get()) != buzz::XMPP_RETURN_OK) {
    SignalStatusUpdate(false);
    // TODO(akalin): This should be STATE_ERROR.
    return STATE_DONE;
  }
  return STATE_RESPONSE;
}

int SubscribeTask::ProcessResponse() {
  VLOG(1) << "P2P: Subscription response received.";
  const buzz::XmlElement* stanza = NextStanza();
  if (stanza == NULL) {
    return STATE_BLOCKED;
  }
  VLOG(1) << "P2P: Subscription response: " << XmlElementToString(*stanza);
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
    const std::vector<std::string>& subscribed_services_list,
    const buzz::Jid& to_jid_bare, const std::string& task_id) {
  DCHECK(to_jid_bare.IsBare());
  static const buzz::QName kQnNotifierGetAll(
      kNotifierNamespace, "getAll");

  // Create the subscription stanza using the notifications protocol.
  // <iq type='get' from='{fullJid}' to='{bareJid}' id='{#}'>
  //   <gn:getAll xmlns:gn="google:notifier" xmlns="">
  //     <ClientActive bool="true" />
  //     <!-- present only if subscribed_services_list is not empty -->
  //     <SubscribedServiceUrl data="google:notifier">
  //     <SubscribedServiceUrl data="http://www.google.com/chrome/sync">
  //     <FilterNonSubscribed bool="true" />
  //   </gn:getAll>
  // </iq>

  buzz::XmlElement* iq = MakeIq(buzz::STR_GET, to_jid_bare, task_id);
  buzz::XmlElement* get_all = new buzz::XmlElement(kQnNotifierGetAll, true);
  iq->AddElement(get_all);

  get_all->AddElement(MakeBoolXmlElement("ClientActive", true));
  for (std::vector<std::string>::const_iterator iter =
        subscribed_services_list.begin();
      iter != subscribed_services_list.end(); ++iter) {
    get_all->AddElement(
        MakeStringXmlElement("SubscribedServiceUrl", iter->c_str()));
  }
  if (!subscribed_services_list.empty()) {
    get_all->AddElement(MakeBoolXmlElement("FilterNonSubscribed", true));
  }
  return iq;
}

}  // namespace notifier
