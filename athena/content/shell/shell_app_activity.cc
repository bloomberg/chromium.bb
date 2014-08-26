// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/shell/shell_app_activity.h"

#include "extensions/shell/browser/shell_app_window.h"

namespace athena {

ShellAppActivity::ShellAppActivity(extensions::ShellAppWindow* app_window)
    : shell_app_window_(app_window) {
}

ShellAppActivity::~ShellAppActivity() {}

content::WebContents* ShellAppActivity::GetWebContents() {
  return shell_app_window_->GetAssociatedWebContents();
}

}  // namespace athena
