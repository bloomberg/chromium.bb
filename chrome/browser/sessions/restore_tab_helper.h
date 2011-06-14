// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_RESTORE_TAB_HELPER_H_
#define CHROME_BROWSER_SESSIONS_RESTORE_TAB_HELPER_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/sessions/session_id.h"
#include "content/browser/tab_contents/tab_contents_observer.h"

class TabContentsWrapper;

class RestoreTabHelper : public TabContentsObserver {
 public:
  explicit RestoreTabHelper(TabContentsWrapper* tab);
  ~RestoreTabHelper();

  // Returns the identifier used by session restore for this tab.
  const SessionID& session_id() const { return session_id_; }

  // Identifier of the window the tab is in.
  void SetWindowID(const SessionID& id);
  const SessionID& window_id() const { return window_id_; }

  // TabContentsObserver:
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;

 private:
  TabContentsWrapper* tab_;

  // Unique identifier of the tab for session restore. This id is only unique
  // within the current session, and is not guaranteed to be unique across
  // sessions.
  const SessionID session_id_;

  // Unique identifier of the window the tab is in.
  SessionID window_id_;

  DISALLOW_COPY_AND_ASSIGN(RestoreTabHelper);
};

#endif  // CHROME_BROWSER_SESSIONS_RESTORE_TAB_HELPER_H_
