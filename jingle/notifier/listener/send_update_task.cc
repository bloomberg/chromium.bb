// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/send_update_task.h"

#include <string>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "jingle/notifier/listener/notification_constants.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "talk/xmllite/qname.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/constants.h"

namespace notifier {

SendUpdateTask::SendUpdateTask(TaskParent* parent,
                               const OutgoingNotificationData& data)
    : XmppTask(parent, buzz::XmppEngine::HL_SINGLE),  // Watch for one reply.
      notification_data_(data) {
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
  VLOG(1) << "P2P: Notification task started.";
  scoped_ptr<buzz::XmlElement> stanza(
      MakeUpdateMessage(notification_data_,
                        GetClient()->jid().BareJid(), task_id()));
  VLOG(1) << "P2P: Notification stanza: " << XmlElementToString(*stanza.get());

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
  VLOG(1) << "P2P: Notification response received.";
  const buzz::XmlElement* stanza = NextStanza();
  if (stanza == NULL) {
    return STATE_BLOCKED;
  }
  VLOG(1) << "P2P: Notification response: " << XmlElementToString(*stanza);
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
    const OutgoingNotificationData& notification_data,
    const buzz::Jid& to_jid_bare, const std::string& task_id) {
  DCHECK(to_jid_bare.IsBare());
  static const buzz::QName kQnNotifierSet(kNotifierNamespace, "set");
  static const buzz::QName kQnId(buzz::STR_EMPTY, "Id");
  static const buzz::QName kQnContent(buzz::STR_EMPTY, "Content");

  // Create our update stanza. The message is constructed as:
  // <iq type='get' from='{fullJid}' to='{bareJid}' id='{#}'>
  //   <gn:set xmlns:gn="google:notifier" xmlns="">
  //     <Id>
  //       <ServiceUrl data="{Service_Url}" />
  //       <ServiceId data="{Service_Id}" />
  //     </Id>
  // If content needs to be sent, then the below element is added
  //     <Content>
  //       <Priority int="{Priority}" />
  //       <RequireSubscription bool="{true/false}" />
  //       <ServiceSpecificData data="{ServiceData}" />
  //       <WriteToCacheOnly bool="{true/false}" />
  //     </Content>
  //   </set>
  // </iq>
  buzz::XmlElement* iq = MakeIq(buzz::STR_GET, to_jid_bare, task_id);
  buzz::XmlElement* set = new buzz::XmlElement(kQnNotifierSet, true);
  buzz::XmlElement* id = new buzz::XmlElement(kQnId, true);
  iq->AddElement(set);
  set->AddElement(id);

  id->AddElement(MakeStringXmlElement("ServiceUrl",
                                      notification_data.service_url.c_str()));
  id->AddElement(MakeStringXmlElement("ServiceId",
                                      notification_data.service_id.c_str()));

  if (notification_data.send_content) {
    buzz::XmlElement* content = new buzz::XmlElement(kQnContent, true);
    set->AddElement(content);
    content->AddElement(MakeIntXmlElement("Priority",
                                          notification_data.priority));
    content->AddElement(
        MakeBoolXmlElement("RequireSubscription",
                           notification_data.require_subscription));
  if (!notification_data.service_specific_data.empty()) {
    content->AddElement(
        MakeStringXmlElement("ServiceSpecificData",
                             notification_data.service_specific_data.c_str()));
  }
    content->AddElement(
        MakeBoolXmlElement("WriteToCacheOnly",
                           notification_data.write_to_cache_only));
  }
  return iq;
}

}  // namespace notifier
