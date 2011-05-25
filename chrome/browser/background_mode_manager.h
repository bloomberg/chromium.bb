// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_MODE_MANAGER_H_
#define CHROME_BROWSER_BACKGROUND_MODE_MANAGER_H_
#pragma once

#include <map>

#include "base/gtest_prod_util.h"
#include "chrome/browser/background_application_list_model.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;
class CommandLine;
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
// "background" permission installed. This class monitors the set of
// installed/loaded extensions to ensure that Chrome enters/exits background
// mode at the appropriate time.
//
// When Chrome is in background mode, it will continue running even after the
// last browser window is closed, until the user explicitly exits the app.
// Additionally, when in background mode, Chrome will launch on OS login with
// no open windows to allow apps with the "background" permission to run in the
// background.
class BackgroundModeManager
    : public NotificationObserver,
      public BackgroundApplicationListModel::Observer,
      public ProfileKeyedService {
 public:
  explicit BackgroundModeManager(CommandLine* command_line);
  virtual ~BackgroundModeManager();

  static void RegisterPrefs(PrefService* prefs);

  virtual void RegisterProfile(Profile* profile);

 private:
  friend class TestBackgroundModeManager;
  friend class BackgroundModeManagerTest;
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundAppLoadUnload);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundAppInstallUninstall);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           BackgroundAppInstallUninstallWhileDisabled);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           EnableAfterBackgroundAppInstall);
  FRIEND_TEST_ALL_PREFIXES(BackgroundModeManagerTest,
                           MultiProfile);

  class BackgroundModeData : public ui::SimpleMenuModel::Delegate {
   public:
    explicit BackgroundModeData(
        Profile* profile,
        BackgroundModeManager* background_mode_manager);
    virtual ~BackgroundModeData();

    // The cached list of BackgroundApplications.
    scoped_ptr<BackgroundApplicationListModel> applications_;

    // Reference to our status icon (if any) - owned by the StatusTray.
    StatusIcon* status_icon_;

    // Reference to our status icon's context menu (if any) - owned by the
    // status_icon_
    ui::SimpleMenuModel* context_menu_;

    // Set to the position of the first application entry in the status icon's
    // context menu.
    int context_menu_application_offset_;

    // The profile associated with this background app data.
    Profile* profile_;

    // The background mode manager which owns this BackgroundModeData
    BackgroundModeManager* background_mode_manager_;

    // Overrides from SimpleMenuModel::Delegate implementation.
    virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
    virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
    virtual bool GetAcceleratorForCommandId(int command_id,
                                            ui::Accelerator* accelerator)
                                            OVERRIDE;
    virtual void ExecuteCommand(int command_id) OVERRIDE;

    // Returns a browser window, or creates one if none are open. Used by
    // operations (like displaying the preferences dialog) that require a
    // Browser window.
    Browser* GetBrowserWindow();

    // Open an application in a new tab, opening a new window if needed.
    virtual void ExecuteApplication(int application_id);

    // Updates the status icon's context menu entry corresponding to
    // |extension| to use the icon associated with |extension| in the
    // BackgroundApplicationListModel.
    // TODO(rlp): Remove after creating one status icon.
    void UpdateContextMenuEntryIcon(const Extension* extension);

    // Returns whether any of the extensions are background apps.
    bool HasBackgroundApp();
  };

  // Ideally we would want our BackgroundModeData to be scoped_ptrs,
  // but since maps copy their entries, we can't used scoped_ptrs.
  // Similarly, we can't just have a map of BackgroundModeData objects,
  // since BackgroundModeData contains a scoped_ptr which once again
  // can't be copied. So rather than using BackgroundModeData* which
  // we'd have to remember to delete, we use the ref-counted linked_ptr
  // which is similar to a shared_ptr.
  typedef linked_ptr<BackgroundModeData> BackgroundModeInfo;

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // BackgroundApplicationListModel::Observer implementation.
  virtual void OnApplicationDataChanged(const Extension* extension,
                                        Profile* profile) OVERRIDE;
  virtual void OnApplicationListChanged(Profile* profile) OVERRIDE;

  // Called when an extension is loaded to manage count of background apps.
  void OnBackgroundAppLoaded();

  // Called when an extension is unloaded to manage count of background apps.
  void OnBackgroundAppUnloaded();

  // Invoked when an extension is installed so we can ensure that
  // launch-on-startup is enabled if appropriate. |extension| can be NULL when
  // called from unit tests.
  void OnBackgroundAppInstalled(const Extension* extension, Profile* profile);

  // Invoked when an extension is uninstalled so we can ensure that
  // launch-on-startup is disabled if appropriate.
  void OnBackgroundAppUninstalled(Profile* profile);

  // Called to make sure that our launch-on-startup mode is properly set.
  // (virtual so we can override for tests).
  virtual void EnableLaunchOnStartup(bool should_launch);

  // Invoked when a background app is installed so we can display a
  // platform-specific notification to the user.
  void DisplayAppInstalledNotification(const Extension* extension,
                                       Profile* profile);

  // Invoked to put Chrome in KeepAlive mode - chrome runs in the background
  // and has a status bar icon.
  void StartBackgroundMode();

  // Invoked to create status icons for any profiles currently running
  // background apps so that there is a way to exit Chrome.
  void InitStatusTrayIcons();

  // Invoked to take Chrome out of KeepAlive mode - chrome stops running in
  // the background and removes its status bar icon.
  void EndBackgroundMode();

  // If --no-startup-window is passed, BackgroundModeManager will manually keep
  // chrome running while waiting for apps to load. This is called when we no
  // longer need to do this (either because the user has chosen to exit chrome
  // manually, or all apps have been loaded).
  void EndKeepAliveForStartup();

  // Return an appropriate name for a Preferences menu entry.  Preferences is
  // sometimes called Options or Settings.
  string16 GetPreferencesMenuLabel();

  // Create a status tray icon to allow the user to shutdown Chrome when running
  // in background mode. Virtual to enable testing.
  virtual void CreateStatusTrayIcon(Profile* profile);

  // Removes the status tray icon because we are exiting background mode.
  // Virtual to enable testing.
  virtual void RemoveStatusTrayIcon(Profile* profile);

  // Updates the status icon's context menu entry corresponding to |extension|
  // to use the icon associated with |extension| in the
  // BackgroundApplicationListModel.
  void UpdateContextMenuEntryIcon(const Extension* extension, Profile* profile);

  // Create a context menu, or replace/update an existing context menu, for the
  // status tray icon which, among other things, allows the user to shutdown
  // Chrome when running in background mode.
  virtual void UpdateStatusTrayIconContextMenu(Profile* profile);

  // Returns the BackgroundModeInfo associated with this profile. If it does
  // not exist, returns an empty BackgroundModeInfo.
  BackgroundModeManager::BackgroundModeInfo GetBackgroundModeInfo(
      Profile* profile);

  // Returns true if the "Let chrome run in the background" pref is checked.
  // (virtual to allow overriding in tests).
  virtual bool IsBackgroundModePrefEnabled();

  // Turns off background mode if it's currently enabled.
  void DisableBackgroundMode();

  // Turns on background mode if it's currently disabled.
  void EnableBackgroundMode();

  // Returns true if background mode is permanently disabled for this chrome
  // session.
  static bool IsBackgroundModePermanentlyDisabled(
      const CommandLine* command_line);

  // Registrars for managing our change observers.
  NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_registrar_;

  // The profile-keyed data for this background mode manager. Keyed on profile.
  std::map<Profile*, BackgroundModeInfo> background_mode_data_;

  // Reference to our status tray. If null, the platform doesn't support status
  // icons.
  StatusTray* status_tray_;

  // The number of background apps currently loaded. This is the total over
  // all profiles.
  int background_app_count_;

  // Set to true when we are running in background mode. Allows us to track our
  // current background state so we can take the appropriate action when the
  // user disables/enables background mode via preferences.
  bool in_background_mode_;

  // Set when we are keeping chrome running during the startup process - this
  // is required when running with the --no-startup-window flag, as otherwise
  // chrome would immediately exit due to having no open windows.
  bool keep_alive_for_startup_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundModeManager);
};

#endif  // CHROME_BROWSER_BACKGROUND_MODE_MANAGER_H_
