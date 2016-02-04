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
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons.h"

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

class ModalSigninDelegateMac;

@interface NavigationButtonClickedHandler : NSObject {
 @private
  SigninViewControllerDelegateMac* delegate_;
}
- (id)initWithModalSigninDelegate:(SigninViewControllerDelegateMac*)delegate;
- (void)buttonClicked:(id)sender;
@end

@implementation NavigationButtonClickedHandler

- (id)initWithModalSigninDelegate:(SigninViewControllerDelegateMac*)delegate {
  if (self = [super init]) {
    delegate_ = delegate;
  }

  return self;
}

- (void)buttonClicked:(id)sender {
  delegate_->ButtonClicked();
}

@end

SigninViewControllerDelegateMac::SigninViewControllerDelegateMac(
    SigninViewController* signin_view_controller,
    scoped_ptr<content::WebContents> web_contents,
    content::WebContents* host_web_contents,
    NSRect frame)
    : SigninViewControllerDelegate(signin_view_controller, web_contents.get()),
      host_view_([[NSView alloc] initWithFrame:frame]),
      web_contents_(std::move(web_contents)),
      window_(
          [[ConstrainedWindowCustomWindow alloc] initWithContentRect:frame]) {
  window_.get().contentView = host_view_;
  [host_view_ addSubview:web_contents_->GetNativeView()];

  base::scoped_nsobject<CustomConstrainedWindowSheet> sheet(
      [[CustomConstrainedWindowSheet alloc] initWithCustomWindow:window_]);
  constrained_window_.reset(
      new ConstrainedWindowMac(this, host_web_contents, sheet));
}

SigninViewControllerDelegateMac::~SigninViewControllerDelegateMac() {}

SigninViewControllerDelegateMac*
SigninViewControllerDelegateMac::CreateModalSigninDelegateWithNavigation(
    SigninViewController* signin_view_controller,
    scoped_ptr<content::WebContents> web_contents,
    content::WebContents* host_web_contents,
    NSRect frame) {
  SigninViewControllerDelegateMac* delegate =
      new SigninViewControllerDelegateMac(signin_view_controller,
                                          std::move(web_contents),
                                          host_web_contents, frame);
  delegate->AddNavigationButton();
  return delegate;
}

void SigninViewControllerDelegateMac::ButtonClicked() {
  NavigationButtonClicked(web_contents_.get());
}

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

void SigninViewControllerDelegateMac::ShowBackArrow() {
  gfx::ImageSkia image = gfx::CreateVectorIcon(gfx::VectorIconId::NAVIGATE_BACK,
                                               16, SK_ColorWHITE);
  NSImage* converted_image = NSImageFromImageSkia(image);
  [back_button_ setImage:converted_image];
  [back_button_ setNeedsDisplay:YES];
}

void SigninViewControllerDelegateMac::ShowCloseButton() {
  gfx::ImageSkia image = gfx::CreateVectorIcon(gfx::VectorIconId::NAVIGATE_STOP,
                                               16, SK_ColorWHITE);
  NSImage* converted_image = NSImageFromImageSkia(image);
  [back_button_ setImage:converted_image];
  [back_button_ setNeedsDisplay:YES];
}

void SigninViewControllerDelegateMac::PerformClose() {
  constrained_window_->CloseWebContentsModalDialog();
}

void SigninViewControllerDelegateMac::AddNavigationButton() {
  NSRect button_frame = NSMakeRect(16, kFixedGaiaViewHeight - 32, 16, 16);
  back_button_.reset([[NSButton alloc] initWithFrame:button_frame]);
  [back_button_ setImagePosition:NSImageOnly];
  [back_button_ setBordered:NO];
  [back_button_ setButtonType:NSMomentaryChangeButton];
  ShowCloseButton();

  handler_.reset([[NavigationButtonClickedHandler alloc]
      initWithModalSigninDelegate:this]);
  [back_button_ setTarget:handler_];
  [back_button_ setAction:@selector(buttonClicked:)];

  // NSView clipping/drawing/invalidating is undefined between overlapping
  // views if one is layer-backed and the other is not. Since we want to
  // overlap an NSView on top of what is ultimately a
  // RenderWidgetHostViewCocoa, which itself is layer-backed, we must request
  // that our back button be layer-backed as well.
  [back_button_ setWantsLayer:YES];

  [host_view_ addSubview:back_button_];
}

// static
SigninViewControllerDelegate*
SigninViewControllerDelegate::CreateModalSigninDelegate(
    SigninViewController* signin_view_controller,
    profiles::BubbleViewMode mode,
    Browser* browser,
    signin_metrics::AccessPoint access_point) {
  return SigninViewControllerDelegateMac::
      CreateModalSigninDelegateWithNavigation(
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
