// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_H_
#define CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_H_
#pragma once

namespace browser {

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

// Attempt to exit by closing all browsers.  This is equivalent to
// CloseAllBrowsers() on platforms where the application exits
// when no more windows are remaining. On other platforms (the Mac),
// this will additionally exit the application if all browsers are
// successfully closed.
//  Note that he exit process may be interrupted by download or
// unload handler, and the browser may or may not exit.
void AttemptExit();

#if defined(OS_CHROMEOS)
// This is equivalent to AttemptUserExit, except that it always set
// exit cleanly bit. ChromeOS checks if it can exit without user
// interactions, so it will always exit the browser.  This is used to
// handle SIGTERM on chromeos which is a signal to force shutdown
// the chrome.
void ExitCleanly();
#endif

// Closes all browsers. If the session is ending the windows are closed
// directly. Otherwise the windows are closed by way of posting a WM_CLOSE
// message.
void CloseAllBrowsers();

// Begins shutdown of the application when the desktop session is ending.
void SessionEnding();

// Tells the BrowserList to keep the application alive after the last Browser
// closes. This is implemented as a count, so callers should pair their calls
// to StartKeepAlive() with matching calls to EndKeepAlive() when they no
// longer need to keep the application running.
void StartKeepAlive();

// Stops keeping the application alive after the last Browser is closed.
// Should match a previous call to StartKeepAlive().
void EndKeepAlive();

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

}  // namespace browser

#endif  // CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_H_
