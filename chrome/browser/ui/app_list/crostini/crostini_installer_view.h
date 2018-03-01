// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_INSTALLER_VIEW_H_
#define CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_INSTALLER_VIEW_H_

#include "base/macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
class ProgressBar;
}  // namespace views

class CrostiniAppItem;

// The Crostini installer. Provides details about Crostini to the user and
// installs it if the user chooses to do so.
class CrostiniInstallerView : public views::DialogDelegateView {
 public:
  static void Show(const CrostiniAppItem* app_item);

  // views::DialogDelegateView:
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  bool Accept() override;
  gfx::Size CalculatePreferredSize() const override;

  static CrostiniInstallerView* GetActiveViewForTesting();

 private:
  explicit CrostiniInstallerView(const CrostiniAppItem* app_item);
  ~CrostiniInstallerView() override;

  enum class State {
    PROMPT,   // Prompting the user to allow installation.
    INSTALL,  // Installation is in progress.
  };
  State state_ = State::PROMPT;
  views::Label* message_label_ = nullptr;
  views::ProgressBar* progress_bar_ = nullptr;

  base::string16 app_name_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniInstallerView);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_INSTALLER_VIEW_H_
