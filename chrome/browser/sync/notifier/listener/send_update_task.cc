// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/listener/send_update_task.h"

#include <string>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/notifier/listener/notification_constants.h"
#include "chrome/browser/sync/notifier/listener/xml_element_util.h"
#include "talk/xmllite/qname.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmppconstants.h"

namespace browser_sync {

SendUpdateTask::SendUpdateTask(Task* parent,
                               NotificationMethod notification_method)
    : XmppTask(parent, buzz::XmppEngine::HL_SINGLE),  // Watch for one reply.
      notification_method_(notification_method) {
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
  scoped_ptr<buzz::XmlElement> stanza(
      MakeUpdateMessage(notification_method_,
                        GetClient()->jid().BareJid(), task_id()));
  LOG(INFO) << "P2P: Notification stanza: "
            << XmlElementToString(*stanza.get());

  if (SendStanza(stanza.get()) != buzz::XMPP_RETURN_OK) {
    // TODO(brg) : Retry on error.
    // TODO(akalin): Or maybe immediately return STATE_ERROR and let
    // retries happen a higher level.  In any case, STATE_ERROR should
    // eventually be returned.
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
  LOG(INFO) << "P2P: Notification response: " << XmlElementToString(*stanza);
  if (stanza->HasAttr(buzz::QN_TYPE) &&
    stanza->Attr(buzz::QN_TYPE) == buzz::STR_RESULT) {
    // Notify listeners of success.
    SignalStatusUpdate(true);
    return STATE_DONE;
  }

  // An error response was received.
  // TODO(brg) : Error handling.
  SignalStatusUpdate(false);
  // TODO(akalin): This should be STATE_ERROR.
  return STATE_DONE;
}

buzz::XmlElement* SendUpdateTask::MakeUpdateMessage(
    NotificationMethod notification_method,
    const buzz::Jid& to_jid_bare, const std::string& task_id) {
  switch (notification_method) {
    case NOTIFICATION_LEGACY:
      return MakeLegacyUpdateMessage(to_jid_bare, task_id);
    case NOTIFICATION_TRANSITIONAL:
      return MakeNonLegacyUpdateMessage(true, to_jid_bare, task_id);
    case NOTIFICATION_NEW:
      return MakeNonLegacyUpdateMessage(false, to_jid_bare, task_id);
  }
  NOTREACHED();
  return NULL;
}

// TODO(akalin): Remove this once we get all clients on at least
// NOTIFICATION_TRANSITIONAL.

buzz::XmlElement* SendUpdateTask::MakeLegacyUpdateMessage(
    const buzz::Jid& to_jid_bare, const std::string& task_id) {
  DCHECK(to_jid_bare.IsBare());
  static const buzz::QName kQnNotifierSet(true, kNotifierNamespace, "set");
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
  buzz::XmlElement* stanza = MakeIq(buzz::STR_GET, to_jid_bare, task_id);
  buzz::XmlElement* notifier_set = new buzz::XmlElement(kQnNotifierSet, true);
  stanza->AddElement(notifier_set);

  buzz::XmlElement* id = new buzz::XmlElement(kQnId, true);
  notifier_set->AddElement(id);

  buzz::XmlElement* service_url = new buzz::XmlElement(kQnServiceUrl, true);
  service_url->AddAttr(kQnData, kSyncLegacyServiceUrl);
  id->AddElement(service_url);

  buzz::XmlElement* service_id = new buzz::XmlElement(kQnServiceId, true);
  service_id->AddAttr(kQnData, kSyncLegacyServiceId);
  id->AddElement(service_id);
  return stanza;
}

// TODO(akalin): Remove the is_transitional switch once we get all
// clients on at least NOTIFICATION_NEW.

buzz::XmlElement* SendUpdateTask::MakeNonLegacyUpdateMessage(
    bool is_transitional,
    const buzz::Jid& to_jid_bare, const std::string& task_id) {
  DCHECK(to_jid_bare.IsBare());
  static const buzz::QName kQnNotifierSet(true, kNotifierNamespace, "set");
  static const buzz::QName kQnId(true, buzz::STR_EMPTY, "Id");
  static const buzz::QName kQnContent(true, buzz::STR_EMPTY, "Content");

  // Create our update stanza.  In the future this may include the revision id,
  // but at the moment simply does a p2p ping.  The message is constructed as:
  // <iq type='get' from='{fullJid}' to='{bareJid}' id='{#}'>
  //   <gn:set xmlns:gn="google:notifier" xmlns="">
  //     <Id>
  //       <ServiceUrl data="http://www.google.com/chrome/sync" />
  //       <ServiceId data="sync-ping" />
  //     </Id>
  //     <Content>
  //       <Priority int="200" />
  //       <!-- If is_transitional is not set, the bool value is "true". -->
  //       <RequireSubscription bool="false" />
  //       <!-- If is_transitional is set, this is omitted. -->
  //       <ServiceSpecificData data="sync-ping-p2p" />
  //       <WriteToCacheOnly bool="true" />
  //     </Content>
  //   </set>
  // </iq>
  buzz::XmlElement* iq = MakeIq(buzz::STR_GET, to_jid_bare, task_id);
  buzz::XmlElement* set = new buzz::XmlElement(kQnNotifierSet, true);
  buzz::XmlElement* id = new buzz::XmlElement(kQnId, true);
  buzz::XmlElement* content = new buzz::XmlElement(kQnContent, true);
  iq->AddElement(set);
  set->AddElement(id);
  set->AddElement(content);

  id->AddElement(MakeStringXmlElement("ServiceUrl", kSyncServiceUrl));
  id->AddElement(MakeStringXmlElement("ServiceId", kSyncServiceId));

  content->AddElement(MakeIntXmlElement("Priority", kSyncPriority));
  content->AddElement(
      MakeBoolXmlElement("RequireSubscription", !is_transitional));
  if (!is_transitional) {
    content->AddElement(
        MakeStringXmlElement("ServiceSpecificData", kSyncServiceSpecificData));
  }
  content->AddElement(MakeBoolXmlElement("WriteToCacheOnly", true));

  return iq;
}

}  // namespace browser_sync
