// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_SERVICE_IPC_SERVER_H_
#define CHROME_SERVICE_SERVICE_IPC_SERVER_H_

#include <string>

#include "base/scoped_ptr.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ipc/ipc_message.h"

// This class handles IPC commands for the service process.
class ServiceIPCServer : public IPC::Channel::Listener,
                         public IPC::Message::Sender {
 public:
  explicit ServiceIPCServer(const std::string& channel_name);
  virtual ~ServiceIPCServer();

  bool Init();

  // IPC::Message::Sender implementation.
  virtual bool Send(IPC::Message* msg);

  IPC::SyncChannel* channel() { return channel_.get(); }

  // Safe to call on any thread, as long as it's guaranteed that the thread's
  // lifetime is less than the main thread.
  IPC::SyncMessageFilter* sync_message_filter() { return sync_message_filter_; }


 private:
  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

  // IPC message handlers.
  void OnEnableCloudPrintProxy(const std::string& lsid);
  void OnEnableCloudPrintProxyWithTokens(const std::string& cloud_print_token,
                                         const std::string& talk_token);
  void OnIsCloudPrintProxyEnabled(bool* is_enabled, std::string* email);

  void OnEnableRemotingWithTokens(const std::string& login,
                                  const std::string& remoting_token,
                                  const std::string& talk_token);
  void OnDisableCloudPrintProxy();
  void OnHello();
  void OnShutdown();

  std::string channel_name_;
  scoped_ptr<IPC::SyncChannel> channel_;

  // Allows threads other than the main thread to send sync messages.
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;


  DISALLOW_COPY_AND_ASSIGN(ServiceIPCServer);
};

#endif  // CHROME_SERVICE_SERVICE_IPC_SERVER_H_
