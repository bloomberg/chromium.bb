// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/metrics/thread_watcher.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_shutdown.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"

#if defined(OS_MACOSX)
#include "chrome/browser/chrome_browser_application_mac.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/update_engine_client.h"
#endif

namespace browser {
namespace {

// Returns true if all browsers can be closed without user interaction.
// This currently checks if there is pending download, or if it needs to
// handle unload handler.
bool AreAllBrowsersCloseable() {
  BrowserList::const_iterator browser_it = BrowserList::begin();
  if (browser_it == BrowserList::end())
    return true;

  // If there are any downloads active, all browsers are not closeable.
  if (DownloadService::DownloadCountAllProfiles() > 0)
    return false;

  // Check TabsNeedBeforeUnloadFired().
  for (; browser_it != BrowserList::end(); ++browser_it) {
    if ((*browser_it)->TabsNeedBeforeUnloadFired())
      return false;
  }
  return true;
}

int g_keep_alive_count = 0;

#if defined(OS_CHROMEOS)
// Whether a session manager requested to shutdown.
bool g_session_manager_requested_shutdown = true;
#endif

}  // namespace

void MarkAsCleanShutdown() {
  // TODO(beng): Can this use ProfileManager::GetLoadedProfiles() instead?
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    (*i)->profile()->MarkAsCleanShutdown();
  }
}

void AttemptExitInternal() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());

#if !defined(OS_MACOSX)
  // On most platforms, closing all windows causes the application to exit.
  CloseAllBrowsers();
#else
  // On the Mac, the application continues to run once all windows are closed.
  // Terminate will result in a CloseAllBrowsers() call, and once (and if)
  // that is done, will cause the application to exit cleanly.
  chrome_browser_application_mac::Terminate();
#endif
}

void NotifyAppTerminating() {
  static bool notified = false;
  if (notified)
    return;
  notified = true;
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void NotifyAndTerminate(bool fast_path) {
#if defined(OS_CHROMEOS)
  static bool notified = false;
  // Don't ask SessionManager to shutdown if
  // a) a shutdown request has already been sent.
  // b) shutdown request comes from session manager.
  if (notified || g_session_manager_requested_shutdown)
    return;
  notified = true;
#endif

  if (fast_path)
    NotifyAppTerminating();

#if defined(OS_CHROMEOS)
  if (base::chromeos::IsRunningOnChromeOS()) {
    // If we're on a ChromeOS device, reboot if an update has been applied,
    // or else signal the session manager to log out.
    chromeos::UpdateEngineClient* update_engine_client
        = chromeos::DBusThreadManager::Get()->GetUpdateEngineClient();
    if (update_engine_client->GetLastStatus().status ==
        chromeos::UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT) {
      update_engine_client->RebootAfterUpdate();
    } else {
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient()
          ->StopSession();
    }
  } else {
    // If running the Chrome OS build, but we're not on the device, act
    // as if we received signal from SessionManager.
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::Bind(&browser::ExitCleanly));
  }
#endif
}

void OnAppExiting() {
  static bool notified = false;
  if (notified)
    return;
  notified = true;
  HandleAppExitingForPlatform();
}

void CloseAllBrowsers() {
  bool session_ending =
      browser_shutdown::GetShutdownType() == browser_shutdown::END_SESSION;
  // Tell everyone that we are shutting down.
  browser_shutdown::SetTryingToQuit(true);

#if defined(ENABLE_SESSION_SERVICE)
  // Before we close the browsers shutdown all session services. That way an
  // exit can restore all browsers open before exiting.
  ProfileManager::ShutdownSessionServices();
#endif

  // If there are no browsers, send the APP_TERMINATING action here. Otherwise,
  // it will be sent by RemoveBrowser() when the last browser has closed.
  if (browser_shutdown::ShuttingDownWithoutClosingBrowsers() ||
      BrowserList::empty()) {
    NotifyAndTerminate(true);
    OnAppExiting();
    return;
  }

#if defined(OS_CHROMEOS)
  chromeos::BootTimesLoader::Get()->AddLogoutTimeMarker(
      "StartedClosingWindows", false);
#endif
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end();) {
    Browser* browser = *i;
    browser->window()->Close();
    if (!session_ending) {
      ++i;
    } else {
      // This path is hit during logoff/power-down. In this case we won't get
      // a final message and so we force the browser to be deleted.
      // Close doesn't immediately destroy the browser
      // (Browser::TabStripEmpty() uses invoke later) but when we're ending the
      // session we need to make sure the browser is destroyed now. So, invoke
      // DestroyBrowser to make sure the browser is deleted and cleanup can
      // happen.
      while (browser->tab_count())
        delete chrome::GetTabContentsAt(browser, 0);
      browser->window()->DestroyBrowser();
      i = BrowserList::begin();
      if (i != BrowserList::end() && browser == *i) {
        // Destroying the browser should have removed it from the browser list.
        // We should never get here.
        NOTREACHED();
        return;
      }
    }
  }
}

void AttemptUserExit() {
#if defined(OS_CHROMEOS)
  chromeos::BootTimesLoader::Get()->AddLogoutTimeMarker("LogoutStarted", false);
  // Write /tmp/uptime-logout-started as well.
  const char kLogoutStarted[] = "logout-started";
  chromeos::BootTimesLoader::Get()->RecordCurrentStats(kLogoutStarted);

  // Login screen should show up in owner's locale.
  PrefService* state = g_browser_process->local_state();
  if (state) {
    std::string owner_locale = state->GetString(prefs::kOwnerLocale);
    if (!owner_locale.empty() &&
        state->GetString(prefs::kApplicationLocale) != owner_locale &&
        !state->IsManagedPreference(prefs::kApplicationLocale)) {
      state->SetString(prefs::kApplicationLocale, owner_locale);
      state->CommitPendingWrite();
    }
  }
  g_session_manager_requested_shutdown = false;
  // On ChromeOS, always terminate the browser, regardless of the result of
  // AreAllBrowsersCloseable(). See crbug.com/123107.
  NotifyAndTerminate(true);
#else
  // Reset the restart bit that might have been set in cancelled restart
  // request.
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, false);
  AttemptExitInternal();
#endif
}

void AttemptRestart() {
  // TODO(beng): Can this use ProfileManager::GetLoadedProfiles instead?
  BrowserList::const_iterator it;
  for (it = BrowserList::begin(); it != BrowserList::end(); ++it)
    content::BrowserContext::SaveSessionState((*it)->profile());

  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);

#if defined(OS_CHROMEOS)
  // For CrOS instead of browser restart (which is not supported) perform a full
  // sign out. Session will be only restored if user has that setting set.
  // Same session restore behavior happens in case of full restart after update.
  AttemptUserExit();
#else
  // Set the flag to restore state after the restart.
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, true);
  AttemptExit();
#endif
}

#if defined(OS_WIN)
void AttemptRestartWithModeSwitch() {
  // The kRestartSwitchMode preference does not exists for Windows 7 and older
  // operating systems so there is no need for OS version check.
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kRestartSwitchMode, true);
  browser::AttemptRestart();
}
#endif

void AttemptExit() {
  // If we know that all browsers can be closed without blocking,
  // don't notify users of crashes beyond this point.
  // Note that MarkAsCleanShutdown does not set UMA's exit cleanly bit
  // so crashes during shutdown are still reported in UMA.
  if (AreAllBrowsersCloseable())
    MarkAsCleanShutdown();
  AttemptExitInternal();
}

#if defined(OS_CHROMEOS)
// A function called when SIGTERM is received.
void ExitCleanly() {
  // We always mark exit cleanly because SessionManager may kill
  // chrome in 3 seconds after SIGTERM.
  g_browser_process->EndSession();

  // Don't block when SIGTERM is received. AreaAllBrowsersCloseable()
  // can be false in following cases. a) power-off b) signout from
  // screen locker.
  if (!AreAllBrowsersCloseable())
    browser_shutdown::OnShutdownStarting(browser_shutdown::END_SESSION);
  AttemptExitInternal();
}
#endif

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

  CloseAllBrowsers();

  // Send out notification. This is used during testing so that the test harness
  // can properly shutdown before we exit.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SESSION_END,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());

  // This will end by terminating the process.
  content::ImmediateShutdownAndExitProcess();
}

void StartKeepAlive() {
  // Increment the browser process refcount as long as we're keeping the
  // application alive.
  if (!WillKeepAlive())
    g_browser_process->AddRefModule();
  ++g_keep_alive_count;
}

void EndKeepAlive() {
  DCHECK_GT(g_keep_alive_count, 0);
  --g_keep_alive_count;

  DCHECK(g_browser_process);
  // Although we should have a browser process, if there is none,
  // there is nothing to do.
  if (!g_browser_process) return;

  // Allow the app to shutdown again.
  if (!WillKeepAlive()) {
    g_browser_process->ReleaseModule();
    // If there are no browsers open and we aren't already shutting down,
    // initiate a shutdown. Also skips shutdown if this is a unit test
    // (MessageLoop::current() == null).
    if (BrowserList::empty() && !browser_shutdown::IsTryingToQuit() &&
        MessageLoop::current())
      CloseAllBrowsers();
  }
}

bool WillKeepAlive() {
  return g_keep_alive_count > 0;
}

}  // namespace browser
