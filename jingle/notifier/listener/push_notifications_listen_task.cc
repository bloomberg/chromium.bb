// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/push_notifications_listen_task.h"

#include "base/base64.h"
#include "base/logging.h"
#include "jingle/notifier/listener/notification_constants.h"
#include "jingle/notifier/listener/notification_defines.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "talk/base/task.h"
#include "talk/xmllite/qname.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/xmppengine.h"

namespace notifier {

PushNotificationsListenTask::PushNotificationsListenTask(
    Task* parent, Delegate* delegate)
        : buzz::XmppTask(parent, buzz::XmppEngine::HL_TYPE),
          delegate_(delegate) {
  DCHECK(delegate_);
}

PushNotificationsListenTask::~PushNotificationsListenTask() {
}

int PushNotificationsListenTask::ProcessStart() {
  LOG(INFO) << "Push notifications: Listener task started.";
  return STATE_RESPONSE;
}

int PushNotificationsListenTask::ProcessResponse() {
  LOG(INFO) << "Push notifications: Listener response received.";
  const buzz::XmlElement* stanza = NextStanza();
  if (stanza == NULL) {
    return STATE_BLOCKED;
  }
  // The push notifications service does not need us to acknowledge receipt of
  // the notification to the buzz server.

  // TODO(sanjeevr): Write unittests to cover this.
  // Extract the service URL and service-specific data from the stanza.
  // Note that we treat the channel name as service URL.
  // The response stanza has the following format.
  // <message from=’someservice.google.com’ to={full_jid}>
  //  <push xmlns=’google:push’ channel={channel name}>
  //    <recipient to={bare_jid}>{base-64 encoded data}</recipient>
  //    <data>{base-64 data}</data>
  //  </push>
  // </message>

  const buzz::XmlElement* push_element =
      stanza->FirstNamed(buzz::QName(kPushNotificationsNamespace, "push"));
  if (push_element) {
    IncomingNotificationData notification_data;
    notification_data.service_url =
        push_element->Attr(buzz::QName(buzz::STR_EMPTY, "channel"));
    const buzz::XmlElement* data_element = push_element->FirstNamed(
        buzz::QName(kPushNotificationsNamespace, "data"));
    if (data_element) {
      base::Base64Decode(data_element->BodyText(),
                         &notification_data.service_specific_data);
    }
    delegate_->OnNotificationReceived(notification_data);
  } else {
    LOG(WARNING) <<
        "Push notifications: No push element found in response stanza";
  }
  return STATE_RESPONSE;
}

bool PushNotificationsListenTask::HandleStanza(const buzz::XmlElement* stanza) {
  LOG(INFO) << "Push notifications: Stanza received: "
      << XmlElementToString(*stanza);
  if (IsValidNotification(stanza)) {
    QueueStanza(stanza);
    return true;
  }
  return false;
}

bool PushNotificationsListenTask::IsValidNotification(
    const buzz::XmlElement* stanza) {
  // An update notification has the following form.
  // <message from=’someservice.google.com’ to={full_jid}>
  //  <push xmlns=’google:push’ channel={channel name}>
  //    <recipient to={bare_jid}>{base-64 encoded data}</recipient>
  //    <data>{base-64 data}</data>
  //  </push>
  // </message>
  return ((stanza->Name() == buzz::QN_MESSAGE) &&
          (stanza->Attr(buzz::QN_TO) == GetClient()->jid().Str()));
}

}  // namespace notifier

