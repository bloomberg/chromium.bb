// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_VIEWS_H_

#include "base/macros.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;
class HostView;
class Profile;

namespace content {
class WebContentsDelegate;
}

namespace signin_metrics {
enum class AccessPoint;
}

namespace views {
class WebView;
}

// Views implementation of SigninViewControllerDelegate. It's responsible for
// managing the Signin and Sync Confirmation tab-modal dialogs.
// Instances of this class delete themselves when the window they're managing
// closes (in the DeleteDelegate callback).
class SigninViewControllerDelegateViews : public views::DialogDelegateView,
                                          public views::ButtonListener,
                                          public SigninViewControllerDelegate {
 public:
  SigninViewControllerDelegateViews(
      SigninViewController* signin_view_controller,
      views::WebView* content_view,
      Browser* browser);

  // Creates the web view that contains the signin flow in |mode| using
  // |profile| as the web content's profile, then sets |delegate| as the created
  // web content's delegate.
  static views::WebView* CreateGaiaWebView(
      content::WebContentsDelegate* delegate,
      profiles::BubbleViewMode mode,
      Profile* profile,
      signin_metrics::AccessPoint access_point);

  static views::WebView* CreateSyncConfirmationWebView(Profile* profile);

  // views::DialogDelegateView:
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  void DeleteDelegate() override;
  ui::ModalType GetModalType() const override;
  bool ShouldShowCloseButton() const override;
  int GetDialogButtons() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  void ShowBackArrow() override;
  void ShowCloseButton() override;
  void PerformClose() override;

  ~SigninViewControllerDelegateViews() override;

  views::WebView* content_view_;
  HostView* host_view_;
  views::Widget* modal_signin_widget_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(SigninViewControllerDelegateViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_VIEWS_H_
