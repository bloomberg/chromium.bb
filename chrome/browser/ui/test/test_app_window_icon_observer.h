// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TEST_TEST_APP_WINDOW_ICON_OBSERVER_H_
#define CHROME_BROWSER_UI_TEST_TEST_APP_WINDOW_ICON_OBSERVER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/image/image_skia.h"

namespace content {
class BrowserContext;
}

// A test helper that waits for AppWindow icon property updates.
class TestAppWindowIconObserver
    : public extensions::AppWindowRegistry::Observer,
      public aura::WindowObserver {
 public:
  explicit TestAppWindowIconObserver(content::BrowserContext* context);
  ~TestAppWindowIconObserver() override;

  // Waits for one icon update.
  void WaitForIconUpdate();
  // Waits for |updates| number of icon updates.
  void WaitForIconUpdates(int updates);

  int icon_updates() const { return icon_updates_; }

  const gfx::ImageSkia& last_app_icon() const { return last_app_icon_; }

 private:
  // AppWindowRegistry::Observer:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override;
  void OnAppWindowRemoved(extensions::AppWindow* app_window) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  content::BrowserContext* const context_;
  int icon_updates_ = 0;
  int expected_icon_updates_ = 0;
  std::vector<aura::Window*> windows_;
  std::map<aura::Window*, std::string> last_app_icon_hash_map_;
  base::OnceClosure icon_updated_callback_;
  gfx::ImageSkia last_app_icon_;

  DISALLOW_COPY_AND_ASSIGN(TestAppWindowIconObserver);
};

#endif  // CHROME_BROWSER_UI_TEST_TEST_APP_WINDOW_ICON_OBSERVER_H_
