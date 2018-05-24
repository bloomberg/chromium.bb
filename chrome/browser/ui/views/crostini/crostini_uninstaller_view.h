// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_UNINSTALLER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_UNINSTALLER_VIEW_H_

#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
class ProgressBar;
}  // namespace views

namespace crostini {
enum class ConciergeClientResult;
}  // namespace crostini

class Profile;

// The Crostini uninstaller. Provides a warning to the user and
// uninstalls Crostinin if the user chooses to do so.
class CrostiniUninstallerView : public views::DialogDelegateView {
 public:
  static void Show(Profile* profile);

  // views::DialogDelegateView:
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  bool Accept() override;
  bool Cancel() override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  enum class UninstallResult;
  enum class State {
    PROMPT,  // Prompting the user to allow uninstallation.
    ERROR,   // Something unexpected happened.
    UNINSTALLING,
  };

  explicit CrostiniUninstallerView(Profile* profile);
  ~CrostiniUninstallerView() override;

  void HandleError(const base::string16& error_message);
  void UninstallCrostiniFinished(crostini::ConciergeClientResult result);
  void RecordUninstallResultHistogram(UninstallResult result);

  State state_ = State::PROMPT;
  views::Label* message_label_ = nullptr;
  views::ProgressBar* progress_bar_ = nullptr;

  bool has_logged_result_ = false;
  base::string16 app_name_;
  Profile* profile_;

  base::WeakPtrFactory<CrostiniUninstallerView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniUninstallerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_UNINSTALLER_VIEW_H_
