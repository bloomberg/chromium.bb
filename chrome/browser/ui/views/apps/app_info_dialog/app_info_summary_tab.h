// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_TAB_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_TAB_H_

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_tab.h"
#include "chrome/common/extensions/extension_constants.h"
#include "ui/base/models/combobox_model.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/link_listener.h"

class Profile;

namespace extensions {
class Extension;
}
namespace gfx {
class Image;
}
namespace views {
class Combobox;
class ImageView;
}

class LaunchOptionsComboboxModel;

// The Summary tab of the app info dialog, which provides basic information and
// controls related to the app.
class AppInfoSummaryTab : public AppInfoTab,
                          public views::LinkListener,
                          public views::ComboboxListener,
                          public base::SupportsWeakPtr<AppInfoSummaryTab> {
 public:
  AppInfoSummaryTab(gfx::NativeWindow parent_window,
                    Profile* profile,
                    const extensions::Extension* app,
                    const base::Closure& close_callback);

  virtual ~AppInfoSummaryTab();

  // Overridden from views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

 private:
  // Overridden from views::ComboboxListener:
  virtual void OnPerformAction(views::Combobox* combobox) OVERRIDE;

  // Load the app icon asynchronously. For the response, check OnAppImageLoaded.
  void LoadAppImageAsync();
  // Called when the app's icon is loaded.
  void OnAppImageLoaded(const gfx::Image& image);

  // Returns the launch type of the app (e.g. pinned tab, fullscreen, etc).
  extensions::LaunchType GetLaunchType() const;
  // Sets the launch type of the app to the given type. Must only be called if
  // CanSetLaunchType() returns true.
  void SetLaunchType(extensions::LaunchType);
  bool CanSetLaunchType();

  // Opens the app in the web store. Only call if CanShowAppInWebStore() returns
  // true.
  virtual void ShowAppInWebStore() const;
  virtual bool CanShowAppInWebStore() const;

  // UI elements on the dialog.
  views::ImageView* app_icon_;
  views::Link* view_in_store_link_;

  scoped_ptr<LaunchOptionsComboboxModel> launch_options_combobox_model_;
  views::Combobox* launch_options_combobox_;

  base::WeakPtrFactory<AppInfoSummaryTab> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppInfoSummaryTab);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_TAB_H_
