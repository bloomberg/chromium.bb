// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_restore_service.h"

#include "content/public/browser/session_storage_namespace.h"

// TimeFactory-----------------------------------------------------------------

TabRestoreService::TimeFactory::~TimeFactory() {}

// Entry ----------------------------------------------------------------------

// ID of the next Entry.
static SessionID::id_type next_entry_id = 1;

TabRestoreService::Entry::Entry()
    : id(next_entry_id++),
      type(TAB),
      from_last_session(false) {}

TabRestoreService::Entry::Entry(Type type)
    : id(next_entry_id++),
      type(type),
      from_last_session(false) {}

TabRestoreService::Entry::~Entry() {}

// Tab ------------------------------------------------------------------------

TabRestoreService::Tab::Tab()
    : Entry(TAB),
      current_navigation_index(-1),
      browser_id(0),
      tabstrip_index(-1),
      pinned(false) {
}

TabRestoreService::Tab::~Tab() {
}

// Window ---------------------------------------------------------------------

TabRestoreService::Window::Window() : Entry(WINDOW), selected_tab_index(-1) {
}

TabRestoreService::Window::~Window() {
}

// TabRestoreService ----------------------------------------------------------

TabRestoreService::~TabRestoreService() {
}
