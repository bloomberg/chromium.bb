// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/athena_apps_client_base.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/extensions/athena_native_app_window_views.h"
#include "extensions/common/extension.h"

namespace athena {

AthenaAppsClientBase::AthenaAppsClientBase() {
}

AthenaAppsClientBase::~AthenaAppsClientBase() {
}

extensions::NativeAppWindow* AthenaAppsClientBase::CreateNativeAppWindow(
    extensions::AppWindow* app_window,
    const extensions::AppWindow::CreateParams& params) {
  AthenaNativeAppWindowViews* native_window = new AthenaNativeAppWindowViews;
  native_window->Init(app_window, params);
  Activity* app_activity = ActivityFactory::Get()->CreateAppActivity(
      app_window, native_window->GetWebView());
  ActivityManager::Get()->AddActivity(app_activity);
  return native_window;
}

void AthenaAppsClientBase::IncrementKeepAliveCount() {
  // No need to keep track of KeepAlive count on ChromeOS.
}

void AthenaAppsClientBase::DecrementKeepAliveCount() {
  // No need to keep track of KeepAlive count on ChromeOS.
}

}  // namespace athena
