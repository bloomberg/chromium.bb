// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_VIEWS_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;

namespace extensions {
class Extension;
}
namespace views {
class TabbedPane;
}

// View the information about a particular chrome application.
class AppInfoDialog : public views::DialogDelegateView {
 public:
  AppInfoDialog(gfx::NativeWindow parent_window,
                Profile* profile,
                const extensions::Extension* app,
                const base::Closure& close_callback);

  virtual ~AppInfoDialog();

 private:
  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from views::DialogDelegate:
  virtual bool Cancel() OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual ui::ModalType GetModalType() const OVERRIDE;

  gfx::NativeWindow parent_window_;
  Profile* profile_;
  const extensions::Extension* app_;
  base::Closure close_callback_;

  DISALLOW_COPY_AND_ASSIGN(AppInfoDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_VIEWS_H_
