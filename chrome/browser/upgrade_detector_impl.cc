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
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/metrics/variations/variations_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_WIN)
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/mac/keystone_glue.h"
#elif defined(OS_POSIX)
#include "base/process_util.h"
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

// Finch Experiment strings to identify if we should check for outdated install.
const char kOutdatedInstallCheckTrialName[] = "OutdatedInstallCheck";
const char kOutdatedInstallCheck12WeeksGroupName[] = "12WeeksOutdatedInstall";

std::string CmdLineInterval() {
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  return cmd_line.GetSwitchValueASCII(switches::kCheckForUpdateIntervalSec);
}

bool IsTesting() {
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  return cmd_line.HasSwitch(switches::kSimulateUpgrade) ||
      cmd_line.HasSwitch(switches::kCheckForUpdateIntervalSec) ||
      cmd_line.HasSwitch(switches::kSimulateCriticalUpdate) ||
      cmd_line.HasSwitch(switches::kSimulateOutdated);
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

bool IsUnstableChannel() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  return channel == chrome::VersionInfo::CHANNEL_DEV ||
         channel == chrome::VersionInfo::CHANNEL_CANARY;
}

// This task identifies whether we are running an unstable version. And then
// it unconditionally calls back the provided task.
void CheckForUnstableChannel(const base::Closure& callback_task,
                             bool* is_unstable_channel) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  *is_unstable_channel = IsUnstableChannel();
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback_task);
}

#if defined(OS_WIN)
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

// This task checks the update policy and calls back the task only if automatic
// updates are allowed. It also identifies whether we are running an unstable
// channel.
void DetectUpdatability(const base::Closure& callback_task,
                        bool* is_unstable_channel) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  string16 app_guid = installer::GetAppGuidForUpdates(IsSystemInstall());
  DCHECK(!app_guid.empty());
  if (GoogleUpdateSettings::AUTOMATIC_UPDATES ==
      GoogleUpdateSettings::GetAppUpdatePolicy(app_guid, NULL)) {
    CheckForUnstableChannel(callback_task, is_unstable_channel);
  }
}
#endif  // defined(OS_WIN)

}  // namespace

UpgradeDetectorImpl::UpgradeDetectorImpl()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      is_unstable_channel_(false),
      build_date_(base::GetBuildTime()) {
  CommandLine command_line(*CommandLine::ForCurrentProcess());
  // The different command line switches that affect testing can't be used
  // simultaneously, if they do, here's the precedence order, based on the order
  // of the if statements below:
  // - kDisableBackgroundNetworking prevents any of the other command line
  //   switch from being taken into account.
  // - kSimulateUpgrade supersedes critical or outdated upgrade switches.
  // - kSimulateCriticalUpdate has precedence over kSimulateOutdated.
  // - kSimulateOutdated can work on its own, or with a specified date.
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
  if (command_line.HasSwitch(switches::kSimulateOutdated)) {
    // The outdated simulation can work without a value, which means outdated
    // now, or with a value that must be a well formed date/time string that
    // overrides the build date.
    // Also note that to test with a given time/date, until the network time
    // tracking moves off of the VariationsService, the "variations-server-url"
    // command line switch must also be specified for the service to be
    // available on non GOOGLE_CHROME_BUILD.
    std::string build_date = command_line.GetSwitchValueASCII(
        switches::kSimulateOutdated);
    base::Time maybe_build_time;
    bool result = base::Time::FromString(build_date.c_str(), &maybe_build_time);
    if (result && !maybe_build_time.is_null()) {
      // We got a valid build date simulation so use it and check for upgrades.
      build_date_ = maybe_build_time;
      StartTimerForUpgradeCheck();
    } else {
      // Without a valid date, we simulate that we are already outdated...
      UpgradeDetected(UPGRADE_NEEDED_OUTDATED_INSTALL);
    }
    return;
  }

  // Windows: only enable upgrade notifications for official builds.
  // Mac: only enable them if the updater (Keystone) is present.
  // Linux (and other POSIX): always enable regardless of branding.
  base::Closure start_upgrade_check_timer_task =
      base::Bind(&UpgradeDetectorImpl::StartTimerForUpgradeCheck,
                 weak_factory_.GetWeakPtr());
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  // On Windows, there might be a policy preventing updates, so validate
  // updatability, and then call StartTimerForUpgradeCheck appropriately.
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&DetectUpdatability,
                                     start_upgrade_check_timer_task,
                                     &is_unstable_channel_));
  return;
#elif defined(OS_WIN) && !defined(GOOGLE_CHROME_BUILD)
  return;  // Chromium has no upgrade channel.
#elif defined(OS_MACOSX)
  if (!keystone_glue::KeystoneEnabled())
    return;  // Keystone updater not enabled.
#elif !defined(OS_POSIX)
  return;
#endif

  // Check whether the build is an unstable channel before starting the timer.
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&CheckForUnstableChannel,
                                     start_upgrade_check_timer_task,
                                     &is_unstable_channel_));
}

UpgradeDetectorImpl::~UpgradeDetectorImpl() {
}

// Static
// This task checks the currently running version of Chrome against the
// installed version. If the installed version is newer, it calls back
// UpgradeDetectorImpl::UpgradeDetected using a weak pointer so that it can
// be interrupted from the UI thread.
void UpgradeDetectorImpl::DetectUpgradeTask(
    base::WeakPtr<UpgradeDetectorImpl> upgrade_detector) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  Version installed_version;
  Version critical_update;

#if defined(OS_WIN)
  // Get the version of the currently *installed* instance of Chrome,
  // which might be newer than the *running* instance if we have been
  // upgraded in the background.
  bool system_install = IsSystemInstall();

  // TODO(tommi): Check if using the default distribution is always the right
  // thing to do.
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  InstallUtil::GetChromeVersion(dist, system_install, &installed_version);

  if (installed_version.IsValid()) {
    InstallUtil::GetCriticalUpdateVersion(dist, system_install,
                                          &critical_update);
  }
#elif defined(OS_MACOSX)
  installed_version =
      Version(UTF16ToASCII(keystone_glue::CurrentlyInstalledVersion()));
#elif defined(OS_POSIX)
  // POSIX but not Mac OS X: Linux, etc.
  CommandLine command_line(*CommandLine::ForCurrentProcess());
  command_line.AppendSwitch(switches::kProductVersion);
  std::string reply;
  if (!base::GetAppOutput(command_line, &reply)) {
    DLOG(ERROR) << "Failed to get current file version";
    return;
  }

  installed_version = Version(reply);
#endif

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
  // Only enable the outdated install check if we are running the trial for it,
  // unless we are simulating an outdated isntall.
  static bool simulate_outdated = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSimulateOutdated);
  if (!simulate_outdated) {
    if (base::FieldTrialList::FindFullName(kOutdatedInstallCheckTrialName) !=
            kOutdatedInstallCheck12WeeksGroupName) {
      return false;
    }

    // Also don't show the bubble if we have a brand code that is NOT organic.
    std::string brand;
    if (google_util::GetBrand(&brand) && !google_util::IsOrganic(brand))
      return false;
  }

  base::Time network_time;
  base::TimeDelta uncertainty;
  if (!g_browser_process->variations_service() ||
      !g_browser_process->variations_service()->GetNetworkTime(&network_time,
                                                               &uncertainty)) {
    return false;
  }

  if (network_time.is_null() || build_date_.is_null() ||
      build_date_ > network_time) {
    NOTREACHED();
    return false;
  }

  if (network_time - build_date_ >
      base::TimeDelta::FromDays(kOutdatedBuildAgeInDays)) {
    UpgradeDetected(UPGRADE_NEEDED_OUTDATED_INSTALL);
    return true;
  }
  // If we simlated an outdated install with a date, we don't want to keep
  // checking for version upgrades, which happens on non-official builds.
  return simulate_outdated;
}

void UpgradeDetectorImpl::UpgradeDetected(UpgradeAvailable upgrade_available) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  upgrade_available_ = upgrade_available;

  // Stop the recurring timer (that is checking for changes).
  detect_upgrade_timer_.Stop();

  NotifyUpgradeDetected();

  // Start the repeating timer for notifying the user after a certain period.
  // The called function will eventually figure out that enough time has passed
  // and stop the timer.
  int cycle_time = IsTesting() ?
      kNotifyCycleTimeForTestingMs : kNotifyCycleTimeMs;
  upgrade_notification_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(cycle_time),
      this, &UpgradeDetectorImpl::NotifyOnUpgrade);
}

void UpgradeDetectorImpl::NotifyOnUpgrade() {
  base::TimeDelta delta = base::Time::Now() - upgrade_detected_time();

  // We'll make testing more convenient by switching to seconds of waiting
  // instead of days between flipping severity.
  bool is_testing = IsTesting();
  int64 time_passed = is_testing ? delta.InSeconds() : delta.InHours();

  bool is_critical_or_outdated = upgrade_available_ > UPGRADE_AVAILABLE_REGULAR;
  if (is_unstable_channel_) {
    // There's only one threat level for unstable channels like dev and
    // canary, and it hits after one hour. During testing, it hits after one
    // minute.
    const int kUnstableThreshold = 1;

    if (is_critical_or_outdated)
      set_upgrade_notification_stage(UPGRADE_ANNOYANCE_CRITICAL);
    else if (time_passed >= kUnstableThreshold) {
      set_upgrade_notification_stage(UPGRADE_ANNOYANCE_LOW);

      // That's as high as it goes.
      upgrade_notification_timer_.Stop();
    } else {
      return;  // Not ready to recommend upgrade.
    }
  } else {
    const int kMultiplier = is_testing ? 1 : 24;
    // 14 days when not testing, otherwise 14 seconds.
    const int kSevereThreshold = 14 * kMultiplier;
    const int kHighThreshold = 7 * kMultiplier;
    const int kElevatedThreshold = 4 * kMultiplier;
    const int kLowThreshold = 2 * kMultiplier;

    // These if statements must be sorted (highest interval first).
    if (time_passed >= kSevereThreshold || is_critical_or_outdated) {
      set_upgrade_notification_stage(
          is_critical_or_outdated ? UPGRADE_ANNOYANCE_CRITICAL :
                                    UPGRADE_ANNOYANCE_SEVERE);

      // We can't get any higher, baby.
      upgrade_notification_timer_.Stop();
    } else if (time_passed >= kHighThreshold) {
      set_upgrade_notification_stage(UPGRADE_ANNOYANCE_HIGH);
    } else if (time_passed >= kElevatedThreshold) {
      set_upgrade_notification_stage(UPGRADE_ANNOYANCE_ELEVATED);
    } else if (time_passed >= kLowThreshold) {
      set_upgrade_notification_stage(UPGRADE_ANNOYANCE_LOW);
    } else {
      return;  // Not ready to recommend upgrade.
    }
  }

  NotifyUpgradeRecommended();
}

// static
UpgradeDetectorImpl* UpgradeDetectorImpl::GetInstance() {
  return Singleton<UpgradeDetectorImpl>::get();
}

// static
UpgradeDetector* UpgradeDetector::GetInstance() {
  return UpgradeDetectorImpl::GetInstance();
}
