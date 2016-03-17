// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/profiles/signin_view_controller_delegate_mac.h"

#import <Cocoa/Cocoa.h>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

namespace {

// Dimensions of the web contents containing the old-style signin flow with the
// username and password challenge on the same form.
const CGFloat kPasswordCombinedFixedGaiaViewHeight = 440;
const CGFloat kPasswordCombinedFixedGaiaViewWidth = 360;

// Dimensions of the tab-modal dialog displaying the password-separated signin
// flow. These match the dimensions of the server content they display.
const CGFloat kFixedGaiaViewHeight = 512;
const CGFloat kFixedGaiaViewWidth = 448;

// Dimensions of the sync confirmation tab-modal dialog. These are taken from
// the design spec for this feature.
const int kSyncConfirmationDialogWidth = 448;
const int kSyncConfirmationDialogHeight = 351;

}  // namespace

SigninViewControllerDelegateMac::SigninViewControllerDelegateMac(
    SigninViewController* signin_view_controller,
    scoped_ptr<content::WebContents> web_contents,
    content::WebContents* host_web_contents,
    NSRect frame)
    : SigninViewControllerDelegate(signin_view_controller, web_contents.get()),
      web_contents_(std::move(web_contents)),
      window_(
          [[ConstrainedWindowCustomWindow alloc] initWithContentRect:frame]) {
  window_.get().contentView = web_contents_->GetNativeView();

  base::scoped_nsobject<CustomConstrainedWindowSheet> sheet(
      [[CustomConstrainedWindowSheet alloc] initWithCustomWindow:window_]);
  constrained_window_ =
      CreateAndShowWebModalDialogMac(this, host_web_contents, sheet);
}

SigninViewControllerDelegateMac::~SigninViewControllerDelegateMac() {}

void SigninViewControllerDelegateMac::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  ResetSigninViewControllerDelegate();
  delete this;
}

// static
scoped_ptr<content::WebContents>
SigninViewControllerDelegateMac::CreateGaiaWebContents(
    content::WebContentsDelegate* delegate,
    profiles::BubbleViewMode mode,
    Profile* profile,
    signin_metrics::AccessPoint access_point) {
  GURL url =
      signin::GetSigninURLFromBubbleViewMode(profile, mode, access_point);

  scoped_ptr<content::WebContents> web_contents(content::WebContents::Create(
      content::WebContents::CreateParams(profile)));

  if (delegate)
    web_contents->SetDelegate(delegate);

  web_contents->GetController().LoadURL(url, content::Referrer(),
                                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                        std::string());
  NSView* webview = web_contents->GetNativeView();
  [webview
      setFrameSize:switches::UsePasswordSeparatedSigninFlow()
                       ? NSMakeSize(kFixedGaiaViewWidth, kFixedGaiaViewHeight)
                       : NSMakeSize(kPasswordCombinedFixedGaiaViewWidth,
                                    kPasswordCombinedFixedGaiaViewHeight)];

  content::RenderWidgetHostView* rwhv = web_contents->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetBackgroundColor(profiles::kAvatarBubbleGaiaBackgroundColor);

  return web_contents;
}

// static
scoped_ptr<content::WebContents>
SigninViewControllerDelegateMac::CreateSyncConfirmationWebContents(
    Profile* profile) {
  scoped_ptr<content::WebContents> web_contents(content::WebContents::Create(
      content::WebContents::CreateParams(profile)));
  web_contents->GetController().LoadURL(
      GURL(chrome::kChromeUISyncConfirmationURL), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());

  NSView* webview = web_contents->GetNativeView();
  [webview setFrameSize:NSMakeSize(kSyncConfirmationDialogWidth,
                                   kSyncConfirmationDialogHeight)];

  return web_contents;
}

void SigninViewControllerDelegateMac::PerformClose() {
  constrained_window_->CloseWebContentsModalDialog();
}

// static
SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateModalSigninDelegate(
    SigninViewController* signin_view_controller,
    profiles::BubbleViewMode mode,
    Browser* browser,
    signin_metrics::AccessPoint access_point) {
  return new SigninViewControllerDelegateMac(
          signin_view_controller,
          SigninViewControllerDelegateMac::CreateGaiaWebContents(
              nullptr, mode, browser->profile(), access_point),
          browser->tab_strip_model()->GetActiveWebContents(),
          NSMakeRect(0, 0, kFixedGaiaViewWidth, kFixedGaiaViewHeight));
}

// static
SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateSyncConfirmationDelegate(
    SigninViewController* signin_view_controller,
    Browser* browser) {
  return new SigninViewControllerDelegateMac(
      signin_view_controller,
      SigninViewControllerDelegateMac::CreateSyncConfirmationWebContents(
          browser->profile()),
      browser->tab_strip_model()->GetActiveWebContents(),
      NSMakeRect(0, 0, kSyncConfirmationDialogWidth,
                 kSyncConfirmationDialogHeight));
}
