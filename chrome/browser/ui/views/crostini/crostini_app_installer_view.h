// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_APP_INSTALLER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_APP_INSTALLER_VIEW_H_

#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;

// TODO(https://crbug.com/921429): Whole dialog needs a UX spec, this
// iteration is based on some UX recommendations for this feature, but
// a full spec will be required before bringing this feature to live.

// The Crostini app installer view. Provides information about the app that a
// user is looking to install and confirms if they want to install it or not.
class CrostiniAppInstallerView : public views::DialogDelegateView {
 public:
  CrostiniAppInstallerView(Profile* profile,
                           const crostini::LinuxPackageInfo& package_info);

  // views::DialogDelegateView:
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  bool Accept() override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  ~CrostiniAppInstallerView() override;

  Profile* profile_;
  crostini::LinuxPackageInfo package_info_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniAppInstallerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_APP_INSTALLER_VIEW_H_
