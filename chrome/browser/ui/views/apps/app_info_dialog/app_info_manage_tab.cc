// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_manage_tab.h"

AppInfoManageTab::AppInfoManageTab(gfx::NativeWindow parent_window,
                                   Profile* profile,
                                   const extensions::Extension* app,
                                   const base::Closure& close_callback)
    : AppInfoTab(parent_window, profile, app, close_callback) {
  // TODO(sashab): Populate this tab.
}

AppInfoManageTab::~AppInfoManageTab() {}
