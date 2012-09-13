// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_id.h"

#include "chrome/browser/sessions/session_tab_helper.h"

static SessionID::id_type next_id = 1;

SessionID::SessionID() {
  id_ = next_id++;
}

SessionID::id_type SessionID::IdForTab(const content::WebContents* tab) {
  const SessionTabHelper* session_tab_helper =
      tab ? SessionTabHelper::FromWebContents(tab) : NULL;
  return session_tab_helper ? session_tab_helper->session_id().id() : -1;
}

SessionID::id_type SessionID::IdForWindowContainingTab(
    const content::WebContents* tab) {
  const SessionTabHelper* session_tab_helper =
      tab ? SessionTabHelper::FromWebContents(tab) : NULL;
  return session_tab_helper ? session_tab_helper->window_id().id() : -1;
}
