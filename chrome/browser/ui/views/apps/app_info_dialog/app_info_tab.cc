// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_tab.h"

AppInfoTab::AppInfoTab(Profile* profile, const extensions::Extension* app)
    : profile_(profile), app_(app) {
}

AppInfoTab::~AppInfoTab() {}
