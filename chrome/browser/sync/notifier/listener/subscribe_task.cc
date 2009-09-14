// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/listener/subscribe_task.h"

#include <string>

#include "base/logging.h"
#include "talk/base/task.h"
#include "talk/xmllite/qname.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmppengine.h"

namespace browser_sync {

SubscribeTask::SubscribeTask(Task* parent)
    : XmppTask(parent, buzz::XmppEngine::HL_SINGLE) {
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
  scoped_ptr<buzz::XmlElement> iq_stanza(NewSubscriptionMessage());

  if (SendStanza(iq_stanza.get()) != buzz::XMPP_RETURN_OK) {
    SignalStatusUpdate(false);
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
  // We've receieved a response to our subscription request.
  if (stanza->HasAttr(buzz::QN_TYPE) &&
    stanza->Attr(buzz::QN_TYPE) == buzz::STR_RESULT) {
    SignalStatusUpdate(true);
    return STATE_DONE;
  }
  // An error response was received.
  // TODO(brg) : Error handling.
  SignalStatusUpdate(false);
  return STATE_DONE;
}

buzz::XmlElement* SubscribeTask::NewSubscriptionMessage() {
  static const buzz::QName kQnNotifierGetAll(true, "google:notifier", "getAll");
  static const buzz::QName kQnNotifierClientActive(true,
                                                   buzz::STR_EMPTY,
                                                   "ClientActive");
  static const buzz::QName kQnBool(true, buzz::STR_EMPTY, "bool");
  static const std::string kTrueString("true");

  // Create the subscription stanza using the notificaitons protocol.
  // <iq type='get' from='{fullJid}' to='{bareJid}' id='{#}'>
  //   <gn:getAll xmlns:gn='google:notifier' xmlns=''>
  //     <ClientActive bool='true'/>
  //   </gn:getAll>
  // </iq>
  buzz::XmlElement* get_all_request =
    MakeIq(buzz::STR_GET, GetClient()->jid().BareJid(), task_id());

  buzz::XmlElement* notifier_get =
      new buzz::XmlElement(kQnNotifierGetAll, true);
  get_all_request->AddElement(notifier_get);

  buzz::XmlElement* client_active =
      new buzz::XmlElement(kQnNotifierClientActive, true);
  client_active->AddAttr(kQnBool, kTrueString);
  notifier_get->AddElement(client_active);

  return get_all_request;
}

}  // namespace browser_sync
