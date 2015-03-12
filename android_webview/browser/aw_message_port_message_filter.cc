// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_message_port_message_filter.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/common/aw_message_port_messages.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/common/message_port_types.h"

using content::BrowserThread;
using content::MessagePortProvider;

namespace android_webview {

AwMessagePortMessageFilter::AwMessagePortMessageFilter(int route_id)
    : BrowserMessageFilter(AwMessagePortMsgStart), route_id_(route_id) {
}

AwMessagePortMessageFilter::~AwMessagePortMessageFilter() {
}

void AwMessagePortMessageFilter::OnChannelClosing() {
  MessagePortProvider::OnMessagePortDelegateClosing(this);
  AwBrowserContext::GetDefault()->GetMessagePortService()->
      OnMessagePortMessageFilterClosing(this);
}

bool AwMessagePortMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AwMessagePortMessageFilter, message)
    IPC_MESSAGE_FORWARD(AwMessagePortHostMsg_ConvertedWebToAppMessage,
                        AwBrowserContext::GetDefault()->GetMessagePortService(),
                        AwMessagePortService::OnConvertedWebToAppMessage)
    IPC_MESSAGE_HANDLER(AwMessagePortHostMsg_ConvertedAppToWebMessage,
                        OnConvertedAppToWebMessage)
    IPC_MESSAGE_HANDLER(AwMessagePortHostMsg_ClosePortAck, OnClosePortAck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AwMessagePortMessageFilter::OnConvertedAppToWebMessage(
    int msg_port_id,
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids) {
  std::vector<content::TransferredMessagePort>
      sent_ports(sent_message_port_ids.size());
  for (size_t i = 0; i < sent_message_port_ids.size(); ++i)
    sent_ports[i].id = sent_message_port_ids[i];
  // TODO(mek): Bypass the extra roundtrip and just send the unconverted message
  // to the renderer directly.
  MessagePortProvider::PostMessageToPort(msg_port_id,
      content::MessagePortMessage(message),
      sent_ports);
}

void AwMessagePortMessageFilter::OnClosePortAck(int message_port_id) {
  MessagePortProvider::ClosePort(message_port_id);
  AwBrowserContext::GetDefault()->GetMessagePortService()->
      CleanupPort(message_port_id);
}

void AwMessagePortMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void AwMessagePortMessageFilter::SendAppToWebMessage(
    int msg_port_route_id,
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids) {
  Send(new AwMessagePortMsg_AppToWebMessage(
      route_id_,
      msg_port_route_id, // same as the port id
      message, sent_message_port_ids));
}

void AwMessagePortMessageFilter::SendClosePortMessage(int message_port_id) {
  Send(new AwMessagePortMsg_ClosePort(route_id_, message_port_id));
}

void AwMessagePortMessageFilter::SendMessage(
    int msg_port_route_id,
    const content::MessagePortMessage& message,
    const std::vector<content::TransferredMessagePort>& sent_message_ports) {
  DCHECK(message.is_string());
  std::vector<int> sent_message_port_ids(sent_message_ports.size());
  for (size_t i = 0; i < sent_message_ports.size(); ++i) {
    DCHECK(!sent_message_ports[i].send_messages_as_values);
    int sent_port_id = sent_message_ports[i].id;
    MessagePortProvider::HoldMessages(sent_port_id);
    MessagePortProvider::UpdateMessagePort(sent_port_id, this);
    sent_message_port_ids[i] = sent_port_id;
  }
  Send(new AwMessagePortMsg_WebToAppMessage(
      route_id_,
      msg_port_route_id, // same as the port id
      message.message_as_string, sent_message_port_ids));
}

void AwMessagePortMessageFilter::SendMessagesAreQueued(int route_id) {
  // TODO(sgurun) implement
  NOTREACHED();
}

}   // namespace android_webview
