// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_tab.h"

AppInfoTab::AppInfoTab(gfx::NativeWindow parent_window,
                       Profile* profile,
                       const extensions::Extension* app,
                       const base::Closure& close_callback)
    : parent_window_(parent_window),
      profile_(profile),
      app_(app),
      close_callback_(close_callback) {}

AppInfoTab::~AppInfoTab() {}
