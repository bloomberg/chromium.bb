// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/athena_app_window_controller.h"

#include "apps/shell/browser/shell_app_window.h"
#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"

namespace athena {

AthenaAppWindowController::AthenaAppWindowController() {
}

AthenaAppWindowController::~AthenaAppWindowController() {
}

apps::ShellAppWindow* AthenaAppWindowController::CreateAppWindow(
    content::BrowserContext* context) {
  apps::ShellAppWindow* app_window = new apps::ShellAppWindow();
  app_window->Init(context, gfx::Size(100, 100));
  ActivityManager::Get()->AddActivity(ActivityFactory::Get()->CreateAppActivity(
      app_window));
  return app_window;
}

void AthenaAppWindowController::CloseAppWindows() {
  // Do nothing.
}

}  // namespace athena
