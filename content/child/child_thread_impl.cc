// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_thread_impl.h"

#include <signal.h>

#include <string>

#include "base/allocator/allocator_extension.h"
#include "base/base_switches.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/timer_slack.h"
#include "base/metrics/field_trial.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local.h"
#include "base/tracked_objects.h"
#include "components/tracing/child_trace_message_filter.h"
#include "content/child/bluetooth/bluetooth_message_filter.h"
#include "content/child/child_discardable_shared_memory_manager.h"
#include "content/child/child_gpu_memory_buffer_manager.h"
#include "content/child/child_histogram_message_filter.h"
#include "content/child/child_process.h"
#include "content/child/child_resource_message_filter.h"
#include "content/child/child_shared_bitmap_manager.h"
#include "content/child/fileapi/file_system_dispatcher.h"
#include "content/child/fileapi/webfilesystem_impl.h"
#include "content/child/geofencing/geofencing_message_filter.h"
#include "content/child/mojo/mojo_application.h"
#include "content/child/navigator_connect/navigator_connect_dispatcher.h"
#include "content/child/notifications/notification_dispatcher.h"
#include "content/child/power_monitor_broadcast_source.h"
#include "content/child/push_messaging/push_dispatcher.h"
#include "content/child/quota_dispatcher.h"
#include "content/child/quota_message_filter.h"
#include "content/child/resource_dispatcher.h"
#include "content/child/service_worker/service_worker_message_filter.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/websocket_dispatcher.h"
#include "content/common/child_process_messages.h"
#include "content/common/mojo/channel_init.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_switches.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ipc/mojo/ipc_channel_mojo.h"
#include "ipc/mojo/scoped_ipc_support.h"

#if defined(OS_WIN)
#include "content/common/handle_enumerator_win.h"
#endif

#if defined(TCMALLOC_TRACE_MEMORY_SUPPORTED)
#include "third_party/tcmalloc/chromium/src/gperftools/heap-profiler.h"
#endif

using tracked_objects::ThreadData;

namespace content {
namespace {

// How long to wait for a connection to the browser process before giving up.
const int kConnectionTimeoutS = 15;

base::LazyInstance<base::ThreadLocalPointer<ChildThreadImpl> > g_lazy_tls =
    LAZY_INSTANCE_INITIALIZER;

// This isn't needed on Windows because there the sandbox's job object
// terminates child processes automatically. For unsandboxed processes (i.e.
// plugins), PluginThread has EnsureTerminateMessageFilter.
#if defined(OS_POSIX)

#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
// A thread delegate that waits for |duration| and then exits the process with
// _exit(0).
class WaitAndExitDelegate : public base::PlatformThread::Delegate {
 public:
  explicit WaitAndExitDelegate(base::TimeDelta duration)
      : duration_(duration) {}

  void ThreadMain() override {
    base::PlatformThread::Sleep(duration_);
    _exit(0);
  }

 private:
  const base::TimeDelta duration_;
  DISALLOW_COPY_AND_ASSIGN(WaitAndExitDelegate);
};

bool CreateWaitAndExitThread(base::TimeDelta duration) {
  scoped_ptr<WaitAndExitDelegate> delegate(new WaitAndExitDelegate(duration));

  const bool thread_created =
      base::PlatformThread::CreateNonJoinable(0, delegate.get());
  if (!thread_created)
    return false;

  // A non joinable thread has been created. The thread will either terminate
  // the process or will be terminated by the process. Therefore, keep the
  // delegate object alive for the lifetime of the process.
  WaitAndExitDelegate* leaking_delegate = delegate.release();
  ANNOTATE_LEAKING_OBJECT_PTR(leaking_delegate);
  ignore_result(leaking_delegate);
  return true;
}
#endif

class SuicideOnChannelErrorFilter : public IPC::MessageFilter {
 public:
  // IPC::MessageFilter
  void OnChannelError() override {
    // For renderer/worker processes:
    // On POSIX, at least, one can install an unload handler which loops
    // forever and leave behind a renderer process which eats 100% CPU forever.
    //
    // This is because the terminate signals (ViewMsg_ShouldClose and the error
    // from the IPC sender) are routed to the main message loop but never
    // processed (because that message loop is stuck in V8).
    //
    // One could make the browser SIGKILL the renderers, but that leaves open a
    // large window where a browser failure (or a user, manually terminating
    // the browser because "it's stuck") will leave behind a process eating all
    // the CPU.
    //
    // So, we install a filter on the sender so that we can process this event
    // here and kill the process.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
    // Some sanitizer tools rely on exit handlers (e.g. to run leak detection,
    // or dump code coverage data to disk). Instead of exiting the process
    // immediately, we give it 60 seconds to run exit handlers.
    CHECK(CreateWaitAndExitThread(base::TimeDelta::FromSeconds(60)));
#if defined(LEAK_SANITIZER)
    // Invoke LeakSanitizer early to avoid detecting shutdown-only leaks. If
    // leaks are found, the process will exit here.
    __lsan_do_leak_check();
#endif
#else
    _exit(0);
#endif
  }

 protected:
  ~SuicideOnChannelErrorFilter() override {}
};

#endif  // OS(POSIX)

#if defined(OS_ANDROID)
ChildThreadImpl* g_child_thread = NULL;
bool g_child_thread_initialized = false;

// A lock protects g_child_thread.
base::LazyInstance<base::Lock>::Leaky g_lazy_child_thread_lock =
    LAZY_INSTANCE_INITIALIZER;

// base::ConditionVariable has an explicit constructor that takes
// a base::Lock pointer as parameter. The base::DefaultLazyInstanceTraits
// doesn't handle the case. Thus, we need our own class here.
struct CondVarLazyInstanceTraits {
  static const bool kRegisterOnExit = false;
#ifndef NDEBUG
  static const bool kAllowedToAccessOnNonjoinableThread = true;
#endif

  static base::ConditionVariable* New(void* instance) {
    return new (instance) base::ConditionVariable(
        g_lazy_child_thread_lock.Pointer());
  }
  static void Delete(base::ConditionVariable* instance) {
    instance->~ConditionVariable();
  }
};

// A condition variable that synchronize threads initializing and waiting
// for g_child_thread.
base::LazyInstance<base::ConditionVariable, CondVarLazyInstanceTraits>
    g_lazy_child_thread_cv = LAZY_INSTANCE_INITIALIZER;

void QuitMainThreadMessageLoop() {
  base::MessageLoop::current()->Quit();
}

#endif

}  // namespace

ChildThread* ChildThread::Get() {
  return ChildThreadImpl::current();
}

// Mojo client channel delegate to be used in single process mode.
class ChildThreadImpl::SingleProcessChannelDelegate
    : public IPC::ChannelMojo::Delegate {
 public:
  explicit SingleProcessChannelDelegate() : weak_factory_(this) {}

  ~SingleProcessChannelDelegate() override {}

  base::WeakPtr<IPC::ChannelMojo::Delegate> ToWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  scoped_refptr<base::TaskRunner> GetIOTaskRunner() override {
    return ChannelInit::GetSingleProcessIOTaskRunner();
  }

  void OnChannelCreated(base::WeakPtr<IPC::ChannelMojo> channel) override {}

  void DeleteSoon() {
    ChannelInit::GetSingleProcessIOTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&base::DeletePointer<SingleProcessChannelDelegate>,
                   base::Unretained(this)));
  }

 private:
  base::WeakPtrFactory<IPC::ChannelMojo::Delegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SingleProcessChannelDelegate);
};

void ChildThreadImpl::SingleProcessChannelDelegateDeleter::operator()(
    SingleProcessChannelDelegate* delegate) const {
  delegate->DeleteSoon();
}

ChildThreadImpl::Options::Options()
    : channel_name(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessChannelID)),
      use_mojo_channel(false),
      in_browser_process(false) {
}

ChildThreadImpl::Options::~Options() {
}

ChildThreadImpl::Options::Builder::Builder() {
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::InBrowserProcess(bool in_browser_process) {
  options_.in_browser_process = in_browser_process;
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::UseMojoChannel(bool use_mojo_channel) {
  options_.use_mojo_channel = use_mojo_channel;
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::WithChannelName(
    const std::string& channel_name) {
  options_.channel_name = channel_name;
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::AddStartupFilter(
    IPC::MessageFilter* filter) {
  options_.startup_filters.push_back(filter);
  return *this;
}

ChildThreadImpl::Options ChildThreadImpl::Options::Builder::Build() {
  return options_;
}

ChildThreadImpl::ChildThreadMessageRouter::ChildThreadMessageRouter(
    IPC::Sender* sender)
    : sender_(sender) {}

bool ChildThreadImpl::ChildThreadMessageRouter::Send(IPC::Message* msg) {
  return sender_->Send(msg);
}

ChildThreadImpl::ChildThreadImpl()
    : router_(this),
      in_browser_process_(false),
      channel_connected_factory_(this) {
  Init(Options::Builder().Build());
}

ChildThreadImpl::ChildThreadImpl(const Options& options)
    : router_(this),
      in_browser_process_(options.in_browser_process),
      channel_connected_factory_(this) {
  Init(options);
}

void ChildThreadImpl::ConnectChannel(bool use_mojo_channel) {
  bool create_pipe_now = true;
  if (use_mojo_channel) {
    VLOG(1) << "Mojo is enabled on child";
    scoped_refptr<base::TaskRunner> io_task_runner =
        ChannelInit::GetSingleProcessIOTaskRunner();
    if (io_task_runner) {
      single_process_channel_delegate_.reset(new SingleProcessChannelDelegate);
    } else {
      io_task_runner = ChildProcess::current()->io_message_loop_proxy();
    }
    DCHECK(io_task_runner);
    ipc_support_.reset(new IPC::ScopedIPCSupport(io_task_runner));
    channel_->Init(
        IPC::ChannelMojo::CreateClientFactory(
            single_process_channel_delegate_.get(),
            channel_name_),
        create_pipe_now);
    return;
  }

  VLOG(1) << "Mojo is disabled on child";
  channel_->Init(channel_name_, IPC::Channel::MODE_CLIENT, create_pipe_now);
}

void ChildThreadImpl::Init(const Options& options) {
  channel_name_ = options.channel_name;

  g_lazy_tls.Pointer()->Set(this);
  on_channel_error_called_ = false;
  message_loop_ = base::MessageLoop::current();
#ifdef IPC_MESSAGE_LOG_ENABLED
  // We must make sure to instantiate the IPC Logger *before* we create the
  // channel, otherwise we can get a callback on the IO thread which creates
  // the logger, and the logger does not like being created on the IO thread.
  IPC::Logging::GetInstance();
#endif
  channel_ = IPC::SyncChannel::Create(
      this, ChildProcess::current()->io_message_loop_proxy(),
      ChildProcess::current()->GetShutDownEvent());
#ifdef IPC_MESSAGE_LOG_ENABLED
  if (!in_browser_process_)
    IPC::Logging::GetInstance()->SetIPCSender(this);
#endif

  mojo_application_.reset(new MojoApplication);

  sync_message_filter_ =
      new IPC::SyncMessageFilter(ChildProcess::current()->GetShutDownEvent());
  thread_safe_sender_ = new ThreadSafeSender(
      base::MessageLoopProxy::current().get(), sync_message_filter_.get());

  resource_dispatcher_.reset(new ResourceDispatcher(
      this, message_loop()->task_runner()));
  websocket_dispatcher_.reset(new WebSocketDispatcher);
  file_system_dispatcher_.reset(new FileSystemDispatcher());

  histogram_message_filter_ = new ChildHistogramMessageFilter();
  resource_message_filter_ =
      new ChildResourceMessageFilter(resource_dispatcher());

  service_worker_message_filter_ =
      new ServiceWorkerMessageFilter(thread_safe_sender_.get());

  quota_message_filter_ =
      new QuotaMessageFilter(thread_safe_sender_.get());
  quota_dispatcher_.reset(new QuotaDispatcher(thread_safe_sender_.get(),
                                              quota_message_filter_.get()));
  geofencing_message_filter_ =
      new GeofencingMessageFilter(thread_safe_sender_.get());
  bluetooth_message_filter_ =
      new BluetoothMessageFilter(thread_safe_sender_.get());
  notification_dispatcher_ =
      new NotificationDispatcher(thread_safe_sender_.get());
  push_dispatcher_ = new PushDispatcher(thread_safe_sender_.get());
  navigator_connect_dispatcher_ =
      new NavigatorConnectDispatcher(thread_safe_sender_.get());

  channel_->AddFilter(histogram_message_filter_.get());
  channel_->AddFilter(sync_message_filter_.get());
  channel_->AddFilter(resource_message_filter_.get());
  channel_->AddFilter(quota_message_filter_->GetFilter());
  channel_->AddFilter(notification_dispatcher_->GetFilter());
  channel_->AddFilter(push_dispatcher_->GetFilter());
  channel_->AddFilter(service_worker_message_filter_->GetFilter());
  channel_->AddFilter(geofencing_message_filter_->GetFilter());
  channel_->AddFilter(bluetooth_message_filter_->GetFilter());
  channel_->AddFilter(navigator_connect_dispatcher_->GetFilter());

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    // In single process mode, browser-side tracing will cover the whole
    // process including renderers.
    channel_->AddFilter(new tracing::ChildTraceMessageFilter(
        ChildProcess::current()->io_message_loop_proxy()));
  }

  // In single process mode we may already have a power monitor
  if (!base::PowerMonitor::Get()) {
    scoped_ptr<PowerMonitorBroadcastSource> power_monitor_source(
      new PowerMonitorBroadcastSource());
    channel_->AddFilter(power_monitor_source->GetMessageFilter());

    power_monitor_.reset(new base::PowerMonitor(
        power_monitor_source.Pass()));
  }

#if defined(OS_POSIX)
  // Check that --process-type is specified so we don't do this in unit tests
  // and single-process mode.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kProcessType))
    channel_->AddFilter(new SuicideOnChannelErrorFilter());
#endif

  // Add filters passed here via options.
  for (auto startup_filter : options.startup_filters) {
    channel_->AddFilter(startup_filter);
  }

  ConnectChannel(options.use_mojo_channel);

  int connection_timeout = kConnectionTimeoutS;
  std::string connection_override =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kIPCConnectionTimeout);
  if (!connection_override.empty()) {
    int temp;
    if (base::StringToInt(connection_override, &temp))
      connection_timeout = temp;
  }

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ChildThreadImpl::EnsureConnected,
                 channel_connected_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(connection_timeout));

#if defined(OS_ANDROID)
  {
    base::AutoLock lock(g_lazy_child_thread_lock.Get());
    g_child_thread = this;
    g_child_thread_initialized = true;
  }
  // Signalling without locking is fine here because only
  // one thread can wait on the condition variable.
  g_lazy_child_thread_cv.Get().Signal();
#endif

#if defined(TCMALLOC_TRACE_MEMORY_SUPPORTED)
  trace_memory_controller_.reset(new base::trace_event::TraceMemoryController(
      message_loop_->message_loop_proxy(), ::HeapProfilerWithPseudoStackStart,
      ::HeapProfilerStop, ::GetHeapProfile));
#endif

  shared_bitmap_manager_.reset(
      new ChildSharedBitmapManager(thread_safe_sender()));

  gpu_memory_buffer_manager_.reset(
      new ChildGpuMemoryBufferManager(thread_safe_sender()));

  discardable_shared_memory_manager_.reset(
      new ChildDiscardableSharedMemoryManager(thread_safe_sender()));
}

ChildThreadImpl::~ChildThreadImpl() {
#if defined(OS_ANDROID)
  {
    base::AutoLock lock(g_lazy_child_thread_lock.Get());
    g_child_thread = nullptr;
  }
#endif

#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::GetInstance()->SetIPCSender(NULL);
#endif

  channel_->RemoveFilter(histogram_message_filter_.get());
  channel_->RemoveFilter(sync_message_filter_.get());

  // The ChannelProxy object caches a pointer to the IPC thread, so need to
  // reset it as it's not guaranteed to outlive this object.
  // NOTE: this also has the side-effect of not closing the main IPC channel to
  // the browser process.  This is needed because this is the signal that the
  // browser uses to know that this process has died, so we need it to be alive
  // until this process is shut down, and the OS closes the handle
  // automatically.  We used to watch the object handle on Windows to do this,
  // but it wasn't possible to do so on POSIX.
  channel_->ClearIPCTaskRunner();
  g_lazy_tls.Pointer()->Set(NULL);
}

void ChildThreadImpl::Shutdown() {
  // Delete objects that hold references to blink so derived classes can
  // safely shutdown blink in their Shutdown implementation.
  file_system_dispatcher_.reset();
  quota_dispatcher_.reset();
  WebFileSystemImpl::DeleteThreadSpecificInstance();
}

void ChildThreadImpl::OnChannelConnected(int32 peer_pid) {
  channel_connected_factory_.InvalidateWeakPtrs();
}

void ChildThreadImpl::OnChannelError() {
  set_on_channel_error_called(true);
  base::MessageLoop::current()->Quit();
}

bool ChildThreadImpl::Send(IPC::Message* msg) {
  DCHECK(base::MessageLoop::current() == message_loop());
  if (!channel_) {
    delete msg;
    return false;
  }

  return channel_->Send(msg);
}

#if defined(OS_WIN)
void ChildThreadImpl::PreCacheFont(const LOGFONT& log_font) {
  Send(new ChildProcessHostMsg_PreCacheFont(log_font));
}

void ChildThreadImpl::ReleaseCachedFonts() {
  Send(new ChildProcessHostMsg_ReleaseCachedFonts());
}
#endif

MessageRouter* ChildThreadImpl::GetRouter() {
  DCHECK(base::MessageLoop::current() == message_loop());
  return &router_;
}

scoped_ptr<base::SharedMemory> ChildThreadImpl::AllocateSharedMemory(
    size_t buf_size) {
  DCHECK(base::MessageLoop::current() == message_loop());
  return AllocateSharedMemory(buf_size, this);
}

// static
scoped_ptr<base::SharedMemory> ChildThreadImpl::AllocateSharedMemory(
    size_t buf_size,
    IPC::Sender* sender) {
  scoped_ptr<base::SharedMemory> shared_buf;
#if defined(OS_WIN)
  shared_buf.reset(new base::SharedMemory);
  if (!shared_buf->CreateAnonymous(buf_size)) {
    NOTREACHED();
    return NULL;
  }
#else
  // On POSIX, we need to ask the browser to create the shared memory for us,
  // since this is blocked by the sandbox.
  base::SharedMemoryHandle shared_mem_handle;
  if (sender->Send(new ChildProcessHostMsg_SyncAllocateSharedMemory(
                           buf_size, &shared_mem_handle))) {
    if (base::SharedMemory::IsHandleValid(shared_mem_handle)) {
      shared_buf.reset(new base::SharedMemory(shared_mem_handle, false));
    } else {
      NOTREACHED() << "Browser failed to allocate shared memory";
      return NULL;
    }
  } else {
    NOTREACHED() << "Browser allocation request message failed";
    return NULL;
  }
#endif
  return shared_buf;
}

bool ChildThreadImpl::OnMessageReceived(const IPC::Message& msg) {
  if (mojo_application_->OnMessageReceived(msg))
    return true;

  // Resource responses are sent to the resource dispatcher.
  if (resource_dispatcher_->OnMessageReceived(msg))
    return true;
  if (websocket_dispatcher_->OnMessageReceived(msg))
    return true;
  if (file_system_dispatcher_->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChildThreadImpl, msg)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_Shutdown, OnShutdown)
#if defined(IPC_MESSAGE_LOG_ENABLED)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_SetIPCLoggingEnabled,
                        OnSetIPCLoggingEnabled)
#endif
    IPC_MESSAGE_HANDLER(ChildProcessMsg_SetProfilerStatus,
                        OnSetProfilerStatus)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_GetChildProfilerData,
                        OnGetChildProfilerData)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_DumpHandles, OnDumpHandles)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_SetProcessBackgrounded,
                        OnProcessBackgrounded)
#if defined(USE_TCMALLOC)
    IPC_MESSAGE_HANDLER(ChildProcessMsg_GetTcmallocStats, OnGetTcmallocStats)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (handled)
    return true;

  if (msg.routing_id() == MSG_ROUTING_CONTROL)
    return OnControlMessageReceived(msg);

  return router_.OnMessageReceived(msg);
}

bool ChildThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  return false;
}

void ChildThreadImpl::OnShutdown() {
  base::MessageLoop::current()->Quit();
}

#if defined(IPC_MESSAGE_LOG_ENABLED)
void ChildThreadImpl::OnSetIPCLoggingEnabled(bool enable) {
  if (enable)
    IPC::Logging::GetInstance()->Enable();
  else
    IPC::Logging::GetInstance()->Disable();
}
#endif  //  IPC_MESSAGE_LOG_ENABLED

void ChildThreadImpl::OnSetProfilerStatus(ThreadData::Status status) {
  ThreadData::InitializeAndSetTrackingStatus(status);
}

void ChildThreadImpl::OnGetChildProfilerData(int sequence_number) {
  tracked_objects::ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);

  Send(new ChildProcessHostMsg_ChildProfilerData(sequence_number,
                                                 process_data));
}

void ChildThreadImpl::OnDumpHandles() {
#if defined(OS_WIN)
  scoped_refptr<HandleEnumerator> handle_enum(
      new HandleEnumerator(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAuditAllHandles)));
  handle_enum->EnumerateHandles();
  Send(new ChildProcessHostMsg_DumpHandlesDone);
#else
  NOTIMPLEMENTED();
#endif
}

#if defined(USE_TCMALLOC)
void ChildThreadImpl::OnGetTcmallocStats() {
  std::string result;
  char buffer[1024 * 32];
  base::allocator::GetStats(buffer, sizeof(buffer));
  result.append(buffer);
  Send(new ChildProcessHostMsg_TcmallocStats(result));
}
#endif

ChildThreadImpl* ChildThreadImpl::current() {
  return g_lazy_tls.Pointer()->Get();
}

#if defined(OS_ANDROID)
// The method must NOT be called on the child thread itself.
// It may block the child thread if so.
void ChildThreadImpl::ShutdownThread() {
  DCHECK(!ChildThreadImpl::current()) <<
      "this method should NOT be called from child thread itself";
  {
    base::AutoLock lock(g_lazy_child_thread_lock.Get());
    while (!g_child_thread_initialized)
      g_lazy_child_thread_cv.Get().Wait();

    // g_child_thread may already have been destructed while we didn't hold the
    // lock.
    if (!g_child_thread)
      return;

    DCHECK_NE(base::MessageLoop::current(), g_child_thread->message_loop());
    g_child_thread->message_loop()->PostTask(
        FROM_HERE, base::Bind(&QuitMainThreadMessageLoop));
  }
}
#endif

void ChildThreadImpl::OnProcessFinalRelease() {
  if (on_channel_error_called_) {
    base::MessageLoop::current()->Quit();
    return;
  }

  // The child process shutdown sequence is a request response based mechanism,
  // where we send out an initial feeler request to the child process host
  // instance in the browser to verify if it's ok to shutdown the child process.
  // The browser then sends back a response if it's ok to shutdown. This avoids
  // race conditions if the process refcount is 0 but there's an IPC message
  // inflight that would addref it.
  Send(new ChildProcessHostMsg_ShutdownRequest);
}

void ChildThreadImpl::EnsureConnected() {
  VLOG(0) << "ChildThreadImpl::EnsureConnected()";
  base::KillProcess(base::GetCurrentProcessHandle(), 0, false);
}

void ChildThreadImpl::OnProcessBackgrounded(bool background) {
  // Set timer slack to maximum on main thread when in background.
  base::TimerSlack timer_slack = base::TIMER_SLACK_NONE;
  if (background)
    timer_slack = base::TIMER_SLACK_MAXIMUM;
  base::MessageLoop::current()->SetTimerSlack(timer_slack);

#ifdef OS_WIN
  // Windows Vista+ has a fancy process backgrounding mode that can only be set
  // from within the process. This used to be how chrome set its renderers into
  // background mode on Windows but was removed due to http://crbug.com/398103.
  // As we experiment with bringing back some other form of background mode for
  // hidden renderers, add a bucket to allow us to trigger this undesired method
  // of setting background state in order to confirm that the metrics which were
  // added to prevent regressions on the aforementioned issue indeed catch such
  // regressions and are thus a reliable way to confirm that our latest proposal
  // doesn't cause such issues. TODO(gab): Remove this once the experiment is
  // over (http://crbug.com/458594).
  base::FieldTrial* trial =
      base::FieldTrialList::Find("BackgroundRendererProcesses");
  if (trial && trial->group_name() == "AllowBackgroundModeFromRenderer")
    base::Process::Current().SetProcessBackgrounded(background);
#endif  // OS_WIN
}

}  // namespace content
