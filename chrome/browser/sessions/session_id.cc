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
  // Explicitly crash if this isn't a tab. This is temporary during refactoring
  // and will go away soon.
  return tab ? SessionTabHelper::FromWebContents(tab)->session_id().id() : -1;
}

SessionID::id_type SessionID::IdForWindowContainingTab(
    const content::WebContents* tab) {
  // Explicitly crash if this isn't a tab. This is temporary during refactoring
  // and will go away soon.
  return tab ? SessionTabHelper::FromWebContents(tab)->window_id().id() : -1;
}
