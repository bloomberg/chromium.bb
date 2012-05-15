// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Represents the browser side of the browser <--> renderer communication
// channel. There will be one RenderProcessHost per renderer process.

#include "content/browser/renderer_host/render_process_host_impl.h"

#if defined(OS_WIN)
#include <objbase.h>  // For CoInitialize/CoUninitialize.
#endif

#include <algorithm>
#include <limits>
#include <vector>

#if defined(OS_POSIX)
#include <utility>  // for pair<>
#endif

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/tracked_objects.h"
#include "content/browser/appcache/appcache_dispatcher_host.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/browser_main.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/device_orientation/message_filter.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/dom_storage/dom_storage_message_filter.h"
#include "content/browser/download/mhtml_generation_manager.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/fileapi/fileapi_message_filter.h"
#include "content/browser/geolocation/geolocation_dispatcher_host.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/in_process_webkit/indexed_db_context_impl.h"
#include "content/browser/in_process_webkit/indexed_db_dispatcher_host.h"
#include "content/browser/mime_registry_message_filter.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/profiler_message_filter.h"
#include "content/browser/renderer_host/clipboard_message_filter.h"
#include "content/browser/renderer_host/database_message_filter.h"
#include "content/browser/renderer_host/file_utilities_message_filter.h"
#include "content/browser/renderer_host/gamepad_browser_message_filter.h"
#include "content/browser/renderer_host/gpu_message_filter.h"
#include "content/browser/renderer_host/media/audio_input_renderer_host.h"
#include "content/browser/renderer_host/media/audio_renderer_host.h"
#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"
#include "content/browser/renderer_host/media/video_capture_host.h"
#include "content/browser/renderer_host/p2p/socket_dispatcher_host.h"
#include "content/browser/renderer_host/pepper_file_message_filter.h"
#include "content/browser/renderer_host/pepper_message_filter.h"
#include "content/browser/renderer_host/quota_dispatcher_host.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/resource_message_filter.h"
#include "content/browser/renderer_host/socket_stream_dispatcher_host.h"
#include "content/browser/renderer_host/text_input_client_message_filter.h"
#include "content/browser/resolve_proxy_msg_helper.h"
#include "content/browser/trace_message_filter.h"
#include "content/browser/worker_host/worker_message_filter.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/child_process_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/resource_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/result_codes.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_switches.h"
#include "media/base/media_switches.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/gl_switches.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/glue/resource_type.h"
#include "webkit/plugins/plugin_switches.h"

#if defined(OS_WIN)
#include "base/synchronization/waitable_event.h"
#include "content/common/font_cache_dispatcher_win.h"
#endif

#if defined(ENABLE_INPUT_SPEECH)
#include "content/browser/speech/input_tag_speech_dispatcher_host.h"
#endif

#include "third_party/skia/include/core/SkBitmap.h"

using content::BrowserContext;
using content::BrowserMessageFilter;
using content::BrowserThread;
using content::ChildProcessHost;
using content::ChildProcessHostImpl;
using content::RenderWidgetHost;
using content::RenderWidgetHostImpl;
using content::RenderViewHost;
using content::RenderViewHostImpl;
using content::UserMetricsAction;
using content::WebUIControllerFactory;

extern bool g_exited_main_message_loop;

// This class creates the IO thread for the renderer when running in
// single-process mode.  It's not used in multi-process mode.
class RendererMainThread : public base::Thread {
 public:
  explicit RendererMainThread(const std::string& channel_id)
      : base::Thread("Chrome_InProcRendererThread"),
        channel_id_(channel_id),
        render_process_(NULL) {
  }

  ~RendererMainThread() {
    Stop();
  }

 protected:
  virtual void Init() {
#if defined(OS_WIN)
    CoInitialize(NULL);
#endif

    render_process_ = new RenderProcessImpl();
    new RenderThreadImpl(channel_id_);
  }

  virtual void CleanUp() {
    delete render_process_;

#if defined(OS_WIN)
    CoUninitialize();
#endif
    // It's a little lame to manually set this flag.  But the single process
    // RendererThread will receive the WM_QUIT.  We don't need to assert on
    // this thread, so just force the flag manually.
    // If we want to avoid this, we could create the InProcRendererThread
    // directly with _beginthreadex() rather than using the Thread class.
    // We used to set this flag in the Init function above. However there
    // other threads like WebThread which are created by this thread
    // which resets this flag. Please see Thread::StartWithOptions. Setting
    // this flag to true in Cleanup works around these problems.
    base::Thread::SetThreadWasQuitProperly(true);
  }

 private:
  std::string channel_id_;
  // Deleted in CleanUp() on the renderer thread, so don't use a smart pointer.
  RenderProcess* render_process_;
};

namespace {

// Helper class that we pass to ResourceMessageFilter so that it can find the
// right net::URLRequestContext for a request.
class RendererURLRequestContextSelector
    : public ResourceMessageFilter::URLRequestContextSelector {
 public:
  RendererURLRequestContextSelector(content::BrowserContext* browser_context,
                                    int render_child_id)
      : request_context_(browser_context->GetRequestContextForRenderProcess(
                             render_child_id)),
        media_request_context_(browser_context->GetRequestContextForMedia()) {
  }

  virtual net::URLRequestContext* GetRequestContext(
      ResourceType::Type resource_type) {
    net::URLRequestContextGetter* request_context = request_context_;
    // If the request has resource type of ResourceType::MEDIA, we use a request
    // context specific to media for handling it because these resources have
    // specific needs for caching.
    if (resource_type == ResourceType::MEDIA)
      request_context = media_request_context_;
    return request_context->GetURLRequestContext();
  }

 private:
  virtual ~RendererURLRequestContextSelector() {}

  scoped_refptr<net::URLRequestContextGetter> request_context_;
  scoped_refptr<net::URLRequestContextGetter> media_request_context_;
};

// the global list of all renderer processes
base::LazyInstance<IDMap<content::RenderProcessHost> >::Leaky
    g_all_hosts = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// Stores the maximum number of renderer processes the content module can
// create.
static size_t g_max_renderer_count_override = 0;

// static
size_t content::RenderProcessHost::GetMaxRendererProcessCount() {
  if (g_max_renderer_count_override)
    return g_max_renderer_count_override;

  // Defines the maximum number of renderer processes according to the
  // amount of installed memory as reported by the OS. The calculation
  // assumes that you want the renderers to use half of the installed
  // RAM and assuming that each WebContents uses ~40MB.
  // If you modify this assumption, you need to adjust the
  // ThirtyFourTabs test to match the expected number of processes.
  //
  // With the given amounts of installed memory below on a 32-bit CPU,
  // the maximum renderer count will roughly be as follows:
  //
  //   128 MB -> 3
  //   512 MB -> 6
  //  1024 MB -> 12
  //  4096 MB -> 51
  // 16384 MB -> 82 (kMaxRendererProcessCount)

  static size_t max_count = 0;
  if (!max_count) {
    const size_t kEstimatedWebContentsMemoryUsage =
#if defined(ARCH_CPU_64_BITS)
        60;  // In MB
#else
        40;  // In MB
#endif
    max_count = base::SysInfo::AmountOfPhysicalMemoryMB() / 2;
    max_count /= kEstimatedWebContentsMemoryUsage;

    const size_t kMinRendererProcessCount = 3;
    max_count = std::max(max_count, kMinRendererProcessCount);
    max_count = std::min(max_count, content::kMaxRendererProcessCount);
  }
  return max_count;
}

// static
bool g_run_renderer_in_process_ = false;

// static
void content::RenderProcessHost::SetMaxRendererProcessCount(size_t count) {
  g_max_renderer_count_override = count;
}

RenderProcessHostImpl::RenderProcessHostImpl(
    content::BrowserContext* browser_context)
        : fast_shutdown_started_(false),
          deleting_soon_(false),
          pending_views_(0),
          visible_widgets_(0),
          backgrounded_(true),
          ALLOW_THIS_IN_INITIALIZER_LIST(cached_dibs_cleaner_(
                FROM_HERE, base::TimeDelta::FromSeconds(5),
                this, &RenderProcessHostImpl::ClearTransportDIBCache)),
          is_initialized_(false),
          id_(ChildProcessHostImpl::GenerateChildProcessUniqueId()),
          browser_context_(browser_context),
          sudden_termination_allowed_(true),
          ignore_input_events_(false) {
  widget_helper_ = new RenderWidgetHelper();

  ChildProcessSecurityPolicyImpl::GetInstance()->Add(GetID());

  // Grant most file permissions to this renderer.
  // PLATFORM_FILE_TEMPORARY, PLATFORM_FILE_HIDDEN and
  // PLATFORM_FILE_DELETE_ON_CLOSE are not granted, because no existing API
  // requests them.
  // This is for the filesystem sandbox.
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantPermissionsForFile(
      GetID(), browser_context->GetPath().Append(
          fileapi::SandboxMountPointProvider::kNewFileSystemDirectory),
      base::PLATFORM_FILE_OPEN |
      base::PLATFORM_FILE_CREATE |
      base::PLATFORM_FILE_OPEN_ALWAYS |
      base::PLATFORM_FILE_CREATE_ALWAYS |
      base::PLATFORM_FILE_OPEN_TRUNCATED |
      base::PLATFORM_FILE_READ |
      base::PLATFORM_FILE_WRITE |
      base::PLATFORM_FILE_EXCLUSIVE_READ |
      base::PLATFORM_FILE_EXCLUSIVE_WRITE |
      base::PLATFORM_FILE_ASYNC |
      base::PLATFORM_FILE_WRITE_ATTRIBUTES |
      base::PLATFORM_FILE_ENUMERATE);
  // This is so that we can read and move stuff out of the old filesystem
  // sandbox.
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantPermissionsForFile(
      GetID(), browser_context->GetPath().Append(
          fileapi::SandboxMountPointProvider::kOldFileSystemDirectory),
      base::PLATFORM_FILE_READ | base::PLATFORM_FILE_WRITE |
      base::PLATFORM_FILE_WRITE_ATTRIBUTES | base::PLATFORM_FILE_ENUMERATE);
  // This is so that we can rename the old sandbox out of the way so that we
  // know we've taken care of it.
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantPermissionsForFile(
      GetID(), browser_context->GetPath().Append(
          fileapi::SandboxMountPointProvider::kRenamedOldFileSystemDirectory),
      base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_CREATE_ALWAYS |
      base::PLATFORM_FILE_WRITE);

  CHECK(!g_exited_main_message_loop);
  RegisterHost(GetID(), this);
  g_all_hosts.Get().set_check_on_null_data(true);
  // Initialize |child_process_activity_time_| to a reasonable value.
  mark_child_process_activity_time();
  // Note: When we create the RenderProcessHostImpl, it's technically
  //       backgrounded, because it has no visible listeners.  But the process
  //       doesn't actually exist yet, so we'll Background it later, after
  //       creation.
}

RenderProcessHostImpl::~RenderProcessHostImpl() {
  ChildProcessSecurityPolicyImpl::GetInstance()->Remove(GetID());

  // We may have some unsent messages at this point, but that's OK.
  channel_.reset();
  while (!queued_messages_.empty()) {
    delete queued_messages_.front();
    queued_messages_.pop();
  }

  ClearTransportDIBCache();
  UnregisterHost(GetID());
}

void RenderProcessHostImpl::EnableSendQueue() {
  is_initialized_ = false;
}

bool RenderProcessHostImpl::Init() {
  // calling Init() more than once does nothing, this makes it more convenient
  // for the view host which may not be sure in some cases
  if (channel_.get())
    return true;

  CommandLine::StringType renderer_prefix;
#if defined(OS_POSIX)
  // A command prefix is something prepended to the command line of the spawned
  // process. It is supported only on POSIX systems.
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  renderer_prefix =
      browser_command_line.GetSwitchValueNative(switches::kRendererCmdPrefix);
#endif  // defined(OS_POSIX)

#if defined(OS_LINUX)
  int flags = renderer_prefix.empty() ? ChildProcessHost::CHILD_ALLOW_SELF :
                                        ChildProcessHost::CHILD_NORMAL;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif

  // Find the renderer before creating the channel so if this fails early we
  // return without creating the channel.
  FilePath renderer_path = ChildProcessHost::GetChildPath(flags);
  if (renderer_path.empty())
    return false;

  // Setup the IPC channel.
  const std::string channel_id =
      IPC::Channel::GenerateVerifiedChannelID(std::string());
  channel_.reset(new IPC::ChannelProxy(
      channel_id, IPC::Channel::MODE_SERVER, this,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));

  // Call the embedder first so that their IPC filters have priority.
  content::GetContentClient()->browser()->RenderProcessHostCreated(
      this);

  CreateMessageFilters();

  if (run_renderer_in_process()) {
    // Crank up a thread and run the initialization there.  With the way that
    // messages flow between the browser and renderer, this thread is required
    // to prevent a deadlock in single-process mode.  Since the primordial
    // thread in the renderer process runs the WebKit code and can sometimes
    // make blocking calls to the UI thread (i.e. this thread), they need to run
    // on separate threads.
    in_process_renderer_.reset(new RendererMainThread(channel_id));

    base::Thread::Options options;
#if !defined(TOOLKIT_GTK)
    // In-process plugins require this to be a UI message loop.
    options.message_loop_type = MessageLoop::TYPE_UI;
#else
    // We can't have multiple UI loops on GTK, so we don't support
    // in-process plugins.
    options.message_loop_type = MessageLoop::TYPE_DEFAULT;
#endif
    in_process_renderer_->StartWithOptions(options);

    OnProcessLaunched();  // Fake a callback that the process is ready.
  } else {
    // Build command line for renderer.  We call AppendRendererCommandLine()
    // first so the process type argument will appear first.
    CommandLine* cmd_line = new CommandLine(renderer_path);
    if (!renderer_prefix.empty())
      cmd_line->PrependWrapper(renderer_prefix);
    AppendRendererCommandLine(cmd_line);
    cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id);

    // Spawn the child process asynchronously to avoid blocking the UI thread.
    // As long as there's no renderer prefix, we can use the zygote process
    // at this stage.
    child_process_launcher_.reset(new ChildProcessLauncher(
#if defined(OS_WIN)
        FilePath(),
#elif defined(OS_POSIX)
        renderer_prefix.empty(),
        base::EnvironmentVector(),
        channel_->TakeClientFileDescriptor(),
#endif
        cmd_line,
        this));

    fast_shutdown_started_ = false;
  }

  is_initialized_ = true;
  return true;
}

void RenderProcessHostImpl::CreateMessageFilters() {
  content::MediaObserver* media_observer =
      content::GetContentClient()->browser()->GetMediaObserver();
  scoped_refptr<RenderMessageFilter> render_message_filter(
      new RenderMessageFilter(
          GetID(),
          PluginServiceImpl::GetInstance(),
          GetBrowserContext(),
          GetBrowserContext()->GetRequestContextForRenderProcess(GetID()),
          widget_helper_,
          media_observer));
  channel_->AddFilter(render_message_filter);
  content::BrowserContext* browser_context = GetBrowserContext();
  content::ResourceContext* resource_context =
      browser_context->GetResourceContext();

  ResourceMessageFilter* resource_message_filter = new ResourceMessageFilter(
      GetID(), content::PROCESS_TYPE_RENDERER, resource_context,
      new RendererURLRequestContextSelector(browser_context, GetID()));

  channel_->AddFilter(resource_message_filter);
#if !defined(OS_ANDROID)
  // TODO(dtrainor, klobag): Enable this when content::BrowserMainLoop gets
  // included in Android builds.  Tracked via 115941.
  media::AudioManager* audio_manager =
      content::BrowserMainLoop::GetAudioManager();
  channel_->AddFilter(new AudioInputRendererHost(
      resource_context, audio_manager));
  channel_->AddFilter(new AudioRendererHost(audio_manager, media_observer));
  channel_->AddFilter(new VideoCaptureHost(resource_context, audio_manager));
#endif
  channel_->AddFilter(new AppCacheDispatcherHost(
      static_cast<ChromeAppCacheService*>(
          BrowserContext::GetAppCacheService(browser_context)),
      GetID()));
  channel_->AddFilter(new ClipboardMessageFilter());
  channel_->AddFilter(new DOMStorageMessageFilter(GetID(),
      static_cast<DOMStorageContextImpl*>(
          BrowserContext::GetDOMStorageContext(browser_context))));
  channel_->AddFilter(new IndexedDBDispatcherHost(GetID(),
      static_cast<IndexedDBContextImpl*>(
          BrowserContext::GetIndexedDBContext(browser_context))));
  channel_->AddFilter(GeolocationDispatcherHost::New(
      GetID(), browser_context->GetGeolocationPermissionContext()));
  gpu_message_filter_ = new GpuMessageFilter(GetID(), widget_helper_.get());
  channel_->AddFilter(gpu_message_filter_);
#if defined(ENABLE_WEBRTC)
  channel_->AddFilter(new media_stream::MediaStreamDispatcherHost(
      resource_context, GetID(), content::BrowserMainLoop::GetAudioManager()));
#endif
  channel_->AddFilter(new PepperFileMessageFilter(GetID(), browser_context));
  channel_->AddFilter(new PepperMessageFilter(PepperMessageFilter::RENDERER,
                                              GetID(), browser_context));
#if defined(ENABLE_INPUT_SPEECH)
  channel_->AddFilter(new speech::InputTagSpeechDispatcherHost(
      GetID(), browser_context->GetRequestContext(),
      browser_context->GetSpeechRecognitionPreferences()));
#endif
  channel_->AddFilter(new FileAPIMessageFilter(
      GetID(),
      browser_context->GetRequestContext(),
      BrowserContext::GetFileSystemContext(browser_context),
      ChromeBlobStorageContext::GetFor(browser_context)));
  channel_->AddFilter(new device_orientation::MessageFilter());
  channel_->AddFilter(new FileUtilitiesMessageFilter(GetID()));
  channel_->AddFilter(new MimeRegistryMessageFilter());
  channel_->AddFilter(new DatabaseMessageFilter(
      BrowserContext::GetDatabaseTracker(browser_context)));
#if defined(OS_MACOSX)
  channel_->AddFilter(new TextInputClientMessageFilter(GetID()));
#elif defined(OS_WIN)
  channel_->AddFilter(new FontCacheDispatcher());
#endif

  SocketStreamDispatcherHost* socket_stream_dispatcher_host =
      new SocketStreamDispatcherHost(GetID(),
          new RendererURLRequestContextSelector(browser_context, GetID()),
          resource_context);
  channel_->AddFilter(socket_stream_dispatcher_host);

  channel_->AddFilter(new WorkerMessageFilter(GetID(), resource_context,
      base::Bind(&RenderWidgetHelper::GetNextRoutingID,
                 base::Unretained(widget_helper_.get()))));

#if defined(ENABLE_P2P_APIS)
  channel_->AddFilter(new content::P2PSocketDispatcherHost(resource_context));
#endif

  channel_->AddFilter(new TraceMessageFilter());
  channel_->AddFilter(new ResolveProxyMsgHelper(
      browser_context->GetRequestContextForRenderProcess(GetID())));
  channel_->AddFilter(new QuotaDispatcherHost(
      GetID(),
      content::BrowserContext::GetQuotaManager(browser_context),
      content::GetContentClient()->browser()->CreateQuotaPermissionContext()));
  channel_->AddFilter(new content::GamepadBrowserMessageFilter(this));
  channel_->AddFilter(new content::ProfilerMessageFilter(
      content::PROCESS_TYPE_RENDERER));
}

int RenderProcessHostImpl::GetNextRoutingID() {
  return widget_helper_->GetNextRoutingID();
}

void RenderProcessHostImpl::CancelResourceRequests(int render_widget_id) {
  widget_helper_->CancelResourceRequests(render_widget_id);
}

void RenderProcessHostImpl::CrossSiteSwapOutACK(
    const ViewMsg_SwapOut_Params& params) {
  widget_helper_->CrossSiteSwapOutACK(params);
}

bool RenderProcessHostImpl::WaitForBackingStoreMsg(
    int render_widget_id,
    const base::TimeDelta& max_delay,
    IPC::Message* msg) {
  // The post task to this thread with the process id could be in queue, and we
  // don't want to dispatch a message before then since it will need the handle.
  if (child_process_launcher_.get() && child_process_launcher_->IsStarting())
    return false;

  return widget_helper_->WaitForBackingStoreMsg(render_widget_id,
                                                max_delay, msg);
}

void RenderProcessHostImpl::ReceivedBadMessage() {
  if (run_renderer_in_process()) {
    // In single process mode it is better if we don't suicide but just
    // crash.
    CHECK(false);
  }
  NOTREACHED();
  base::KillProcess(GetHandle(), content::RESULT_CODE_KILLED_BAD_MESSAGE,
                    false);
}

void RenderProcessHostImpl::WidgetRestored() {
  // Verify we were properly backgrounded.
  DCHECK_EQ(backgrounded_, (visible_widgets_ == 0));
  visible_widgets_++;
  SetBackgrounded(false);
}

void RenderProcessHostImpl::WidgetHidden() {
  // On startup, the browser will call Hide
  if (backgrounded_)
    return;

  DCHECK_EQ(backgrounded_, (visible_widgets_ == 0));
  visible_widgets_--;
  DCHECK_GE(visible_widgets_, 0);
  if (visible_widgets_ == 0) {
    DCHECK(!backgrounded_);
    SetBackgrounded(true);
  }
}

int RenderProcessHostImpl::VisibleWidgetCount() const {
  return visible_widgets_;
}

void RenderProcessHostImpl::AppendRendererCommandLine(
    CommandLine* command_line) const {
  // Pass the process type first, so it shows first in process listings.
  command_line->AppendSwitchASCII(switches::kProcessType,
                                  switches::kRendererProcess);

  // Now send any options from our own command line we want to propagate.
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  PropagateBrowserCommandLineToRenderer(browser_command_line, command_line);

  // Pass on the browser locale.
  const std::string locale =
      content::GetContentClient()->browser()->GetApplicationLocale();
  command_line->AppendSwitchASCII(switches::kLang, locale);

  // If we run base::FieldTrials, we want to pass to their state to the
  // renderer so that it can act in accordance with each state, or record
  // histograms relating to the base::FieldTrial states.
  std::string field_trial_states;
  base::FieldTrialList::StatesToString(&field_trial_states);
  if (!field_trial_states.empty()) {
    command_line->AppendSwitchASCII(switches::kForceFieldTrials,
                                    field_trial_states);
  }

  content::GetContentClient()->browser()->AppendExtraCommandLineSwitches(
      command_line, GetID());

  // Appending disable-gpu-feature switches due to software rendering list.
  GpuDataManagerImpl* gpu_data_manager = GpuDataManagerImpl::GetInstance();
  DCHECK(gpu_data_manager);
  gpu_data_manager->AppendRendererCommandLine(command_line);
}

void RenderProcessHostImpl::PropagateBrowserCommandLineToRenderer(
    const CommandLine& browser_cmd,
    CommandLine* renderer_cmd) const {
  // Propagate the following switches to the renderer command line (along
  // with any associated values) if present in the browser command line.
  static const char* const kSwitchNames[] = {
    // We propagate the Chrome Frame command line here as well in case the
    // renderer is not run in the sandbox.
    switches::kAuditAllHandles,
    switches::kAuditHandles,
    switches::kChromeFrame,
    switches::kDisable3DAPIs,
    switches::kDisableAcceleratedCompositing,
    switches::kDisableApplicationCache,
    switches::kDisableAudio,
    switches::kDisableBreakpad,
#if defined(OS_MACOSX)
    switches::kDisableCompositedCoreAnimationPlugins,
#endif
    switches::kDisableDataTransferItems,
    switches::kDisableDatabases,
    switches::kDisableDesktopNotifications,
    switches::kDisableDeviceOrientation,
    switches::kDisableFileSystem,
    switches::kDisableGeolocation,
    switches::kDisableGLMultisampling,
    switches::kDisableGpuVsync,
    switches::kDisableJavaScriptI18NAPI,
    switches::kDisableLocalStorage,
    switches::kDisableLogging,
    switches::kDisableSeccompSandbox,
    switches::kDisableSessionStorage,
    switches::kDisableSharedWorkers,
    switches::kDisableSpeechInput,
    switches::kEnableScriptedSpeech,
    switches::kDisableWebAudio,
    switches::kDisableWebSockets,
    switches::kDomAutomationController,
    switches::kEnableAccessibilityLogging,
    switches::kEnableBrowserPlugin,
    switches::kEnableDCHECK,
    switches::kEnableEncryptedMedia,
    switches::kEnableFixedLayout,
    switches::kEnableGamepad,
    switches::kEnableGPUServiceLogging,
    switches::kEnableGPUClientLogging,
    switches::kEnableLogging,
    switches::kEnableMediaSource,
    switches::kEnablePeerConnection,
    switches::kEnableShadowDOM,
    switches::kEnableStrictSiteIsolation,
    switches::kEnableStyleScoped,
    switches::kDisableFullScreen,
    switches::kEnablePepperTesting,
    switches::kEnablePointerLock,
    switches::kEnablePreparsedJsCaching,
    switches::kEnablePruneGpuCommandBuffers,
#if defined(OS_MACOSX)
    // Allow this to be set when invoking the browser and relayed along.
    switches::kEnableSandboxLogging,
#endif
    switches::kEnableSeccompSandbox,
    switches::kEnableStatsTable,
    switches::kEnableThreadedCompositing,
    switches::kDisableThreadedCompositing,
    switches::kEnableTouchEvents,
    switches::kEnableVideoTrack,
    switches::kEnableViewport,
    switches::kFullMemoryCrashReport,
#if !defined (GOOGLE_CHROME_BUILD)
    // These are unsupported and not fully tested modes, so don't enable them
    // for official Google Chrome builds.
    switches::kInProcessPlugins,
#endif  // GOOGLE_CHROME_BUILD
    switches::kInProcessWebGL,
    switches::kJavaScriptFlags,
    switches::kLoggingLevel,
    switches::kNoReferrers,
    switches::kNoSandbox,
    switches::kPpapiOutOfProcess,
    switches::kRegisterPepperPlugins,
    switches::kRendererAssertTest,
#if defined(OS_POSIX)
    switches::kRendererCleanExit,
#endif
    switches::kRendererStartupDialog,
    switches::kShowPaintRects,
    switches::kTestSandbox,
    switches::kTraceStartup,
    // This flag needs to be propagated to the renderer process for
    // --in-process-webgl.
    switches::kUseGL,
    switches::kUserAgent,
    switches::kV,
    switches::kVideoThreads,
    switches::kVModule,
    switches::kWebCoreLogChannels,
  };
  renderer_cmd->CopySwitchesFrom(browser_cmd, kSwitchNames,
                                 arraysize(kSwitchNames));

  // Disable databases in incognito mode.
  if (GetBrowserContext()->IsOffTheRecord() &&
      !browser_cmd.HasSwitch(switches::kDisableDatabases)) {
    renderer_cmd->AppendSwitch(switches::kDisableDatabases);
  }
}

base::ProcessHandle RenderProcessHostImpl::GetHandle() {
  if (run_renderer_in_process())
    return base::Process::Current().handle();

  if (!child_process_launcher_.get() || child_process_launcher_->IsStarting())
    return base::kNullProcessHandle;

  return child_process_launcher_->GetHandle();
}

bool RenderProcessHostImpl::FastShutdownIfPossible() {
  if (run_renderer_in_process())
    return false;  // Single process mode can't do fast shutdown.

  if (!content::GetContentClient()->browser()->IsFastShutdownPossible())
    return false;

  if (!child_process_launcher_.get() ||
      child_process_launcher_->IsStarting() ||
      !GetHandle())
    return false;  // Render process hasn't started or is probably crashed.

  // Test if there's an unload listener.
  // NOTE: It's possible that an onunload listener may be installed
  // while we're shutting down, so there's a small race here.  Given that
  // the window is small, it's unlikely that the web page has much
  // state that will be lost by not calling its unload handlers properly.
  if (!SuddenTerminationAllowed())
    return false;

  // Store the handle before it gets changed.
  base::ProcessHandle handle = GetHandle();
  ProcessDied(handle, base::TERMINATION_STATUS_NORMAL_TERMINATION, 0, false);
  fast_shutdown_started_ = true;
  return true;
}

void RenderProcessHostImpl::DumpHandles() {
#if defined(OS_WIN)
  Send(new ChildProcessMsg_DumpHandles());
  return;
#endif

  NOTIMPLEMENTED();
}

// This is a platform specific function for mapping a transport DIB given its id
TransportDIB* RenderProcessHostImpl::MapTransportDIB(
    TransportDIB::Id dib_id) {
#if defined(OS_WIN)
  // On Windows we need to duplicate the handle from the remote process
  HANDLE section;
  DuplicateHandle(GetHandle(), dib_id.handle, GetCurrentProcess(), &section,
                  STANDARD_RIGHTS_REQUIRED | FILE_MAP_READ | FILE_MAP_WRITE,
                  FALSE, 0);
  return TransportDIB::Map(section);
#elif defined(OS_MACOSX)
  // On OSX, the browser allocates all DIBs and keeps a file descriptor around
  // for each.
  return widget_helper_->MapTransportDIB(dib_id);
#elif defined(OS_ANDROID)
  return TransportDIB::Map(dib_id);
#elif defined(OS_POSIX)
  return TransportDIB::Map(dib_id.shmkey);
#endif  // defined(OS_POSIX)
}

TransportDIB* RenderProcessHostImpl::GetTransportDIB(
    TransportDIB::Id dib_id) {
  if (!TransportDIB::is_valid_id(dib_id))
    return NULL;

  const std::map<TransportDIB::Id, TransportDIB*>::iterator
      i = cached_dibs_.find(dib_id);
  if (i != cached_dibs_.end()) {
    cached_dibs_cleaner_.Reset();
    return i->second;
  }

  TransportDIB* dib = MapTransportDIB(dib_id);
  if (!dib)
    return NULL;

  if (cached_dibs_.size() >= MAX_MAPPED_TRANSPORT_DIBS) {
    // Clean a single entry from the cache
    std::map<TransportDIB::Id, TransportDIB*>::iterator smallest_iterator;
    size_t smallest_size = std::numeric_limits<size_t>::max();

    for (std::map<TransportDIB::Id, TransportDIB*>::iterator
         i = cached_dibs_.begin(); i != cached_dibs_.end(); ++i) {
      if (i->second->size() <= smallest_size) {
        smallest_iterator = i;
        smallest_size = i->second->size();
      }
    }

    delete smallest_iterator->second;
    cached_dibs_.erase(smallest_iterator);
  }

  cached_dibs_[dib_id] = dib;
  cached_dibs_cleaner_.Reset();
  return dib;
}

void RenderProcessHostImpl::ClearTransportDIBCache() {
  STLDeleteContainerPairSecondPointers(
      cached_dibs_.begin(), cached_dibs_.end());
  cached_dibs_.clear();
}

bool RenderProcessHostImpl::Send(IPC::Message* msg) {
  if (!channel_.get()) {
    if (!is_initialized_) {
      queued_messages_.push(msg);
      return true;
    } else {
      delete msg;
      return false;
    }
  }

  if (child_process_launcher_.get() && child_process_launcher_->IsStarting()) {
    queued_messages_.push(msg);
    return true;
  }

  return channel_->Send(msg);
}

bool RenderProcessHostImpl::OnMessageReceived(const IPC::Message& msg) {
  // If we're about to be deleted, or have initiated the fast shutdown sequence,
  // we ignore incoming messages.

  if (deleting_soon_ || fast_shutdown_started_)
    return false;

  mark_child_process_activity_time();
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    // Dispatch control messages.
    bool msg_is_ok = true;
    IPC_BEGIN_MESSAGE_MAP_EX(RenderProcessHostImpl, msg, msg_is_ok)
      IPC_MESSAGE_HANDLER(ChildProcessHostMsg_ShutdownRequest,
                          OnShutdownRequest)
      IPC_MESSAGE_HANDLER(ChildProcessHostMsg_DumpHandlesDone,
                          OnDumpHandlesDone)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SuddenTerminationChanged,
                          SuddenTerminationChanged)
      IPC_MESSAGE_HANDLER(ViewHostMsg_UserMetricsRecordAction,
                          OnUserMetricsRecordAction)
      IPC_MESSAGE_HANDLER(ViewHostMsg_RevealFolderInOS, OnRevealFolderInOS)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SavedPageAsMHTML, OnSavedPageAsMHTML)
      IPC_MESSAGE_UNHANDLED_ERROR()
    IPC_END_MESSAGE_MAP_EX()

    if (!msg_is_ok) {
      // The message had a handler, but its de-serialization failed.
      // We consider this a capital crime. Kill the renderer if we have one.
      LOG(ERROR) << "bad message " << msg.type() << " terminating renderer.";
      content::RecordAction(UserMetricsAction("BadMessageTerminate_BRPH"));
      ReceivedBadMessage();
    }
    return true;
  }

  // Dispatch incoming messages to the appropriate RenderView/WidgetHost.
  RenderWidgetHost* rwh = render_widget_hosts_.Lookup(msg.routing_id());
  if (!rwh) {
    if (msg.is_sync()) {
      // The listener has gone away, so we must respond or else the caller will
      // hang waiting for a reply.
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
      reply->set_reply_error();
      Send(reply);
    }

    // If this is a SwapBuffers, we need to ack it if we're not going to handle
    // it so that the GPU process doesn't get stuck in unscheduled state.
    bool msg_is_ok = true;
    IPC_BEGIN_MESSAGE_MAP_EX(RenderProcessHostImpl, msg, msg_is_ok)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CompositorSurfaceBuffersSwapped,
                          OnCompositorSurfaceBuffersSwappedNoHost)
    IPC_END_MESSAGE_MAP_EX()
    return true;
  }
  return RenderWidgetHostImpl::From(rwh)->OnMessageReceived(msg);
}

void RenderProcessHostImpl::OnChannelConnected(int32 peer_pid) {
#if defined(IPC_MESSAGE_LOG_ENABLED)
  Send(new ChildProcessMsg_SetIPCLoggingEnabled(
      IPC::Logging::GetInstance()->Enabled()));
#endif

  tracked_objects::ThreadData::Status status =
      tracked_objects::ThreadData::status();
  Send(new ChildProcessMsg_SetProfilerStatus(status));
}

void RenderProcessHostImpl::OnChannelError() {
  if (!channel_.get())
    return;

  // Store the handle before it gets changed.
  base::ProcessHandle handle = GetHandle();

  // child_process_launcher_ can be NULL in single process mode or if fast
  // termination happened.
  int exit_code = 0;
  base::TerminationStatus status =
      child_process_launcher_.get() ?
      child_process_launcher_->GetChildTerminationStatus(&exit_code) :
      base::TERMINATION_STATUS_NORMAL_TERMINATION;

#if defined(OS_WIN)
  if (!run_renderer_in_process()) {
    if (status == base::TERMINATION_STATUS_STILL_RUNNING) {
      HANDLE process = child_process_launcher_->GetHandle();
      child_process_watcher_.StartWatching(
          new base::WaitableEvent(process), this);
      return;
    }
  }
#endif
  ProcessDied(handle, status, exit_code, false);
}

// Called when the renderer process handle has been signaled.
void RenderProcessHostImpl::OnWaitableEventSignaled(
    base::WaitableEvent* waitable_event) {
#if defined (OS_WIN)
  base::ProcessHandle handle = GetHandle();
  int exit_code = 0;
  base::TerminationStatus status =
      base::GetTerminationStatus(waitable_event->Release(), &exit_code);
  delete waitable_event;
  ProcessDied(handle, status, exit_code, true);
#endif
}

content::BrowserContext* RenderProcessHostImpl::GetBrowserContext() const {
  return browser_context_;
}

int RenderProcessHostImpl::GetID() const {
  return id_;
}

bool RenderProcessHostImpl::HasConnection() const {
  return channel_.get() != NULL;
}

RenderWidgetHost* RenderProcessHostImpl::GetRenderWidgetHostByID(
    int routing_id) {
  return render_widget_hosts_.Lookup(routing_id);
}

void RenderProcessHostImpl::SetIgnoreInputEvents(bool ignore_input_events) {
  ignore_input_events_ = ignore_input_events;
}

bool RenderProcessHostImpl::IgnoreInputEvents() const {
  return ignore_input_events_;
}

void RenderProcessHostImpl::Attach(RenderWidgetHost* host,
                                   int routing_id) {
  render_widget_hosts_.AddWithID(host, routing_id);
}

void RenderProcessHostImpl::Release(int routing_id) {
  DCHECK(render_widget_hosts_.Lookup(routing_id) != NULL);
  render_widget_hosts_.Remove(routing_id);

  // Make sure that all associated resource requests are stopped.
  CancelResourceRequests(routing_id);

#if defined(OS_WIN)
  // Dump the handle table if handle auditing is enabled.
  const CommandLine& browser_command_line =
      *CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kAuditHandles) ||
      browser_command_line.HasSwitch(switches::kAuditAllHandles)) {
    DumpHandles();

    // We wait to close the channels until the child process has finished
    // dumping handles and sends us ChildProcessHostMsg_DumpHandlesDone.
    return;
  }
#endif
  Cleanup();
}

void RenderProcessHostImpl::Cleanup() {
  // When no other owners of this object, we can delete ourselves
  if (render_widget_hosts_.IsEmpty()) {
    content::NotificationService::current()->Notify(
        content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
        content::Source<RenderProcessHost>(this),
        content::NotificationService::NoDetails());
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
    deleting_soon_ = true;
    // It's important not to wait for the DeleteTask to delete the channel
    // proxy. Kill it off now. That way, in case the profile is going away, the
    // rest of the objects attached to this RenderProcessHost start going
    // away first, since deleting the channel proxy will post a
    // OnChannelClosed() to IPC::ChannelProxy::Context on the IO thread.
    channel_.reset();
    gpu_message_filter_ = NULL;

    // Remove ourself from the list of renderer processes so that we can't be
    // reused in between now and when the Delete task runs.
    g_all_hosts.Get().Remove(GetID());
  }
}

void RenderProcessHostImpl::AddPendingView() {
  pending_views_++;
}

void RenderProcessHostImpl::RemovePendingView() {
  DCHECK(pending_views_);
  pending_views_--;
}

void RenderProcessHostImpl::SetSuddenTerminationAllowed(bool enabled) {
  sudden_termination_allowed_ = enabled;
}

bool RenderProcessHostImpl::SuddenTerminationAllowed() const {
  return sudden_termination_allowed_;
}

base::TimeDelta RenderProcessHostImpl::GetChildProcessIdleTime() const {
  return base::TimeTicks::Now() - child_process_activity_time_;
}

void RenderProcessHostImpl::SurfaceUpdated(int32 surface_id) {
  if (!gpu_message_filter_)
    return;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &GpuMessageFilter::SurfaceUpdated,
      gpu_message_filter_,
      surface_id));
}

IPC::ChannelProxy* RenderProcessHostImpl::GetChannel() {
  return channel_.get();
}

content::RenderProcessHost::RenderWidgetHostsIterator
    RenderProcessHostImpl::GetRenderWidgetHostsIterator() {
  return RenderWidgetHostsIterator(&render_widget_hosts_);
}

bool RenderProcessHostImpl::FastShutdownForPageCount(size_t count) {
  if (render_widget_hosts_.size() == count)
    return FastShutdownIfPossible();
  return false;
}

bool RenderProcessHostImpl::FastShutdownStarted() const {
  return fast_shutdown_started_;
}

// static
void RenderProcessHostImpl::RegisterHost(int host_id,
                                         content::RenderProcessHost* host) {
  g_all_hosts.Get().AddWithID(host, host_id);
}

// static
void RenderProcessHostImpl::UnregisterHost(int host_id) {
  if (g_all_hosts.Get().Lookup(host_id))
    g_all_hosts.Get().Remove(host_id);
}

// static
bool RenderProcessHostImpl::IsSuitableHost(
    content::RenderProcessHost* host,
    content::BrowserContext* browser_context,
    const GURL& site_url) {
  if (run_renderer_in_process())
    return true;

  if (host->GetBrowserContext() != browser_context)
    return false;

  WebUIControllerFactory* factory =
      content::GetContentClient()->browser()->GetWebUIControllerFactory();
  if (factory &&
      ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          host->GetID()) !=
      factory->UseWebUIBindingsForURL(browser_context, site_url)) {
    return false;
  }

  return content::GetContentClient()->browser()->IsSuitableHost(host, site_url);
}

// static
bool content::RenderProcessHost::run_renderer_in_process() {
  return g_run_renderer_in_process_;
}

void content::RenderProcessHost::set_run_renderer_in_process(bool value) {
  g_run_renderer_in_process_ = value;
}

content::RenderProcessHost::iterator
    content::RenderProcessHost::AllHostsIterator() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return iterator(g_all_hosts.Pointer());
}

// static
content::RenderProcessHost* content::RenderProcessHost::FromID(
    int render_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return g_all_hosts.Get().Lookup(render_process_id);
}

// static
bool content::RenderProcessHost::ShouldTryToUseExistingProcessHost(
    BrowserContext* browser_context, const GURL& url) {

  if (run_renderer_in_process())
    return true;

  // NOTE: Sometimes it's necessary to create more render processes than
  //       GetMaxRendererProcessCount(), for instance when we want to create
  //       a renderer process for a browser context that has no existing
  //       renderers. This is OK in moderation, since the
  //       GetMaxRendererProcessCount() is conservative.
  if (g_all_hosts.Get().size() >= GetMaxRendererProcessCount())
    return true;

  return content::GetContentClient()->browser()->
      ShouldTryToUseExistingProcessHost(browser_context, url);
}

// static
content::RenderProcessHost*
    content::RenderProcessHost::GetExistingProcessHost(
        content::BrowserContext* browser_context,
    const GURL& site_url) {
  // First figure out which existing renderers we can use.
  std::vector<content::RenderProcessHost*> suitable_renderers;
  suitable_renderers.reserve(g_all_hosts.Get().size());

  iterator iter(AllHostsIterator());
  while (!iter.IsAtEnd()) {
    if (RenderProcessHostImpl::IsSuitableHost(
            iter.GetCurrentValue(),
            browser_context, site_url))
      suitable_renderers.push_back(iter.GetCurrentValue());

    iter.Advance();
  }

  // Now pick a random suitable renderer, if we have any.
  if (!suitable_renderers.empty()) {
    int suitable_count = static_cast<int>(suitable_renderers.size());
    int random_index = base::RandInt(0, suitable_count - 1);
    return suitable_renderers[random_index];
  }

  return NULL;
}

void RenderProcessHostImpl::ProcessDied(base::ProcessHandle handle,
                                           base::TerminationStatus status,
                                           int exit_code,
                                           bool was_alive) {
  // Our child process has died.  If we didn't expect it, it's a crash.
  // In any case, we need to let everyone know it's gone.
  // The OnChannelError notification can fire multiple times due to nested sync
  // calls to a renderer. If we don't have a valid channel here it means we
  // already handled the error.

  RendererClosedDetails details(handle, status, exit_code, was_alive);
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<RenderProcessHost>(this),
      content::Details<RendererClosedDetails>(&details));

  child_process_launcher_.reset();
  channel_.reset();
  gpu_message_filter_ = NULL;

  IDMap<RenderWidgetHost>::iterator iter(&render_widget_hosts_);
  while (!iter.IsAtEnd()) {
    RenderWidgetHostImpl::From(iter.GetCurrentValue())->OnMessageReceived(
        ViewHostMsg_RenderViewGone(iter.GetCurrentKey(),
                                   static_cast<int>(status),
                                   exit_code));
    iter.Advance();
  }

  ClearTransportDIBCache();

  // this object is not deleted at this point and may be reused later.
  // TODO(darin): clean this up
}

void RenderProcessHostImpl::OnShutdownRequest() {
  // Don't shut down if there are more active RenderViews than the one asking
  // to close, or if there are pending RenderViews being swapped back in.
  int num_active_views = 0;
  for (RenderWidgetHostsIterator iter = GetRenderWidgetHostsIterator();
       iter.IsAtEnd();
       iter.Advance()) {
    const RenderWidgetHost* widget = iter.GetCurrentValue();
    DCHECK(widget);
    if (!widget)
      continue;

    // All RenderWidgetHosts are swapped in.
    if (!widget->IsRenderView()) {
      num_active_views++;
      continue;
    }

    // Don't count swapped out views.
    RenderViewHost* rvh =
        RenderViewHost::From(const_cast<RenderWidgetHost*>(widget));
    if (!static_cast<RenderViewHostImpl*>(rvh)->is_swapped_out())
      num_active_views++;
  }
  if (pending_views_ || num_active_views > 1)
    return;

  // Notify any contents that might have swapped out renderers from this
  // process. They should not attempt to swap them back in.
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSING,
      content::Source<RenderProcessHost>(this),
      content::NotificationService::NoDetails());

  Send(new ChildProcessMsg_Shutdown());
}

void RenderProcessHostImpl::SuddenTerminationChanged(bool enabled) {
  SetSuddenTerminationAllowed(enabled);
}

void RenderProcessHostImpl::OnDumpHandlesDone() {
  Cleanup();
}

void RenderProcessHostImpl::SetBackgrounded(bool backgrounded) {
  // Note: we always set the backgrounded_ value.  If the process is NULL
  // (and hence hasn't been created yet), we will set the process priority
  // later when we create the process.
  backgrounded_ = backgrounded;
  if (!child_process_launcher_.get() || child_process_launcher_->IsStarting())
    return;

#if defined(OS_WIN)
  // The cbstext.dll loads as a global GetMessage hook in the browser process
  // and intercepts/unintercepts the kernel32 API SetPriorityClass in a
  // background thread. If the UI thread invokes this API just when it is
  // intercepted the stack is messed up on return from the interceptor
  // which causes random crashes in the browser process. Our hack for now
  // is to not invoke the SetPriorityClass API if the dll is loaded.
  if (GetModuleHandle(L"cbstext.dll"))
    return;
#endif  // OS_WIN

  child_process_launcher_->SetProcessBackgrounded(backgrounded);
}

void RenderProcessHostImpl::OnProcessLaunched() {
  // No point doing anything, since this object will be destructed soon.  We
  // especially don't want to send the RENDERER_PROCESS_CREATED notification,
  // since some clients might expect a RENDERER_PROCESS_TERMINATED afterwards to
  // properly cleanup.
  if (deleting_soon_)
    return;

  if (child_process_launcher_.get()) {
    if (!child_process_launcher_->GetHandle()) {
      OnChannelError();
      return;
    }

    child_process_launcher_->SetProcessBackgrounded(backgrounded_);
  }

  // NOTE: This needs to be before sending queued messages because
  // ExtensionService uses this notification to initialize the renderer process
  // with state that must be there before any JavaScript executes.
  //
  // The queued messages contain such things as "navigate". If this notification
  // was after, we can end up executing JavaScript before the initialization
  // happens.
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_RENDERER_PROCESS_CREATED,
      content::Source<RenderProcessHost>(this),
      content::NotificationService::NoDetails());

  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }
}

void RenderProcessHostImpl::OnUserMetricsRecordAction(
    const std::string& action) {
  content::RecordComputedAction(action);
}

void RenderProcessHostImpl::OnRevealFolderInOS(const FilePath& path) {
  // Only honor the request if appropriate persmissions are granted.
  if (ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(GetID(),
                                                                 path))
    content::GetContentClient()->browser()->OpenItem(path);
}

void RenderProcessHostImpl::OnSavedPageAsMHTML(int job_id, int64 data_size) {
  MHTMLGenerationManager::GetInstance()->MHTMLGenerated(job_id, data_size);
}

void RenderProcessHostImpl::OnCompositorSurfaceBuffersSwappedNoHost(
      int32 surface_id,
      uint64 surface_handle,
      int32 route_id,
      int32 gpu_process_host_id) {
  TRACE_EVENT0("renderer_host",
               "RenderWidgetHostImpl::OnCompositorSurfaceBuffersSwappedNoHost");
  RenderWidgetHostImpl::AcknowledgeSwapBuffers(route_id,
                                               gpu_process_host_id);
}
