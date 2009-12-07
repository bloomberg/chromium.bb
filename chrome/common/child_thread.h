// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_THREAD_H_
#define CHROME_COMMON_CHILD_THREAD_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/common/message_router.h"
#include "chrome/common/resource_dispatcher.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_message.h"

class NotificationService;
class SocketStreamDispatcher;

// The main thread of a child process derives from this class.
class ChildThread : public IPC::Channel::Listener,
                    public IPC::Message::Sender {
 public:
  // Creates the thread.
  ChildThread();
  // Used for single-process mode.
  explicit ChildThread(const std::string& channel_name);
  virtual ~ChildThread();

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  // See documentation on MessageRouter for AddRoute and RemoveRoute
  void AddRoute(int32 routing_id, IPC::Channel::Listener* listener);
  void RemoveRoute(int32 routing_id);

  ResourceDispatcher* resource_dispatcher() {
    return resource_dispatcher_.get();
  }

  SocketStreamDispatcher* socket_stream_dispatcher() {
    return socket_stream_dispatcher_.get();
  }

  MessageLoop* message_loop() { return message_loop_; }

  // Returns the one child thread.
  static ChildThread* current();

 protected:
  friend class ChildProcess;

  // Called when the process refcount is 0.
  void OnProcessFinalRelease();

  virtual void OnControlMessageReceived(const IPC::Message& msg) { }
  virtual void OnAskBeforeShutdown();
  virtual void OnShutdown();

#ifdef IPC_MESSAGE_LOG_ENABLED
  virtual void OnSetIPCLoggingEnabled(bool enable);
#endif

  IPC::SyncChannel* channel() { return channel_.get(); }

 private:
  void Init();

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

  std::string channel_name_;
  scoped_ptr<IPC::SyncChannel> channel_;

  // Implements message routing functionality to the consumers of ChildThread.
  MessageRouter router_;

  // Handles resource loads for this process.
  scoped_ptr<ResourceDispatcher> resource_dispatcher_;

  // Handles SocketStream for this process.
  scoped_ptr<SocketStreamDispatcher> socket_stream_dispatcher_;

  // If true, checks with the browser process before shutdown.  This avoids race
  // conditions if the process refcount is 0 but there's an IPC message inflight
  // that would addref it.
  bool check_with_browser_before_shutdown_;

  MessageLoop* message_loop_;

  scoped_ptr<NotificationService> notification_service_;

  DISALLOW_COPY_AND_ASSIGN(ChildThread);
};

#endif  // CHROME_COMMON_CHILD_THREAD_H_
