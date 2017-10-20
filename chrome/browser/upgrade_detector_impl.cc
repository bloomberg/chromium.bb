// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector_impl.h"

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/build_time.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/process/launch.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/mac/keystone_glue.h"
#endif

namespace {

// How long (in milliseconds) to wait (each cycle) before checking whether
// Chrome's been upgraded behind our back.
const int kCheckForUpgradeMs = 2 * 60 * 60 * 1000;  // 2 hours.

// How long to wait (each cycle) before checking which severity level we should
// be at. Once we reach the highest severity, the timer will stop.
const int kNotifyCycleTimeMs = 20 * 60 * 1000;  // 20 minutes.

// Same as kNotifyCycleTimeMs but only used during testing.
const int kNotifyCycleTimeForTestingMs = 500;  // Half a second.

// The number of days after which we identify a build/install as outdated.
const uint64_t kOutdatedBuildAgeInDays = 12 * 7;

// Return the string that was passed as a value for the
// kCheckForUpdateIntervalSec switch.
std::string CmdLineInterval() {
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  return cmd_line.GetSwitchValueASCII(switches::kCheckForUpdateIntervalSec);
}

// Check if one of the outdated simulation switches was present on the command
// line.
bool SimulatingOutdated() {
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  return cmd_line.HasSwitch(switches::kSimulateOutdated) ||
      cmd_line.HasSwitch(switches::kSimulateOutdatedNoAU);
}

// Check if any of the testing switches was present on the command line.
bool IsTesting() {
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  return cmd_line.HasSwitch(switches::kSimulateUpgrade) ||
      cmd_line.HasSwitch(switches::kCheckForUpdateIntervalSec) ||
      cmd_line.HasSwitch(switches::kSimulateCriticalUpdate) ||
      SimulatingOutdated();
}

// How often to check for an upgrade.
int GetCheckForUpgradeEveryMs() {
  // Check for a value passed via the command line.
  int interval_ms;
  std::string interval = CmdLineInterval();
  if (!interval.empty() && base::StringToInt(interval, &interval_ms))
    return interval_ms * 1000;  // Command line value is in seconds.

  return kCheckForUpgradeMs;
}

#if !defined(OS_WIN) || defined(GOOGLE_CHROME_BUILD)
// Return true if the current build is one of the unstable channels.
bool IsUnstableChannel() {
  version_info::Channel channel = chrome::GetChannel();
  return channel == version_info::Channel::DEV ||
         channel == version_info::Channel::CANARY;
}
#endif  // !defined(OS_WIN) || defined(GOOGLE_CHROME_BUILD)

// Gets the currently installed version. On Windows, if |critical_update| is not
// NULL, also retrieves the critical update version info if available.
base::Version GetCurrentlyInstalledVersionImpl(base::Version* critical_update) {
  base::AssertBlockingAllowed();

  base::Version installed_version;
#if defined(OS_WIN)
  // Get the version of the currently *installed* instance of Chrome,
  // which might be newer than the *running* instance if we have been
  // upgraded in the background.
  bool system_install = !InstallUtil::IsPerUserInstall();

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  InstallUtil::GetChromeVersion(dist, system_install, &installed_version);
  if (critical_update && installed_version.IsValid()) {
    InstallUtil::GetCriticalUpdateVersion(dist, system_install,
                                          critical_update);
  }
#elif defined(OS_MACOSX)
  installed_version = base::Version(
      base::UTF16ToASCII(keystone_glue::CurrentlyInstalledVersion()));
#elif defined(OS_POSIX)
  // POSIX but not Mac OS X: Linux, etc.
  base::CommandLine command_line(*base::CommandLine::ForCurrentProcess());
  command_line.AppendSwitch(switches::kProductVersion);
  std::string reply;
  if (!base::GetAppOutput(command_line, &reply)) {
    DLOG(ERROR) << "Failed to get current file version";
    return installed_version;
  }
  base::TrimWhitespaceASCII(reply, base::TRIM_ALL, &reply);

  installed_version = base::Version(reply);
#endif
  return installed_version;
}

}  // namespace

UpgradeDetectorImpl::UpgradeDetectorImpl()
    : blocking_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
           base::MayBlock()})),
      is_unstable_channel_(false),
      is_auto_update_enabled_(true),
      build_date_(base::GetBuildTime()),
      weak_factory_(this) {
  base::CommandLine command_line(*base::CommandLine::ForCurrentProcess());
  // The different command line switches that affect testing can't be used
  // simultaneously, if they do, here's the precedence order, based on the order
  // of the if statements below:
  // - kDisableBackgroundNetworking prevents any of the other command line
  //   switch from being taken into account.
  // - kSimulateUpgrade supersedes critical or outdated upgrade switches.
  // - kSimulateCriticalUpdate has precedence over kSimulateOutdated.
  // - kSimulateOutdatedNoAU has precedence over kSimulateOutdated.
  // - kSimulateOutdated[NoAu] can work on its own, or with a specified date.
  if (command_line.HasSwitch(switches::kDisableBackgroundNetworking))
    return;
  if (command_line.HasSwitch(switches::kSimulateUpgrade)) {
    UpgradeDetected(UPGRADE_AVAILABLE_REGULAR);
    return;
  }
  if (command_line.HasSwitch(switches::kSimulateCriticalUpdate)) {
    UpgradeDetected(UPGRADE_AVAILABLE_CRITICAL);
    return;
  }
  if (SimulatingOutdated()) {
    // The outdated simulation can work without a value, which means outdated
    // now, or with a value that must be a well formed date/time string that
    // overrides the build date.
    // Also note that to test with a given time/date, until the network time
    // tracking moves off of the VariationsService, the "variations-server-url"
    // command line switch must also be specified for the service to be
    // available on non GOOGLE_CHROME_BUILD.
    std::string switch_name;
    if (command_line.HasSwitch(switches::kSimulateOutdatedNoAU)) {
      is_auto_update_enabled_ = false;
      switch_name = switches::kSimulateOutdatedNoAU;
    } else {
      switch_name = switches::kSimulateOutdated;
    }
    std::string build_date = command_line.GetSwitchValueASCII(switch_name);
    base::Time maybe_build_time;
    bool result = base::Time::FromString(build_date.c_str(), &maybe_build_time);
    if (result && !maybe_build_time.is_null()) {
      // We got a valid build date simulation so use it and check for upgrades.
      build_date_ = maybe_build_time;
      StartTimerForUpgradeCheck();
    } else {
      // Without a valid date, we simulate that we are already outdated...
      UpgradeDetected(
          is_auto_update_enabled_ ? UPGRADE_NEEDED_OUTDATED_INSTALL
                                  : UPGRADE_NEEDED_OUTDATED_INSTALL_NO_AU);
    }
    return;
  }

  // Register for experiment notifications. Note that since this class is a
  // singleton, it does not need to unregister for notifications when destroyed,
  // since it outlives the VariationsService.
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  if (variations_service)
    variations_service->AddObserver(this);

#if defined(OS_WIN)
// Only enable upgrade notifications for Google Chrome builds. Chromium has no
// upgrade channel.
#if defined(GOOGLE_CHROME_BUILD)
  // Check whether the build is an unstable channel before starting the timer.
  is_unstable_channel_ = IsUnstableChannel();
  // There might be a policy/enterprise environment preventing updates, so
  // validate updatability and then call StartTimerForUpgradeCheck
  // appropriately. Skip this step if a past attempt has been made to enable
  // auto updates.
  if (g_browser_process->local_state() &&
      g_browser_process->local_state()->GetBoolean(
          prefs::kAttemptedToEnableAutoupdate)) {
    StartTimerForUpgradeCheck();
  } else {
    base::PostTaskAndReplyWithResult(
        blocking_task_runner_.get(), FROM_HERE,
        base::BindOnce(&GoogleUpdateSettings::AreAutoupdatesEnabled),
        base::BindOnce(&UpgradeDetectorImpl::OnAutoupdatesEnabledResult,
                       weak_factory_.GetWeakPtr()));
  }
#endif  // defined(GOOGLE_CHROME_BUILD)
#else   // defined(OS_WIN)
#if defined(OS_MACOSX)
  // Only enable upgrade notifications if the updater (Keystone) is present.
  if (!keystone_glue::KeystoneEnabled()) {
    is_auto_update_enabled_ = false;
    return;
  }
#elif defined(OS_POSIX)
  // Always enable upgrade notifications regardless of branding.
#else
  return;
#endif
  // Check whether the build is an unstable channel before starting the timer.
  is_unstable_channel_ = IsUnstableChannel();
  StartTimerForUpgradeCheck();
#endif  // defined(OS_WIN)
}

UpgradeDetectorImpl::~UpgradeDetectorImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
base::Version UpgradeDetectorImpl::GetCurrentlyInstalledVersion() {
  return GetCurrentlyInstalledVersionImpl(NULL);
}

// static
void UpgradeDetectorImpl::DetectUpgradeTask(
    scoped_refptr<base::TaskRunner> callback_task_runner,
    UpgradeDetectedCallback callback) {
  base::Version critical_update;
  base::Version installed_version =
      GetCurrentlyInstalledVersionImpl(&critical_update);

  // Get the version of the currently *running* instance of Chrome.
  base::Version running_version(version_info::GetVersionNumber());
  if (!running_version.IsValid()) {
    NOTREACHED();
    return;
  }

  // |installed_version| may be NULL when the user downgrades on Linux (by
  // switching from dev to beta channel, for example). The user needs a
  // restart in this case as well. See http://crbug.com/46547
  if (!installed_version.IsValid() || installed_version > running_version) {
    // If a more recent version is available, it might be that we are lacking
    // a critical update, such as a zero-day fix.
    UpgradeAvailable upgrade_available = UPGRADE_AVAILABLE_REGULAR;
    if (critical_update.IsValid() && critical_update > running_version)
      upgrade_available = UPGRADE_AVAILABLE_CRITICAL;

    // Fire off the upgrade detected task.
    callback_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), upgrade_available));
  }
}

void UpgradeDetectorImpl::StartTimerForUpgradeCheck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  detect_upgrade_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(GetCheckForUpgradeEveryMs()),
      this, &UpgradeDetectorImpl::CheckForUpgrade);
}

void UpgradeDetectorImpl::StartUpgradeNotificationTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The timer may already be running (e.g. due to both a software upgrade and
  // experiment updates being available).
  if (upgrade_notification_timer_.IsRunning())
    return;

  upgrade_detected_time_ = base::TimeTicks::Now();

  // Start the repeating timer for notifying the user after a certain period.
  // The called function will eventually figure out that enough time has passed
  // and stop the timer.
  const int cycle_time_ms = IsTesting() ?
      kNotifyCycleTimeForTestingMs : kNotifyCycleTimeMs;
  upgrade_notification_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(cycle_time_ms),
      this, &UpgradeDetectorImpl::NotifyOnUpgrade);
}

void UpgradeDetectorImpl::CheckForUpgrade() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Interrupt any (unlikely) unfinished execution of DetectUpgradeTask, or at
  // least prevent the callback from being executed, because we will potentially
  // call it from within DetectOutdatedInstall() or will post
  // DetectUpgradeTask again below anyway.
  weak_factory_.InvalidateWeakPtrs();

  // No need to look for upgrades if the install is outdated.
  if (DetectOutdatedInstall())
    return;

  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UpgradeDetectorImpl::DetectUpgradeTask,
                     base::SequencedTaskRunnerHandle::Get(),
                     base::BindOnce(&UpgradeDetectorImpl::UpgradeDetected,
                                    weak_factory_.GetWeakPtr())));
}

bool UpgradeDetectorImpl::DetectOutdatedInstall() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr base::Feature kOutdatedBuildDetector = {
      "OutdatedBuildDetector", base::FEATURE_ENABLED_BY_DEFAULT};

  if (!base::FeatureList::IsEnabled(kOutdatedBuildDetector))
    return false;

  // Don't show the bubble if we have a brand code that is NOT organic, unless
  // an outdated build is being simulated by command line switches.
  static bool simulate_outdated = SimulatingOutdated();
  if (!simulate_outdated) {
    std::string brand;
    if (google_brand::GetBrand(&brand) && !google_brand::IsOrganic(brand))
      return false;

#if defined(OS_WIN)
    // Don't show the update bubbles to enterprise users.
    if (base::win::IsEnterpriseManaged())
      return false;
#endif
  }

  base::Time network_time;
  base::TimeDelta uncertainty;
  if (g_browser_process->network_time_tracker()->GetNetworkTime(&network_time,
                                                                &uncertainty) !=
      network_time::NetworkTimeTracker::NETWORK_TIME_AVAILABLE) {
    // When network time has not been initialized yet, simply rely on the
    // machine's current time.
    network_time = base::Time::Now();
  }

  if (network_time.is_null() || build_date_.is_null() ||
      build_date_ > network_time) {
    NOTREACHED();
    return false;
  }

  if (network_time - build_date_ >
      base::TimeDelta::FromDays(kOutdatedBuildAgeInDays)) {
    UpgradeDetected(is_auto_update_enabled_ ?
        UPGRADE_NEEDED_OUTDATED_INSTALL :
        UPGRADE_NEEDED_OUTDATED_INSTALL_NO_AU);
    return true;
  }
  // If we simlated an outdated install with a date, we don't want to keep
  // checking for version upgrades, which happens on non-official builds.
  return simulate_outdated;
}

void UpgradeDetectorImpl::OnExperimentChangesDetected(Severity severity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  set_best_effort_experiment_updates_available(severity == BEST_EFFORT);
  set_critical_experiment_updates_available(severity == CRITICAL);
  StartUpgradeNotificationTimer();
}

void UpgradeDetectorImpl::UpgradeDetected(UpgradeAvailable upgrade_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  set_upgrade_available(upgrade_available);

  // Stop the recurring timer (that is checking for changes).
  detect_upgrade_timer_.Stop();
  set_critical_update_acknowledged(false);

  StartUpgradeNotificationTimer();
}

void UpgradeDetectorImpl::NotifyOnUpgradeWithTimePassed(
    base::TimeDelta time_passed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool is_critical_or_outdated =
      upgrade_available() > UPGRADE_AVAILABLE_REGULAR ||
      critical_experiment_updates_available();
  if (is_unstable_channel_) {
    // There's only one threat level for unstable channels like dev and
    // canary, and it hits after one hour. During testing, it hits after one
    // second.
    const base::TimeDelta unstable_threshold = IsTesting() ?
        base::TimeDelta::FromSeconds(1) : base::TimeDelta::FromHours(1);

    if (is_critical_or_outdated) {
      set_upgrade_notification_stage(UPGRADE_ANNOYANCE_CRITICAL);
    } else if (time_passed >= unstable_threshold) {
      set_upgrade_notification_stage(UPGRADE_ANNOYANCE_LOW);

      // That's as high as it goes.
      upgrade_notification_timer_.Stop();
    } else {
      return;  // Not ready to recommend upgrade.
    }
  } else {
    const base::TimeDelta multiplier = IsTesting() ?
        base::TimeDelta::FromSeconds(10) : base::TimeDelta::FromDays(1);

    // 14 days when not testing, otherwise 140 seconds.
    const base::TimeDelta severe_threshold = 14 * multiplier;
    const base::TimeDelta high_threshold = 7 * multiplier;
    const base::TimeDelta elevated_threshold = 4 * multiplier;
    const base::TimeDelta low_threshold = 2 * multiplier;

    // These if statements must be sorted (highest interval first).
    if (time_passed >= severe_threshold || is_critical_or_outdated) {
      set_upgrade_notification_stage(
          is_critical_or_outdated ? UPGRADE_ANNOYANCE_CRITICAL :
                                    UPGRADE_ANNOYANCE_SEVERE);

      // We can't get any higher, baby.
      upgrade_notification_timer_.Stop();
    } else if (time_passed >= high_threshold) {
      set_upgrade_notification_stage(UPGRADE_ANNOYANCE_HIGH);
    } else if (time_passed >= elevated_threshold) {
      set_upgrade_notification_stage(UPGRADE_ANNOYANCE_ELEVATED);
    } else if (time_passed >= low_threshold) {
      set_upgrade_notification_stage(UPGRADE_ANNOYANCE_LOW);
    } else {
      return;  // Not ready to recommend upgrade.
    }
  }

  NotifyUpgrade();
}

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
void UpgradeDetectorImpl::OnAutoupdatesEnabledResult(
    bool auto_updates_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_auto_update_enabled_ = auto_updates_enabled;
  StartTimerForUpgradeCheck();
}
#endif  // defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)

void UpgradeDetectorImpl::NotifyOnUpgrade() {
  const base::TimeDelta time_passed =
      base::TimeTicks::Now() - upgrade_detected_time_;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyOnUpgradeWithTimePassed(time_passed);
}

// static
UpgradeDetectorImpl* UpgradeDetectorImpl::GetInstance() {
  static auto* const instance = new UpgradeDetectorImpl();
  return instance;
}

// static
UpgradeDetector* UpgradeDetector::GetInstance() {
  return UpgradeDetectorImpl::GetInstance();
}
