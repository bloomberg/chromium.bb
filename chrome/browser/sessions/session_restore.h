// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_RESTORE_H_
#define CHROME_BROWSER_SESSIONS_SESSION_RESTORE_H_

#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/ui/host_desktop.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}

// SessionRestore handles restoring either the last or saved session. Session
// restore come in two variants, asynchronous or synchronous. The synchronous
// variety is meant for startup and blocks until restore is complete.
class SessionRestore {
 public:
  enum Behavior {
    // Indicates the active tab of the supplied browser should be closed.
    CLOBBER_CURRENT_TAB          = 1 << 0,

    // Indicates that if there is a problem restoring the last session then a
    // new tabbed browser should be created.
    ALWAYS_CREATE_TABBED_BROWSER = 1 << 1,

    // Restore blocks until complete. This is intended for use during startup
    // when we want to block until restore is complete.
    SYNCHRONOUS                  = 1 << 2,
  };

  // Restores the last session. |behavior| is a bitmask of Behaviors, see it
  // for details. If |browser| is non-null the tabs for the first window are
  // added to it. Returns the last active browser.
  // Every additional browser created will be created on the desktop specified
  // by |host_desktop_type|, if |browser| is non-null it should have the same
  // desktop type.
  //
  // If |urls_to_open| is non-empty, a tab is added for each of the URLs.
  static Browser* RestoreSession(Profile* profile,
                                 Browser* browser,
                                 chrome::HostDesktopType host_desktop_type,
                                 uint32 behavior,
                                 const std::vector<GURL>& urls_to_open);

  // Specifically used in the restoration of a foreign session.  This method
  // restores the given session windows to multiple browsers all of which
  // will be created on the desktop specified by |host_desktop_type|.
  static void RestoreForeignSessionWindows(
      Profile* profile,
      chrome::HostDesktopType host_desktop_type,
      std::vector<const SessionWindow*>::const_iterator begin,
      std::vector<const SessionWindow*>::const_iterator end);

  // Specifically used in the restoration of a foreign session.  This method
  // restores the given session tab to the browser of |source_web_contents| if
  // the disposition is not NEW_WINDOW.
  static void RestoreForeignSessionTab(
      content::WebContents* source_web_contents,
      const SessionTab& tab,
      WindowOpenDisposition disposition);

  // Returns true if we're in the process of restoring |profile|.
  static bool IsRestoring(const Profile* profile);

  // The max number of non-selected tabs SessionRestore loads when restoring
  // a session. A value of 0 indicates all tabs are loaded at once.
  static size_t num_tabs_to_load_;

 private:
  SessionRestore();

  DISALLOW_COPY_AND_ASSIGN(SessionRestore);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_RESTORE_H_
