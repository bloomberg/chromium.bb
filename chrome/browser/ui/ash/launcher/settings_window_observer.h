// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_SETTINGS_WINDOW_OBSERVER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_SETTINGS_WINDOW_OBSERVER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/settings_window_manager_observer_chromeos.h"
#include "ui/aura/window_tracker.h"

// Sets the window title and shelf item properties for settings windows.
// Settings windows are not handled by BrowserShortcutLauncherItemController.
class SettingsWindowObserver : public chrome::SettingsWindowManagerObserver {
 public:
  SettingsWindowObserver();
  ~SettingsWindowObserver() override;

  // chrome::SettingsWindowManagerObserver:
  void OnNewSettingsWindow(Browser* settings_browser) override;

 private:
  // Set of windows whose title is forced to 'Settings.'
  std::unique_ptr<aura::WindowTracker> aura_window_tracker_;

  DISALLOW_COPY_AND_ASSIGN(SettingsWindowObserver);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_SETTINGS_WINDOW_OBSERVER_H_
