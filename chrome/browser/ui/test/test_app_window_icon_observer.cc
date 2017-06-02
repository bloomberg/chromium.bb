// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_app_window_icon_observer.h"

#include "base/run_loop.h"
#include "extensions/browser/app_window/app_window.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

TestAppWindowIconObserver::TestAppWindowIconObserver(
    content::BrowserContext* context)
    : context_(context) {
  extensions::AppWindowRegistry::Get(context_)->AddObserver(this);
}

TestAppWindowIconObserver::~TestAppWindowIconObserver() {
  extensions::AppWindowRegistry::Get(context_)->RemoveObserver(this);
  for (aura::Window* window : windows_)
    window->RemoveObserver(this);
}

void TestAppWindowIconObserver::WaitForIconUpdate() {
  WaitForIconUpdates(1);
}

void TestAppWindowIconObserver::WaitForIconUpdates(int updates) {
  base::RunLoop run_loop;
  expected_icon_updates_ = updates + icon_updates_;
  icon_updated_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestAppWindowIconObserver::OnAppWindowAdded(
    extensions::AppWindow* app_window) {
  aura::Window* window = app_window->GetNativeWindow();
  window->AddObserver(this);
  windows_.push_back(window);
}

void TestAppWindowIconObserver::OnAppWindowRemoved(
    extensions::AppWindow* app_window) {
  aura::Window* window = app_window->GetNativeWindow();
  if (window) {
    windows_.erase(std::find(windows_.begin(), windows_.end(), window));
    window->RemoveObserver(this);
  }
}

void TestAppWindowIconObserver::OnWindowPropertyChanged(aura::Window* window,
                                                        const void* key,
                                                        intptr_t old) {
  if (key == aura::client::kAppIconKey) {
    ++icon_updates_;
    if (icon_updates_ == expected_icon_updates_ &&
        !icon_updated_callback_.is_null()) {
      base::ResetAndReturn(&icon_updated_callback_).Run();
    }
  }
}
