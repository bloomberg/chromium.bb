// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector.h"

#include <string>

#include "base/command_line.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/browser_distribution.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"

#if defined(OS_WIN)
#include "chrome/installer/util/install_util.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/cocoa/keystone_glue.h"
#elif defined(OS_POSIX)
#include "base/process_util.h"
#include "base/version.h"
#endif

namespace {

// How often to check for an upgrade.
int GetCheckForUpgradeEveryMs() {
  // Check for a value passed via the command line.
  int interval_ms;
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  std::string interval =
      cmd_line.GetSwitchValueASCII(switches::kCheckForUpdateIntervalSec);
  if (!interval.empty() && base::StringToInt(interval, &interval_ms))
    return interval_ms * 1000;  // Command line value is in seconds.

  // Otherwise check once an hour for dev channel and once a day for all other
  // channels/builds.
  const std::string channel = platform_util::GetVersionStringModifier();
  int hours;
  if (channel == "dev")
    hours = 1;
  else
    hours = 24;

  return hours * 60 * 60 * 1000;
}

// How long to wait before notifying the user about the upgrade.
const int kNotifyUserAfterMs = 0;

// This task checks the currently running version of Chrome against the
// installed version. If the installed version is newer, it runs the passed
// callback task. Otherwise it just deletes the task.
class DetectUpgradeTask : public Task {
 public:
  explicit DetectUpgradeTask(Task* upgrade_detected_task)
      : upgrade_detected_task_(upgrade_detected_task) {
  }

  virtual ~DetectUpgradeTask() {
    if (upgrade_detected_task_) {
      // This has to get deleted on the same thread it was created.
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              new DeleteTask<Task>(upgrade_detected_task_));
    }
  }

  virtual void Run() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

    scoped_ptr<Version> installed_version;

#if defined(OS_WIN)
    // Get the version of the currently *installed* instance of Chrome,
    // which might be newer than the *running* instance if we have been
    // upgraded in the background.
    // TODO(tommi): Check if using the default distribution is always the right
    // thing to do.
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    installed_version.reset(InstallUtil::GetChromeVersion(dist, false));
    if (!installed_version.get()) {
      // User level Chrome is not installed, check system level.
      installed_version.reset(InstallUtil::GetChromeVersion(dist, true));
    }
#elif defined(OS_MACOSX)
    installed_version.reset(
        Version::GetVersionFromString(UTF16ToASCII(
            keystone_glue::CurrentlyInstalledVersion())));
#elif defined(OS_POSIX)
    // POSIX but not Mac OS X: Linux, etc.
    CommandLine command_line(*CommandLine::ForCurrentProcess());
    command_line.AppendSwitch(switches::kProductVersion);
    std::string reply;
    if (!base::GetAppOutput(command_line, &reply)) {
      DLOG(ERROR) << "Failed to get current file version";
      return;
    }

    installed_version.reset(Version::GetVersionFromString(reply));
#endif

    // Get the version of the currently *running* instance of Chrome.
    chrome::VersionInfo version_info;
    if (!version_info.is_valid()) {
      NOTREACHED() << "Failed to get current file version";
      return;
    }
    scoped_ptr<Version> running_version(
        Version::GetVersionFromString(version_info.Version()));
    if (running_version.get() == NULL) {
      NOTREACHED() << "Failed to parse version info";
      return;
    }

    // |installed_version| may be NULL when the user downgrades on Linux (by
    // switching from dev to beta channel, for example). The user needs a
    // restart in this case as well. See http://crbug.com/46547
    if (!installed_version.get() ||
        (installed_version->CompareTo(*running_version) > 0)) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              upgrade_detected_task_);
      upgrade_detected_task_ = NULL;
    }
  }

 private:
  Task* upgrade_detected_task_;
};

}  // namespace

// static
void UpgradeDetector::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kRestartLastSessionOnShutdown, false);
}

UpgradeDetector::UpgradeDetector()
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      notify_upgrade_(false) {
  // Windows: only enable upgrade notifications for official builds.
  // Mac: only enable them if the updater (Keystone) is present.
  // Linux (and other POSIX): always enable regardless of branding.
#if (defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)) || defined(OS_POSIX)
#if defined(OS_MACOSX)
  if (keystone_glue::KeystoneEnabled())
#endif
  {
    detect_upgrade_timer_.Start(
        base::TimeDelta::FromMilliseconds(GetCheckForUpgradeEveryMs()),
        this, &UpgradeDetector::CheckForUpgrade);
  }
#endif
}

UpgradeDetector::~UpgradeDetector() {
}

// static
UpgradeDetector* UpgradeDetector::GetInstance() {
  return Singleton<UpgradeDetector>::get();
}

void UpgradeDetector::CheckForUpgrade() {
  method_factory_.RevokeAll();
  Task* callback_task =
      method_factory_.NewRunnableMethod(&UpgradeDetector::UpgradeDetected);
  // We use FILE as the thread to run the upgrade detection code on all
  // platforms. For Linux, this is because we don't want to block the UI thread
  // while launching a background process and reading its output; on the Mac and
  // on Windows checking for an upgrade requires reading a file.
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          new DetectUpgradeTask(callback_task));
}

void UpgradeDetector::UpgradeDetected() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Stop the recurring timer (that is checking for changes).
  detect_upgrade_timer_.Stop();

  NotificationService::current()->Notify(
      NotificationType::UPGRADE_DETECTED,
      Source<UpgradeDetector>(this),
      NotificationService::NoDetails());

  // Start the OneShot timer for notifying the user after a certain period.
  upgrade_notification_timer_.Start(
      base::TimeDelta::FromMilliseconds(kNotifyUserAfterMs),
      this, &UpgradeDetector::NotifyOnUpgrade);
}

void UpgradeDetector::NotifyOnUpgrade() {
  notify_upgrade_ = true;

  NotificationService::current()->Notify(
      NotificationType::UPGRADE_RECOMMENDED,
      Source<UpgradeDetector>(this),
      NotificationService::NoDetails());
}
