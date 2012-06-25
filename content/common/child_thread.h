// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CHILD_THREAD_H_
#define CONTENT_COMMON_CHILD_THREAD_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/tracked_objects.h"
#include "content/common/content_export.h"
#include "content/common/message_router.h"
#include "ipc/ipc_message.h"  // For IPC_MESSAGE_LOG_ENABLED.
#include "webkit/glue/resource_loader_bridge.h"

class FileSystemDispatcher;
class MessageLoop;
class QuotaDispatcher;
class ResourceDispatcher;
class SocketStreamDispatcher;

namespace IPC {
class SyncChannel;
class SyncMessageFilter;
}

namespace WebKit {
class WebFrame;
}

// The main thread of a child process derives from this class.
class CONTENT_EXPORT ChildThread : public IPC::Listener, public IPC::Sender {
 public:
  // Creates the thread.
  ChildThread();
  // Used for single-process mode.
  explicit ChildThread(const std::string& channel_name);
  virtual ~ChildThread();

  // IPC::Sender implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // See documentation on MessageRouter for AddRoute and RemoveRoute
  void AddRoute(int32 routing_id, IPC::Listener* listener);
  void RemoveRoute(int32 routing_id);

  IPC::Listener* ResolveRoute(int32 routing_id);

  IPC::SyncChannel* channel() { return channel_.get(); }

  // Creates a ResourceLoaderBridge.
  // Tests can override this method if they want a custom loading behavior.
  virtual webkit_glue::ResourceLoaderBridge* CreateBridge(
      const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info);

  // Allocates a block of shared memory of the given size and
  // maps in into the address space. Returns NULL of failure.
  // Note: On posix, this requires a sync IPC to the browser process,
  // but on windows the child process directly allocates the block.
  base::SharedMemory* AllocateSharedMemory(size_t buf_size);

  ResourceDispatcher* resource_dispatcher();

  SocketStreamDispatcher* socket_stream_dispatcher() {
    return socket_stream_dispatcher_.get();
  }

  FileSystemDispatcher* file_system_dispatcher() const {
    return file_system_dispatcher_.get();
  }

  QuotaDispatcher* quota_dispatcher() const {
    return quota_dispatcher_.get();
  }

  // Safe to call on any thread, as long as it's guaranteed that the thread's
  // lifetime is less than the main thread.
  IPC::SyncMessageFilter* sync_message_filter();

  MessageLoop* message_loop();

  // Returns the one child thread.
  static ChildThread* current();

  virtual bool IsWebFrameValid(WebKit::WebFrame* frame);

 protected:
  friend class ChildProcess;

  // Called when the process refcount is 0.
  void OnProcessFinalRelease();

  virtual bool OnControlMessageReceived(const IPC::Message& msg);
  virtual void OnShutdown();

#ifdef IPC_MESSAGE_LOG_ENABLED
  virtual void OnSetIPCLoggingEnabled(bool enable);
#endif

  virtual void OnSetProfilerStatus(tracked_objects::ThreadData::Status status);
  virtual void OnGetChildProfilerData(int sequence_number);

  virtual void OnDumpHandles();

  void set_on_channel_error_called(bool on_channel_error_called) {
    on_channel_error_called_ = on_channel_error_called;
  }

 private:
  void Init();

  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

#if defined(USE_TCMALLOC)
  void OnGetTcmallocStats();
#endif

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

  // The OnChannelError() callback was invoked - the channel is dead, don't
  // attempt to communicate.
  bool on_channel_error_called_;

  MessageLoop* message_loop_;

  scoped_ptr<FileSystemDispatcher> file_system_dispatcher_;

  scoped_ptr<QuotaDispatcher> quota_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(ChildThread);
};

#endif  // CONTENT_COMMON_CHILD_THREAD_H_
