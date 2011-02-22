// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_RESTORE_H_
#define CHROME_BROWSER_SESSIONS_SESSION_RESTORE_H_
#pragma once

#include <vector>

#include "chrome/browser/history/history.h"
#include "chrome/browser/sessions/session_types.h"
#include "base/basictypes.h"

class Browser;
class Profile;

// SessionRestore handles restoring either the last or saved session. Session
// restore come in two variants, asynchronous or synchronous. The synchronous
// variety is meant for startup, and blocks until restore is complete.
class SessionRestore {
 public:
  // Asnchronously restores the specified session.
  // If |browser| is non-null the tabs for the first window are added to it.
  // If clobber_existing_window is true and there is an open browser window,
  // it is closed after restoring.
  // If always_create_tabbed_browser is true at least one tabbed browser is
  // created. For example, if there is an error restoring, or the last session
  // session is empty and always_create_tabbed_browser is true, a new empty
  // tabbed browser is created.
  //
  // If urls_to_open is non-empty, a tab is added for each of the URLs.
  static void RestoreSession(Profile* profile,
                             Browser* browser,
                             bool clobber_existing_window,
                             bool always_create_tabbed_browser,
                             const std::vector<GURL>& urls_to_open);

  // Specifically used in the restoration of a foreign session.  This method
  // restores the given session windows to a browser.
  static void RestoreForeignSessionWindows(
      Profile* profile,
      std::vector<SessionWindow*>::const_iterator begin,
      std::vector<SessionWindow*>::const_iterator end);

  // Specifically used in the restoration of a foreign session.  This method
  // restores the given session tab to a browser.
  static void RestoreForeignSessionTab(Profile* profile,
      const SessionTab& tab);

  // Synchronously restores the last session. At least one tabbed browser is
  // created, even if there is an error in restoring.
  //
  // If urls_to_open is non-empty, a tab is added for each of the URLs.
  //
  // Returns the last active Browser (which may have been created by the act of
  // restoring).
  static Browser* RestoreSessionSynchronously(
      Profile* profile,
      const std::vector<GURL>& urls_to_open);

  // Returns true if we're in the process of restoring.
  static bool IsRestoring();

  // The max number of non-selected tabs SessionRestore loads when restoring
  // a session. A value of 0 indicates all tabs are loaded at once.
  static size_t num_tabs_to_load_;

 private:
  SessionRestore();

  DISALLOW_COPY_AND_ASSIGN(SessionRestore);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_RESTORE_H_
