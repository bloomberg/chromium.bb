// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_CONTROLLER_DELEGATE_H_

#include <stdint.h>

#include <string>

#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/constants.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace extensions {
class Extension;
class ExtensionSet;
class InstallTracker;
}

namespace gfx {
class Rect;
}

// Interface to allow the view delegate to call out to whatever is controlling
// the app list. This will have different implementations for different
// platforms.
class AppListControllerDelegate {
 public:
  // Indicates the source of an app list activation, for tracking purposes.
  enum AppListSource {
    LAUNCH_FROM_UNKNOWN,
    LAUNCH_FROM_APP_LIST,
    LAUNCH_FROM_APP_LIST_SEARCH
  };

  // Whether apps can be pinned, and whether pinned apps are editable or fixed.
  // TODO(khmel): Find better home for Pinnable enum.
  enum Pinnable {
    NO_PIN,
    PIN_EDITABLE,
    PIN_FIXED
  };

  virtual ~AppListControllerDelegate();

  // Dismisses the view.
  virtual void DismissView() = 0;

  // Handles the view being closed.
  virtual void ViewClosing();

  // Gets app list window.
  virtual gfx::NativeWindow GetAppListWindow() = 0;

  // Gets display ID of app list window.
  int64_t GetAppListDisplayId();

  // Gets the content bounds of the app info dialog of the app list in the
  // screen coordinates. On platforms that do not use views, this returns a 0x0
  // rectangle.
  virtual gfx::Rect GetAppInfoDialogBounds();

  // Control of pinning apps.
  virtual bool IsAppPinned(const std::string& app_id) = 0;
  virtual void PinApp(const std::string& app_id) = 0;
  virtual void UnpinApp(const std::string& app_id) = 0;
  virtual Pinnable GetPinnable(const std::string& app_id) = 0;

  // Returns true if requested app is open.
  virtual bool IsAppOpen(const std::string& app_id) const = 0;

  // Called before and after a dialog opens in the app list. For example,
  // displays an overlay that disables the app list while the dialog is open.
  virtual void OnShowChildDialog();
  virtual void OnCloseChildDialog();

  // Whether the controller supports a Show App Info flow.
  virtual bool CanDoShowAppInfoFlow();

  // Show the dialog with the application's information. Call only if
  // CanDoShowAppInfoFlow() returns true.
  virtual void DoShowAppInfoFlow(Profile* profile,
                                 const std::string& extension_id);

  // Handle the "create window" context menu items of Chrome App.
  // |incognito| is true to create an incognito window.
  virtual void CreateNewWindow(Profile* profile, bool incognito) = 0;

  // Opens the URL.
  virtual void OpenURL(Profile* profile,
                       const GURL& url,
                       ui::PageTransition transition,
                       WindowOpenDisposition disposition) = 0;

  // Show the app's most recent window, or launch it if it is not running.
  virtual void ActivateApp(Profile* profile,
                           const extensions::Extension* extension,
                           AppListSource source,
                           int event_flags) = 0;

  // Launch the app on the display identified by |display_id|.
  virtual void LaunchApp(Profile* profile,
                         const extensions::Extension* extension,
                         AppListSource source,
                         int event_flags,
                         int64_t display_id) = 0;

  static std::string AppListSourceToString(AppListSource source);

  // True if the user has permission to modify the given app's settings.
  bool UserMayModifySettings(Profile* profile, const std::string& app_id);

  // Uninstall the app identified by |app_id| from |profile|.
  void UninstallApp(Profile* profile, const std::string& app_id);

  // True if the app was installed from the web store.
  bool IsAppFromWebStore(Profile* profile,
                         const std::string& app_id);

  // Shows the user the webstore site for the given app.
  void ShowAppInWebStore(Profile* profile,
                         const std::string& app_id,
                         bool is_search_result);

  // True if the given extension has an options page.
  bool HasOptionsPage(Profile* profile, const std::string& app_id);

  // Shows the user the options page for the app.
  void ShowOptionsPage(Profile* profile, const std::string& app_id);

  // Gets/sets the launch type for an app.
  // The launch type specifies whether a hosted app should launch as a separate
  // window, fullscreened or as a tab.
  extensions::LaunchType GetExtensionLaunchType(
      Profile* profile, const std::string& app_id);
  virtual void SetExtensionLaunchType(
      Profile* profile,
      const std::string& extension_id,
      extensions::LaunchType launch_type);

  // Returns true if the given extension is installed.
  virtual bool IsExtensionInstalled(Profile* profile,
                                    const std::string& app_id);

  extensions::InstallTracker* GetInstallTrackerFor(Profile* profile);

  // Get the list of installed apps for the given profile.
  void GetApps(Profile* profile, extensions::ExtensionSet* out_apps);

  // Called when a search is started using the app list search box.
  void OnSearchStarted();
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_CONTROLLER_DELEGATE_H_
