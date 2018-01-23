// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_PENDING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_PENDING_VIEW_H_

#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_delegate_view_base.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/view.h"

namespace views {
class Combobox;
class Label;
class StyledLabel;
class ToggleImageButton;
}  // namespace views

class DesktopIOSPromotionBubbleView;
class ManagePasswordSignInPromoView;

// A view offering the user the ability to save credentials. Contains a
// username and password field, along with a "Save Passwords" button and a
// "Never" button.
class ManagePasswordPendingView : public ManagePasswordsBubbleDelegateViewBase,
                                  public views::ButtonListener,
                                  public views::StyledLabelListener {
 public:
  ManagePasswordPendingView(content::WebContents* web_contents,
                            views::View* anchor_view,
                            const gfx::Point& anchor_point,
                            DisplayReason reason);

#if defined(UNIT_TEST)
  const View* username_field() const { return username_field_; }
#endif

 private:
  ~ManagePasswordPendingView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // ManagePasswordsBubbleDelegateViewBase:
  gfx::Size CalculatePreferredSize() const override;
  views::View* GetInitiallyFocusedView() override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  void AddedToWidget() override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;
  bool ShouldShowCloseButton() const override;
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;

  void CreateAndSetLayout(bool show_password_label);
  void CreatePasswordField();
  void TogglePasswordVisibility();
  void UpdateUsernameAndPasswordInModel();
  void ReplaceWithPromo();
  void UpdateTitleText(views::StyledLabel* title_view);

  // Different promo dialogs that helps the user get access to credentials
  // across devices. One of these are non-null when the promotion dialog is
  // active.
  ManagePasswordSignInPromoView* sign_in_promo_;
  DesktopIOSPromotionBubbleView* desktop_ios_promo_;

  views::View* username_field_;
  views::ToggleImageButton* password_view_button_;
  views::View* initially_focused_view_;

  // The view for the password value. Only one of |password_dropdown_| and
  // |password_label_| should be available.
  views::Combobox* password_dropdown_;
  views::Label* password_label_;

  bool are_passwords_revealed_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordPendingView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_PENDING_VIEW_H_
