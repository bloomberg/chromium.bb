// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CHILD_THREAD_H_
#define CONTENT_COMMON_CHILD_THREAD_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "content/common/message_router.h"
#include "webkit/glue/resource_loader_bridge.h"

class FileSystemDispatcher;
class MessageLoop;
class NotificationService;
class ResourceDispatcher;
class SocketStreamDispatcher;

namespace IPC {
class SyncChannel;
class SyncMessageFilter;
}

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

  IPC::Channel::Listener* ResolveRoute(int32 routing_id);

  // Creates a ResourceLoaderBridge.
  // Tests can override this method if they want a custom loading behavior.
  virtual webkit_glue::ResourceLoaderBridge* CreateBridge(
      const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info);

  ResourceDispatcher* resource_dispatcher();

  SocketStreamDispatcher* socket_stream_dispatcher() {
    return socket_stream_dispatcher_.get();
  }

  FileSystemDispatcher* file_system_dispatcher() const {
    return file_system_dispatcher_.get();
  }

  // Safe to call on any thread, as long as it's guaranteed that the thread's
  // lifetime is less than the main thread.
  IPC::SyncMessageFilter* sync_message_filter();

  MessageLoop* message_loop();

  // Returns the one child thread.
  static ChildThread* current();

 protected:
  friend class ChildProcess;

  // Called when the process refcount is 0.
  void OnProcessFinalRelease();

  virtual bool OnControlMessageReceived(const IPC::Message& msg);
  virtual void OnAskBeforeShutdown();
  virtual void OnShutdown();

#ifdef IPC_MESSAGE_LOG_ENABLED
  virtual void OnSetIPCLoggingEnabled(bool enable);
#endif

  IPC::SyncChannel* channel() { return channel_.get(); }

  void set_on_channel_error_called(bool on_channel_error_called) {
    on_channel_error_called_ = on_channel_error_called;
  }

 private:
  void Init();

  // IPC::Channel::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

  std::string channel_name_;
  scoped_ptr<IPC::SyncChannel> channel_;

  // Allows threads other than the main thread to send sync messages.
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;

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

  // The OnChannelError() callback was invoked - the channel is dead, don't
  // attempt to communicate.
  bool on_channel_error_called_;

  MessageLoop* message_loop_;

  scoped_ptr<NotificationService> notification_service_;

  scoped_ptr<FileSystemDispatcher> file_system_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(ChildThread);
};

#endif  // CONTENT_COMMON_CHILD_THREAD_H_
