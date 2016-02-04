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
#include "chrome/common/url_constants.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

const int kPasswordCombinedFixedGaiaViewHeight = 440;
const int kPasswordCombinedFixedGaiaViewWidth = 360;
const int kFixedGaiaViewHeight = 512;
const int kFixedGaiaViewWidth = 448;
const int kNavigationButtonSize = 16;
const int kNavigationButtonOffset = 16;
const int kSyncConfirmationDialogWidth = 448;
const int kSyncConfirmationDialogHeight = 351;

// View that contains the signin web contents and the back/close overlay button.
class HostView : public views::View {
 public:
  HostView(views::View* contents, views::ButtonListener* button_listener)
      : contents_(contents),
        back_button_(new views::ImageButton(button_listener)) {
    back_button_->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                    views::ImageButton::ALIGN_MIDDLE);
    back_button_->SetFocusable(true);
    ShowCloseButton();
    AddChildView(contents_);
    SetLayoutManager(new views::FillLayout);
  }

  gfx::Size GetPreferredSize() const override {
    return contents_->GetPreferredSize();
  }

  void ShowCloseButton() {
    gfx::ImageSkia image = gfx::CreateVectorIcon(
        gfx::VectorIconId::NAVIGATE_STOP, kNavigationButtonSize, SK_ColorWHITE);
    back_button_->SetImage(views::Button::STATE_NORMAL, &image);
    back_button_->SchedulePaint();
  }

  void ShowBackArrow() {
    gfx::ImageSkia image = gfx::CreateVectorIcon(
        gfx::VectorIconId::NAVIGATE_BACK, kNavigationButtonSize, SK_ColorWHITE);
    back_button_->SetImage(views::Button::STATE_NORMAL, &image);
    back_button_->SchedulePaint();
  }

 private:
  // views::View:
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override {
    if (back_button_widget_ || !GetWidget())
      return;

    views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
    params.parent = GetWidget()->GetNativeView();
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

    back_button_widget_.reset(new views::Widget);
    back_button_widget_->Init(params);
    back_button_widget_->SetContentsView(back_button_);

    gfx::Rect bounds(back_button_->GetPreferredSize());
    back_button_->SetBoundsRect(bounds);
    bounds.Offset(kNavigationButtonOffset, kNavigationButtonOffset);
    back_button_widget_->SetBounds(bounds);
  }

  views::View* contents_;
  views::ImageButton* back_button_;
  scoped_ptr<views::Widget> back_button_widget_;
  DISALLOW_COPY_AND_ASSIGN(HostView);
};

SigninViewControllerDelegateViews::SigninViewControllerDelegateViews(
    SigninViewController* signin_view_controller,
    views::WebView* content_view,
    Browser* browser)
    : SigninViewControllerDelegate(signin_view_controller,
                                   content_view->GetWebContents()),
      content_view_(content_view),
      host_view_(new HostView(content_view_, this)),
      modal_signin_widget_(nullptr) {
  modal_signin_widget_ = constrained_window::ShowWebModalDialogViews(
      this, browser->tab_strip_model()->GetActiveWebContents());
}

SigninViewControllerDelegateViews::~SigninViewControllerDelegateViews() {}

// views::DialogDelegateView:
views::View* SigninViewControllerDelegateViews::GetContentsView() {
  return host_view_;
}

views::Widget* SigninViewControllerDelegateViews::GetWidget() {
  return host_view_->GetWidget();
}

const views::Widget* SigninViewControllerDelegateViews::GetWidget() const {
  return host_view_->GetWidget();
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

// views::ButtonListener:
void SigninViewControllerDelegateViews::ButtonPressed(views::Button* sender,
                                                      const ui::Event& event) {
  NavigationButtonClicked(content_view_->GetWebContents());
}

void SigninViewControllerDelegateViews::ShowBackArrow() {
  host_view_->ShowBackArrow();
}

void SigninViewControllerDelegateViews::ShowCloseButton() {
  host_view_->ShowCloseButton();
}

void SigninViewControllerDelegateViews::PerformClose() {
  modal_signin_widget_->Close();
}

// static
views::WebView* SigninViewControllerDelegateViews::CreateGaiaWebView(
    content::WebContentsDelegate* delegate,
    profiles::BubbleViewMode mode,
    Profile* profile,
    signin_metrics::AccessPoint access_point) {
  GURL url =
      signin::GetSigninURLFromBubbleViewMode(profile, mode, access_point);

  // Adds Gaia signin webview.
  const gfx::Size pref_size =
      switches::UsePasswordSeparatedSigninFlow()
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

views::WebView*
SigninViewControllerDelegateViews::CreateSyncConfirmationWebView(
    Profile* profile) {
  views::WebView* web_view = new views::WebView(profile);
  web_view->LoadInitialURL(GURL(chrome::kChromeUISyncConfirmationURL));
  web_view->SetPreferredSize(
      gfx::Size(kSyncConfirmationDialogWidth, kSyncConfirmationDialogHeight));

  return web_view;
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
          nullptr, mode, browser->profile(), access_point),
      browser);
}

SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateSyncConfirmationDelegate(
    SigninViewController* signin_view_controller,
    Browser* browser) {
  return new SigninViewControllerDelegateViews(
      signin_view_controller,
      SigninViewControllerDelegateViews::CreateSyncConfirmationWebView(
          browser->profile()),
      browser);
}
