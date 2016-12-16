// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_SETTINGS_WINDOW_OBSERVER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_SETTINGS_WINDOW_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/ui/settings_window_manager_observer.h"
#include "services/ui/public/cpp/window_tracker.h"
#include "ui/aura/window_tracker.h"

// Sets the window title and shelf item properties for settings windows.
// Settings windows are not handled by BrowserShortcutLauncherItemController.
class SettingsWindowObserver : public chrome::SettingsWindowManagerObserver {
 public:
  SettingsWindowObserver();
  ~SettingsWindowObserver() override;

  // SettingsWindowManagerObserver:
  void OnNewSettingsWindow(Browser* settings_browser) override;

 private:
  // Window trackers that rename the Settings window in classic ash and mash.
  std::unique_ptr<aura::WindowTracker> aura_window_tracker_;
  std::unique_ptr<ui::WindowTracker> ui_window_tracker_;

  DISALLOW_COPY_AND_ASSIGN(SettingsWindowObserver);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_SETTINGS_WINDOW_OBSERVER_H_
