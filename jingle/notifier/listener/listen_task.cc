// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/listen_task.h"

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

ListenTask::ListenTask(Task* parent)
    : buzz::XmppTask(parent, buzz::XmppEngine::HL_TYPE) {
}

ListenTask::~ListenTask() {
}

int ListenTask::ProcessStart() {
  VLOG(1) << "P2P: Listener task started.";
  return STATE_RESPONSE;
}

int ListenTask::ProcessResponse() {
  VLOG(1) << "P2P: Listener response received.";
  const buzz::XmlElement* stanza = NextStanza();
  if (stanza == NULL)
    return STATE_BLOCKED;
  // Acknowledge receipt of the notification to the buzz server.
  scoped_ptr<buzz::XmlElement> response_stanza(MakeIqResult(stanza));
  SendStanza(response_stanza.get());

  // TODO(akalin): Write unittests to cover this.
  // Extract the service URL and service-specific data from the stanza.
  // The response stanza has the following format.
  //  <iq from="{bare_jid}" to="{full_jid}" id="#" type="set">
  //    <not:getAll xmlns:not="google:notifier">
  //      <Timestamp long="#" xmlns=""/>
  //      <Result xmlns="">
  //        <Id>
  //          <ServiceUrl data="{service_url}"/>
  //          <ServiceId data="{service_id}"/>
  //        </Id>
  //        <Timestamp long="#"/>
  //        <Content>
  //          <Priority int="#"/>
  //          <ServiceSpecificData data="{service_specific_data}"/>
  //          <RequireSubscription bool="true"/>
  //        </Content>
  //        <State>
  //          <Type int="#"/>
  //          <Read bool="{true/false}"/>
  //        </State>
  //        <ClientActive bool="{true/false}"/>
  //      </Result>
  //    </not:getAll>
  //  </iq> "
  // Note that there can be multiple "Result" elements, so we need to loop
  // through all of them.
  bool update_signaled = false;
  const buzz::QName kQnNotifierGetAll(kNotifierNamespace, "getAll");
  const buzz::XmlElement* get_all_element =
      stanza->FirstNamed(kQnNotifierGetAll);
  if (get_all_element) {
    const buzz::XmlElement* result_element =
        get_all_element->FirstNamed(
            buzz::QName(buzz::STR_EMPTY, "Result"));
    while (result_element) {
      IncomingNotificationData notification_data;
      const buzz::XmlElement* id_element =
          result_element->FirstNamed(buzz::QName(buzz::STR_EMPTY, "Id"));
      if (id_element) {
        const buzz::XmlElement* service_url_element =
            id_element->FirstNamed(
                buzz::QName(buzz::STR_EMPTY, "ServiceUrl"));
        if (service_url_element) {
          notification_data.service_url = service_url_element->Attr(
              buzz::QName(buzz::STR_EMPTY, "data"));
        }
      }
      const buzz::XmlElement* content_element =
          result_element->FirstNamed(
              buzz::QName(buzz::STR_EMPTY, "Content"));
      if (content_element) {
        const buzz::XmlElement* service_data_element =
            content_element->FirstNamed(
                buzz::QName(buzz::STR_EMPTY, "ServiceSpecificData"));
        if (service_data_element) {
          notification_data.service_specific_data = service_data_element->Attr(
              buzz::QName(buzz::STR_EMPTY, "data"));
        }
      }
      // Inform listeners that a notification has been received.
      SignalUpdateAvailable(notification_data);
      update_signaled = true;
      // Now go to the next Result element
      result_element = result_element->NextNamed(
          buzz::QName(buzz::STR_EMPTY, "Result"));
    }
  }
  if (!update_signaled) {
    LOG(WARNING) <<
        "No getAll element or Result element found in response stanza";
    // Signal an empty update to preserve old behavior
    SignalUpdateAvailable(IncomingNotificationData());
  }
  return STATE_RESPONSE;
}

bool ListenTask::HandleStanza(const buzz::XmlElement* stanza) {
  VLOG(1) << "P2P: Stanza received: " << XmlElementToString(*stanza);
  // TODO(akalin): Do more verification on stanza depending on
  // the sync notification method
  if (IsValidNotification(stanza)) {
    QueueStanza(stanza);
    return true;
  }
  return false;
}

bool ListenTask::IsValidNotification(const buzz::XmlElement* stanza) {
  // An update notificaiton has the following form.
  //  <cli:iq from="{bare_jid}" to="{full_jid}"
  //      id="#" type="set" xmlns:cli="jabber:client">
  //    <not:getAll xmlns:not="google:notifier">
  //      <Timestamp long="#" xmlns=""/>
  //    </not:getAll>
  //  </cli:iq>
  //
  // We deliberately minimize the verification we do here, though: see
  // http://crbug.com/71285 .
  const buzz::QName kQnNotifierGetAll(kNotifierNamespace, "getAll");
  return MatchRequestIq(stanza, buzz::STR_SET, kQnNotifierGetAll);
}

}  // namespace notifier
