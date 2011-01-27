// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SOCKET_STREAM_DISPATCHER_H_
#define CHROME_COMMON_SOCKET_STREAM_DISPATCHER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "ipc/ipc_channel.h"

namespace WebKit {
class WebSocketStreamHandle;
}

namespace webkit_glue {
class WebSocketStreamHandleBridge;
class WebSocketStreamHandleDelegate;
}

// Dispatches socket stream related messages sent to a child process from the
// main browser process.  There is one instance per child process.  Messages
// are dispatched on the main child thread.  The RenderThread class
// creates an instance of SocketStreamDispatcher and delegates calls to it.
class SocketStreamDispatcher : public IPC::Channel::Listener {
 public:
  SocketStreamDispatcher();
  virtual ~SocketStreamDispatcher() {}

  static webkit_glue::WebSocketStreamHandleBridge* CreateBridge(
      WebKit::WebSocketStreamHandle* handle,
      webkit_glue::WebSocketStreamHandleDelegate* delegate);

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  void OnConnected(int socket_id, int max_amount_send_allowed);
  void OnSentData(int socket_id, int amount_sent);
  void OnReceivedData(int socket_id, const std::vector<char>& data);
  void OnClosed(int socket_id);

  DISALLOW_COPY_AND_ASSIGN(SocketStreamDispatcher);
};

#endif  // CHROME_COMMON_SOCKET_STREAM_DISPATCHER_H_
