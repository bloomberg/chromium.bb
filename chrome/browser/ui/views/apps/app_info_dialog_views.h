// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_VIEWS_H_

#include "base/compiler_specific.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;

namespace extensions {
class Extension;
}

namespace views {
class Label;
}

// View the information about a particular chrome application.
class AppInfoView : public views::DialogDelegateView {
 public:
  AppInfoView(Profile* profile,
              const extensions::Extension* app,
              const base::Closure& close_callback);

 private:
  virtual ~AppInfoView();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from views::DialogDelegate:
  virtual bool Cancel() OVERRIDE;
  virtual base::string16 GetDialogButtonLabel(ui::DialogButton button)
      const OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;

  // Profile in which the shortcuts will be created.
  Profile* profile_;

  // UI elements on the dialog.
  views::Label* app_name_label;
  views::Label* app_description_label;

  const extensions::Extension* app_;
  base::Closure close_callback_;

  DISALLOW_COPY_AND_ASSIGN(AppInfoView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_VIEWS_H_
