// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_browser_main_parts.h"

#include <stddef.h>
#include <string.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "chromecast/base/bind_to_task_runner.h"
#include "chromecast/base/cast_constants.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/base/metrics/grouped_histogram.h"
#include "chromecast/base/version.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_content_browser_client.h"
#include "chromecast/browser/cast_feature_list_creator.h"
#include "chromecast/browser/cast_system_memory_pressure_evaluator.h"
#include "chromecast/browser/cast_system_memory_pressure_evaluator_adjuster.h"
#include "chromecast/browser/devtools/remote_debugging_server.h"
#include "chromecast/browser/media/media_caps_impl.h"
#include "chromecast/browser/metrics/cast_browser_metrics.h"
#include "chromecast/browser/service_connector.h"
#include "chromecast/browser/tts/tts_controller_impl.h"
#include "chromecast/browser/tts/tts_platform_stub.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "chromecast/media/base/key_systems_common.h"
#include "chromecast/media/base/media_resource_tracker.h"
#include "chromecast/media/base/video_plane_controller.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"
#include "chromecast/metrics/cast_metrics_service_client.h"
#include "chromecast/net/connectivity_checker.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/service/cast_service.h"
#include "components/prefs/pref_service.h"
#include "components/viz/common/switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_LINUX)
#include <fontconfig/fontconfig.h>
#include <signal.h>
#include <sys/prctl.h>
#include "ui/gfx/linux/fontconfig_util.h"
#endif

#if defined(OS_ANDROID)
#include "chromecast/app/android/crash_handler.h"
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "components/crash/content/browser/child_process_crash_observer_android.h"
#include "net/android/network_change_notifier_factory_android.h"
#elif defined(OS_FUCHSIA)
#include "chromecast/net/network_change_notifier_factory_fuchsia.h"
#else  // defined(OS_FUCHSIA)
#include "chromecast/net/network_change_notifier_factory_cast.h"
#endif  // !(defined(OS_ANDROID) || defined(OS_FUCHSIA))

#if defined(OS_FUCHSIA)
#include "chromecast/net/fake_connectivity_checker.h"
#endif

#if defined(USE_AURA)
// gn check ignored on OverlayManagerCast as it's not a public ozone
// header, but is exported to allow injecting the overlay-composited
// callback.
#include "chromecast/browser/accessibility/accessibility_manager.h"
#include "chromecast/browser/cast_display_configurator.h"
#include "chromecast/browser/devtools/cast_ui_devtools.h"
#include "chromecast/graphics/cast_screen.h"
#include "chromecast/graphics/cast_window_manager_aura.h"
#include "chromecast/media/service/cast_renderer.h"  // nogncheck
#if !defined(OS_FUCHSIA)
#include "components/ui_devtools/devtools_server.h"  // nogncheck
#include "components/ui_devtools/switches.h"         // nogncheck
#endif
#include "components/viz/service/display/overlay_strategy_underlay_cast.h"  // nogncheck
#include "ui/display/screen.h"
#include "ui/views/views_delegate.h"  // nogncheck
#else
#include "chromecast/graphics/cast_window_manager_default.h"
#endif

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
#include "chromecast/browser/extensions/api/tts/tts_extension_api.h"
#include "chromecast/browser/extensions/cast_extension_system.h"
#include "chromecast/browser/extensions/cast_extension_system_factory.h"
#include "chromecast/browser/extensions/cast_extensions_browser_client.h"
#include "chromecast/browser/extensions/cast_prefs.h"
#include "chromecast/common/cast_extensions_client.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"  // nogncheck
#include "extensions/browser/browser_context_keyed_service_factories.h"  // nogncheck
#include "extensions/browser/extension_prefs.h"  // nogncheck
#endif

#if defined(OS_LINUX) && defined(USE_OZONE)
#include "chromecast/browser/exo/wayland_server_controller.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
#include "device/bluetooth/cast/bluetooth_adapter_cast.h"
#endif  // !defined(OS_ANDROID) && !defined(OS_FUCHSIA)

#if !defined(OS_FUCHSIA)
#include "base/bind_helpers.h"
#include "chromecast/base/cast_sys_info_util.h"
#include "chromecast/public/cast_sys_info.h"
#include "components/heap_profiling/multi_process/client_connection_manager.h"
#include "components/heap_profiling/multi_process/supervisor.h"
#endif  // !defined(OS_FUCHSIA)

namespace {

#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
int kSignalsToRunClosure[] = {
    SIGTERM, SIGINT,
};
// Closure to run on SIGTERM and SIGINT.
base::OnceClosure* g_signal_closure = nullptr;
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
  if (*g_signal_closure)
    std::move(*g_signal_closure).Run();
}

void RegisterClosureOnSignal(base::OnceClosure closure) {
  DCHECK(!g_signal_closure);
  DCHECK(closure);
  DCHECK_GT(base::size(kSignalsToRunClosure), 0U);

  // Memory leak on purpose, since |g_signal_closure| should live until
  // process exit.
  g_signal_closure = new base::OnceClosure(std::move(closure));
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

#endif  // !defined(OS_ANDROID) && !defined(OS_FUCHSIA)

#if !defined(OS_FUCHSIA)

std::unique_ptr<heap_profiling::ClientConnectionManager>
CreateClientConnectionManager(
    base::WeakPtr<heap_profiling::Controller> controller_weak_ptr,
    heap_profiling::Mode mode) {
  return std::make_unique<heap_profiling::ClientConnectionManager>(
      std::move(controller_weak_ptr), mode);
}

#endif

#if defined(USE_AURA)

// Provide a basic implementation. No need to override anything since we're not
// planning on customizing any behavior at this point.
class CastViewsDelegate : public views::ViewsDelegate {
 public:
  CastViewsDelegate() = default;
  ~CastViewsDelegate() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastViewsDelegate);
};

#endif  // defined(USE_AURA)

#if defined(OS_LINUX)

base::FilePath GetApplicationFontsDir() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string fontconfig_sysroot;
  if (env->GetVar("FONTCONFIG_SYSROOT", &fontconfig_sysroot)) {
    // Running with hermetic fontconfig; using the full path will not work.
    // Assume the root is base::DIR_MODULE as set by base::SetUpFontconfig().
    return base::FilePath("/fonts");
  } else {
    base::FilePath dir_module;
    base::PathService::Get(base::DIR_MODULE, &dir_module);
    return dir_module.Append("fonts");
  }
}

#endif  // defined(OS_LINUX)

}  // namespace

namespace chromecast {
namespace shell {

namespace {

struct DefaultCommandLineSwitch {
  const char* const switch_name;
  const char* const switch_value;
};

const DefaultCommandLineSwitch kDefaultSwitches[] = {
#if !defined(OS_ANDROID)
    // GPU shader disk cache disabling is largely to conserve disk space.
    {switches::kDisableGpuShaderDiskCache, ""},
#endif
#if BUILDFLAG(IS_CAST_AUDIO_ONLY)
    {switches::kDisableGpu, ""},
    {switches::kDisableSoftwareRasterizer, ""},
#if defined(OS_ANDROID)
    {switches::kDisableFrameRateLimit, ""},
    {switches::kDisableGLDrawingForTests, ""},
    {switches::kDisableGpuCompositing, ""},
    {cc::switches::kDisableThreadedAnimation, ""},
#endif  // defined(OS_ANDROID)
#endif  // BUILDFLAG(IS_CAST_AUDIO_ONLY)
#if defined(OS_LINUX)
#if defined(ARCH_CPU_X86_FAMILY)
    // This is needed for now to enable the x11 Ozone platform to work with
    // current Linux/NVidia OpenGL drivers.
    {switches::kIgnoreGpuBlacklist, ""},
#elif defined(ARCH_CPU_ARM_FAMILY)
#if !BUILDFLAG(IS_CAST_AUDIO_ONLY)
    {switches::kEnableHardwareOverlays, "cast"},
#endif
#endif
#endif  // defined(OS_LINUX)
    // It's better to start GPU process on demand. For example, for TV platforms
    // cast starts in background and can't render until TV switches to cast
    // input.
    {switches::kDisableGpuEarlyInit, ""},
    // Enable navigator.connection API.
    // TODO(derekjchow): Remove this switch when enabled by default.
    {switches::kEnableNetworkInformationDownlinkMax, ""},
    // TODO(halliwell): Remove after fixing b/35422666.
    {switches::kEnableUseZoomForDSF, "false"},
    // TODO(halliwell): Revert after fix for b/63101386.
    {switches::kDisallowNonExactResourceReuse, ""},
    // Enable autoplay without requiring any user gesture.
    {switches::kAutoplayPolicy,
     switches::autoplay::kNoUserGestureRequiredPolicy},
    // Disable pinch zoom gesture.
    {switches::kDisablePinch, ""},
};

void AddDefaultCommandLineSwitches(base::CommandLine* command_line) {
  std::string default_command_line_flags_string =
      BUILDFLAG(DEFAULT_COMMAND_LINE_FLAGS);
  std::vector<std::string> default_command_line_flags_list =
      base::SplitString(default_command_line_flags_string, ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (auto default_command_line_flag : default_command_line_flags_list) {
    std::vector<std::string> default_command_line_flag_content =
        base::SplitString(default_command_line_flag, "=", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    if (default_command_line_flag_content.size() == 2 &&
        !command_line->HasSwitch(default_command_line_flag_content[0])) {
      DVLOG(2) << "Set default command line switch '"
               << default_command_line_flag_content[0] << "' = '"
               << default_command_line_flag_content[1] << "'";
      command_line->AppendSwitchASCII(default_command_line_flag_content[0],
                                      default_command_line_flag_content[1]);
    }
  }

  for (const auto& default_switch : kDefaultSwitches) {
    // Don't override existing command line switch values with these defaults.
    // This could occur primarily (or only) on Android, where the command line
    // is initialized in Java first.
    std::string name(default_switch.switch_name);
    if (!command_line->HasSwitch(name)) {
      std::string value(default_switch.switch_value);
      DVLOG(2) << "Set default switch '" << name << "' = '" << value << "'";
      command_line->AppendSwitchASCII(name, value);
    } else {
      DVLOG(2) << "Skip setting default switch '" << name << "', already set";
    }
  }
}

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
// Instantiates all cast KeyedService factories, which is especially important
// for services that should be created at profile creation time as compared to
// lazily on first access.
void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  extensions::EnsureBrowserContextKeyedServiceFactoriesBuilt();

  extensions::CastExtensionSystemFactory::GetInstance();
}
#endif

}  // namespace

CastBrowserMainParts::CastBrowserMainParts(
    const content::MainFunctionParams& parameters,
    CastContentBrowserClient* cast_content_browser_client)
    : BrowserMainParts(),
      cast_browser_process_(new CastBrowserProcess()),
      parameters_(parameters),
      cast_content_browser_client_(cast_content_browser_client),

      media_caps_(new media::MediaCapsImpl()),
      cast_system_memory_pressure_evaluator_adjuster_(nullptr) {
  DCHECK(cast_content_browser_client);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  AddDefaultCommandLineSwitches(command_line);
}

CastBrowserMainParts::~CastBrowserMainParts() {
  if (cast_content_browser_client_->GetMediaTaskRunner() &&
      media_pipeline_backend_manager_) {
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
    cast_content_browser_client_->GetMediaTaskRunner()->DeleteSoon(
        FROM_HERE, media_pipeline_backend_manager_.release());
  }
}

media::MediaPipelineBackendManager*
CastBrowserMainParts::media_pipeline_backend_manager() {
  if (!media_pipeline_backend_manager_) {
    media_pipeline_backend_manager_ =
        std::make_unique<media::MediaPipelineBackendManager>(
            cast_content_browser_client_->GetMediaTaskRunner(),
            cast_content_browser_client_->media_resource_tracker());
  }
  return media_pipeline_backend_manager_.get();
}

media::MediaCapsImpl* CastBrowserMainParts::media_caps() {
  return media_caps_.get();
}

content::BrowserContext* CastBrowserMainParts::browser_context() {
  return cast_browser_process_->browser_context();
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
#elif defined(OS_FUCHSIA)
  net::NetworkChangeNotifier::SetFactory(
      new NetworkChangeNotifierFactoryFuchsia());
#else   // defined(OS_FUCHSIA)
  net::NetworkChangeNotifier::SetFactory(
      new NetworkChangeNotifierFactoryCast());
#endif  // !(defined(OS_ANDROID) || defined(OS_FUCHSIA))
}

void CastBrowserMainParts::PostMainMessageLoopStart() {
  // Ensure CastMetricsHelper initialized on UI thread.
  metrics::CastMetricsHelper::GetInstance();
}

void CastBrowserMainParts::ToolkitInitialized() {
#if defined(USE_AURA)
  // Needs to be initialize before any UI is created.
  if (!views::ViewsDelegate::GetInstance())
    views_delegate_ = std::make_unique<CastViewsDelegate>();
#endif  // defined(USE_AURA)

#if defined(OS_LINUX)
  base::FilePath dir_font = GetApplicationFontsDir();
  const FcChar8 *dir_font_char8 = reinterpret_cast<const FcChar8*>(dir_font.value().data());
  if (!FcConfigAppFontAddDir(gfx::GetGlobalFontConfig(), dir_font_char8)) {
    LOG(ERROR) << "Cannot load fonts from " << dir_font_char8;
  }
#endif
}

int CastBrowserMainParts::PreCreateThreads() {
#if defined(OS_ANDROID)
  crash_reporter::ChildExitObserver::Create();
  crash_reporter::ChildExitObserver::GetInstance()->RegisterClient(
      std::make_unique<crash_reporter::ChildProcessCrashObserver>());
#endif

  service_connector_ = cast_content_browser_client_->CreateServiceConnector();

  cast_browser_process_->SetPrefService(
      cast_content_browser_client_->GetCastFeatureListCreator()
          ->TakePrefService());

#if defined(USE_AURA)
  cast_screen_ = std::make_unique<CastScreen>();
  cast_browser_process_->SetCastScreen(cast_screen_.get());
  DCHECK(!display::Screen::GetScreen());
  display::Screen::SetScreenInstance(cast_screen_.get());
  cast_browser_process_->SetDisplayConfigurator(
      std::make_unique<CastDisplayConfigurator>(cast_screen_.get()));
#endif  // defined(USE_AURA)

  content::ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeScheme(
      kChromeResourceScheme);
  return 0;
}

void CastBrowserMainParts::PreMainMessageLoopRun() {
#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
  memory_pressure_monitor_.reset(new util::MultiSourceMemoryPressureMonitor());
  auto cast_system_memory_pressure_evaluator =
      std::make_unique<CastSystemMemoryPressureEvaluator>(
          memory_pressure_monitor_->CreateVoter());
  cast_system_memory_pressure_evaluator_adjuster_ =
      cast_system_memory_pressure_evaluator.get();
  memory_pressure_monitor_->SetSystemEvaluator(
      std::move(cast_system_memory_pressure_evaluator));

  // base::Unretained() is safe because the browser client will outlive any
  // component in the browser; this factory method will not be called after
  // the browser starts to tear down.
  device::BluetoothAdapterCast::SetFactory(base::BindRepeating(
      &CastContentBrowserClient::CreateBluetoothAdapter,
      base::Unretained(cast_browser_process_->browser_client())));
#endif  // !defined(OS_ANDROID) && !defined(OS_FUCHSIA)

#if defined(OS_ANDROID)
  crash_reporter_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  crash_reporter_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CastBrowserMainParts::StartPeriodicCrashReportUpload,
                     base::Unretained(this)));
#endif  // defined(OS_ANDROID)

  cast_browser_process_->SetBrowserContext(
      std::make_unique<CastBrowserContext>());

  cast_browser_process_->SetConnectivityChecker(ConnectivityChecker::Create(
      base::CreateSingleThreadTaskRunner({content::BrowserThread::IO}),
      content::BrowserContext::GetDefaultStoragePartition(
          cast_browser_process_->browser_context())
          ->GetURLLoaderFactoryForBrowserProcessIOThread(),
      content::GetNetworkConnectionTracker()));

  cast_browser_process_->SetMetricsServiceClient(
      std::make_unique<metrics::CastMetricsServiceClient>(
          cast_browser_process_->browser_client(),
          cast_browser_process_->pref_service(),
          content::BrowserContext::GetDefaultStoragePartition(
              cast_browser_process_->browser_context())
              ->GetURLLoaderFactoryForBrowserProcess()));
  cast_browser_process_->SetRemoteDebuggingServer(
      std::make_unique<RemoteDebuggingServer>(
          cast_browser_process_->browser_client()
              ->EnableRemoteDebuggingImmediately()));

#if defined(USE_AURA) && !BUILDFLAG(IS_CAST_AUDIO_ONLY)
  // TODO(halliwell) move audio builds to use ozone_platform_cast, then can
  // simplify this by removing IS_CAST_AUDIO_ONLY condition.  Should then also
  // assert(ozone_platform_cast) in BUILD.gn where it depends on //ui/ozone.
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel();
  video_plane_controller_.reset(new media::VideoPlaneController(
      Size(display_size.width(), display_size.height()),
      cast_content_browser_client_->GetMediaTaskRunner()));
  // TODO(crbug.com/925450): Once compositor migrates to GPU process, We
  // don't need to set the callback in viz::OverlayStrategyUnderlayCast.
  viz::OverlayStrategyUnderlayCast::SetOverlayCompositedCallback(
      base::BindRepeating(&media::VideoPlaneController::SetGeometry,
                          base::Unretained(video_plane_controller_.get())));
  media::CastRenderer::SetOverlayCompositedCallback(BindToCurrentThread(
      base::BindRepeating(&media::VideoPlaneController::SetGeometry,
                          base::Unretained(video_plane_controller_.get()))));
#endif

#if defined(USE_AURA)

#if !defined(OS_FUCHSIA)
  // Start UI devtools if this is a dev device or explicitly enabled.
  // Note that this must happen before the window tree host is created by the
  // window manager.
  auto build_type = CreateSysInfo()->GetBuildType();
  if (CAST_IS_DEBUG_BUILD() || build_type == CastSysInfo::BUILD_ENG ||
      ::ui_devtools::UiDevToolsServer::IsUiDevToolsEnabled(
          ::ui_devtools::switches::kEnableUiDevTools)) {
    // Starts the UI Devtools server for browser Aura UI
    ui_devtools_ = std::make_unique<CastUIDevTools>(
        cast_content_browser_client_->GetSystemNetworkContext());
  }
#endif

  window_manager_ = std::make_unique<CastWindowManagerAura>(
      CAST_IS_DEBUG_BUILD() ||
      GetSwitchValueBoolean(switches::kEnableInput, false));
  window_manager_->Setup();

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  cast_browser_process_->SetAccessibilityManager(
      std::make_unique<AccessibilityManager>(window_manager_.get()));
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)

#else   // defined(USE_AURA)
  window_manager_ = std::make_unique<CastWindowManagerDefault>();
#endif  // defined(USE_AURA)

  cast_browser_process_->SetCastService(
      cast_browser_process_->browser_client()->CreateCastService(
          cast_browser_process_->browser_context(),
          cast_system_memory_pressure_evaluator_adjuster_,
          cast_browser_process_->pref_service(),
          video_plane_controller_.get(), window_manager_.get()));
  cast_browser_process_->cast_service()->Initialize();

  cast_content_browser_client_->media_resource_tracker()->InitializeMediaLib();
  ::media::InitializeMediaLibrary();
  media_caps_->Initialize();

  cast_browser_process_->SetTtsController(std::make_unique<TtsControllerImpl>(
      std::make_unique<TtsPlatformImplStub>()));

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  user_pref_service_ = extensions::cast_prefs::CreateUserPrefService(
      cast_browser_process_->browser_context());

  extensions_client_ = std::make_unique<extensions::CastExtensionsClient>();
  extensions::ExtensionsClient::Set(extensions_client_.get());

  extensions_browser_client_ =
      std::make_unique<extensions::CastExtensionsBrowserClient>(
          cast_browser_process_->browser_context(), user_pref_service_.get(),
          cast_content_browser_client_->cast_network_contexts());
  extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());

  EnsureBrowserContextKeyedServiceFactoriesBuilt();

  extensions::CastExtensionSystem* extension_system =
      static_cast<extensions::CastExtensionSystem*>(
          extensions::ExtensionSystem::Get(
              cast_browser_process_->browser_context()));

  extension_system->InitForRegularProfile(true);
  extension_system->Init();

  extensions::ExtensionPrefs::Get(cast_browser_process_->browser_context());

  // Force TTS to be available. It's lazy and this makes it eager.
  // TODO(rdaum): There has to be a better way.
  extensions::TtsAPI::GetFactoryInstance()->Get(
      cast_browser_process_->browser_context());
#endif

#if defined(OS_LINUX) && defined(USE_OZONE)
  wayland_server_controller_ =
      std::make_unique<WaylandServerController>(window_manager_.get());
#endif

  // Initializing metrics service and network delegates must happen after cast
  // service is initialized because CastMetricsServiceClient,
  // CastURLLoaderThrottle and CastNetworkDelegate may use components
  // initialized by cast service.
  cast_browser_process_->cast_browser_metrics()->Initialize();
  cast_content_browser_client_->InitializeURLLoaderThrottleDelegate();

  cast_content_browser_client_->CreateGeneralAudienceBrowsingService();

  // Disable RenderFrameHost's Javascript injection restrictions so that the
  // Cast Web Service can implement its own JS injection policy at a higher
  // level.
  content::RenderFrameHost::AllowInjectingJavaScript();

  cast_browser_process_->cast_service()->Start();

  if (parameters_.ui_task) {
    std::move(*parameters_.ui_task).Run();
    delete parameters_.ui_task;
    run_message_loop_ = false;
  }
}

#if defined(OS_ANDROID)
void CastBrowserMainParts::StartPeriodicCrashReportUpload() {
  OnStartPeriodicCrashReportUpload();
  crash_reporter_timer_.reset(new base::RepeatingTimer());
  crash_reporter_timer_->Start(
      FROM_HERE, base::TimeDelta::FromMinutes(20), this,
      &CastBrowserMainParts::OnStartPeriodicCrashReportUpload);
}

void CastBrowserMainParts::OnStartPeriodicCrashReportUpload() {
  base::FilePath crash_dir;
  if (!CrashHandler::GetCrashDumpLocation(&crash_dir))
    return;
  base::FilePath reports_dir;
  if (!CrashHandler::GetCrashReportsLocation(&reports_dir))
    return;
  CrashHandler::UploadDumps(crash_dir, reports_dir, "", "");
}
#endif  // defined(OS_ANDROID)

bool CastBrowserMainParts::MainMessageLoopRun(int* result_code) {
#if defined(OS_ANDROID)
  // Android does not use native main MessageLoop.
  NOTREACHED();
  return true;
#else
  if (run_message_loop_) {
    base::RunLoop run_loop;

#if !defined(OS_FUCHSIA)
    // Fuchsia doesn't have signals.
    RegisterClosureOnSignal(run_loop.QuitClosure());
#endif  // !defined(OS_FUCHSIA)

    run_loop.Run();
  }

#if !defined(OS_FUCHSIA)
  // Once the main loop has stopped running, we give the browser process a few
  // seconds to stop cast service and finalize all resources. If a hang occurs
  // and cast services refuse to terminate successfully, then we SIGKILL the
  // current process to avoid indefinite hangs.
  //
  // TODO(sergeyu): Fuchsia doesn't implement POSIX signals. Implement a
  // different shutdown watchdog mechanism.
  RegisterKillOnAlarm(kKillOnAlarmTimeoutSec);
#endif  // !defined(OS_FUCHSIA)

  cast_browser_process_->cast_service()->Stop();
  return true;
#endif
}

void CastBrowserMainParts::PostMainMessageLoopRun() {
#if defined(OS_LINUX) && defined(USE_OZONE)
  wayland_server_controller_.reset();
#endif
#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      browser_context());
  extensions::ExtensionsBrowserClient::Set(nullptr);
  extensions_browser_client_.reset();
  user_pref_service_.reset();
  cast_browser_process_->ClearAccessibilityManager();
#endif

#if defined(OS_ANDROID)
  // Android does not use native main MessageLoop.
  NOTREACHED();
#else
  cast_browser_process_->cast_service()->Finalize();
  cast_browser_process_->cast_browser_metrics()->Finalize();
  cast_browser_process_.reset();

  window_manager_.reset();
#if defined(USE_AURA)
  display::Screen::SetScreenInstance(nullptr);
  cast_screen_.reset();
#endif

#if !defined(OS_FUCHSIA)
  DeregisterKillOnAlarm();
#endif  // !defined(OS_FUCHSIA)
#endif
}

void CastBrowserMainParts::PostCreateThreads() {
#if !defined(OS_FUCHSIA)
  heap_profiling::Supervisor* supervisor =
      heap_profiling::Supervisor::GetInstance();
  supervisor->SetClientConnectionManagerConstructor(
      &CreateClientConnectionManager);
  supervisor->Start(base::NullCallback());
#endif  // !defined(OS_FUCHSIA)
}

void CastBrowserMainParts::PostDestroyThreads() {
#if !defined(OS_ANDROID)
  cast_content_browser_client_->ResetMediaResourceTracker();
#endif  // !defined(OS_ANDROID)
}

}  // namespace shell
}  // namespace chromecast
