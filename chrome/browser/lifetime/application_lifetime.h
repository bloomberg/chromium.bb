// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_H_
#define CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_H_

#include "base/compiler_specific.h"
#include "build/build_config.h"

class Browser;

namespace chrome {

// Starts a user initiated exit process. Called from Browser::Exit.
// On platforms other than ChromeOS, this is equivalent to
// CloseAllBrowsers() On ChromeOS, this tells session manager
// that chrome is signing out, which lets session manager send
// SIGTERM to start actual exit process.
void AttemptUserExit();

// Starts a user initiated restart process. On platforms other than
// chromeos, this sets a restart bit in the preference so that
// chrome will be restarted at the end of shutdown process. On
// ChromeOS, this simply exits the chrome, which lets sesssion
// manager re-launch the browser with restore last session flag.
void AttemptRestart();

// Starts a user initiated relaunch process. On platforms other than
// chromeos, this is equivalent to AttemptRestart. On ChromeOS, this relaunches
// the entire OS, instead of just relaunching the browser.
void AttemptRelaunch();

// Attempt to exit by closing all browsers.  This is equivalent to
// CloseAllBrowsers() on platforms where the application exits
// when no more windows are remaining. On other platforms (the Mac),
// this will additionally exit the application if all browsers are
// successfully closed.
//  Note that the exit process may be interrupted by download or
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

#if !defined(OS_ANDROID)
// Closes all browsers and if successful, quits.
void CloseAllBrowsersAndQuit();

// Closes all browsers. If the session is ending the windows are closed
// directly. Otherwise the windows are closed by way of posting a WM_CLOSE
// message. This will quit the application if there is nothing other than
// browser windows keeping it alive or the application is quitting.
void CloseAllBrowsers();

// If there are no browsers open and we aren't already shutting down,
// initiate a shutdown.
void ShutdownIfNeeded();

// Begins shutdown of the application when the desktop session is ending.
void SessionEnding();

#endif  // !defined(OS_ANDROID)

// Emits APP_TERMINATING notification. It is guaranteed that the
// notification is sent only once.
void NotifyAppTerminating();

// Send out notifications.
// For ChromeOS, also request session manager to end the session.
// |lifetime| is used to signal whether or not a reboot should be forced. By
// default, the functions only reboot the system if an update is available. When
// a component flash update is present, but not a system update, the
// kForceReboot flag is passed.
enum class RebootPolicy { kForceReboot, kOptionalReboot };
void NotifyAndTerminate(bool fast_path);
void NotifyAndTerminate(bool fast_path, RebootPolicy reboot_policy);

#if !defined(OS_ANDROID)
// Called once the application is exiting.
void OnAppExiting();

// Called once the application is exiting to do any platform specific
// processing required.
void HandleAppExitingForPlatform();
#endif  // !defined(OS_ANDROID)

}  // namespace chrome

#endif  // CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_H_
