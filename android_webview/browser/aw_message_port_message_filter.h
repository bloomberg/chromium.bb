// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_MESSAGE_PORT_MESSAGE_FILTER_H_
#define ANDROID_WEBVIEW_BROWSER_MESSAGE_PORT_MESSAGE_FILTER_H_

#include "base/callback.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/message_port_delegate.h"

namespace android_webview {

// Filter for Aw specific MessagePort related IPC messages (creating and
// destroying a MessagePort, sending a message via a MessagePort etc).
class AwMessagePortMessageFilter : public content::BrowserMessageFilter,
                                   public content::MessagePortDelegate {
 public:
  explicit AwMessagePortMessageFilter(int route_id);

  // BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() const override;

  void SendAppToWebMessage(int msg_port_route_id,
                           const base::string16& message,
                           const std::vector<int>& sent_message_port_ids);
  void SendClosePortMessage(int message_port_id);

  // MessagePortDelegate implementation.
  void SendMessage(
      int msg_port_route_id,
      const content::MessagePortMessage& message,
      const std::vector<content::TransferredMessagePort>& sent_message_ports)
      override;
  void SendMessagesAreQueued(int route_id) override;
 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<AwMessagePortMessageFilter>;

  void OnConvertedAppToWebMessage(
      int msg_port_id,
      const base::string16& message,
      const std::vector<int>& sent_message_port_ids);
  void OnClosePortAck(int message_port_id);

  ~AwMessagePortMessageFilter() override;

  int route_id_;

  DISALLOW_COPY_AND_ASSIGN(AwMessagePortMessageFilter);
};

}   // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_MESSAGE_PORT_MESSAGE_FILTER_H_
