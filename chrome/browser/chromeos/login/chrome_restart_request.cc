// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/chrome_restart_request.h"

#include <vector>

#include "ash/ash_switches.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/pref_service.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "cc/base/switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/login/user_names.h"
#include "components/policy/core/common/policy_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "media/base/media_switches.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/events/event_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/wm/core/wm_core_switches.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Increase logging level for Guest mode to avoid INFO messages in logs.
const char kGuestModeLoggingLevel[] = "1";

// Format of command line switch.
const char kSwitchFormatString[] = " --%s=\"%s\"";

// Derives the new command line from |base_command_line| by doing the following:
// - Forward a given switches list to new command;
// - Set start url if given;
// - Append/override switches using |new_switches|;
std::string DeriveCommandLine(const GURL& start_url,
                              const CommandLine& base_command_line,
                              const base::DictionaryValue& new_switches,
                              CommandLine* command_line) {
  DCHECK_NE(&base_command_line, command_line);

  static const char* kForwardSwitches[] = {
    ::switches::kDisableAccelerated2dCanvas,
    ::switches::kDisableAcceleratedOverflowScroll,
    ::switches::kDisableAcceleratedVideoDecode,
    ::switches::kDisableDelegatedRenderer,
    ::switches::kDisableDistanceFieldText,
    ::switches::kDisableGpu,
    ::switches::kDisableGpuShaderDiskCache,
    ::switches::kDisableGpuWatchdog,
    ::switches::kDisableGpuCompositing,
    ::switches::kDisableGpuRasterization,
    ::switches::kDisableImplSidePainting,
    ::switches::kDisableLowResTiling,
    ::switches::kDisableMediaSource,
    ::switches::kDisablePrefixedEncryptedMedia,
    ::switches::kDisablePanelFitting,
    ::switches::kDisableSeccompFilterSandbox,
    ::switches::kDisableSetuidSandbox,
    ::switches::kDisableTouchDragDrop,
    ::switches::kDisableTouchEditing,
    ::switches::kDisableAcceleratedFixedRootBackground,
    ::switches::kDisableZeroCopy,
    ::switches::kEnableAcceleratedFixedRootBackground,
    ::switches::kEnableAcceleratedOverflowScroll,
    ::switches::kEnableBeginFrameScheduling,
    ::switches::kEnableCompositingForFixedPosition,
    ::switches::kEnableDelegatedRenderer,
    ::switches::kEnableDisplayList2dCanvas,
    ::switches::kEnableEncryptedMedia,
    ::switches::kDisableGpuSandbox,
    ::switches::kDisableDeferredFilters,
    ::switches::kEnableContainerCulling,
    ::switches::kEnableDistanceFieldText,
    ::switches::kEnableGpuRasterization,
    ::switches::kEnableImplSidePainting,
    ::switches::kEnableLogging,
    ::switches::kEnableLowResTiling,
    ::switches::kEnableOneCopy,
    ::switches::kEnablePinch,
    ::switches::kEnableTouchDragDrop,
    ::switches::kEnableTouchEditing,
    ::switches::kEnableViewport,
    ::switches::kEnableViewportMeta,
    ::switches::kEnableZeroCopy,
    ::switches::kMainFrameResizesAreOrientationChanges,
    ::switches::kForceDeviceScaleFactor,
    ::switches::kForceGpuRasterization,
    ::switches::kGpuStartupDialog,
    ::switches::kGpuSandboxAllowSysVShm,
    ::switches::kGpuSandboxFailuresFatal,
    ::switches::kGpuSandboxStartEarly,
    ::switches::kIgnoreResolutionLimitsForAcceleratedVideoDecode,
    ::switches::kNoSandbox,
    ::switches::kNumRasterThreads,
    ::switches::kPpapiFlashArgs,
    ::switches::kPpapiFlashPath,
    ::switches::kPpapiFlashVersion,
    ::switches::kPpapiInProcess,
    ::switches::kRendererStartupDialog,
    ::switches::kEnableShareGroupAsyncTextureUpload,
    ::switches::kTabCaptureUpscaleQuality,
    ::switches::kTabCaptureDownscaleQuality,
#if defined(USE_XI2_MT)
    ::switches::kTouchCalibration,
#endif
    ::switches::kTouchDevices,
    ::switches::kTouchEvents,
    ::switches::kUIDisableThreadedCompositing,
    ::switches::kUIPrioritizeInGpuProcess,
#if defined(USE_CRAS)
    ::switches::kUseCras,
#endif
    ::switches::kUseDiscardableMemory,
    ::switches::kUseGL,
    ::switches::kUserDataDir,
    ::switches::kV,
    ::switches::kVModule,
    ::switches::kEnableWebGLDraftExtensions,
    ::switches::kEnableWebGLImageChromium,
#if defined(ENABLE_WEBRTC)
    ::switches::kDisableAudioTrackProcessing,
    ::switches::kDisableWebRtcHWDecoding,
    ::switches::kDisableWebRtcHWEncoding,
    ::switches::kEnableWebRtcHWVp8Encoding,
#endif
    ::switches::kDisableVaapiAcceleratedVideoEncode,
#if defined(USE_OZONE)
    ::switches::kOzonePlatform,
#endif
    app_list::switches::kDisableSyncAppList,
    app_list::switches::kEnableSyncAppList,
    ash::switches::kAshDefaultWallpaperLarge,
    ash::switches::kAshDefaultWallpaperSmall,
    ash::switches::kAshGuestWallpaperLarge,
    ash::switches::kAshGuestWallpaperSmall,
    ash::switches::kAshHostWindowBounds,
    ash::switches::kAshTouchHud,
    ash::switches::kAuraLegacyPowerButton,
    // Please keep these in alphabetical order. Non-UI Compositor switches
    // here should also be added to
    // content/browser/renderer_host/render_process_host_impl.cc.
    cc::switches::kCompositeToMailbox,
    cc::switches::kDisableCompositedAntialiasing,
    cc::switches::kDisableMainFrameBeforeActivation,
    cc::switches::kDisableMainFrameBeforeDraw,
    cc::switches::kDisablePinchVirtualViewport,
    cc::switches::kDisableThreadedAnimation,
    cc::switches::kEnableGpuBenchmarking,
    cc::switches::kEnablePinchVirtualViewport,
    cc::switches::kEnableMainFrameBeforeActivation,
    cc::switches::kEnableTopControlsPositionCalculation,
    cc::switches::kMaxTilesForInterestArea,
    cc::switches::kMaxUnusedResourceMemoryUsagePercentage,
    cc::switches::kShowCompositedLayerBorders,
    cc::switches::kShowFPSCounter,
    cc::switches::kShowLayerAnimationBounds,
    cc::switches::kShowNonOccludingRects,
    cc::switches::kShowOccludingRects,
    cc::switches::kShowPropertyChangedRects,
    cc::switches::kShowReplicaScreenSpaceRects,
    cc::switches::kShowScreenSpaceRects,
    cc::switches::kShowSurfaceDamageRects,
    cc::switches::kSlowDownRasterScaleFactor,
    cc::switches::kUIDisablePartialSwap,
    chromeos::switches::kConsumerDeviceManagementUrl,
    chromeos::switches::kDbusStub,
    chromeos::switches::kDbusUnstubClients,
    chromeos::switches::kDisableLoginAnimations,
    chromeos::switches::kEnableConsumerManagement,
    chromeos::switches::kEnterpriseEnableForcedReEnrollment,
    chromeos::switches::kHasChromeOSDiamondKey,
    chromeos::switches::kHasChromeOSKeyboard,
    chromeos::switches::kLoginProfile,
    chromeos::switches::kNaturalScrollDefault,
    chromeos::switches::kSystemInDevMode,
    policy::switches::kDeviceManagementUrl,
    ::switches::kEnableBrowserTextSubpixelPositioning,
    ::switches::kEnableWebkitTextSubpixelPositioning,
    wm::switches::kWindowAnimationsDisabled,
  };
  command_line->CopySwitchesFrom(base_command_line,
                                 kForwardSwitches,
                                 arraysize(kForwardSwitches));

  if (start_url.is_valid())
    command_line->AppendArg(start_url.spec());

  for (base::DictionaryValue::Iterator it(new_switches);
       !it.IsAtEnd();
       it.Advance()) {
    std::string value;
    CHECK(it.value().GetAsString(&value));
    command_line->AppendSwitchASCII(it.key(), value);
  }

  std::string cmd_line_str = command_line->GetCommandLineString();
  // Special workaround for the arguments that should be quoted.
  // Copying switches won't be needed when Guest mode won't need restart
  // http://crosbug.com/6924
  if (base_command_line.HasSwitch(::switches::kRegisterPepperPlugins)) {
    cmd_line_str += base::StringPrintf(
        kSwitchFormatString,
        ::switches::kRegisterPepperPlugins,
        base_command_line.GetSwitchValueNative(
            ::switches::kRegisterPepperPlugins).c_str());
  }

  return cmd_line_str;
}

// Simulates a session manager restart by launching give command line
// and exit current process.
void ReLaunch(const std::string& command_line) {
  std::vector<std::string> argv;

  // This is not a proper way to get |argv| but it's good enough for debugging.
  base::SplitString(command_line, ' ', &argv);

  base::LaunchProcess(argv, base::LaunchOptions(), NULL);
  chrome::AttemptUserExit();
}

// Empty function that run by the local state task runner to ensure last
// commit goes through.
void EnsureLocalStateIsWritten() {}

// Wraps the work of sending chrome restart request to session manager.
// If local state is present, try to commit it first. The request is fired when
// the commit goes through or some time (3 seconds) has elapsed.
class ChromeRestartRequest
    : public base::SupportsWeakPtr<ChromeRestartRequest> {
 public:
  explicit ChromeRestartRequest(const std::string& command_line);
  ~ChromeRestartRequest();

  // Starts the request.
  void Start();

 private:
  // Fires job restart request to session manager.
  void RestartJob();

  const int pid_;
  const std::string command_line_;
  base::OneShotTimer<ChromeRestartRequest> timer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRestartRequest);
};

ChromeRestartRequest::ChromeRestartRequest(const std::string& command_line)
    : pid_(getpid()),
      command_line_(command_line) {}

ChromeRestartRequest::~ChromeRestartRequest() {}

void ChromeRestartRequest::Start() {
  VLOG(1) << "Requesting a restart with PID " << pid_
          << " and command line: " << command_line_;

  // Session Manager may kill the chrome anytime after this point.
  // Write exit_cleanly and other stuff to the disk here.
  g_browser_process->EndSession();

  PrefService* local_state = g_browser_process->local_state();
  if (!local_state) {
    RestartJob();
    return;
  }

  // XXX: normally this call must not be needed, however RestartJob
  // just kills us so settings may be lost. See http://crosbug.com/13102
  local_state->CommitPendingWrite();
  timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(3), this,
      &ChromeRestartRequest::RestartJob);

  // Post a task to local state task runner thus it occurs last on the task
  // queue, so it would be executed after committing pending write on that
  // thread.
  scoped_refptr<base::SequencedTaskRunner> local_state_task_runner =
      JsonPrefStore::GetTaskRunnerForFile(
          base::FilePath(chrome::kLocalStorePoolName),
          BrowserThread::GetBlockingPool());
  local_state_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&EnsureLocalStateIsWritten),
      base::Bind(&ChromeRestartRequest::RestartJob, AsWeakPtr()));
}

void ChromeRestartRequest::RestartJob() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DBusThreadManager::Get()->GetSessionManagerClient()->RestartJob(
      pid_, command_line_);

  delete this;
}

}  // namespace

std::string GetOffTheRecordCommandLine(
    const GURL& start_url,
    bool is_oobe_completed,
    const CommandLine& base_command_line,
    CommandLine* command_line) {
  base::DictionaryValue otr_switches;
  otr_switches.SetString(switches::kGuestSession, std::string());
  otr_switches.SetString(::switches::kIncognito, std::string());
  otr_switches.SetString(::switches::kLoggingLevel, kGuestModeLoggingLevel);
  otr_switches.SetString(switches::kLoginUser, chromeos::login::kGuestUserName);

  // Override the home page.
  otr_switches.SetString(::switches::kHomePage,
                         GURL(chrome::kChromeUINewTabURL).spec());

  // If OOBE is not finished yet, lock down the guest session to not allow
  // surfing the web. Guest mode is still useful to inspect logs and run network
  // diagnostics.
  if (!is_oobe_completed)
    otr_switches.SetString(switches::kOobeGuestSession, std::string());

  return DeriveCommandLine(start_url,
                           base_command_line,
                           otr_switches,
                           command_line);
}

void RestartChrome(const std::string& command_line) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BootTimesLoader::Get()->set_restart_requested();

  static bool restart_requested = false;
  if (restart_requested) {
    NOTREACHED() << "Request chrome restart for more than once.";
  }
  restart_requested = true;

  if (!base::SysInfo::IsRunningOnChromeOS()) {
    // Relaunch chrome without session manager on dev box.
    ReLaunch(command_line);
    return;
  }

  // ChromeRestartRequest deletes itself after request sent to session manager.
  (new ChromeRestartRequest(command_line))->Start();
}

}  // namespace chromeos
