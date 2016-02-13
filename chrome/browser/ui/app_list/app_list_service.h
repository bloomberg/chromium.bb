// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/native_widget_types.h"

class AppListControllerDelegate;
class PrefRegistrySimple;
class Profile;

namespace base {
class CommandLine;
class FilePath;
}

namespace content {
struct SpeechRecognitionSessionPreamble;
}

namespace gfx {
class ImageSkia;
}

class AppListService {
 public:
  // Source that triggers the app launcher being enabled. This is used for UMA
  // to track discoverability of the app lancher shortcut after install. Also
  // used to provide custom install behavior (e.g. "always" enable).
  enum AppListEnableSource {
    ENABLE_NOT_RECORDED,        // Indicates app launcher not recently enabled.
    ENABLE_FOR_APP_INSTALL,     // Triggered by a webstore packaged app install.
    ENABLE_VIA_WEBSTORE_LINK,   // Triggered by webstore explicitly via API.
    ENABLE_VIA_COMMAND_LINE,    // Triggered by --enable-app-list.
    ENABLE_ON_REINSTALL_UNUSED, // Triggered by Chrome reinstall finding pref.
                                // Unused since detecting a reinstall and
                                // detecting a pref are mutually exclusive.
    ENABLE_SHOWN_UNDISCOVERED,  // This overrides a prior ENABLE_FOR_APP_INSTALL
                                // when the launcher is auto-shown without
                                // being "discovered" beforehand.
    ENABLE_NUM_ENABLE_SOURCES
  };

  // Get the AppListService.
  static AppListService* Get();

  // Call Init for all AppListService instances on this platform.
  static void InitAll(Profile* initial_profile,
                      const base::FilePath& profile_path);

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Initializes the AppListService, and returns true if |command_line| is for
  // showing the app list.
  static bool HandleLaunchCommandLine(const base::CommandLine& command_line,
                                      Profile* launch_profile);

  // Indicates that |callback| should be called next time the app list is
  // painted.
  virtual void SetAppListNextPaintCallback(void (*callback)()) = 0;

  virtual base::FilePath GetProfilePath(
      const base::FilePath& user_data_dir) = 0;
  virtual void SetProfilePath(const base::FilePath& profile_path) = 0;

  // Show the app list for the profile configured in the user data dir for the
  // current browser process.
  virtual void Show() = 0;

  // Show the app list for the given profile. If it differs from the profile the
  // app list is currently showing, repopulate the app list and save the new
  // profile to local prefs as the default app list profile.
  virtual void ShowForProfile(Profile* requested_profile) = 0;

  // Shows the app list, and switches to voice search. Used by always-on
  // hotwording.
  virtual void ShowForVoiceSearch(
      Profile* profile,
      const scoped_refptr<content::SpeechRecognitionSessionPreamble>& preamble)
      = 0;

  // Shows the app list, and reveals the page that contains |extension_id|. This
  // should only be called for things that show in the app list, and only when
  // they begin or complete installing. If |start_discovery_tracking| is set,
  // the app launcher will not actually be shown, but will start tracking UMA
  // for app launcher discovery.
  virtual void ShowForAppInstall(Profile* profile,
                                 const std::string& extension_id,
                                 bool start_discovery_tracking) = 0;

  // Shows the app list, and switches to the custom launcher page.
  virtual void ShowForCustomLauncherPage(Profile* profile) = 0;

  // Hides the custom launcher page if it is currently being shown. Does nothing
  // otherwise.
  virtual void HideCustomLauncherPage() = 0;

  // Dismiss the app list.
  virtual void DismissAppList() = 0;

  // Get the profile the app list is currently showing.
  virtual Profile* GetCurrentAppListProfile() = 0;

  // Returns true if the app list is visible.
  virtual bool IsAppListVisible() const = 0;

  // Enable the app list. What this does specifically will depend on the host
  // operating system and shell.
  virtual void EnableAppList(Profile* initial_profile,
                             AppListEnableSource enable_source) = 0;

  // Get the window the app list is in, or NULL if the app list isn't visible.
  virtual gfx::NativeWindow GetAppListWindow() = 0;

  // Returns a pointer to the platform specific AppListControllerDelegate.
  virtual AppListControllerDelegate* GetControllerDelegate() = 0;

  // Create a platform-specific shortcut for the app list.
  virtual void CreateShortcut() = 0;

 protected:
  AppListService() {}
  virtual ~AppListService() {}

  // Do any once off initialization needed for the app list.
  virtual void Init(Profile* initial_profile) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListService);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_H_
