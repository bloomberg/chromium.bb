// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector_impl.h"

#include <string>

#include "base/bind.h"
#include "base/build_time.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "components/network_time/network_time_tracker.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/mac/keystone_glue.h"
#endif

using content::BrowserThread;

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
const uint64 kOutdatedBuildAgeInDays = 12 * 7;

// Return the string that was passed as a value for the
// kCheckForUpdateIntervalSec switch.
std::string CmdLineInterval() {
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  return cmd_line.GetSwitchValueASCII(switches::kCheckForUpdateIntervalSec);
}

// Check if one of the outdated simulation switches was present on the command
// line.
bool SimulatingOutdated() {
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  return cmd_line.HasSwitch(switches::kSimulateOutdated) ||
      cmd_line.HasSwitch(switches::kSimulateOutdatedNoAU);
}

// Check if any of the testing switches was present on the command line.
bool IsTesting() {
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
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

// Return true if the current build is one of the unstable channels.
bool IsUnstableChannel() {
  // TODO(mad): Investigate whether we still need to be on the file thread for
  // this. On Windows, the file thread used to be required for registry access
  // but no anymore. But other platform may still need the file thread.
  // crbug.com/366647.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  return channel == chrome::VersionInfo::CHANNEL_DEV ||
         channel == chrome::VersionInfo::CHANNEL_CANARY;
}

// This task identifies whether we are running an unstable version. And then it
// unconditionally calls back the provided task.
void CheckForUnstableChannel(const base::Closure& callback_task,
                             bool* is_unstable_channel) {
  *is_unstable_channel = IsUnstableChannel();
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback_task);
}

#if defined(OS_WIN)
// Return true if the currently running Chrome is a system install.
bool IsSystemInstall() {
  // Get the version of the currently *installed* instance of Chrome,
  // which might be newer than the *running* instance if we have been
  // upgraded in the background.
  base::FilePath exe_path;
  if (!PathService::Get(base::DIR_EXE, &exe_path)) {
    NOTREACHED() << "Failed to find executable path";
    return false;
  }

  return !InstallUtil::IsPerUserInstall(exe_path.value().c_str());
}

// Sets |is_unstable_channel| to true if the current chrome is on the dev or
// canary channels. Sets |is_auto_update_enabled| to true if Google Update will
// update the current chrome. Unconditionally posts |callback_task| to the UI
// thread to continue processing.
void DetectUpdatability(const base::Closure& callback_task,
                        bool* is_unstable_channel,
                        bool* is_auto_update_enabled) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::string16 app_guid = installer::GetAppGuidForUpdates(IsSystemInstall());
  DCHECK(!app_guid.empty());
  // Don't try to turn on autoupdate when we failed previously.
  if (is_auto_update_enabled) {
    *is_auto_update_enabled =
        GoogleUpdateSettings::AreAutoupdatesEnabled(app_guid);
  }
  *is_unstable_channel = IsUnstableChannel();
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback_task);
}
#endif  // defined(OS_WIN)

// Gets the currently installed version. On Windows, if |critical_update| is not
// NULL, also retrieves the critical update version info if available.
base::Version GetCurrentlyInstalledVersionImpl(Version* critical_update) {
  base::ThreadRestrictions::AssertIOAllowed();

  Version installed_version;
#if defined(OS_WIN)
  // Get the version of the currently *installed* instance of Chrome,
  // which might be newer than the *running* instance if we have been
  // upgraded in the background.
  bool system_install = IsSystemInstall();

  // TODO(tommi): Check if using the default distribution is always the right
  // thing to do.
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  InstallUtil::GetChromeVersion(dist, system_install, &installed_version);
  if (critical_update && installed_version.IsValid()) {
    InstallUtil::GetCriticalUpdateVersion(dist, system_install,
                                          critical_update);
  }
#elif defined(OS_MACOSX)
  installed_version =
      Version(base::UTF16ToASCII(keystone_glue::CurrentlyInstalledVersion()));
#elif defined(OS_POSIX)
  // POSIX but not Mac OS X: Linux, etc.
  CommandLine command_line(*CommandLine::ForCurrentProcess());
  command_line.AppendSwitch(switches::kProductVersion);
  std::string reply;
  if (!base::GetAppOutput(command_line, &reply)) {
    DLOG(ERROR) << "Failed to get current file version";
    return installed_version;
  }

  installed_version = Version(reply);
#endif
  return installed_version;
}

}  // namespace

UpgradeDetectorImpl::UpgradeDetectorImpl()
    : is_unstable_channel_(false),
      is_auto_update_enabled_(true),
      build_date_(base::GetBuildTime()),
      weak_factory_(this) {
  CommandLine command_line(*CommandLine::ForCurrentProcess());
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
  chrome_variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  if (variations_service)
    variations_service->AddObserver(this);

  base::Closure start_upgrade_check_timer_task =
      base::Bind(&UpgradeDetectorImpl::StartTimerForUpgradeCheck,
                 weak_factory_.GetWeakPtr());

#if defined(OS_WIN)
  // Only enable upgrade notifications for official builds.  Chromium has no
  // upgrade channel.
#if defined(GOOGLE_CHROME_BUILD)
  // On Windows, there might be a policy/enterprise environment preventing
  // updates, so validate updatability, and then call StartTimerForUpgradeCheck
  // appropriately. And don't check for autoupdate if we already attempted to
  // enable it in the past.
  bool attempted_enabling_autoupdate = g_browser_process->local_state() &&
      g_browser_process->local_state()->GetBoolean(
          prefs::kAttemptedToEnableAutoupdate);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&DetectUpdatability,
                                     start_upgrade_check_timer_task,
                                     &is_unstable_channel_,
                                     attempted_enabling_autoupdate ?
                                         NULL : &is_auto_update_enabled_));
#endif
#else
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
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&CheckForUnstableChannel,
                                     start_upgrade_check_timer_task,
                                     &is_unstable_channel_));
#endif
}

UpgradeDetectorImpl::~UpgradeDetectorImpl() {
}

// static
base::Version UpgradeDetectorImpl::GetCurrentlyInstalledVersion() {
  return GetCurrentlyInstalledVersionImpl(NULL);
}

// static
// This task checks the currently running version of Chrome against the
// installed version. If the installed version is newer, it calls back
// UpgradeDetectorImpl::UpgradeDetected using a weak pointer so that it can
// be interrupted from the UI thread.
void UpgradeDetectorImpl::DetectUpgradeTask(
    base::WeakPtr<UpgradeDetectorImpl> upgrade_detector) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  Version critical_update;
  Version installed_version =
      GetCurrentlyInstalledVersionImpl(&critical_update);

  // Get the version of the currently *running* instance of Chrome.
  chrome::VersionInfo version_info;
  if (!version_info.is_valid()) {
    NOTREACHED() << "Failed to get current file version";
    return;
  }
  Version running_version(version_info.Version());
  if (!running_version.IsValid()) {
    NOTREACHED();
    return;
  }

  // |installed_version| may be NULL when the user downgrades on Linux (by
  // switching from dev to beta channel, for example). The user needs a
  // restart in this case as well. See http://crbug.com/46547
  if (!installed_version.IsValid() ||
      (installed_version.CompareTo(running_version) > 0)) {
    // If a more recent version is available, it might be that we are lacking
    // a critical update, such as a zero-day fix.
    UpgradeAvailable upgrade_available = UPGRADE_AVAILABLE_REGULAR;
    if (critical_update.IsValid() &&
        critical_update.CompareTo(running_version) > 0) {
      upgrade_available = UPGRADE_AVAILABLE_CRITICAL;
    }

    // Fire off the upgrade detected task.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&UpgradeDetectorImpl::UpgradeDetected,
                                       upgrade_detector,
                                       upgrade_available));
  }
}

void UpgradeDetectorImpl::StartTimerForUpgradeCheck() {
  detect_upgrade_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(GetCheckForUpgradeEveryMs()),
      this, &UpgradeDetectorImpl::CheckForUpgrade);
}

void UpgradeDetectorImpl::StartUpgradeNotificationTimer() {
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
  // Interrupt any (unlikely) unfinished execution of DetectUpgradeTask, or at
  // least prevent the callback from being executed, because we will potentially
  // call it from within DetectOutdatedInstall() or will post
  // DetectUpgradeTask again below anyway.
  weak_factory_.InvalidateWeakPtrs();

  // No need to look for upgrades if the install is outdated.
  if (DetectOutdatedInstall())
    return;

  // We use FILE as the thread to run the upgrade detection code on all
  // platforms. For Linux, this is because we don't want to block the UI thread
  // while launching a background process and reading its output; on the Mac and
  // on Windows checking for an upgrade requires reading a file.
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&UpgradeDetectorImpl::DetectUpgradeTask,
                                     weak_factory_.GetWeakPtr()));
}

bool UpgradeDetectorImpl::DetectOutdatedInstall() {
  // Don't show the bubble if we have a brand code that is NOT organic, unless
  // an outdated build is being simulated by command line switches.
  static bool simulate_outdated = SimulatingOutdated();
  if (!simulate_outdated) {
    std::string brand;
    if (google_brand::GetBrand(&brand) && !google_brand::IsOrganic(brand))
      return false;

#if defined(OS_WIN)
    // Don't show the update bubbles to enterprise users (i.e., on a domain).
    if (base::win::IsEnrolledToDomain())
      return false;
#endif
  }

  base::Time network_time;
  base::TimeDelta uncertainty;
  if (!g_browser_process->network_time_tracker()->GetNetworkTime(
          base::TimeTicks::Now(), &network_time, &uncertainty)) {
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
  set_best_effort_experiment_updates_available(severity == BEST_EFFORT);
  set_critical_experiment_updates_available(severity == CRITICAL);
  StartUpgradeNotificationTimer();
}

void UpgradeDetectorImpl::UpgradeDetected(UpgradeAvailable upgrade_available) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  set_upgrade_available(upgrade_available);

  // Stop the recurring timer (that is checking for changes).
  detect_upgrade_timer_.Stop();
  set_critical_update_acknowledged(false);

  StartUpgradeNotificationTimer();
}

void UpgradeDetectorImpl::NotifyOnUpgradeWithTimePassed(
    base::TimeDelta time_passed) {
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

  NotifyUpgradeRecommended();
}

void UpgradeDetectorImpl::NotifyOnUpgrade() {
  const base::TimeDelta time_passed =
      base::TimeTicks::Now() - upgrade_detected_time_;
  NotifyOnUpgradeWithTimePassed(time_passed);
}

// static
UpgradeDetectorImpl* UpgradeDetectorImpl::GetInstance() {
  return Singleton<UpgradeDetectorImpl>::get();
}

// static
UpgradeDetector* UpgradeDetector::GetInstance() {
  return UpgradeDetectorImpl::GetInstance();
}
