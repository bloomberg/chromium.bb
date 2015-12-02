// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/signin_view_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

const int kPasswordCombinedFixedGaiaViewHeight = 440;
const int kPasswordCombinedFixedGaiaViewWidth = 360;
const int kFixedGaiaViewHeight = 512;
const int kFixedGaiaViewWidth = 448;

class ModalSigninDelegate : public views::DialogDelegateView {
 public:
  ModalSigninDelegate(SigninViewController* signin_view_controller,
                      views::WebView* content_view,
                      Browser* browser)
      : content_view_(content_view),
        signin_view_controller_(signin_view_controller) {
    modal_signin_widget_ = constrained_window::ShowWebModalDialogViews(
        this, browser->tab_strip_model()->GetActiveWebContents());
  }

  void CloseModalSignin() {
    ResetSigninViewControllerDelegate();
    modal_signin_widget_->Close();
  }

  void ResetSigninViewControllerDelegate() {
    if (signin_view_controller_) {
      signin_view_controller_->ResetModalSigninDelegate();
      signin_view_controller_ = nullptr;
    }
  }

  // views::DialogDelegateView:
  views::View* GetContentsView() override {
    return content_view_;
  }

  views::Widget* GetWidget() override {
    return content_view_->GetWidget();
  }

  const views::Widget* GetWidget() const override {
    return content_view_->GetWidget();
  }

  void DeleteDelegate() override {
    ResetSigninViewControllerDelegate();
    delete this;
  }

  ui::ModalType GetModalType() const override {
    return ui::MODAL_TYPE_CHILD;
  }

  bool ShouldShowCloseButton() const override {
    return false;
  }

  int GetDialogButtons() const override {
    return ui::DIALOG_BUTTON_NONE;
  }

 private:
  views::WebView* content_view_;
  SigninViewController* signin_view_controller_;  // Not owned.
  views::Widget* modal_signin_widget_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(ModalSigninDelegate);
};

SigninViewController::SigninViewController()
    : modal_signin_delegate_(nullptr) {}

SigninViewController::~SigninViewController() {
  CloseModalSignin();
}

// static
views::WebView* SigninViewController::CreateGaiaWebView(
    content::WebContentsDelegate* delegate,
    profiles::BubbleViewMode mode,
    Profile* profile) {
  GURL url = signin::GetSigninURLFromBubbleViewMode(profile, mode);

  // Adds Gaia signin webview.
  const gfx::Size pref_size = switches::UsePasswordSeparatedSigninFlow()
      ? gfx::Size(kFixedGaiaViewWidth, kFixedGaiaViewHeight)
      : gfx::Size(kPasswordCombinedFixedGaiaViewWidth,
                  kPasswordCombinedFixedGaiaViewHeight);
  views::WebView* web_view = new views::WebView(profile);
  web_view->LoadInitialURL(url);

  if (delegate)
    web_view->GetWebContents()->SetDelegate(delegate);

  web_view->SetPreferredSize(pref_size);
  content::RenderWidgetHostView* rwhv =
      web_view->GetWebContents()->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetBackgroundColor(profiles::kAvatarBubbleGaiaBackgroundColor);

  return web_view;
}

void SigninViewController::ShowModalSignin(
    profiles::BubbleViewMode mode, Browser* browser) {
  CloseModalSignin();
  // The delegate will delete itself on request of the views code when the
  // widget is closed.
  modal_signin_delegate_ = new ModalSigninDelegate(
    this, CreateGaiaWebView(nullptr, mode, browser->profile()), browser);
}

void SigninViewController::CloseModalSignin() {
  if (modal_signin_delegate_)
    modal_signin_delegate_->CloseModalSignin();
  DCHECK(!modal_signin_delegate_);
}

void SigninViewController::ResetModalSigninDelegate() {
  modal_signin_delegate_ = nullptr;
}

// static
bool SigninViewController::ShouldShowModalSigninForMode(
    profiles::BubbleViewMode mode) {
  return switches::UsePasswordSeparatedSigninFlow() &&
        (mode == profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN ||
         mode == profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT ||
         mode == profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH);
}
