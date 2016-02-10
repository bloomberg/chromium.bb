// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/lifetime/browser_close_manager.h"
#include "chrome/browser/metrics/thread_watcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#include "chrome/browser/chromeos/boot_times_recorder.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/update_engine_client.h"
#endif

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

namespace chrome {
namespace {

#if !defined(OS_ANDROID)
// Returns true if all browsers can be closed without user interaction.
// This currently checks if there is pending download, or if it needs to
// handle unload handler.
bool AreAllBrowsersCloseable() {
  if (BrowserList::GetInstance()->empty())
    return true;

  // If there are any downloads active, all browsers are not closeable.
  // However, this does not block for malicious downloads.
  if (DownloadService::NonMaliciousDownloadCountAllProfiles() > 0)
    return false;

  // Check TabsNeedBeforeUnloadFired().
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->TabsNeedBeforeUnloadFired())
      return false;
  }
  return true;
}

int g_keep_alive_count = 0;
bool g_disable_shutdown_for_testing = false;
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
// Whether chrome should send stop request to a session manager.
bool g_send_stop_request_to_session_manager = false;
#endif

}  // namespace

void MarkAsCleanShutdown() {
  // TODO(beng): Can this use ProfileManager::GetLoadedProfiles() instead?
  for (auto* browser : *BrowserList::GetInstance())
    browser->profile()->SetExitType(Profile::EXIT_NORMAL);
}

void AttemptExitInternal(bool try_to_quit_application) {
  // On Mac, the platform-specific part handles setting this.
#if !defined(OS_MACOSX)
  if (try_to_quit_application)
    browser_shutdown::SetTryingToQuit(true);
#endif

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());

  g_browser_process->platform_part()->AttemptExit();
}

#if !defined(OS_ANDROID)
void CloseAllBrowsersAndQuit() {
  browser_shutdown::SetTryingToQuit(true);
  CloseAllBrowsers();
}

void CloseAllBrowsers() {
  // If there are no browsers and closing the last browser would quit the
  // application, send the APP_TERMINATING action here. Otherwise, it will be
  // sent by RemoveBrowser() when the last browser has closed.
  if (chrome::GetTotalBrowserCount() == 0 &&
      (browser_shutdown::IsTryingToQuit() || !chrome::WillKeepAlive())) {
    // Tell everyone that we are shutting down.
    browser_shutdown::SetTryingToQuit(true);

#if defined(ENABLE_SESSION_SERVICE)
    // If ShuttingDownWithoutClosingBrowsers() returns true, the session
    // services may not get a chance to shut down normally, so explicitly shut
    // them down here to ensure they have a chance to persist their data.
    ProfileManager::ShutdownSessionServices();
#endif

    chrome::NotifyAndTerminate(true);
    chrome::OnAppExiting();
    return;
  }

#if defined(OS_CHROMEOS)
  chromeos::BootTimesRecorder::Get()->AddLogoutTimeMarker(
      "StartedClosingWindows", false);
#endif
  scoped_refptr<BrowserCloseManager> browser_close_manager =
      new BrowserCloseManager;
  browser_close_manager->StartClosingBrowsers();
}
#endif  // !defined(OS_ANDROID)

void AttemptUserExit() {
#if defined(OS_CHROMEOS)
  browser_shutdown::StartShutdownTracing();
  chromeos::BootTimesRecorder::Get()->AddLogoutTimeMarker("LogoutStarted",
                                                          false);

  PrefService* state = g_browser_process->local_state();
  if (state) {
    chromeos::BootTimesRecorder::Get()->OnLogoutStarted(state);

    // Login screen should show up in owner's locale.
    std::string owner_locale = state->GetString(prefs::kOwnerLocale);
    if (!owner_locale.empty() &&
        state->GetString(prefs::kApplicationLocale) != owner_locale &&
        !state->IsManagedPreference(prefs::kApplicationLocale)) {
      state->SetString(prefs::kApplicationLocale, owner_locale);
      TRACE_EVENT0("shutdown", "CommitPendingWrite");
      state->CommitPendingWrite();
    }
  }
  g_send_stop_request_to_session_manager = true;
  // On ChromeOS, always terminate the browser, regardless of the result of
  // AreAllBrowsersCloseable(). See crbug.com/123107.
  chrome::NotifyAndTerminate(true);
#else
  // Reset the restart bit that might have been set in cancelled restart
  // request.
  UserManager::Hide();
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, false);
  AttemptExitInternal(false);
#endif
}

// The Android implementation is in application_lifetime_android.cc
#if !defined(OS_ANDROID)
void AttemptRestart() {
  // TODO(beng): Can this use ProfileManager::GetLoadedProfiles instead?
  for (auto* browser : *BrowserList::GetInstance())
    content::BrowserContext::SaveSessionState(browser->profile());

  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);

#if defined(OS_CHROMEOS)
  chromeos::BootTimesRecorder::Get()->set_restart_requested();

  DCHECK(!g_send_stop_request_to_session_manager);
  // Make sure we don't send stop request to the session manager.
  g_send_stop_request_to_session_manager = false;
  // Run exit process in clean stack.
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(&ExitCleanly));
#else
  // Set the flag to restore state after the restart.
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, true);
  AttemptExit();
#endif
}
#endif

void AttemptExit() {
#if defined(OS_CHROMEOS)
  // On ChromeOS, user exit and system exits are the same.
  AttemptUserExit();
#else
  // If we know that all browsers can be closed without blocking,
  // don't notify users of crashes beyond this point.
  // Note that MarkAsCleanShutdown() does not set UMA's exit cleanly bit
  // so crashes during shutdown are still reported in UMA.
#if !defined(OS_ANDROID)
  // Android doesn't use Browser.
  if (AreAllBrowsersCloseable())
    MarkAsCleanShutdown();
#endif
  AttemptExitInternal(true);
#endif
}

#if defined(OS_CHROMEOS)
// A function called when SIGTERM is received.
void ExitCleanly() {
  // We always mark exit cleanly.
  MarkAsCleanShutdown();

  // Don't block when SIGTERM is received. AreaAllBrowsersCloseable()
  // can be false in following cases. a) power-off b) signout from
  // screen locker.
  if (!AreAllBrowsersCloseable())
    browser_shutdown::OnShutdownStarting(browser_shutdown::END_SESSION);
  else
    browser_shutdown::OnShutdownStarting(browser_shutdown::BROWSER_EXIT);
  AttemptExitInternal(true);
}
#endif

#if !defined(OS_ANDROID)
void SessionEnding() {
  // This is a time-limited shutdown where we need to write as much to
  // disk as we can as soon as we can, and where we must kill the
  // process within a hang timeout to avoid user prompts.

  // Start watching for hang during shutdown, and crash it if takes too long.
  // We disarm when |shutdown_watcher| object is destroyed, which is when we
  // exit this function.
  ShutdownWatcherHelper shutdown_watcher;
  shutdown_watcher.Arm(base::TimeDelta::FromSeconds(90));

  // EndSession is invoked once per frame. Only do something the first time.
  static bool already_ended = false;
  // We may get called in the middle of shutdown, e.g. http://crbug.com/70852
  // In this case, do nothing.
  if (already_ended || !content::NotificationService::current())
    return;
  already_ended = true;

  browser_shutdown::OnShutdownStarting(browser_shutdown::END_SESSION);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());

  // Write important data first.
  g_browser_process->EndSession();

#if defined(OS_WIN)
  base::win::SetShouldCrashOnProcessDetach(false);
#endif
  // On Windows 7 and later, the system will consider the process ripe for
  // termination as soon as it hides or destroys its windows. Since any
  // execution past that point will be non-deterministically cut short, we
  // might as well put ourselves out of that misery deterministically.
  base::Process::Current().Terminate(0, false);
}

void IncrementKeepAliveCount() {
  // Increment the browser process refcount as long as we're keeping the
  // application alive.
  if (!WillKeepAlive())
    g_browser_process->AddRefModule();
  ++g_keep_alive_count;
}

void CloseAllBrowsersIfNeeded() {
  // If there are no browsers open and we aren't already shutting down,
  // initiate a shutdown. Also skips shutdown if this is a unit test.
  // (MessageLoop::current() == null or explicitly disabled).
  if (chrome::GetTotalBrowserCount() == 0 &&
      !browser_shutdown::IsTryingToQuit() && base::MessageLoop::current() &&
      !g_disable_shutdown_for_testing) {
    CloseAllBrowsers();
  }
}

void DecrementKeepAliveCount() {
  DCHECK_GT(g_keep_alive_count, 0);
  --g_keep_alive_count;
  // Although we should have a browser process, if there is none,
  // there is nothing to do.
  if (!g_browser_process) return;

  // Allow the app to shutdown again.
  if (!WillKeepAlive()) {
    g_browser_process->ReleaseModule();
    CloseAllBrowsersIfNeeded();
  }
}

bool WillKeepAlive() {
  return g_keep_alive_count > 0;
}
#endif  // !defined(OS_ANDROID)

void NotifyAppTerminating() {
  static bool notified = false;
  if (notified)
    return;
  notified = true;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void NotifyAndTerminate(bool fast_path) {
#if defined(OS_CHROMEOS)
  static bool notified = false;
  // Return if a shutdown request has already been sent.
  if (notified)
    return;
  notified = true;
#endif

  if (fast_path)
    NotifyAppTerminating();

#if defined(OS_CHROMEOS)
  if (base::SysInfo::IsRunningOnChromeOS()) {
    // If we're on a ChromeOS device, reboot if an update has been applied,
    // or else signal the session manager to log out.
    chromeos::UpdateEngineClient* update_engine_client
        = chromeos::DBusThreadManager::Get()->GetUpdateEngineClient();
    if (update_engine_client->GetLastStatus().status ==
        chromeos::UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT) {
      update_engine_client->RebootAfterUpdate();
    } else if (g_send_stop_request_to_session_manager) {
      // Don't ask SessionManager to stop session if the shutdown request comes
      // from session manager.
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient()
          ->StopSession();
    }
  } else {
    if (g_send_stop_request_to_session_manager) {
      // If running the Chrome OS build, but we're not on the device, act
      // as if we received signal from SessionManager.
      content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                       base::Bind(&ExitCleanly));
    }
  }
#endif
}

#if !defined(OS_ANDROID)
void OnAppExiting() {
  static bool notified = false;
  if (notified)
    return;
  notified = true;
  HandleAppExitingForPlatform();
}

void DisableShutdownForTesting(bool disable_shutdown_for_testing) {
  g_disable_shutdown_for_testing = disable_shutdown_for_testing;
  if (!g_disable_shutdown_for_testing && !WillKeepAlive())
    CloseAllBrowsersIfNeeded();
}
#endif  // !defined(OS_ANDROID)

}  // namespace chrome
