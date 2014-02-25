// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_H_
#define CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_H_

#include "base/compiler_specific.h"

class Browser;

namespace chrome {

// Starts a user initiated exit process. Called from Browser::Exit.
// On platforms other than ChromeOS, this is equivalent to
// CloseAllBrowsers() On ChromeOS, this tells session manager
// that chrome is signing out, which lets session manager send
// SIGTERM to start actual exit process.
void AttemptUserExit();

// Starts to collect shutdown traces. On ChromeOS this will start immediately
// on AttemptUserExit() and all other systems will start once all tabs are
// closed.
void StartShutdownTracing();

// Starts a user initiated restart process. On platforms other than
// chromeos, this sets a restart bit in the preference so that
// chrome will be restarted at the end of shutdown process. On
// ChromeOS, this simply exits the chrome, which lets sesssion
// manager re-launch the browser with restore last session flag.
void AttemptRestart();

#if defined(OS_WIN)
enum AshExecutionStatus {
  ASH_KEEP_RUNNING,
  ASH_TERMINATE,
};

// Helper function to activate the desktop from Ash mode. The
// |ash_execution_status| parameter indicates if we should exit Ash after
// activating desktop.
void ActivateDesktopHelper(AshExecutionStatus ash_execution_status);

// Windows 8 specific: Like AttemptRestart but if chrome is running
// in desktop mode it starts in metro mode and vice-versa. The switching like
// the restarting is controlled by a preference.
void AttemptRestartWithModeSwitch();
void AttemptRestartToDesktopMode();
void AttemptRestartToMetroMode();
#endif

// Attempt to exit by closing all browsers.  This is equivalent to
// CloseAllBrowsers() on platforms where the application exits
// when no more windows are remaining. On other platforms (the Mac),
// this will additionally exit the application if all browsers are
// successfully closed.
//  Note that he exit process may be interrupted by download or
// unload handler, and the browser may or may not exit.
void AttemptExit();

#if defined(OS_CHROMEOS)
// Shutdown chrome cleanly without blocking. This is called
// when SIGTERM is received on Chrome OS, and always sets
// exit-cleanly bit and exits the browser, even if there is
// ongoing downloads or a page with onbeforeunload handler.
//
// If you need to exit or restart in your code on ChromeOS,
// use AttemptExit or AttemptRestart respectively.
void ExitCleanly();
#endif

// Closes all browsers and if successful, quits.
void CloseAllBrowsersAndQuit();

// Closes all browsers. If the session is ending the windows are closed
// directly. Otherwise the windows are closed by way of posting a WM_CLOSE
// message. This will quit the application if there is nothing other than
// browser windows keeping it alive or the application is quitting.
void CloseAllBrowsers();

// Begins shutdown of the application when the desktop session is ending.
void SessionEnding();

// Tells the BrowserList to keep the application alive after the last Browser
// closes. This is implemented as a count, so callers should pair their calls
// to IncrementKeepAliveCount() with matching calls to DecrementKeepAliveCount()
// when they no
// longer need to keep the application running.
void IncrementKeepAliveCount();

// Stops keeping the application alive after the last Browser is closed.
// Should match a previous call to IncrementKeepAliveCount().
void DecrementKeepAliveCount();

// Returns true if application will continue running after the last Browser
// closes.
bool WillKeepAlive();

// Emits APP_TERMINATING notification. It is guaranteed that the
// notification is sent only once.
void NotifyAppTerminating();

// Send out notifications.
// For ChromeOS, also request session manager to end the session.
void NotifyAndTerminate(bool fast_path);

// Called once the application is exiting.
void OnAppExiting();

// Called once the application is exiting to do any platform specific
// processing required.
void HandleAppExitingForPlatform();

// Returns true if we can start the shutdown sequence for the browser, i.e. the
// last browser window is being closed.
bool ShouldStartShutdown(Browser* browser);

}  // namespace chrome

#endif  // CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_H_
