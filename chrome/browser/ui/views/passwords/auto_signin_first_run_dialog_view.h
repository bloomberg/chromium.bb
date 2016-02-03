// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_AUTO_SIGNIN_FIRST_RUN_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_AUTO_SIGNIN_FIRST_RUN_DIALOG_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/window/dialog_delegate.h"

class AutoSigninFirstRunDialogView : public views::DialogDelegateView,
                                     public views::StyledLabelListener,
                                     public views::ButtonListener,
                                     public AutoSigninFirstRunPrompt {
 public:
  AutoSigninFirstRunDialogView(PasswordDialogController* controller,
                               content::WebContents* web_contents);
  ~AutoSigninFirstRunDialogView() override;

  // AutoSigninFirstRunPrompt:
  void ShowAutoSigninPrompt() override;
  void ControllerGone() override;

 private:
  // views::WidgetDelegate:
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  views::View* GetInitiallyFocusedView() override;

  // views::DialogDelegate:
  int GetDialogButtons() const override;
  void OnClosed() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // Sets up the child views.
  void InitWindow();

  views::LabelButton* ok_button_;
  views::LabelButton* turn_off_button_;

  // A weak pointer to the controller.
  PasswordDialogController* controller_;
  content::WebContents* const web_contents_;

  DISALLOW_COPY_AND_ASSIGN(AutoSigninFirstRunDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_AUTO_SIGNIN_FIRST_RUN_DIALOG_VIEW_H_
