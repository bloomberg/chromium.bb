// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_TAB_HELPER_H_
#define CHROME_BROWSER_SESSIONS_SESSION_TAB_HELPER_H_

#include "base/basictypes.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/tab_contents/web_contents_user_data.h"
#include "content/public/browser/web_contents_observer.h"

// This class keeps the extension API's windowID up-to-date with the current
// window of the tab.
class SessionTabHelper : public content::WebContentsObserver,
                         public WebContentsUserData<SessionTabHelper> {
 public:
  virtual ~SessionTabHelper();

  // Returns the identifier used by session restore for this tab.
  const SessionID& session_id() const { return session_id_; }

  // Identifier of the window the tab is in.
  void SetWindowID(const SessionID& id);
  const SessionID& window_id() const { return window_id_; }

  // content::WebContentsObserver:
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void UserAgentOverrideSet(const std::string& user_agent) OVERRIDE;

 private:
  explicit SessionTabHelper(content::WebContents* contents);
  static int kUserDataKey;
  friend class WebContentsUserData<SessionTabHelper>;

  // Unique identifier of the tab for session restore. This id is only unique
  // within the current session, and is not guaranteed to be unique across
  // sessions.
  const SessionID session_id_;

  // Unique identifier of the window the tab is in.
  SessionID window_id_;

  DISALLOW_COPY_AND_ASSIGN(SessionTabHelper);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_TAB_HELPER_H_
