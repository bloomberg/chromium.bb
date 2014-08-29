// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/content_activity_factory.h"

#include "athena/content/shell/shell_app_activity.h"

namespace athena {

Activity* ContentActivityFactory::CreateAppActivity(
    extensions::ShellAppWindow* app_window,
    const std::string& app_id) {
  return new ShellAppActivity(app_window, app_id);
}

Activity* ContentActivityFactory::CreateAppActivity(
    extensions::AppWindow* app_window) {
  return NULL;
}

}  // namespace athena
