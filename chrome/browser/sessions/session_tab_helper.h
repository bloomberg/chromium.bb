// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_TAB_HELPER_H_
#define CHROME_BROWSER_SESSIONS_SESSION_TAB_HELPER_H_

#include "base/basictypes.h"
#include "components/sessions/session_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// This class keeps the extension API's windowID up-to-date with the current
// window of the tab.
class SessionTabHelper : public content::WebContentsObserver,
                         public content::WebContentsUserData<SessionTabHelper> {
 public:
  virtual ~SessionTabHelper();

  // Returns the identifier used by session restore for this tab.
  const SessionID& session_id() const { return session_id_; }

  // Identifier of the window the tab is in.
  void SetWindowID(const SessionID& id);
  const SessionID& window_id() const { return window_id_; }

  // If the specified WebContents has a SessionTabHelper (probably because it
  // was used as the contents of a tab), returns a tab id. This value is
  // immutable for a given tab. It will be unique across Chrome within the
  // current session, but may be re-used across sessions. Returns -1
  // for a NULL WebContents or if the WebContents has no SessionTabHelper.
  static SessionID::id_type IdForTab(const content::WebContents* tab);

  // If the specified WebContents has a SessionTabHelper (probably because it
  // was used as the contents of a tab), and has ever been attached to a Browser
  // window, returns Browser::session_id().id() for that Browser. If the tab is
  // being dragged between Browser windows, returns the old window's id value.
  // If the WebContents has a SessionTabHelper but has never been attached to a
  // Browser window, returns an id value that is different from that of any
  // Browser. Returns -1 for a NULL WebContents or if the WebContents has no
  // SessionTabHelper.
  static SessionID::id_type IdForWindowContainingTab(
      const content::WebContents* tab);

  // content::WebContentsObserver:
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void UserAgentOverrideSet(const std::string& user_agent) OVERRIDE;

 private:
  explicit SessionTabHelper(content::WebContents* contents);
  friend class content::WebContentsUserData<SessionTabHelper>;

  // Unique identifier of the tab for session restore. This id is only unique
  // within the current session, and is not guaranteed to be unique across
  // sessions.
  const SessionID session_id_;

  // Unique identifier of the window the tab is in.
  SessionID window_id_;

  DISALLOW_COPY_AND_ASSIGN(SessionTabHelper);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_TAB_HELPER_H_
