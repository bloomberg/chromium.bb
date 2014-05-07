// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/app_window_waiter.h"

#include "apps/app_window.h"

namespace chromeos {

AppWindowWaiter::AppWindowWaiter(apps::AppWindowRegistry* registry,
                                 const std::string& app_id)
    : registry_(registry), app_id_(app_id), window_(NULL) {
  registry_->AddObserver(this);
}

AppWindowWaiter::~AppWindowWaiter() {
  registry_->RemoveObserver(this);
}

apps::AppWindow* AppWindowWaiter::Wait() {
  window_ = registry_->GetCurrentAppWindowForApp(app_id_);
  if (window_)
    return window_;

  run_loop_.Run();

  return window_;
}

void AppWindowWaiter::OnAppWindowAdded(apps::AppWindow* app_window) {
  if (!run_loop_.running())
    return;

  if (app_window->extension_id() == app_id_) {
    window_ = app_window;
    run_loop_.Quit();
  }
}

}  // namespace chromeos
