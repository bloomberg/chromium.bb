// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_TAB_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_TAB_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/views/view.h"

class AppInfoView;
class Profile;

namespace extensions {
class Extension;
}
namespace views {
class View;
}

// A tab in the App Info dialog that displays information for a particular
// profile and app. Tabs in the App Info dialog extend this class.
class AppInfoTab : public views::View {
 public:
  AppInfoTab(gfx::NativeWindow parent_window,
             Profile* profile,
             const extensions::Extension* app,
             const base::Closure& close_callback);

  virtual ~AppInfoTab();

 protected:
  gfx::NativeWindow parent_window_;
  Profile* profile_;
  const extensions::Extension* app_;
  const base::Closure& close_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppInfoTab);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_TAB_H_
