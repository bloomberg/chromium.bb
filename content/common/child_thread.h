// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CHILD_THREAD_H_
#define CONTENT_COMMON_CHILD_THREAD_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/shared_memory.h"
#include "base/tracked_objects.h"
#include "content/common/content_export.h"
#include "content/common/message_router.h"
#include "ipc/ipc_message.h"  // For IPC_MESSAGE_LOG_ENABLED.
#include "webkit/glue/resource_loader_bridge.h"

namespace base {
class MessageLoop;
}

namespace IPC {
class SyncChannel;
class SyncMessageFilter;
}

namespace WebKit {
class WebFrame;
}

namespace content {
class ChildHistogramMessageFilter;
class FileSystemDispatcher;
class QuotaDispatcher;
class ResourceDispatcher;
class SocketStreamDispatcher;
class ThreadSafeSender;

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

  // A static variant that can be called on background threads provided
  // the |sender| passed in is safe to use on background threads.
  static base::SharedMemory* AllocateSharedMemory(size_t buf_size,
                                                  IPC::Sender* sender);

  ResourceDispatcher* resource_dispatcher() const {
    return resource_dispatcher_.get();
  }

  SocketStreamDispatcher* socket_stream_dispatcher() const {
    return socket_stream_dispatcher_.get();
  }

  FileSystemDispatcher* file_system_dispatcher() const {
    return file_system_dispatcher_.get();
  }

  QuotaDispatcher* quota_dispatcher() const {
    return quota_dispatcher_.get();
  }

  // Safe to call on any thread, as long as it's guaranteed that the thread's
  // lifetime is less than the main thread. The |filter| returned may only
  // be used on background threads.
  IPC::SyncMessageFilter* sync_message_filter() const {
    return sync_message_filter_;
  }

  // The getter should only be called on the main thread, however the
  // IPC::Sender it returns may be safely called on any thread including
  // the main thread.
  ThreadSafeSender* thread_safe_sender() const {
    return thread_safe_sender_;
  }

  ChildHistogramMessageFilter* child_histogram_message_filter() const {
    return histogram_message_filter_.get();
  }

  base::MessageLoop* message_loop() const { return message_loop_; }

  // Returns the one child thread.
  static ChildThread* current();

  virtual bool IsWebFrameValid(WebKit::WebFrame* frame);

 protected:
  friend class ChildProcess;

  // Called when the process refcount is 0.
  void OnProcessFinalRelease();

  virtual bool OnControlMessageReceived(const IPC::Message& msg);

  void set_on_channel_error_called(bool on_channel_error_called) {
    on_channel_error_called_ = on_channel_error_called;
  }

  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

 private:
  void Init();

  // IPC message handlers.
  void OnShutdown();
  void OnSetProfilerStatus(tracked_objects::ThreadData::Status status);
  void OnGetChildProfilerData(int sequence_number);
  void OnDumpHandles();
#ifdef IPC_MESSAGE_LOG_ENABLED
  void OnSetIPCLoggingEnabled(bool enable);
#endif
#if defined(USE_TCMALLOC)
  void OnGetTcmallocStats();
#endif

  void EnsureConnected();

  std::string channel_name_;
  scoped_ptr<IPC::SyncChannel> channel_;

  // Allows threads other than the main thread to send sync messages.
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  // Implements message routing functionality to the consumers of ChildThread.
  MessageRouter router_;

  // Handles resource loads for this process.
  scoped_ptr<ResourceDispatcher> resource_dispatcher_;

  // Handles SocketStream for this process.
  scoped_ptr<SocketStreamDispatcher> socket_stream_dispatcher_;

  // The OnChannelError() callback was invoked - the channel is dead, don't
  // attempt to communicate.
  bool on_channel_error_called_;

  base::MessageLoop* message_loop_;

  scoped_ptr<FileSystemDispatcher> file_system_dispatcher_;

  scoped_ptr<QuotaDispatcher> quota_dispatcher_;

  scoped_refptr<ChildHistogramMessageFilter> histogram_message_filter_;

  base::WeakPtrFactory<ChildThread> channel_connected_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildThread);
};

}  // namespace content

#endif  // CONTENT_COMMON_CHILD_THREAD_H_
