// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_PERMISSIONS_TAB_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_PERMISSIONS_TAB_H_

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_tab.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/button/button.h"

class Profile;

namespace extensions {
class Extension;
class PermissionSet;
}
namespace ui {
class Event;
}
namespace views {
class LabelButton;
class ScrollView;
}

// The Permissions tab of the app info dialog, which provides insight and
// control over the app's various permissions.
class AppInfoPermissionsTab : public AppInfoTab, views::ButtonListener {
 public:
  AppInfoPermissionsTab(gfx::NativeWindow parent_window,
                        Profile* profile,
                        const extensions::Extension* app,
                        const base::Closure& close_callback);

  virtual ~AppInfoPermissionsTab();

 private:
  FRIEND_TEST_ALL_PREFIXES(AppInfoPermissionsTabTest,
                           NoPermissionsObtainedCorrectly);
  FRIEND_TEST_ALL_PREFIXES(AppInfoPermissionsTabTest,
                           RequiredPermissionsObtainedCorrectly);
  FRIEND_TEST_ALL_PREFIXES(AppInfoPermissionsTabTest,
                           OptionalPermissionsObtainedCorrectly);
  FRIEND_TEST_ALL_PREFIXES(AppInfoPermissionsTabTest,
                           RetainedFilePermissionsObtainedCorrectly);

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Clears all retained file permissions for this app, restarts the app and
  // closes this dialog.
  void RevokeFilePermissions();

  const extensions::PermissionSet* GetRequiredPermissions() const;
  const std::vector<base::string16> GetRequiredPermissionMessages() const;

  const extensions::PermissionSet* GetOptionalPermissions() const;
  const std::vector<base::string16> GetOptionalPermissionMessages() const;

  const std::vector<base::FilePath> GetRetainedFilePermissions() const;
  const std::vector<base::string16> GetRetainedFilePermissionMessages() const;

  views::ScrollView* scroll_view_;
  views::LabelButton* revoke_file_permissions_button_;

  DISALLOW_COPY_AND_ASSIGN(AppInfoPermissionsTab);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_PERMISSIONS_TAB_H_
