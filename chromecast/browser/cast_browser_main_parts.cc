// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_browser_main_parts.h"

#include <stddef.h>
#include <string.h>

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "chromecast/base/cast_constants.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/base/cast_sys_info_util.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/base/metrics/grouped_histogram.h"
#include "chromecast/base/version.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_content_browser_client.h"
#include "chromecast/browser/cast_memory_pressure_monitor.h"
#include "chromecast/browser/cast_net_log.h"
#include "chromecast/browser/devtools/remote_debugging_server.h"
#include "chromecast/browser/geolocation/cast_access_token_store.h"
#include "chromecast/browser/media/media_caps_impl.h"
#include "chromecast/browser/metrics/cast_metrics_prefs.h"
#include "chromecast/browser/metrics/cast_metrics_service_client.h"
#include "chromecast/browser/pref_service_helper.h"
#include "chromecast/browser/url_request_context_factory.h"
#include "chromecast/chromecast_features.h"
#include "chromecast/common/global_descriptors.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "chromecast/media/base/key_systems_common.h"
#include "chromecast/media/base/media_resource_tracker.h"
#include "chromecast/media/base/video_plane_controller.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"
#include "chromecast/net/connectivity_checker.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/cast_sys_info.h"
#include "chromecast/service/cast_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "device/geolocation/geolocation_delegate.h"
#include "device/geolocation/geolocation_provider.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_switches.h"

#if !defined(OS_ANDROID)
#include <signal.h>
#include <sys/prctl.h>
#endif
#if defined(OS_LINUX)
#include <fontconfig/fontconfig.h>
#endif

#if defined(OS_ANDROID)
#include "chromecast/app/android/crash_handler.h"
#include "components/crash/content/browser/crash_dump_manager_android.h"
#include "net/android/network_change_notifier_factory_android.h"
#else
#include "chromecast/net/network_change_notifier_factory_cast.h"
#endif

#if defined(USE_AURA)
// gn check ignored on OverlayManagerCast as it's not a public ozone
// header, but is exported to allow injecting the overlay-composited
// callback.
#include "chromecast/graphics/cast_screen.h"
#include "ui/display/screen.h"
#include "ui/ozone/platform/cast/overlay_manager_cast.h"  // nogncheck
#endif

namespace {

#if !defined(OS_ANDROID)
int kSignalsToRunClosure[] = { SIGTERM, SIGINT, };
// Closure to run on SIGTERM and SIGINT.
base::Closure* g_signal_closure = nullptr;
base::PlatformThreadId g_main_thread_id;

void RunClosureOnSignal(int signum) {
  if (base::PlatformThread::CurrentId() != g_main_thread_id) {
    RAW_LOG(INFO, "Received signal on non-main thread\n");
    return;
  }

  char message[48] = "Received close signal: ";
  strncat(message, sys_siglist[signum], sizeof(message) - strlen(message) - 1);
  RAW_LOG(INFO, message);

  DCHECK(g_signal_closure);
  g_signal_closure->Run();
}

void RegisterClosureOnSignal(const base::Closure& closure) {
  DCHECK(!g_signal_closure);
  DCHECK_GT(arraysize(kSignalsToRunClosure), 0U);

  // Memory leak on purpose, since |g_signal_closure| should live until
  // process exit.
  g_signal_closure = new base::Closure(closure);
  g_main_thread_id = base::PlatformThread::CurrentId();

  struct sigaction sa_new;
  memset(&sa_new, 0, sizeof(sa_new));
  sa_new.sa_handler = RunClosureOnSignal;
  sigfillset(&sa_new.sa_mask);
  sa_new.sa_flags = SA_RESTART;

  for (int sig : kSignalsToRunClosure) {
    struct sigaction sa_old;
    if (sigaction(sig, &sa_new, &sa_old) == -1) {
      NOTREACHED();
    } else {
      DCHECK_EQ(sa_old.sa_handler, SIG_DFL);
    }
  }

  // Get the first signal to exit when the parent process dies.
  prctl(PR_SET_PDEATHSIG, kSignalsToRunClosure[0]);
}

const int kKillOnAlarmTimeoutSec = 5;  // 5 seconds

void KillOnAlarm(int signum) {
  LOG(ERROR) << "Got alarm signal for termination: " << signum;
  raise(SIGKILL);
}

void RegisterKillOnAlarm(int timeout_seconds) {
  struct sigaction sa_new;
  memset(&sa_new, 0, sizeof(sa_new));
  sa_new.sa_handler = KillOnAlarm;
  sigfillset(&sa_new.sa_mask);
  sa_new.sa_flags = SA_RESTART;

  struct sigaction sa_old;
  if (sigaction(SIGALRM, &sa_new, &sa_old) == -1) {
    NOTREACHED();
  } else {
    DCHECK_EQ(sa_old.sa_handler, SIG_DFL);
  }

  if (alarm(timeout_seconds) > 0)
    NOTREACHED() << "Previous alarm() was cancelled";
}

void DeregisterKillOnAlarm() {
  // Explicitly cancel any outstanding alarm() calls.
  alarm(0);

  struct sigaction sa_new;
  memset(&sa_new, 0, sizeof(sa_new));
  sa_new.sa_handler = SIG_DFL;
  sigfillset(&sa_new.sa_mask);
  sa_new.sa_flags = SA_RESTART;

  struct sigaction sa_old;
  if (sigaction(SIGALRM, &sa_new, &sa_old) == -1) {
    NOTREACHED();
  } else {
    DCHECK_EQ(sa_old.sa_handler, KillOnAlarm);
  }
}

#endif  // !defined(OS_ANDROID)

}  // namespace

namespace chromecast {
namespace shell {

namespace {

// A provider of services for Geolocation.
class CastGeolocationDelegate : public device::GeolocationDelegate {
 public:
  explicit CastGeolocationDelegate(CastBrowserContext* context)
      : context_(context) {}

  scoped_refptr<device::AccessTokenStore> CreateAccessTokenStore() override {
    return new CastAccessTokenStore(context_);
  }

 private:
  CastBrowserContext* context_;

  DISALLOW_COPY_AND_ASSIGN(CastGeolocationDelegate);
};

struct DefaultCommandLineSwitch {
  const char* const switch_name;
  const char* const switch_value;
};

DefaultCommandLineSwitch g_default_switches[] = {
#if defined(OS_ANDROID)
  // Disables Chromecast-specific WiFi-related features on ATV for now.
  { switches::kNoWifi, "" },
  { switches::kDisableGestureRequirementForMediaPlayback, ""},
  { switches::kDisableMediaSuspend, ""},
#else
  // GPU shader disk cache disabling is largely to conserve disk space.
  { switches::kDisableGpuShaderDiskCache, "" },
  // Enable media sessions by default (even on non-Android platforms).
  { switches::kEnableDefaultMediaSession, "" },
#endif
#if BUILDFLAG(IS_CAST_AUDIO_ONLY)
  { switches::kDisableGpu, "" },
#endif
#if defined(OS_LINUX)
#if defined(ARCH_CPU_X86_FAMILY)
  // This is needed for now to enable the x11 Ozone platform to work with
  // current Linux/NVidia OpenGL drivers.
  { switches::kIgnoreGpuBlacklist, ""},
#elif defined(ARCH_CPU_ARM_FAMILY)
#if !BUILDFLAG(IS_CAST_AUDIO_ONLY)
  {switches::kEnableHardwareOverlays, "cast"},
#endif
#endif
#endif  // defined(OS_LINUX)
  // Needed so that our call to GpuDataManager::SetGLStrings doesn't race
  // against GPU process creation (which is otherwise triggered from
  // BrowserThreadsStarted).  The GPU process will be created as soon as a
  // renderer needs it, which always happens after main loop starts.
  { switches::kDisableGpuEarlyInit, "" },
  // TODO(halliwell): Cast builds don't support ES3. Remove this switch when
  // support is added (crbug.com/659395)
  { switches::kDisableES3GLContext, "" },
  // Enable navigator.connection API.
  // TODO(derekjchow): Remove this switch when enabled by default.
  { switches::kEnableNetworkInformation, "" },
  // TODO(halliwell): Remove after fixing b/35422666.
  { switches::kEnableUseZoomForDSF, "false" },
  { NULL, NULL },  // Termination
};

void AddDefaultCommandLineSwitches(base::CommandLine* command_line) {
  int i = 0;
  while (g_default_switches[i].switch_name != NULL) {
    command_line->AppendSwitchASCII(
        std::string(g_default_switches[i].switch_name),
        std::string(g_default_switches[i].switch_value));
    ++i;
  }
}

}  // namespace

CastBrowserMainParts::CastBrowserMainParts(
    const content::MainFunctionParams& parameters,
    URLRequestContextFactory* url_request_context_factory)
    : BrowserMainParts(),
      cast_browser_process_(new CastBrowserProcess()),
      parameters_(parameters),
      url_request_context_factory_(url_request_context_factory),
      net_log_(new CastNetLog()),
      media_caps_(new media::MediaCapsImpl()) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  AddDefaultCommandLineSwitches(command_line);

#if !defined(OS_ANDROID)
  media_resource_tracker_ = nullptr;
#endif  // !defined(OS_ANDROID)
}

CastBrowserMainParts::~CastBrowserMainParts() {
#if !defined(OS_ANDROID)
  if (media_thread_ && media_pipeline_backend_manager_) {
    // Make sure that media_pipeline_backend_manager_ is destroyed after any
    // pending media thread tasks. The CastAudioOutputStream implementation
    // calls into media_pipeline_backend_manager_ when the stream is closed;
    // therefore, we must be sure that all CastAudioOutputStreams are gone
    // before destroying media_pipeline_backend_manager_. This is guaranteed
    // once the AudioManager is destroyed; the AudioManager destruction is
    // posted to the media thread in the BrowserMainLoop destructor, just before
    // the BrowserMainParts are destroyed (ie, here). Therefore, if we delete
    // the media_pipeline_backend_manager_ using DeleteSoon on the media thread,
    // it is guaranteed that the AudioManager and all AudioOutputStreams have
    // been destroyed before media_pipeline_backend_manager_ is destroyed.
    media_thread_->task_runner()->DeleteSoon(
        FROM_HERE, media_pipeline_backend_manager_.release());
  }
#endif  // !defined(OS_ANDROID)
}

scoped_refptr<base::SingleThreadTaskRunner>
CastBrowserMainParts::GetMediaTaskRunner() {
#if defined(OS_ANDROID)
  return nullptr;
#else
  if (!media_thread_) {
    media_thread_.reset(new base::Thread("CastMediaThread"));
    base::Thread::Options options;
    options.priority = base::ThreadPriority::REALTIME_AUDIO;
    CHECK(media_thread_->StartWithOptions(options));
  }
  return media_thread_->task_runner();
#endif
}

#if !defined(OS_ANDROID)
media::MediaResourceTracker* CastBrowserMainParts::media_resource_tracker() {
  if (!media_resource_tracker_) {
    media_resource_tracker_ = new media::MediaResourceTracker(
        base::ThreadTaskRunnerHandle::Get(), GetMediaTaskRunner());
  }
  return media_resource_tracker_;
}

media::MediaPipelineBackendManager*
CastBrowserMainParts::media_pipeline_backend_manager() {
  if (!media_pipeline_backend_manager_) {
    media_pipeline_backend_manager_.reset(
        new media::MediaPipelineBackendManager(GetMediaTaskRunner()));
  }
  return media_pipeline_backend_manager_.get();
}
#endif

media::MediaCapsImpl* CastBrowserMainParts::media_caps() {
  return media_caps_.get();
}

void CastBrowserMainParts::PreMainMessageLoopStart() {
  // GroupedHistograms needs to be initialized before any threads are created
  // to prevent race conditions between calls to Preregister and those threads
  // attempting to collect metrics.
  // This call must also be before NetworkChangeNotifier, as it generates
  // Net/DNS metrics.
  metrics::PreregisterAllGroupedHistograms();

#if defined(OS_ANDROID)
  net::NetworkChangeNotifier::SetFactory(
      new net::NetworkChangeNotifierFactoryAndroid());
#else
  net::NetworkChangeNotifier::SetFactory(
      new NetworkChangeNotifierFactoryCast());
#endif  // defined(OS_ANDROID)
}

void CastBrowserMainParts::PostMainMessageLoopStart() {
  cast_browser_process_->SetMetricsHelper(
      base::MakeUnique<metrics::CastMetricsHelper>(
          base::ThreadTaskRunnerHandle::Get()));

#if defined(OS_ANDROID)
  base::MessageLoopForUI::current()->Start();
#endif  // defined(OS_ANDROID)
}

void CastBrowserMainParts::ToolkitInitialized() {
#if defined(OS_LINUX)
  // Without this call, the FontConfig library gets implicitly initialized
  // on the first call to FontConfig. Since it's not safe to initialize it
  // concurrently from multiple threads, we explicitly initialize it here
  // to prevent races when there are multiple renderer's querying the library:
  // http://crbug.com/404311
  // Also, implicit initialization can cause a long delay on the first
  // rendering if the font cache has to be regenerated for some reason. Doing it
  // explicitly here helps in cases where the browser process is starting up in
  // the background (resources have not yet been granted to cast) since it
  // prevents the long delay the user would have seen on first rendering. Note
  // that future calls to FcInit() are safe no-ops per the FontConfig interface.
  FcInit();
#endif
}

int CastBrowserMainParts::PreCreateThreads() {
#if defined(OS_ANDROID)
  // GPU process is started immediately after threads are created, requiring
  // CrashDumpManager to be initialized beforehand.
  base::FilePath crash_dumps_dir;
  if (!chromecast::CrashHandler::GetCrashDumpLocation(&crash_dumps_dir)) {
    LOG(ERROR) << "Could not find crash dump location.";
  }
  breakpad::CrashDumpObserver::Create();
  breakpad::CrashDumpObserver::GetInstance()->RegisterClient(
      base::MakeUnique<breakpad::CrashDumpManager>(crash_dumps_dir,
                                                   kAndroidMinidumpDescriptor));
#else
  base::FilePath home_dir;
  CHECK(PathService::Get(DIR_CAST_HOME, &home_dir));
  if (!base::CreateDirectory(home_dir))
    return 1;

  // Hook for internal code
  cast_browser_process_->browser_client()->PreCreateThreads();

  // Set GL strings so GPU config code can make correct feature blacklisting/
  // whitelisting decisions.
  // Note: SetGLStrings can be called before GpuDataManager::Initialize.
  std::unique_ptr<CastSysInfo> sys_info = CreateSysInfo();
  content::GpuDataManager::GetInstance()->SetGLStrings(
      sys_info->GetGlVendor(), sys_info->GetGlRenderer(),
      sys_info->GetGlVersion());
#endif

#if defined(USE_AURA)
  cast_browser_process_->SetCastScreen(base::WrapUnique(new CastScreen()));
  DCHECK(!display::Screen::GetScreen());
  display::Screen::SetScreenInstance(cast_browser_process_->cast_screen());
#endif

  content::ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeScheme(
      kChromeResourceScheme);
  return 0;
}

void CastBrowserMainParts::PreMainMessageLoopRun() {
  scoped_refptr<PrefRegistrySimple> pref_registry(new PrefRegistrySimple());
  metrics::RegisterPrefs(pref_registry.get());
  PrefProxyConfigTrackerImpl::RegisterPrefs(pref_registry.get());
  cast_browser_process_->SetPrefService(
      PrefServiceHelper::CreatePrefService(pref_registry.get()));

#if !defined(OS_ANDROID)
  memory_pressure_monitor_.reset(new CastMemoryPressureMonitor());
#endif  // defined(OS_ANDROID)

  cast_browser_process_->SetConnectivityChecker(ConnectivityChecker::Create(
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::IO),
      url_request_context_factory_->GetSystemGetter()));

  cast_browser_process_->SetNetLog(net_log_.get());

  url_request_context_factory_->InitializeOnUIThread(net_log_.get());

  cast_browser_process_->SetBrowserContext(
      base::MakeUnique<CastBrowserContext>(url_request_context_factory_));
  cast_browser_process_->SetMetricsServiceClient(
      metrics::CastMetricsServiceClient::Create(
          content::BrowserThread::GetBlockingPool(),
          cast_browser_process_->pref_service(),
          content::BrowserContext::GetDefaultStoragePartition(
              cast_browser_process_->browser_context())->
                  GetURLRequestContext()));

  cast_browser_process_->SetRemoteDebuggingServer(
      base::MakeUnique<RemoteDebuggingServer>(
          cast_browser_process_->browser_client()
              ->EnableRemoteDebuggingImmediately()));

#if defined(USE_AURA) && !BUILDFLAG(IS_CAST_AUDIO_ONLY)
  // TODO(halliwell) move audio builds to use ozone_platform_cast, then can
  // simplify this by removing IS_CAST_AUDIO_ONLY condition.  Should then also
  // assert(ozone_platform_cast) in BUILD.gn where it depends on //ui/ozone.
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel();
  video_plane_controller_.reset(new media::VideoPlaneController(
      Size(display_size.width(), display_size.height()), GetMediaTaskRunner()));
  ui::OverlayManagerCast::SetOverlayCompositedCallback(
      base::Bind(&media::VideoPlaneController::SetGeometry,
                 base::Unretained(video_plane_controller_.get())));
#endif

  window_manager_ =
      CastWindowManager::Create(CAST_IS_DEBUG_BUILD() /* enable input */);

  cast_browser_process_->SetCastService(
      cast_browser_process_->browser_client()->CreateCastService(
          cast_browser_process_->browser_context(),
          cast_browser_process_->pref_service(),
          url_request_context_factory_->GetSystemGetter(),
          base::BindOnce(&URLRequestContextFactory::DisableQuic,
                         // Safe since |url_request_context_factory_| is owned
                         // by CastContentBrowserClient, which lives for the
                         // entire lifetime of cast_shell.
                         base::Unretained(url_request_context_factory_)),
          video_plane_controller_.get(), window_manager_.get()));
  cast_browser_process_->cast_service()->Initialize();

#if !defined(OS_ANDROID)
  media_resource_tracker()->InitializeMediaLib();
#endif
  ::media::InitializeMediaLibrary();
  media_caps_->Initialize();

  device::GeolocationProvider::SetGeolocationDelegate(
      new CastGeolocationDelegate(cast_browser_process_->browser_context()));

  // Initializing metrics service and network delegates must happen after cast
  // service is intialized because CastMetricsServiceClient and
  // CastNetworkDelegate may use components initialized by cast service.
  cast_browser_process_->metrics_service_client()
      ->Initialize(cast_browser_process_->cast_service());
  url_request_context_factory_->InitializeNetworkDelegates();

  cast_browser_process_->cast_service()->Start();
}

bool CastBrowserMainParts::MainMessageLoopRun(int* result_code) {
#if defined(OS_ANDROID)
  // Android does not use native main MessageLoop.
  NOTREACHED();
  return true;
#else
  base::RunLoop run_loop;
  base::Closure quit_closure(run_loop.QuitClosure());
  RegisterClosureOnSignal(quit_closure);

  // If parameters_.ui_task is not NULL, we are running browser tests.
  if (parameters_.ui_task) {
    base::MessageLoop* message_loop = base::MessageLoopForUI::current();
    message_loop->task_runner()->PostTask(FROM_HERE, *parameters_.ui_task);
    message_loop->task_runner()->PostTask(FROM_HERE, quit_closure);
  }

  run_loop.Run();

  // Once the main loop has stopped running, we give the browser process a few
  // seconds to stop cast service and finalize all resources. If a hang occurs
  // and cast services refuse to terminate successfully, then we SIGKILL the
  // current process to avoid indefinte hangs.
  RegisterKillOnAlarm(kKillOnAlarmTimeoutSec);

  cast_browser_process_->cast_service()->Stop();
  return true;
#endif
}

void CastBrowserMainParts::PostMainMessageLoopRun() {
#if defined(OS_ANDROID)
  // Android does not use native main MessageLoop.
  NOTREACHED();
#else
  window_manager_.reset();

  cast_browser_process_->cast_service()->Finalize();
  cast_browser_process_->metrics_service_client()->Finalize();
  cast_browser_process_.reset();

  DeregisterKillOnAlarm();
#endif
}

void CastBrowserMainParts::PostDestroyThreads() {
#if !defined(OS_ANDROID)
  media_resource_tracker_->FinalizeAndDestroy();
  media_resource_tracker_ = nullptr;
#endif
}

}  // namespace shell
}  // namespace chromecast
