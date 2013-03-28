// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/chrome_restart_request.h"

#include <vector>

#include "ash/ash_switches.h"
#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/pref_service.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_split.h"
#include "base/timer.h"
#include "base/values.h"
#include "cc/base/switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "googleurl/src/gurl.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "media/base/media_switches.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/views/corewm/corewm_switches.h"
#include "webkit/plugins/plugin_switches.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Increase logging level for Guest mode to avoid LOG(INFO) messages in logs.
const char kGuestModeLoggingLevel[] = "1";

// Format of command line switch.
const char kSwitchFormatString[] = " --%s=\"%s\"";

// User name which is used in the Guest session.
const char kGuestUserName[] = "";

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
      ::switches::kAllowWebUICompositing,
      ::switches::kDeviceManagementUrl,
      ::switches::kDisableAccelerated2dCanvas,
      ::switches::kDisableAcceleratedOverflowScroll,
      ::switches::kDisableAcceleratedPlugins,
      ::switches::kDisableAcceleratedVideoDecode,
      ::switches::kDisableBrowserPluginCompositing,
      ::switches::kDisableEncryptedMedia,
      ::switches::kDisableForceCompositingMode,
      ::switches::kDisableGpuShaderDiskCache,
      ::switches::kDisableGpuWatchdog,
      ::switches::kDisableLoginAnimations,
      ::switches::kDisableOobeAnimation,
      ::switches::kDisablePanelFitting,
      ::switches::kDisableSeccompFilterSandbox,
      ::switches::kDisableSeccompSandbox,
      ::switches::kDisableThreadedCompositing,
      ::switches::kEnableAcceleratedOverflowScroll,
      ::switches::kEnableCompositingForFixedPosition,
      ::switches::kEnableGestureTapHighlight,
      ::switches::kDisableGestureTapHighlight,
      ::switches::kEnableLogging,
      ::switches::kEnablePinch,
      ::switches::kEnableThreadedCompositing,
      ::switches::kEnableViewport,
      ::switches::kEnableVsyncNotification,
      ::switches::kForceDeviceScaleFactor,
      ::switches::kGpuStartupDialog,
      ::switches::kHasChromeOSDiamondKey,
      ::switches::kHasChromeOSKeyboard,
      ::switches::kLoginProfile,
      ::switches::kNaturalScrollDefault,
      ::switches::kNoSandbox,
      ::switches::kPpapiFlashArgs,
      ::switches::kPpapiFlashInProcess,
      ::switches::kPpapiFlashPath,
      ::switches::kPpapiFlashVersion,
      ::switches::kPpapiInProcess,
      ::switches::kRendererStartupDialog,
#if defined(USE_XI2_MT)
      ::switches::kTouchCalibration,
#endif
      ::switches::kTouchDevices,
      ::switches::kTouchEvents,
      ::switches::kTouchOptimizedUI,
      ::switches::kUIEnableThreadedCompositing,
      ::switches::kUIMaxFramesPending,
      ::switches::kUIPrioritizeInGpuProcess,
#if defined(USE_CRAS)
      ::switches::kUseCras,
#endif
      ::switches::kUseGL,
      ::switches::kUserDataDir,
      ::switches::kUseExynosVda,
      ::switches::kV,
      ash::switches::kAshTouchHud,
      ash::switches::kAuraLegacyPowerButton,
      ash::switches::kAshDisableNewNetworkStatusArea,
      // Please keep these in alphabetical order. Non-UI Compositor switches
      // here should also be added to
      // content/browser/renderer_host/render_process_host_impl.cc.
      cc::switches::kBackgroundColorInsteadOfCheckerboard,
      cc::switches::kCompositeToMailbox,
      cc::switches::kDisableCheapnessEstimator,
      cc::switches::kDisableImplSidePainting,
      cc::switches::kDisableThreadedAnimation,
      cc::switches::kEnableCompositorFrameMessage,
      cc::switches::kEnableImplSidePainting,
      cc::switches::kEnablePartialSwap,
      cc::switches::kEnablePerTilePainting,
      cc::switches::kEnablePredictionBenchmarking,
      cc::switches::kEnableRightAlignedScheduling,
      cc::switches::kEnableTopControlsPositionCalculation,
      cc::switches::kLowResolutionContentsScaleFactor,
      cc::switches::kMaxPrepaintTileDistance,
      cc::switches::kMaxTilesForInterestArea,
      cc::switches::kMaxUnusedResourceMemoryUsagePercentage,
      cc::switches::kNumRasterThreads,
      cc::switches::kShowCompositedLayerBorders,
      cc::switches::kShowCompositedLayerTree,
      cc::switches::kShowFPSCounter,
      cc::switches::kShowNonOccludingRects,
      cc::switches::kShowOccludingRects,
      cc::switches::kShowPropertyChangedRects,
      cc::switches::kShowReplicaScreenSpaceRects,
      cc::switches::kShowScreenSpaceRects,
      cc::switches::kShowSurfaceDamageRects,
      cc::switches::kSlowDownRasterScaleFactor,
      cc::switches::kTraceAllRenderedFrames,
      cc::switches::kTraceOverdraw,
      cc::switches::kUseColorEstimator,
      cc::switches::kUIEnablePartialSwap,
      cc::switches::kUIEnablePerTilePainting,
      chromeos::switches::kDbusStub,
      gfx::switches::kEnableBrowserTextSubpixelPositioning,
      gfx::switches::kEnableWebkitTextSubpixelPositioning,
      views::corewm::switches::kNoDropShadows,
      views::corewm::switches::kWindowAnimationsDisabled,
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

  // TODO(zelidrag): Remove this hack that get us around compositing bug from
  // http://crbug.com/179256 once that bug is resolved.
  if (command_line->HasSwitch(::switches::kForceAppMode)) {
    std::string switch_to_remove("--");
    switch_to_remove.append(cc::switches::kEnablePartialSwap);
    cmd_line_str = cmd_line_str.replace(cmd_line_str.find(switch_to_remove),
                                        switch_to_remove.length(), "");
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
    const CommandLine& base_command_line,
    CommandLine* command_line) {
  base::DictionaryValue otr_switches;
  otr_switches.SetString(::switches::kGuestSession, std::string());
  otr_switches.SetString(::switches::kIncognito, std::string());
  otr_switches.SetString(::switches::kLoggingLevel, kGuestModeLoggingLevel);
  otr_switches.SetString(::switches::kLoginUser, kGuestUserName);

  // Override the home page.
  otr_switches.SetString(::switches::kHomePage,
                         GURL(chrome::kChromeUINewTabURL).spec());

  return DeriveCommandLine(start_url,
                           base_command_line,
                           otr_switches,
                           command_line);
}

void RestartChrome(const std::string& command_line) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  static bool restart_requested = false;
  if (restart_requested) {
    NOTREACHED() << "Request chrome restart for more than once.";
  }
  restart_requested = true;

  if (!base::chromeos::IsRunningOnChromeOS()) {
    // Relaunch chrome without session manager on dev box.
    ReLaunch(command_line);
    return;
  }

  // ChromeRestartRequest deletes itself after request sent to session manager.
  (new ChromeRestartRequest(command_line))->Start();
}

}  // namespace chromeos
