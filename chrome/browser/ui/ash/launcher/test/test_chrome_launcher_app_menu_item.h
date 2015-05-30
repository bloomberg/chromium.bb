// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_TEST_TEST_CHROME_LAUNCHER_APP_MENU_ITEM_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_TEST_TEST_CHROME_LAUNCHER_APP_MENU_ITEM_H_

#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"

#include "base/macros.h"

// A simple test double for a ChromeLauncherAppMenuItem.
class TestChromeLauncherAppMenuItem : public ChromeLauncherAppMenuItem {
 public:
  TestChromeLauncherAppMenuItem();
  ~TestChromeLauncherAppMenuItem() override;

  void set_is_active(bool is_active) { is_active_ = is_active; }
  void set_is_enabled(bool is_enabled) { is_enabled_ = is_enabled; }
  int execute_count() const { return execute_count_; }

  // ChromeLauncherAppMenuItem:
  bool IsActive() const override;
  bool IsEnabled() const override;
  void Execute(int event_flags) override;

 private:
  // Stub return value for the IsActive() function.
  bool is_active_ = false;

  // Stub return value for the IsEnabled() function.
  bool is_enabled_ = false;

  // Tracks how many times the Execute(int) function has been called.
  int execute_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestChromeLauncherAppMenuItem);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_TEST_TEST_CHROME_LAUNCHER_APP_MENU_ITEM_H_
