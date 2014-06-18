// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_VIEWS_H_

#include "ui/views/view.h"

class Profile;

namespace extensions {
class Extension;
}

// View the information about a particular chrome application.

class AppInfoDialog : public views::View {
 public:
  AppInfoDialog(Profile* profile, const extensions::Extension* app);
  virtual ~AppInfoDialog();

 private:
  DISALLOW_COPY_AND_ASSIGN(AppInfoDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_VIEWS_H_
