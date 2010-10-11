// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_MODE_MANAGER_H_
#define CHROME_BROWSER_BACKGROUND_MODE_MANAGER_H_
#pragma once

#include "app/menus/simple_menu_model.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class Browser;
class Extension;
class PrefService;
class Profile;
class StatusIcon;
class StatusTray;

// BackgroundModeManager is responsible for switching Chrome into and out of
// "background mode" and for providing UI for the user to exit Chrome when there
// are no open browser windows.
//
// Chrome enters background mode whenever there is an application with the
// "background" permission installed and the appropriate user preference is
// set (kBackgroundModeEnabled). This class monitors the set of installed/loaded
// extensions to ensure that Chrome enters/exits background mode at the
// appropriate time.
//
// When Chrome is in background mode, it will continue running even after the
// last browser window is closed, until the user explicitly exits the app.
// Additionally, when in background mode, Chrome will launch on OS login with
// no open windows to allow apps with the "background" permission to run in the
// background.
class BackgroundModeManager
    : private NotificationObserver,
      private menus::SimpleMenuModel::Delegate {
 public:
  BackgroundModeManager(Profile* profile, CommandLine* command_line);
  virtual ~BackgroundModeManager();

  static void RegisterUserPrefs(PrefService* prefs);
 private:
  friend class TestBackgroundModeManager;
  friend class BackgroundModeManagerTest;
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundAppLoadUnload);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundAppInstallUninstall);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundPrefDisabled);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundPrefDynamicDisable);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundPrefDynamicEnable);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

  // Called when an extension is loaded to manage count of background apps.
  void OnBackgroundAppLoaded();

  // Called when an extension is unloaded to manage count of background apps.
  void OnBackgroundAppUnloaded();

  // Invoked when an extension is installed so we can ensure that
  // launch-on-startup is enabled if appropriate.
  void OnBackgroundAppInstalled();

  // Invoked when an extension is uninstalled so we can ensure that
  // launch-on-startup is disabled if appropriate.
  void OnBackgroundAppUninstalled();

  // Invoked when the kBackgroundModeEnabled preference has changed.
  void OnBackgroundModePrefChanged();

  // Returns true if the background mode preference is enabled
  bool IsBackgroundModeEnabled();

  // Returns true if chrome has set "launch on startup" property for itself
  // earlier and is allowed to reset it later, reducing likelihood of
  // overriding user choices.
  bool IsLaunchOnStartupResetAllowed();
  void SetLaunchOnStartupResetAllowed(bool allowed);

  // Called to make sure that our launch-on-startup mode is properly set.
  // (virtual so we can override for tests).
  virtual void EnableLaunchOnStartup(bool should_launch);

  // Invoked to put Chrome in KeepAlive mode - chrome runs in the background
  // and has a status bar icon.
  void StartBackgroundMode();

  // Invoked to take Chrome out of KeepAlive mode - chrome stops running in
  // the background and removes its status bar icon.
  void EndBackgroundMode();

  // If --no-startup-window is passed, BackgroundModeManager will manually keep
  // chrome running while waiting for apps to load. This is called when we no
  // longer need to do this (either because the user has chosen to exit chrome
  // manually, or all apps have been loaded).
  void EndKeepAliveForStartup();

  // Create a status tray icon to allow the user to shutdown Chrome when running
  // in background mode. Virtual to enable testing.
  virtual void CreateStatusTrayIcon();

  // Removes the status tray icon because we are exiting background mode.
  // Virtual to enable testing.
  virtual void RemoveStatusTrayIcon();

  // Returns a browser window, or creates one if none are open. Used by
  // operations (like displaying the preferences dialog) that require a Browser
  // window.
  Browser* GetBrowserWindow();

  NotificationRegistrar registrar_;

  // The parent profile for this object.
  Profile* profile_;

  // The number of background apps currently loaded.
  int background_app_count_;

  // Set to true when we are running in background mode. Allows us to track our
  // current background state so we can take the appropriate action when the
  // user disables/enables background mode via preferences.
  bool in_background_mode_;

  // Set when we are keeping chrome running during the startup process - this
  // is required when running with the --no-startup-window flag, as otherwise
  // chrome would immediately exit due to having no open windows.
  bool keep_alive_for_startup_;

  // Reference to our status tray (owned by our parent profile). If null, the
  // platform doesn't support status icons.
  StatusTray* status_tray_;

  // Reference to our status icon (if any) - owned by the StatusTray.
  StatusIcon* status_icon_;

  // Ensure observed preferences are properly cleaned up.
  PrefChangeRegistrar pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundModeManager);
};

#endif  // CHROME_BROWSER_BACKGROUND_MODE_MANAGER_H_
