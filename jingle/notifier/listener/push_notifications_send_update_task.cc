// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/push_notifications_send_update_task.h"

#include <string>

#include "base/base64.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "jingle/notifier/listener/notification_constants.h"
#include "jingle/notifier/listener/xml_element_util.h"
#include "talk/xmllite/qname.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/jid.h"
#include "talk/xmpp/xmppclient.h"

namespace notifier {

PushNotificationsSendUpdateTask::PushNotificationsSendUpdateTask(
    TaskParent* parent, const Notification& notification)
    : XmppTask(parent), notification_(notification) {}

PushNotificationsSendUpdateTask::~PushNotificationsSendUpdateTask() {}

int PushNotificationsSendUpdateTask::ProcessStart() {
  scoped_ptr<buzz::XmlElement> stanza(
      MakeUpdateMessage(notification_,
                        GetClient()->jid().BareJid()));
  VLOG(1) << "P2P: Sending notification: " << XmlElementToString(*stanza);
  if (SendStanza(stanza.get()) != buzz::XMPP_RETURN_OK) {
    LOG(WARNING) << "Could not send: " << XmlElementToString(*stanza);
  }
  return STATE_DONE;
}

buzz::XmlElement* PushNotificationsSendUpdateTask::MakeUpdateMessage(
    const Notification& notification,
    const buzz::Jid& to_jid_bare) {
  DCHECK(to_jid_bare.IsBare());
  const buzz::QName kQnPush(kPushNotificationsNamespace, "push");
  const buzz::QName kQnChannel(buzz::STR_EMPTY, "channel");
  const buzz::QName kQnData(buzz::STR_EMPTY, "data");

  // Create our update stanza. The message is constructed as:
  // <message from='{fullJid}' to='{bareJid}' type='headline'>
  //   <push xmlns='google:push' channel='{channel}'>
  //     <data>{base-64 encoded data}</data>
  //   </push>
  // </message>

  buzz::XmlElement* message = new buzz::XmlElement(buzz::QN_MESSAGE);
  message->AddAttr(buzz::QN_TO, to_jid_bare.Str());
  message->AddAttr(buzz::QN_TYPE, "headline");

  buzz::XmlElement* push = new buzz::XmlElement(kQnPush, true);
  push->AddAttr(kQnChannel, notification.channel);
  message->AddElement(push);

  buzz::XmlElement* data = new buzz::XmlElement(kQnData, true);
  std::string base64_data;
  if (!base::Base64Encode(notification.data, &base64_data)) {
    LOG(WARNING) << "Could not encode data: " << notification.data;
  }
  data->SetBodyText(base64_data);
  push->AddElement(data);

  return message;
}

}  // namespace notifier
