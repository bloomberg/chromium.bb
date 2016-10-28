// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/signin_view_controller_delegate_views.h"

#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/url_constants.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace {

const int kPasswordCombinedFixedGaiaViewHeight = 440;
const int kPasswordCombinedFixedGaiaViewWidth = 360;
const int kFixedGaiaViewHeight = 612;
const int kModalDialogWidth = 448;
const int kSyncConfirmationDialogHeight = 487;
const int kSigninErrorDialogHeight = 164;

int GetSyncConfirmationDialogPreferredHeight(Profile* profile) {
  // If sync is disabled, then the sync confirmation dialog looks like an error
  // dialog and thus it has the same preferred size.
  return profile->IsSyncAllowed() ? kSyncConfirmationDialogHeight
                                  : kSigninErrorDialogHeight;
}

}  // namespace

SigninViewControllerDelegateViews::SigninViewControllerDelegateViews(
    SigninViewController* signin_view_controller,
    std::unique_ptr<views::WebView> content_view,
    Browser* browser,
    bool wait_for_size)
    : SigninViewControllerDelegate(signin_view_controller,
                                   content_view->GetWebContents()),
      content_view_(content_view.release()),
      modal_signin_widget_(nullptr),
      wait_for_size_(wait_for_size),
      browser_(browser) {
  if (!wait_for_size_)
    DisplayModal();
}

SigninViewControllerDelegateViews::~SigninViewControllerDelegateViews() {}

// views::DialogDelegateView:
views::View* SigninViewControllerDelegateViews::GetContentsView() {
  return content_view_;
}

views::Widget* SigninViewControllerDelegateViews::GetWidget() {
  return content_view_->GetWidget();
}

const views::Widget* SigninViewControllerDelegateViews::GetWidget() const {
  return content_view_->GetWidget();
}

void SigninViewControllerDelegateViews::DeleteDelegate() {
  ResetSigninViewControllerDelegate();
  delete this;
}

ui::ModalType SigninViewControllerDelegateViews::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

bool SigninViewControllerDelegateViews::ShouldShowCloseButton() const {
  return false;
}

int SigninViewControllerDelegateViews::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void SigninViewControllerDelegateViews::PerformClose() {
  if (modal_signin_widget_)
    modal_signin_widget_->Close();
}

void SigninViewControllerDelegateViews::ResizeNativeView(int height) {
  int max_height = browser_
      ->window()
      ->GetWebContentsModalDialogHost()
      ->GetMaximumDialogSize().height();
  content_view_->SetPreferredSize(
      gfx::Size(kModalDialogWidth, std::min(height, max_height)));
  content_view_->Layout();

  if (wait_for_size_) {
    // The modal wasn't displayed yet so just show it with the already resized
    // view.
    DisplayModal();
  }
}

void SigninViewControllerDelegateViews::DisplayModal() {
  modal_signin_widget_ = constrained_window::ShowWebModalDialogViews(
      this, browser_->tab_strip_model()->GetActiveWebContents());
  content_view_->RequestFocus();
}

// static
std::unique_ptr<views::WebView>
SigninViewControllerDelegateViews::CreateGaiaWebView(
    content::WebContentsDelegate* delegate,
    profiles::BubbleViewMode mode,
    Browser* browser,
    signin_metrics::AccessPoint access_point) {
  GURL url =
      signin::GetSigninURLFromBubbleViewMode(
          browser->profile(), mode, access_point);

  int max_height = browser
      ->window()
      ->GetWebContentsModalDialogHost()
      ->GetMaximumDialogSize().height();
  // Adds Gaia signin webview.
  const gfx::Size pref_size =
      switches::UsePasswordSeparatedSigninFlow()
          ? gfx::Size(kModalDialogWidth,
                      std::min(kFixedGaiaViewHeight, max_height))
          : gfx::Size(kPasswordCombinedFixedGaiaViewWidth,
                      kPasswordCombinedFixedGaiaViewHeight);
  views::WebView* web_view = new views::WebView(browser->profile());
  web_view->LoadInitialURL(url);

  if (delegate)
    web_view->GetWebContents()->SetDelegate(delegate);

  web_view->SetPreferredSize(pref_size);
  content::RenderWidgetHostView* rwhv =
      web_view->GetWebContents()->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetBackgroundColor(profiles::kAvatarBubbleGaiaBackgroundColor);

  return std::unique_ptr<views::WebView>(web_view);
}

std::unique_ptr<views::WebView>
SigninViewControllerDelegateViews::CreateSyncConfirmationWebView(
    Browser* browser) {
  views::WebView* web_view = new views::WebView(browser->profile());
  web_view->LoadInitialURL(GURL(chrome::kChromeUISyncConfirmationURL));

  int dialog_preferred_height =
      GetSyncConfirmationDialogPreferredHeight(browser->profile());
  int max_height = browser
      ->window()
      ->GetWebContentsModalDialogHost()
      ->GetMaximumDialogSize().height();
  web_view->SetPreferredSize(gfx::Size(
      kModalDialogWidth, std::min(dialog_preferred_height, max_height)));

  return std::unique_ptr<views::WebView>(web_view);
}

std::unique_ptr<views::WebView>
SigninViewControllerDelegateViews::CreateSigninErrorWebView(Browser* browser) {
  views::WebView* web_view = new views::WebView(browser->profile());
  web_view->LoadInitialURL(GURL(chrome::kChromeUISigninErrorURL));

  int max_height = browser->window()
                       ->GetWebContentsModalDialogHost()
                       ->GetMaximumDialogSize()
                       .height();
  web_view->SetPreferredSize(gfx::Size(
      kModalDialogWidth, std::min(kSigninErrorDialogHeight, max_height)));

  return std::unique_ptr<views::WebView>(web_view);
}

SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateModalSigninDelegate(
    SigninViewController* signin_view_controller,
    profiles::BubbleViewMode mode,
    Browser* browser,
    signin_metrics::AccessPoint access_point) {
  return new SigninViewControllerDelegateViews(
      signin_view_controller,
      SigninViewControllerDelegateViews::CreateGaiaWebView(
          nullptr, mode, browser, access_point),
      browser, false);
}

SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateSyncConfirmationDelegate(
    SigninViewController* signin_view_controller,
    Browser* browser) {
  return new SigninViewControllerDelegateViews(
      signin_view_controller,
      SigninViewControllerDelegateViews::CreateSyncConfirmationWebView(browser),
      browser, true);
}

SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateSigninErrorDelegate(
    SigninViewController* signin_view_controller,
    Browser* browser) {
  return new SigninViewControllerDelegateViews(
      signin_view_controller,
      SigninViewControllerDelegateViews::CreateSigninErrorWebView(browser),
      browser, true);
}
