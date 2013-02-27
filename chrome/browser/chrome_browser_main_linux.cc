// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_linux.h"

#include "base/message_loop_proxy.h"
#include "chrome/browser/storage_monitor/media_transfer_protocol_device_observer_linux.h"
#include "chrome/common/chrome_switches.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/storage_monitor/removable_device_notifications_linux.h"
#include "content/public/browser/browser_thread.h"
#endif

#if defined(USE_LINUX_BREAKPAD)
#include <stdlib.h>

#include "base/command_line.h"
#include "base/linux_util.h"
#include "base/prefs/pref_service.h"
#include "chrome/app/breakpad_linux.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/pref_names.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/common/chrome_version_info.h"
#endif

#endif  // defined(USE_LINUX_BREAKPAD)

namespace {

#if defined(USE_LINUX_BREAKPAD)
#if !defined(OS_CHROMEOS)
void GetLinuxDistroCallback() {
  base::GetLinuxDistro();  // Initialize base::linux_distro if needed.
}
#endif

bool IsCrashReportingEnabled(const PrefService* local_state) {
  // Check whether we should initialize the crash reporter. It may be disabled
  // through configuration policy or user preference. It must be disabled for
  // Guest mode on Chrome OS in Stable channel.
  // Also allow crash reporting to be enabled with a command-line flag if the
  // crash service is under control of the user. It is used by QA
  // testing infrastructure to switch on generation of crash reports.
  bool use_switch = true;

  // Convert #define to a variable so that we can use if() rather than
  // #if below and so at least compile-test the Chrome code in
  // Chromium builds.
#if defined(GOOGLE_CHROME_BUILD)
  bool is_chrome_build = true;
#else
  bool is_chrome_build = false;
#endif

  // Check these settings in Chrome builds only, to reduce the chance
  // that we accidentally upload crash dumps from Chromium builds.
  bool breakpad_enabled = false;
  if (is_chrome_build) {
#if defined(OS_CHROMEOS)
    bool is_guest_session =
        CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession);
    bool is_stable_channel =
        chrome::VersionInfo::GetChannel() ==
        chrome::VersionInfo::CHANNEL_STABLE;
    // TODO(pastarmovj): Consider the TrustedGet here.
    bool reporting_enabled;
    chromeos::CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref,
                                              &reporting_enabled);
    breakpad_enabled =
        !(is_guest_session && is_stable_channel) && reporting_enabled;
#else
    const PrefService::Preference* metrics_reporting_enabled =
        local_state->FindPreference(prefs::kMetricsReportingEnabled);
    CHECK(metrics_reporting_enabled);
    breakpad_enabled = local_state->GetBoolean(prefs::kMetricsReportingEnabled);
    use_switch = metrics_reporting_enabled->IsUserModifiable();
#endif  // defined(OS_CHROMEOS)
  }

  if (use_switch) {
    // Linux Breakpad interferes with the debug stack traces produced
    // by EnableInProcessStackDumping(), used in browser_tests, so we
    // do not allow CHROME_HEADLESS=1 to enable Breakpad in Chromium
    // because the buildbots have CHROME_HEADLESS set.  However, we
    // allow CHROME_HEADLESS to enable Breakpad in Chrome for
    // compatibility with Breakpad/Chrome tests that may rely on this.
    // TODO(mseaborn): Change tests to use --enable-crash-reporter-for-testing
    // instead.
    if (is_chrome_build && !breakpad_enabled)
      breakpad_enabled = getenv(env_vars::kHeadless) != NULL;
    if (!breakpad_enabled)
      breakpad_enabled = CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableCrashReporterForTesting);
  }

  return breakpad_enabled;
}
#endif  // defined(USE_LINUX_BREAKPAD)

}  // namespace

ChromeBrowserMainPartsLinux::ChromeBrowserMainPartsLinux(
    const content::MainFunctionParams& parameters)
    : ChromeBrowserMainPartsPosix(parameters),
      initialized_media_transfer_protocol_manager_(false) {
}

ChromeBrowserMainPartsLinux::~ChromeBrowserMainPartsLinux() {
  if (initialized_media_transfer_protocol_manager_)
    device::MediaTransferProtocolManager::Shutdown();
}

void ChromeBrowserMainPartsLinux::PreProfileInit() {
#if defined(USE_LINUX_BREAKPAD)
#if !defined(OS_CHROMEOS)
  // Needs to be called after we have chrome::DIR_USER_DATA and
  // g_browser_process.  This happens in PreCreateThreads.
  content::BrowserThread::PostTask(content::BrowserThread::FILE,
                                   FROM_HERE,
                                   base::Bind(&GetLinuxDistroCallback));
#endif

  if (IsCrashReportingEnabled(local_state()))
    InitCrashReporter();
#endif

#if !defined(OS_CHROMEOS)
  const base::FilePath kDefaultMtabPath("/etc/mtab");
  removable_device_notifications_linux_ =
      new chrome::RemovableDeviceNotificationsLinux(kDefaultMtabPath);
  removable_device_notifications_linux_->Init();
#endif

  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    scoped_refptr<base::MessageLoopProxy> loop_proxy;
#if !defined(OS_CHROMEOS)
    loop_proxy = content::BrowserThread::GetMessageLoopProxyForThread(
        content::BrowserThread::FILE);
#endif
    device::MediaTransferProtocolManager::Initialize(loop_proxy);
    initialized_media_transfer_protocol_manager_ = true;
  }

  ChromeBrowserMainPartsPosix::PreProfileInit();
}

void ChromeBrowserMainPartsLinux::PostProfileInit() {
  // TODO(gbillock): Make this owned by RemovableDeviceNotificationsLinux.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    media_transfer_protocol_device_observer_.reset(
        new chrome::MediaTransferProtocolDeviceObserverLinux());
    media_transfer_protocol_device_observer_->SetNotifications(
      chrome::StorageMonitor::GetInstance()->receiver());
  }

  ChromeBrowserMainPartsPosix::PostProfileInit();
}

void ChromeBrowserMainPartsLinux::PostMainMessageLoopRun() {
#if !defined(OS_CHROMEOS)
  // Release it now. Otherwise the FILE thread would be gone when we try to
  // release it in the dtor and Valgrind would report a leak on almost ever
  // single browser_test.
  removable_device_notifications_linux_ = NULL;
#endif

  media_transfer_protocol_device_observer_.reset();

  ChromeBrowserMainPartsPosix::PostMainMessageLoopRun();
}
