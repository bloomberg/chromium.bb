// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/listener/send_update_task.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "talk/xmllite/qname.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/xmppclient.h"

namespace browser_sync {

SendUpdateTask::SendUpdateTask(Task* parent)
    : XmppTask(parent, buzz::XmppEngine::HL_SINGLE) {  // Watch for one reply.
}

SendUpdateTask::~SendUpdateTask() {
}

bool SendUpdateTask::HandleStanza(const buzz::XmlElement* stanza) {
  if (!MatchResponseIq(stanza, GetClient()->jid().BareJid(), task_id()))
    return false;
  QueueStanza(stanza);
  return true;
}

int SendUpdateTask::ProcessStart() {
  LOG(INFO) << "P2P: Notification task started.";
  scoped_ptr<buzz::XmlElement> stanza(NewUpdateMessage());
  if (SendStanza(stanza.get()) != buzz::XMPP_RETURN_OK) {
    // TODO(brg) : Retry on error.
    SignalStatusUpdate(false);
    return STATE_DONE;
  }
  return STATE_RESPONSE;
}

int SendUpdateTask::ProcessResponse() {
  LOG(INFO) << "P2P: Notification response received.";
  const buzz::XmlElement* stanza = NextStanza();
  if (stanza == NULL) {
    return STATE_BLOCKED;
  }
  if (stanza->HasAttr(buzz::QN_TYPE) &&
    stanza->Attr(buzz::QN_TYPE) == buzz::STR_RESULT) {
    // Notify listeners of success.
    SignalStatusUpdate(true);
    return STATE_DONE;
  }

  // An error response was received.
  // TODO(brg) : Error handling.
  SignalStatusUpdate(false);
  return STATE_DONE;
}

buzz::XmlElement* SendUpdateTask::NewUpdateMessage() {
  static const std::string kNSNotifier = "google:notifier";
  static const buzz::QName kQnNotifierSet(true, kNSNotifier, "set");
  static const buzz::QName kQnId(true, buzz::STR_EMPTY, "Id");
  static const buzz::QName kQnServiceUrl(true, buzz::STR_EMPTY, "ServiceUrl");
  static const buzz::QName kQnData(true, buzz::STR_EMPTY, "data");
  static const buzz::QName kQnServiceId(true, buzz::STR_EMPTY, "ServiceId");

  // Create our update stanza.  In the future this may include the revision id,
  // but at the moment simply does a p2p ping.  The message is constructed as:
  // <iq type='get' from='{fullJid}' to='{bareJid}' id='{#}'>
  //  <set xmlns="google:notifier">
  //    <Id xmlns="">
  //      <ServiceUrl xmlns="" data="google:notifier"/>
  //      <ServiceId xmlns="" data="notification"/>
  //    </Id>
  //  </set>
  // </iq>
  buzz::XmlElement* stanza =
      MakeIq(buzz::STR_GET, GetClient()->jid().BareJid(), task_id());
  buzz::XmlElement* notifier_set = new buzz::XmlElement(kQnNotifierSet, true);
  stanza->AddElement(notifier_set);

  buzz::XmlElement* id = new buzz::XmlElement(kQnId, true);
  notifier_set->AddElement(id);

  buzz::XmlElement* service_url = new buzz::XmlElement(kQnServiceUrl, true);
  service_url->AddAttr(kQnData, kNSNotifier);
  id->AddElement(service_url);

  buzz::XmlElement* service_id = new buzz::XmlElement(kQnServiceId, true);
  service_id->AddAttr(kQnData, "notification");
  id->AddElement(service_id);
  return stanza;
}

}  // namespace browser_sync
