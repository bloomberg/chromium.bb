// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_THREAD_IMPL_H_
#define CONTENT_CHILD_CHILD_THREAD_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/power_monitor/power_monitor.h"
#include "base/tracked_objects.h"
#include "content/child/mojo/mojo_application.h"
#include "content/common/content_export.h"
#include "content/common/message_router.h"
#include "content/public/child/child_thread.h"
#include "ipc/ipc_message.h"  // For IPC_MESSAGE_LOG_ENABLED.

namespace base {
class MessageLoop;

namespace trace_event {
class TraceMemoryController;
}  // namespace trace_event
}  // namespace base

namespace IPC {
class MessageFilter;
class ScopedIPCSupport;
class SyncChannel;
class SyncMessageFilter;
}  // namespace IPC

namespace blink {
class WebFrame;
}  // namespace blink

namespace content {
class ChildMessageFilter;
class ChildDiscardableSharedMemoryManager;
class ChildGpuMemoryBufferManager;
class ChildHistogramMessageFilter;
class ChildResourceMessageFilter;
class ChildSharedBitmapManager;
class FileSystemDispatcher;
class NavigatorConnectDispatcher;
class NotificationDispatcher;
class PushDispatcher;
class ServiceWorkerMessageFilter;
class QuotaDispatcher;
class QuotaMessageFilter;
class ResourceDispatcher;
class ThreadSafeSender;
class WebSocketDispatcher;
struct RequestInfo;

// The main thread of a child process derives from this class.
class CONTENT_EXPORT ChildThreadImpl
    : public IPC::Listener,
      virtual public ChildThread {
 public:
  struct CONTENT_EXPORT Options;

  // Creates the thread.
  ChildThreadImpl();
  // Allow to be used for single-process mode and for in process gpu mode via
  // options.
  explicit ChildThreadImpl(const Options& options);
  // ChildProcess::main_thread() is reset after Shutdown(), and before the
  // destructor, so any subsystem that relies on ChildProcess::main_thread()
  // must be terminated before Shutdown returns. In particular, if a subsystem
  // has a thread that post tasks to ChildProcess::main_thread(), that thread
  // should be joined in Shutdown().
  ~ChildThreadImpl() override;
  virtual void Shutdown();

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  // ChildThread implementation:
#if defined(OS_WIN)
  void PreCacheFont(const LOGFONT& log_font) override;
  void ReleaseCachedFonts() override;
#endif

  IPC::SyncChannel* channel() { return channel_.get(); }

  MessageRouter* GetRouter();

  // Allocates a block of shared memory of the given size. Returns NULL on
  // failure.
  // Note: On posix, this requires a sync IPC to the browser process,
  // but on windows the child process directly allocates the block.
  scoped_ptr<base::SharedMemory> AllocateSharedMemory(size_t buf_size);

  // A static variant that can be called on background threads provided
  // the |sender| passed in is safe to use on background threads.
  static scoped_ptr<base::SharedMemory> AllocateSharedMemory(
      size_t buf_size,
      IPC::Sender* sender);

  ChildSharedBitmapManager* shared_bitmap_manager() const {
    return shared_bitmap_manager_.get();
  }

  ChildGpuMemoryBufferManager* gpu_memory_buffer_manager() const {
    return gpu_memory_buffer_manager_.get();
  }

  ChildDiscardableSharedMemoryManager* discardable_shared_memory_manager()
      const {
    return discardable_shared_memory_manager_.get();
  }

  ResourceDispatcher* resource_dispatcher() const {
    return resource_dispatcher_.get();
  }

  WebSocketDispatcher* websocket_dispatcher() const {
    return websocket_dispatcher_.get();
  }

  FileSystemDispatcher* file_system_dispatcher() const {
    return file_system_dispatcher_.get();
  }

  QuotaDispatcher* quota_dispatcher() const {
    return quota_dispatcher_.get();
  }

  NotificationDispatcher* notification_dispatcher() const {
    return notification_dispatcher_.get();
  }

  PushDispatcher* push_dispatcher() const {
    return push_dispatcher_.get();
  }

  IPC::SyncMessageFilter* sync_message_filter() const {
    return sync_message_filter_.get();
  }

  // The getter should only be called on the main thread, however the
  // IPC::Sender it returns may be safely called on any thread including
  // the main thread.
  ThreadSafeSender* thread_safe_sender() const {
    return thread_safe_sender_.get();
  }

  ChildHistogramMessageFilter* child_histogram_message_filter() const {
    return histogram_message_filter_.get();
  }

  ServiceWorkerMessageFilter* service_worker_message_filter() const {
    return service_worker_message_filter_.get();
  }

  QuotaMessageFilter* quota_message_filter() const {
    return quota_message_filter_.get();
  }

  ChildResourceMessageFilter* child_resource_message_filter() const {
    return resource_message_filter_.get();
  }

  base::MessageLoop* message_loop() const { return message_loop_; }

  // Returns the one child thread. Can only be called on the main thread.
  static ChildThreadImpl* current();

#if defined(OS_ANDROID)
  // Called on Android's service thread to shutdown the main thread of this
  // process.
  static void ShutdownThread();
#endif

  ServiceRegistry* service_registry() const {
    return mojo_application_->service_registry();
  }

 protected:
  friend class ChildProcess;

  // Called when the process refcount is 0.
  void OnProcessFinalRelease();

  virtual bool OnControlMessageReceived(const IPC::Message& msg);

  void set_on_channel_error_called(bool on_channel_error_called) {
    on_channel_error_called_ = on_channel_error_called;
  }

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelConnected(int32 peer_pid) override;
  void OnChannelError() override;

 private:
  class ChildThreadMessageRouter : public MessageRouter {
   public:
    // |sender| must outlive this object.
    explicit ChildThreadMessageRouter(IPC::Sender* sender);
    bool Send(IPC::Message* msg) override;

   private:
    IPC::Sender* const sender_;
  };

  void Init(const Options& options);

  // We create the channel first without connecting it so we can add filters
  // prior to any messages being received, then connect it afterwards.
  void ConnectChannel(bool use_mojo_channel);

  // IPC message handlers.
  void OnShutdown();
  void OnSetProfilerStatus(tracked_objects::ThreadData::Status status);
  void OnGetChildProfilerData(int sequence_number);
  void OnDumpHandles();
  void OnProcessBackgrounded(bool background);
#ifdef IPC_MESSAGE_LOG_ENABLED
  void OnSetIPCLoggingEnabled(bool enable);
#endif
#if defined(USE_TCMALLOC)
  void OnGetTcmallocStats();
#endif

  void EnsureConnected();

  class SingleProcessChannelDelegate;
  class SingleProcessChannelDelegateDeleter {
   public:
    void operator()(SingleProcessChannelDelegate* delegate) const;
  };

  scoped_ptr<IPC::ScopedIPCSupport> ipc_support_;
  scoped_ptr<SingleProcessChannelDelegate, SingleProcessChannelDelegateDeleter>
      single_process_channel_delegate_;
  scoped_ptr<MojoApplication> mojo_application_;

  std::string channel_name_;
  scoped_ptr<IPC::SyncChannel> channel_;

  // Allows threads other than the main thread to send sync messages.
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  // Implements message routing functionality to the consumers of
  // ChildThreadImpl.
  ChildThreadMessageRouter router_;

  // Handles resource loads for this process.
  scoped_ptr<ResourceDispatcher> resource_dispatcher_;

  scoped_ptr<WebSocketDispatcher> websocket_dispatcher_;

  // The OnChannelError() callback was invoked - the channel is dead, don't
  // attempt to communicate.
  bool on_channel_error_called_;

  base::MessageLoop* message_loop_;

  scoped_ptr<FileSystemDispatcher> file_system_dispatcher_;

  scoped_ptr<QuotaDispatcher> quota_dispatcher_;

  scoped_refptr<ChildHistogramMessageFilter> histogram_message_filter_;

  scoped_refptr<ChildResourceMessageFilter> resource_message_filter_;

  scoped_refptr<ServiceWorkerMessageFilter> service_worker_message_filter_;

  scoped_refptr<QuotaMessageFilter> quota_message_filter_;

  scoped_refptr<NotificationDispatcher> notification_dispatcher_;

  scoped_refptr<PushDispatcher> push_dispatcher_;

  scoped_ptr<ChildSharedBitmapManager> shared_bitmap_manager_;

  scoped_ptr<ChildGpuMemoryBufferManager> gpu_memory_buffer_manager_;

  scoped_ptr<ChildDiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;

  // Observes the trace event system. When tracing is enabled, optionally
  // starts profiling the tcmalloc heap.
  scoped_ptr<base::trace_event::TraceMemoryController> trace_memory_controller_;

  scoped_ptr<base::PowerMonitor> power_monitor_;

  scoped_refptr<ChildMessageFilter> geofencing_message_filter_;
  scoped_refptr<ChildMessageFilter> bluetooth_message_filter_;

  scoped_refptr<NavigatorConnectDispatcher> navigator_connect_dispatcher_;

  bool in_browser_process_;

  base::WeakPtrFactory<ChildThreadImpl> channel_connected_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChildThreadImpl);
};

struct ChildThreadImpl::Options {
  ~Options();

  class Builder;

  std::string channel_name;
  bool use_mojo_channel;
  bool in_browser_process;
  std::vector<IPC::MessageFilter*> startup_filters;

 private:
  Options();
};

class ChildThreadImpl::Options::Builder {
 public:
  Builder();

  Builder& InBrowserProcess(bool in_browser_process);
  Builder& UseMojoChannel(bool use_mojo_channel);
  Builder& WithChannelName(const std::string& channel_name);
  Builder& AddStartupFilter(IPC::MessageFilter* filter);

  Options Build();

 private:
  struct Options options_;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_THREAD_IMPL_H_
