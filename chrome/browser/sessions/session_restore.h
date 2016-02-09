// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_RESTORE_H_
#define CHROME_BROWSER_SESSIONS_SESSION_RESTORE_H_

#include <stdint.h>

#include <vector>

#include "base/callback_list.h"
#include "base/macros.h"
#include "components/history/core/browser/history_service.h"
#include "components/sessions/core/session_types.h"
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

  // Notification callback list.
  using CallbackList = base::CallbackList<void(int)>;

  // Used by objects calling RegisterOnSessionRestoredCallback() to de-register
  // themselves when they are destroyed.
  using CallbackSubscription =
      scoped_ptr<base::CallbackList<void(int)>::Subscription>;

  // Restores the last session. |behavior| is a bitmask of Behaviors, see it
  // for details. If |browser| is non-null the tabs for the first window are
  // added to it. Returns the last active browser.
  //
  // If |urls_to_open| is non-empty, a tab is added for each of the URLs.
  static Browser* RestoreSession(Profile* profile,
                                 Browser* browser,
                                 uint32_t behavior,
                                 const std::vector<GURL>& urls_to_open);

  // Restores the last session when the last session crashed. It's a wrapper
  // of function RestoreSession.
  static void RestoreSessionAfterCrash(Browser* browser);

  // Specifically used in the restoration of a foreign session.  This function
  // restores the given session windows to multiple browsers. Returns the
  // created Browsers.
  static std::vector<Browser*> RestoreForeignSessionWindows(
      Profile* profile,
      std::vector<const sessions::SessionWindow*>::const_iterator begin,
      std::vector<const sessions::SessionWindow*>::const_iterator end);

  // Specifically used in the restoration of a foreign session.  This method
  // restores the given session tab to the browser of |source_web_contents| if
  // the disposition is not NEW_WINDOW. Returns the WebContents corresponding
  // to the restored tab. If |disposition| is CURRENT_TAB, |source_web_contents|
  // may be destroyed.
  static content::WebContents* RestoreForeignSessionTab(
      content::WebContents* source_web_contents,
      const sessions::SessionTab& tab,
      WindowOpenDisposition disposition);

  // Returns true if we're in the process of restoring |profile|.
  static bool IsRestoring(const Profile* profile);

  // Returns true if synchronously restoring a session.
  static bool IsRestoringSynchronously();

  // Registers a callback that is notified every time session restore completes.
  // Note that 'complete' means all the browsers and tabs have been created but
  // have not necessarily finished loading. The integer supplied to the callback
  // indicates the number of tabs that were created.
  static CallbackSubscription RegisterOnSessionRestoredCallback(
      const base::Callback<void(int)>& callback);

 private:
  SessionRestore();

  // Accessor for |*on_session_restored_callbacks_|. Creates a new object the
  // first time so that it always returns a valid object.
  static CallbackList* on_session_restored_callbacks() {
    if (!on_session_restored_callbacks_)
      on_session_restored_callbacks_ = new CallbackList();
    return on_session_restored_callbacks_;
  }

  // Contains all registered callbacks for session restore notifications.
  static CallbackList* on_session_restored_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(SessionRestore);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_RESTORE_H_
