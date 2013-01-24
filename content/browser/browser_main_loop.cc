// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main_loop.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/hi_res_timer_manager.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/run_loop.h"
#include "base/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/download/save_file_manager.h"
#include "content/browser/gamepad/gamepad_service.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/histogram_synchronizer.h"
#include "content/browser/in_process_webkit/webkit_thread.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/net/browser_online_state_observer.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/renderer_host/media/audio_mirroring_manager.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "content/browser/trace_controller_impl.h"
#include "content/browser/webui/url_data_manager.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/browser_shutdown.h"
#include "content/public/browser/compositor_util.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
#include "crypto/nss_util.h"
#include "media/audio/audio_manager.h"
#include "net/base/network_change_notifier.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/tcp_client_socket.h"
#include "ui/base/clipboard/clipboard.h"

#if defined(USE_AURA)
#include "content/browser/renderer_host/image_transport_factory.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "content/browser/android/surface_texture_peer_browser_impl.h"
#include "content/browser/device_orientation/data_fetcher_impl_android.h"
// TODO(epenner): Move thread priorities to base. (crbug.com/170549)
#include <sys/resource.h>
#endif

#if defined(OS_WIN)
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include "content/browser/system_message_window_win.h"
#include "content/common/sandbox_policy.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "net/base/winsock_init.h"
#endif

#if defined(OS_LINUX) || defined(OS_OPENBSD)
#include <glib-object.h>
#endif

#if defined(OS_LINUX)
#include "content/browser/device_monitor_linux.h"
#elif defined(OS_MACOSX) && !defined(OS_IOS)
#include "content/browser/device_monitor_mac.h"
#endif

#if defined(TOOLKIT_GTK)
#include "ui/gfx/gtk_util.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <sys/stat.h>

#include "base/process_util.h"
#include "content/browser/renderer_host/render_sandbox_host_linux.h"
#include "content/browser/zygote_host/zygote_host_impl_linux.h"
#endif

#if defined(USE_X11)
#include <X11/Xlib.h>
#endif

// One of the linux specific headers defines this as a macro.
#ifdef DestroyAll
#undef DestroyAll
#endif

namespace content {
namespace {

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
void SetupSandbox(const CommandLine& parsed_command_line) {
  // TODO(evanm): move this into SandboxWrapper; I'm just trying to move this
  // code en masse out of chrome_main for now.
  const char* sandbox_binary = NULL;
  struct stat st;

  // In Chromium branded builds, developers can set an environment variable to
  // use the development sandbox. See
  // http://code.google.com/p/chromium/wiki/LinuxSUIDSandboxDevelopment
  if (stat(base::kProcSelfExe, &st) == 0 && st.st_uid == getuid())
    sandbox_binary = getenv("CHROME_DEVEL_SANDBOX");

#if defined(LINUX_SANDBOX_PATH)
  if (!sandbox_binary)
    sandbox_binary = LINUX_SANDBOX_PATH;
#endif

  std::string sandbox_cmd;
  if (sandbox_binary && !parsed_command_line.HasSwitch(switches::kNoSandbox))
    sandbox_cmd = sandbox_binary;

  // Tickle the sandbox host and zygote host so they fork now.
  RenderSandboxHostLinux::GetInstance()->Init(sandbox_cmd);
  ZygoteHostImpl::GetInstance()->Init(sandbox_cmd);
}
#endif

#if defined(OS_LINUX) || defined(OS_OPENBSD)
static void GLibLogHandler(const gchar* log_domain,
                           GLogLevelFlags log_level,
                           const gchar* message,
                           gpointer userdata) {
  if (!log_domain)
    log_domain = "<unknown>";
  if (!message)
    message = "<no message>";

  if (strstr(message, "Loading IM context type") ||
      strstr(message, "wrong ELF class: ELFCLASS64")) {
    // http://crbug.com/9643
    // Until we have a real 64-bit build or all of these 32-bit package issues
    // are sorted out, don't fatal on ELF 32/64-bit mismatch warnings and don't
    // spam the user with more than one of them.
    static bool alerted = false;
    if (!alerted) {
      LOG(ERROR) << "Bug 9643: " << log_domain << ": " << message;
      alerted = true;
    }
  } else if (strstr(message, "Unable to retrieve the file info for")) {
    LOG(ERROR) << "GTK File code error: " << message;
  } else if (strstr(message, "Theme file for default has no") ||
             strstr(message, "Theme directory") ||
             strstr(message, "theme pixmap") ||
             strstr(message, "locate theme engine")) {
    LOG(ERROR) << "GTK theme error: " << message;
  } else if (strstr(message, "Unable to create Ubuntu Menu Proxy") &&
             strstr(log_domain, "<unknown>")) {
    LOG(ERROR) << "GTK menu proxy create failed";
  } else if (strstr(message, "gtk_drag_dest_leave: assertion")) {
    LOG(ERROR) << "Drag destination deleted: http://crbug.com/18557";
  } else if (strstr(message, "Out of memory") &&
             strstr(log_domain, "<unknown>")) {
    LOG(ERROR) << "DBus call timeout or out of memory: "
               << "http://crosbug.com/15496";
  } else if (strstr(message, "XDG_RUNTIME_DIR variable not set")) {
    LOG(ERROR) << message << " (http://bugs.chromium.org/97293)";
  } else if (strstr(message, "Attempting to store changes into") ||
             strstr(message, "Attempting to set the permissions of")) {
    LOG(ERROR) << message << " (http://bugs.chromium.org/161366)";
  } else {
    LOG(DFATAL) << log_domain << ": " << message;
  }
}

static void SetUpGLibLogHandler() {
  // Register GLib-handled assertions to go through our logging system.
  const char* kLogDomains[] = { NULL, "Gtk", "Gdk", "GLib", "GLib-GObject" };
  for (size_t i = 0; i < arraysize(kLogDomains); i++) {
    g_log_set_handler(kLogDomains[i],
                      static_cast<GLogLevelFlags>(G_LOG_FLAG_RECURSION |
                                                  G_LOG_FLAG_FATAL |
                                                  G_LOG_LEVEL_ERROR |
                                                  G_LOG_LEVEL_CRITICAL |
                                                  G_LOG_LEVEL_WARNING),
                      GLibLogHandler,
                      NULL);
  }
}
#endif

}  // namespace

// The currently-running BrowserMainLoop.  There can be one or zero.
BrowserMainLoop* g_current_browser_main_loop = NULL;

// This is just to be able to keep ShutdownThreadsAndCleanUp out of
// the public interface of BrowserMainLoop.
class BrowserShutdownImpl {
 public:
  static void ImmediateShutdownAndExitProcess() {
    DCHECK(g_current_browser_main_loop);
    g_current_browser_main_loop->ShutdownThreadsAndCleanUp();

#if defined(OS_WIN)
    // At this point the message loop is still running yet we've shut everything
    // down. If any messages are processed we'll likely crash. Exit now.
    ExitProcess(RESULT_CODE_NORMAL_EXIT);
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
    _exit(RESULT_CODE_NORMAL_EXIT);
#else
    NOTIMPLEMENTED();
#endif
  }
};

void ImmediateShutdownAndExitProcess() {
  BrowserShutdownImpl::ImmediateShutdownAndExitProcess();
}

// static
media::AudioManager* BrowserMainLoop::GetAudioManager() {
  return g_current_browser_main_loop->audio_manager_.get();
}

// static
AudioMirroringManager* BrowserMainLoop::GetAudioMirroringManager() {
  return g_current_browser_main_loop->audio_mirroring_manager_.get();
}

// static
MediaStreamManager* BrowserMainLoop::GetMediaStreamManager() {
  return g_current_browser_main_loop->media_stream_manager_.get();
}
// BrowserMainLoop construction / destruction =============================

BrowserMainLoop::BrowserMainLoop(const MainFunctionParams& parameters)
    : parameters_(parameters),
      parsed_command_line_(parameters.command_line),
      result_code_(RESULT_CODE_NORMAL_EXIT) {
  DCHECK(!g_current_browser_main_loop);
  g_current_browser_main_loop = this;
}

BrowserMainLoop::~BrowserMainLoop() {
  DCHECK_EQ(this, g_current_browser_main_loop);
#if !defined(OS_IOS)
  ui::Clipboard::DestroyClipboardForCurrentThread();
#endif  // !defined(OS_IOS)
  g_current_browser_main_loop = NULL;
}

void BrowserMainLoop::Init() {
  parts_.reset(
      GetContentClient()->browser()->CreateBrowserMainParts(parameters_));
}

// BrowserMainLoop stages ==================================================

void BrowserMainLoop::EarlyInitialization() {
#if defined(USE_X11)
  if (parsed_command_line_.HasSwitch(switches::kSingleProcess) ||
      parsed_command_line_.HasSwitch(switches::kInProcessGPU)) {
    if (!XInitThreads()) {
      LOG(ERROR) << "Failed to put Xlib into threaded mode.";
    }
  }
#endif

  if (parts_.get())
    parts_->PreEarlyInitialization();

#if defined(OS_WIN)
  net::EnsureWinsockInit();
#endif

#if !defined(USE_OPENSSL)
  // We want to be sure to init NSPR on the main thread.
  crypto::EnsureNSPRInit();
#endif  // !defined(USE_OPENSSL)

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  SetupSandbox(parsed_command_line_);
#endif

  if (parsed_command_line_.HasSwitch(switches::kEnableSSLCachedInfo))
    net::SSLConfigService::EnableCachedInfo();

  // TODO(abarth): Should this move to InitializeNetworkOptions?  This doesn't
  // seem dependent on SSL initialization().
  if (parsed_command_line_.HasSwitch(switches::kEnableTcpFastOpen))
    net::set_tcp_fastopen_enabled(true);

#if !defined(OS_IOS)
  if (parsed_command_line_.HasSwitch(switches::kRendererProcessLimit)) {
    std::string limit_string = parsed_command_line_.GetSwitchValueASCII(
        switches::kRendererProcessLimit);
    size_t process_limit;
    if (base::StringToSizeT(limit_string, &process_limit)) {
      RenderProcessHost::SetMaxRendererProcessCount(process_limit);
    }
  }
#endif  // !defined(OS_IOS)

  if (parts_.get())
    parts_->PostEarlyInitialization();
}

void BrowserMainLoop::MainMessageLoopStart() {
  if (parts_.get())
    parts_->PreMainMessageLoopStart();

#if defined(OS_WIN)
  // If we're running tests (ui_task is non-null), then the ResourceBundle
  // has already been initialized.
  if (!parameters_.ui_task) {
    // Override the configured locale with the user's preferred UI language.
    l10n_util::OverrideLocaleWithUILanguageList();
  }
#endif

  // Create a MessageLoop if one does not already exist for the current thread.
  if (!MessageLoop::current())
    main_message_loop_.reset(new MessageLoop(MessageLoop::TYPE_UI));

  InitializeMainThread();

  system_monitor_.reset(new base::SystemMonitor);
  hi_res_timer_manager_.reset(new HighResolutionTimerManager);
  network_change_notifier_.reset(net::NetworkChangeNotifier::Create());
  audio_manager_.reset(media::AudioManager::Create());
#if !defined(OS_IOS)
  audio_mirroring_manager_.reset(new AudioMirroringManager());
#endif

#if !defined(OS_IOS)
  // Start tracing to a file if needed.
  if (base::debug::TraceLog::GetInstance()->IsEnabled()) {
    TraceControllerImpl::GetInstance()->InitStartupTracing(
        parsed_command_line_);
  }

  online_state_observer_.reset(new BrowserOnlineStateObserver);
#endif  // !defined(OS_IOS)

#if defined(ENABLE_PLUGINS)
  // Prior to any processing happening on the io thread, we create the
  // plugin service as it is predominantly used from the io thread,
  // but must be created on the main thread. The service ctor is
  // inexpensive and does not invoke the io_thread() accessor.
  PluginService::GetInstance()->Init();
#endif

#if defined(OS_WIN)
  system_message_window_.reset(new SystemMessageWindowWin);
#endif

  if (parts_.get())
    parts_->PostMainMessageLoopStart();

#if defined(OS_ANDROID)
  SurfaceTexturePeer::InitInstance(new SurfaceTexturePeerBrowserImpl(
      parameters_.command_line.HasSwitch(
          switches::kMediaPlayerInRenderProcess)));
  DataFetcherImplAndroid::Init(base::android::AttachCurrentThread());
#endif
}

void BrowserMainLoop::CreateThreads() {
  if (parts_.get())
    result_code_ = parts_->PreCreateThreads();

#if !defined(OS_IOS) && (!defined(GOOGLE_CHROME_BUILD) || defined(OS_ANDROID))
  // Single-process is an unsupported and not fully tested mode, so
  // don't enable it for official Chrome builds (except on Android).
  if (parsed_command_line_.HasSwitch(switches::kSingleProcess))
    RenderProcessHost::SetRunRendererInProcess(true);
#endif

  if (result_code_ > 0)
    return;

  base::Thread::Options default_options;
  base::Thread::Options io_message_loop_options;
  io_message_loop_options.message_loop_type = MessageLoop::TYPE_IO;
  base::Thread::Options ui_message_loop_options;
  ui_message_loop_options.message_loop_type = MessageLoop::TYPE_UI;

  // Start threads in the order they occur in the BrowserThread::ID
  // enumeration, except for BrowserThread::UI which is the main
  // thread.
  //
  // Must be size_t so we can increment it.
  for (size_t thread_id = BrowserThread::UI + 1;
       thread_id < BrowserThread::ID_COUNT;
       ++thread_id) {
    scoped_ptr<BrowserProcessSubThread>* thread_to_start = NULL;
    base::Thread::Options* options = &default_options;

    switch (thread_id) {
      case BrowserThread::DB:
        thread_to_start = &db_thread_;
        break;
      case BrowserThread::WEBKIT_DEPRECATED:
        // Special case as WebKitThread is a separate
        // type.  |thread_to_start| is not used in this case.
        break;
      case BrowserThread::FILE_USER_BLOCKING:
        thread_to_start = &file_user_blocking_thread_;
        break;
      case BrowserThread::FILE:
        thread_to_start = &file_thread_;
#if defined(OS_WIN)
        // On Windows, the FILE thread needs to be have a UI message loop
        // which pumps messages in such a way that Google Update can
        // communicate back to us.
        options = &ui_message_loop_options;
#else
        options = &io_message_loop_options;
#endif
        break;
      case BrowserThread::PROCESS_LAUNCHER:
        thread_to_start = &process_launcher_thread_;
        break;
      case BrowserThread::CACHE:
        thread_to_start = &cache_thread_;
        options = &io_message_loop_options;
        break;
      case BrowserThread::IO:
        thread_to_start = &io_thread_;
        options = &io_message_loop_options;
        break;
      case BrowserThread::UI:
      case BrowserThread::ID_COUNT:
      default:
        NOTREACHED();
        break;
    }

    BrowserThread::ID id = static_cast<BrowserThread::ID>(thread_id);

    if (thread_id == BrowserThread::WEBKIT_DEPRECATED) {
#if !defined(OS_IOS)
      webkit_thread_.reset(new WebKitThread);
      webkit_thread_->Initialize();
#endif
    } else if (thread_to_start) {
      (*thread_to_start).reset(new BrowserProcessSubThread(id));
      (*thread_to_start)->StartWithOptions(*options);
    } else {
      NOTREACHED();
    }
  }

  BrowserThreadsStarted();

  if (parts_.get())
    parts_->PreMainMessageLoopRun();

  // If the UI thread blocks, the whole UI is unresponsive.
  // Do not allow disk IO from the UI thread.
  base::ThreadRestrictions::SetIOAllowed(false);
  base::ThreadRestrictions::DisallowWaiting();
}

void BrowserMainLoop::RunMainMessageLoopParts() {
  TRACE_EVENT_BEGIN_ETW("BrowserMain:MESSAGE_LOOP", 0, "");

  bool ran_main_loop = false;
  if (parts_.get())
    ran_main_loop = parts_->MainMessageLoopRun(&result_code_);

  if (!ran_main_loop)
    MainMessageLoopRun();

  TRACE_EVENT_END_ETW("BrowserMain:MESSAGE_LOOP", 0, "");
}

void BrowserMainLoop::ShutdownThreadsAndCleanUp() {
  // Teardown may start in PostMainMessageLoopRun, and during teardown we
  // need to be able to perform IO.
  base::ThreadRestrictions::SetIOAllowed(true);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(base::IgnoreResult(&base::ThreadRestrictions::SetIOAllowed),
                 true));

  if (parts_.get())
    parts_->PostMainMessageLoopRun();

#if !defined(OS_IOS)
  // Destroying the GpuProcessHostUIShims on the UI thread posts a task to
  // delete related objects on the GPU thread. This must be done before
  // stopping the GPU thread. The GPU thread will close IPC channels to renderer
  // processes so this has to happen before stopping the IO thread.
  GpuProcessHostUIShim::DestroyAll();

  // Cancel pending requests and prevent new requests.
  if (resource_dispatcher_host_.get())
    resource_dispatcher_host_.get()->Shutdown();

#if defined(USE_AURA)
  ImageTransportFactory::Terminate();
#endif

  // The device monitors are using |system_monitor_| as dependency, so delete
  // them before |system_monitor_| goes away.
  // On Mac and windows, the monitor needs to be destroyed on the same thread
  // as they were created. On Linux, the monitor will be deleted when IO thread
  // goes away.
#if defined(OS_WIN)
  system_message_window_.reset();
#elif defined(OS_MACOSX)
  device_monitor_mac_.reset();
#endif
#endif  // !defined(OS_IOS)

  // Must be size_t so we can subtract from it.
  for (size_t thread_id = BrowserThread::ID_COUNT - 1;
       thread_id >= (BrowserThread::UI + 1);
       --thread_id) {
    // Find the thread object we want to stop. Looping over all valid
    // BrowserThread IDs and DCHECKing on a missing case in the switch
    // statement helps avoid a mismatch between this code and the
    // BrowserThread::ID enumeration.
    //
    // The destruction order is the reverse order of occurrence in the
    // BrowserThread::ID list. The rationale for the order is as
    // follows (need to be filled in a bit):
    //
    //
    // - The IO thread is the only user of the CACHE thread.
    //
    // - The PROCESS_LAUNCHER thread must be stopped after IO in case
    //   the IO thread posted a task to terminate a process on the
    //   process launcher thread.
    //
    // - (Not sure why FILE needs to stop before WEBKIT.)
    //
    // - The WEBKIT thread (which currently is the responsibility of
    //   the embedder to stop, by destroying ResourceDispatcherHost
    //   before the DB thread is stopped)
    //
    // - (Not sure why DB stops last.)
    scoped_ptr<BrowserProcessSubThread>* thread_to_stop = NULL;
    switch (thread_id) {
      case BrowserThread::DB:
        thread_to_stop = &db_thread_;
        break;
      case BrowserThread::WEBKIT_DEPRECATED:
        // Special case as WebKitThread is a separate
        // type.  |thread_to_stop| is not used in this case.

        // Need to destroy ResourceDispatcherHost before PluginService
        // and since it caches a pointer to it.
        resource_dispatcher_host_.reset();
        break;
      case BrowserThread::FILE_USER_BLOCKING:
        thread_to_stop = &file_user_blocking_thread_;
        break;
      case BrowserThread::FILE:
        thread_to_stop = &file_thread_;

#if !defined(OS_IOS)
        // Clean up state that lives on or uses the file_thread_ before
        // it goes away.
        if (resource_dispatcher_host_.get())
          resource_dispatcher_host_.get()->save_file_manager()->Shutdown();
#endif  // !defined(OS_IOS)
        break;
      case BrowserThread::PROCESS_LAUNCHER:
        thread_to_stop = &process_launcher_thread_;
        break;
      case BrowserThread::CACHE:
        thread_to_stop = &cache_thread_;
        break;
      case BrowserThread::IO:
        thread_to_stop = &io_thread_;
        break;
      case BrowserThread::UI:
      case BrowserThread::ID_COUNT:
      default:
        NOTREACHED();
        break;
    }

    BrowserThread::ID id = static_cast<BrowserThread::ID>(thread_id);

    if (id == BrowserThread::WEBKIT_DEPRECATED) {
#if !defined(OS_IOS)
      webkit_thread_.reset();
#endif
    } else if (thread_to_stop) {
      thread_to_stop->reset();
    } else {
      NOTREACHED();
    }
  }

  // Close the blocking I/O pool after the other threads. Other threads such
  // as the I/O thread may need to schedule work like closing files or flushing
  // data during shutdown, so the blocking pool needs to be available. There
  // may also be slow operations pending that will blcok shutdown, so closing
  // it here (which will block until required operations are complete) gives
  // more head start for those operations to finish.
  BrowserThreadImpl::ShutdownThreadPool();

#if !defined(OS_IOS)
  // Must happen after the IO thread is shutdown since this may be accessed from
  // it.
  BrowserGpuChannelHostFactory::Terminate();

  // Must happen after the I/O thread is shutdown since this class lives on the
  // I/O thread and isn't threadsafe.
  GamepadService::GetInstance()->Terminate();

  ChromeURLDataManager::DeleteDataSources();
#endif  // !defined(OS_IOS)

  if (parts_.get())
    parts_->PostDestroyThreads();
}

void BrowserMainLoop::InitializeMainThread() {
  const char* kThreadName = "CrBrowserMain";
  base::PlatformThread::SetName(kThreadName);
  if (main_message_loop_.get())
    main_message_loop_->set_thread_name(kThreadName);

  // Register the main thread by instantiating it, but don't call any methods.
  main_thread_.reset(new BrowserThreadImpl(BrowserThread::UI,
                                           MessageLoop::current()));
}

#if defined(OS_ANDROID)
// TODO(epenner): Move thread priorities to base. (crbug.com/170549)
namespace {
void SetHighThreadPriority() {
  int nice_value = -6; // High priority.
  setpriority(PRIO_PROCESS, base::PlatformThread::CurrentId(), nice_value);
}
}
#endif

void BrowserMainLoop::BrowserThreadsStarted() {
#if defined(OS_ANDROID)
// TODO(epenner): Move thread priorities to base. (crbug.com/170549)
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&SetHighThreadPriority));
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&SetHighThreadPriority));
#endif

#if !defined(OS_IOS)
  HistogramSynchronizer::GetInstance();

  BrowserGpuChannelHostFactory::Initialize();
#if defined(USE_AURA)
  ImageTransportFactory::Initialize();
#endif

#if defined(OS_LINUX)
  device_monitor_linux_.reset(new DeviceMonitorLinux());
#elif defined(OS_MACOSX)
  device_monitor_mac_.reset(new DeviceMonitorMac());
#endif

  // RDH needs the IO thread to be created.
  resource_dispatcher_host_.reset(new ResourceDispatcherHostImpl());

  // MediaStreamManager needs the IO thread to be created.
  media_stream_manager_.reset(new MediaStreamManager(audio_manager_.get()));

  // Initialize the GpuDataManager before we set up the MessageLoops because
  // otherwise we'll trigger the assertion about doing IO on the UI thread.
  GpuDataManagerImpl::GetInstance()->Initialize();
#endif  // !OS_IOS

#if defined(ENABLE_INPUT_SPEECH)
  speech_recognition_manager_.reset(new SpeechRecognitionManagerImpl());
#endif

#if !defined(OS_IOS)
  // Alert the clipboard class to which threads are allowed to access the
  // clipboard:
  std::vector<base::PlatformThreadId> allowed_clipboard_threads;
  // The current thread is the UI thread.
  allowed_clipboard_threads.push_back(base::PlatformThread::CurrentId());
#if defined(OS_WIN)
  // On Windows, clipboards are also used on the File or IO threads.
  allowed_clipboard_threads.push_back(file_thread_->thread_id());
  allowed_clipboard_threads.push_back(io_thread_->thread_id());
#endif
  ui::Clipboard::SetAllowedThreads(allowed_clipboard_threads);

  // When running the GPU thread in-process, avoid optimistically starting it
  // since creating the GPU thread races against creation of the one-and-only
  // ChildProcess instance which is created by the renderer thread.
  if (GpuDataManagerImpl::GetInstance()->GpuAccessAllowed() &&
      IsForceCompositingModeEnabled() &&
      !parsed_command_line_.HasSwitch(switches::kDisableGpuProcessPrelaunch) &&
      !parsed_command_line_.HasSwitch(switches::kSingleProcess) &&
      !parsed_command_line_.HasSwitch(switches::kInProcessGPU)) {
    TRACE_EVENT_INSTANT0("gpu", "Post task to launch GPU process");
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, base::Bind(
            base::IgnoreResult(&GpuProcessHost::Get),
            GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
            CAUSE_FOR_GPU_LAUNCH_BROWSER_STARTUP));
  }
#endif  // !defined(OS_IOS)
}

void BrowserMainLoop::InitializeToolkit() {
  // TODO(evan): this function is rather subtle, due to the variety
  // of intersecting ifdefs we have.  To keep it easy to follow, there
  // are no #else branches on any #ifs.
  // TODO(stevenjb): Move platform specific code into platform specific Parts
  // (Need to add InitializeToolkit stage to BrowserParts).
#if defined(OS_LINUX) || defined(OS_OPENBSD)
  // g_type_init will be deprecated in 2.36. 2.35 is the development
  // version for 2.36, hence do not call g_type_init starting 2.35.
  // http://developer.gnome.org/gobject/unstable/gobject-Type-Information.html#g-type-init
#if !GLIB_CHECK_VERSION(2, 35, 0)
  // Glib type system initialization. Needed at least for gconf,
  // used in net/proxy/proxy_config_service_linux.cc. Most likely
  // this is superfluous as gtk_init() ought to do this. It's
  // definitely harmless, so retained as a reminder of this
  // requirement for gconf.
  g_type_init();
#endif

#if !defined(USE_AURA)
  gfx::GtkInitFromCommandLine(parsed_command_line_);
#endif

  SetUpGLibLogHandler();
#endif

#if defined(TOOLKIT_GTK)
  // It is important for this to happen before the first run dialog, as it
  // styles the dialog as well.
  gfx::InitRCStyles();
#endif

#if defined(OS_WIN)
  // Init common control sex.
  INITCOMMONCONTROLSEX config;
  config.dwSize = sizeof(config);
  config.dwICC = ICC_WIN95_CLASSES;
  if (!InitCommonControlsEx(&config))
    LOG_GETLASTERROR(FATAL);
#endif

  if (parts_.get())
    parts_->ToolkitInitialized();
}

void BrowserMainLoop::MainMessageLoopRun() {
#if defined(OS_ANDROID)
  // Android's main message loop is the Java message loop.
  NOTREACHED();
#else
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());
  if (parameters_.ui_task)
    MessageLoopForUI::current()->PostTask(FROM_HERE, *parameters_.ui_task);

  base::RunLoop run_loop;
  run_loop.Run();
#endif
}

}  // namespace content
