// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_PANEL_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_PANEL_H_

#include "ui/views/view.h"

class Profile;

namespace extensions {
class Extension;
}
namespace views {
class Label;
}

// A piece of the App Info dialog that displays information for a particular
// profile and app. Panels in the App Info dialog extend this class.
class AppInfoPanel : public views::View {
 public:
  AppInfoPanel(Profile* profile, const extensions::Extension* app);

  virtual ~AppInfoPanel();

 protected:
  // Create a heading label with the given text.
  views::Label* CreateHeading(const base::string16& text) const;

  // Create a view with a vertically-stacked box layout, which can have child
  // views appended to it.
  views::View* CreateVerticalStack() const;

  Profile* profile_;
  const extensions::Extension* app_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppInfoPanel);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_PANEL_H_
