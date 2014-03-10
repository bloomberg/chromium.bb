// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_TAB_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_TAB_H_

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_tab.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace extensions {
class Extension;
}
namespace gfx {
class Image;
}
namespace views {
class ImageView;
}

// The Summary tab of the app info dialog, which provides basic information and
// controls related to the app.
class AppInfoSummaryTab : public AppInfoTab,
                          public base::SupportsWeakPtr<AppInfoSummaryTab> {
 public:
  AppInfoSummaryTab(gfx::NativeWindow parent_window,
                    Profile* profile,
                    const extensions::Extension* app,
                    const base::Closure& close_callback);

  virtual ~AppInfoSummaryTab();

 private:
  // Load the app icon asynchronously. For the response, check OnAppImageLoaded.
  virtual void LoadAppImageAsync();
  // Called when the app's icon is loaded.
  virtual void OnAppImageLoaded(const gfx::Image& image);

  // UI elements on the dialog.
  views::ImageView* app_icon_;

  base::WeakPtrFactory<AppInfoSummaryTab> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppInfoSummaryTab);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_TAB_H_
