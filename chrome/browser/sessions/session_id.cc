// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_id.h"

#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"

static SessionID::id_type next_id = 1;

SessionID::SessionID() {
  id_ = next_id++;
}

SessionID::id_type SessionID::IdForTab(const TabContents* tab) {
  return tab ? tab->restore_tab_helper()->session_id().id() : -1;
}

SessionID::id_type SessionID::IdForWindowContainingTab(const TabContents* tab) {
  return tab ? tab->restore_tab_helper()->window_id().id() : -1;
}
