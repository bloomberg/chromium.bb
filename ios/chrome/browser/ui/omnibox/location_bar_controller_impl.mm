// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/omnibox/location_bar_controller_impl.h"

#import <UIKit/UIKit.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/security_state/core/security_state_ui.h"
#include "components/strings/grit/components_strings.h"
#include "components/toolbar/toolbar_model.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_delegate.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_popup_view_ios.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_view_ios.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/ssl_status.h"
#include "ios/web/public/web_state/web_state.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kClearTextButtonWidth = 28;
const CGFloat kClearTextButtonHeight = 28;

// Workaround for https://crbug.com/527084 . If there is connection
// information, always show the icon. Remove this once connection info
// is available via other UI: https://crbug.com/533581
bool DoesCurrentPageHaveCertInfo(web::WebState* webState) {
  if (!webState)
    return false;
  web::NavigationManager* navigationMangager = webState->GetNavigationManager();
  if (!navigationMangager)
    return false;
  web::NavigationItem* visibleItem = navigationMangager->GetVisibleItem();
  if (!visibleItem)
    return false;

  const web::SSLStatus& SSLStatus = visibleItem->GetSSL();
  // Evaluation of |security_style| SSLStatus field in WKWebView based app is an
  // asynchronous operation, so for a short period of time SSLStatus may have
  // non-null certificate and SECURITY_STYLE_UNKNOWN |security_style|.
  return SSLStatus.certificate &&
         SSLStatus.security_style != web::SECURITY_STYLE_UNKNOWN;
}

// Returns whether the |webState| is presenting an offline page.
bool IsCurrentPageOffline(web::WebState* webState) {
  if (!webState)
    return false;
  auto* navigationManager = webState->GetNavigationManager();
  auto* visibleItem = navigationManager->GetVisibleItem();
  if (!visibleItem)
    return false;
  const GURL& url = visibleItem->GetURL();
  return url.SchemeIs(kChromeUIScheme) && url.host() == kChromeUIOfflineHost;
}

}  // namespace

// An ObjC bridge class to allow taps on the clear button to be sent to a C++
// class.
@interface OmniboxClearButtonBridge : NSObject

- (instancetype)initWithOmniboxView:(OmniboxViewIOS*)omniboxView
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)clearText;

@end

@implementation OmniboxClearButtonBridge {
  OmniboxViewIOS* _omniboxView;
}

- (instancetype)initWithOmniboxView:(OmniboxViewIOS*)omniboxView {
  self = [super init];
  if (self) {
    _omniboxView = omniboxView;
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)clearText {
  _omniboxView->ClearText();
}

@end

// An ObjC bridge class to map between a UIControl action and the
// dispatcher command that displays the page info popup.
@interface PageInfoBridge : NSObject

// A method (in the form of a UIControl action) that invokes the command on the
// dispatcher to open the page info popup.
- (void)showPageInfoPopup:(id)sender;

// The dispatcher to which commands invoked by the bridge will be sent.
@property(nonatomic, weak) id<BrowserCommands> dispatcher;

@end

@implementation PageInfoBridge
@synthesize dispatcher = _dispatcher;

- (void)showPageInfoPopup:(id)sender {
  UIView* view = base::mac::ObjCCastStrict<UIView>(sender);
  CGPoint originPoint =
      CGPointMake(CGRectGetMidX(view.bounds), CGRectGetMidY(view.bounds));
  [self.dispatcher
      showPageInfoForOriginPoint:[view convertPoint:originPoint toView:nil]];
}

@end

LocationBarControllerImpl::LocationBarControllerImpl(
    OmniboxTextFieldIOS* field,
    ios::ChromeBrowserState* browser_state,
    id<LocationBarDelegate> delegate,
    id<BrowserCommands> dispatcher)
    : edit_view_(base::MakeUnique<OmniboxViewIOS>(field, this, browser_state)),
      field_(field),
      delegate_(delegate),
      dispatcher_(dispatcher) {
  DCHECK([delegate_ toolbarModel]);
  show_hint_text_ = true;

  InstallLocationIcon();
  CreateClearTextIcon(browser_state->IsOffTheRecord());
}

LocationBarControllerImpl::~LocationBarControllerImpl() {}

std::unique_ptr<OmniboxPopupViewIOS> LocationBarControllerImpl::CreatePopupView(
    id<OmniboxPopupPositioner> positioner) {
  std::unique_ptr<OmniboxPopupViewIOS> popup_view =
      base::MakeUnique<OmniboxPopupViewIOS>(edit_view_->browser_state(),
                                            edit_view_->model(),
                                            edit_view_.get(), positioner);

  edit_view_->model()->set_popup_model(popup_view->model());
  edit_view_->SetPopupProvider(popup_view.get());

  return popup_view;
}

void LocationBarControllerImpl::HideKeyboardAndEndEditing() {
  edit_view_->HideKeyboardAndEndEditing();
}

void LocationBarControllerImpl::SetShouldShowHintText(bool show_hint_text) {
  show_hint_text_ = show_hint_text;
}

const OmniboxView* LocationBarControllerImpl::GetLocationEntry() const {
  return edit_view_.get();
}

OmniboxView* LocationBarControllerImpl::GetLocationEntry() {
  return edit_view_.get();
}

void LocationBarControllerImpl::OnToolbarUpdated() {
  edit_view_->UpdateAppearance();
  OnChanged();
}

void LocationBarControllerImpl::OnAutocompleteAccept(
    const GURL& gurl,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    AutocompleteMatchType::Type type) {
  if (gurl.is_valid()) {
    transition = ui::PageTransitionFromInt(
        transition | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    [delegate_ loadGURLFromLocationBar:gurl transition:transition];
  }
}

void LocationBarControllerImpl::OnChanged() {
  const bool page_is_offline = IsCurrentPageOffline(GetWebState());
  const int resource_id = edit_view_->GetIcon(page_is_offline);
  [field_ setPlaceholderImage:resource_id];

  // TODO(rohitrao): Can we get focus information from somewhere other than the
  // model?
  if (!IsIPadIdiom() && !edit_view_->model()->has_focus()) {
    ToolbarModel* toolbarModel = [delegate_ toolbarModel];
    if (toolbarModel) {
      bool show_icon_for_state = security_state::ShouldAlwaysShowIcon(
          toolbarModel->GetSecurityLevel(false));
      bool page_has_downgraded_HTTPS =
          experimental_flags::IsPageIconForDowngradedHTTPSEnabled() &&
          DoesCurrentPageHaveCertInfo(GetWebState());
      if (show_icon_for_state || page_has_downgraded_HTTPS || page_is_offline) {
        [field_ showPlaceholderImage];
        is_showing_placeholder_while_collapsed_ = true;
      } else {
        [field_ hidePlaceholderImage];
        is_showing_placeholder_while_collapsed_ = false;
      }
    }
  }
  UpdateRightDecorations();

  NSString* placeholderText =
      show_hint_text_ ? l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT) : nil;
  [field_ setPlaceholder:placeholderText];
}

bool LocationBarControllerImpl::IsShowingPlaceholderWhileCollapsed() {
  return is_showing_placeholder_while_collapsed_;
}

void LocationBarControllerImpl::OnInputInProgress(bool in_progress) {
  if ([delegate_ toolbarModel])
    [delegate_ toolbarModel]->set_input_in_progress(in_progress);
  if (in_progress)
    [delegate_ locationBarBeganEdit];
}

void LocationBarControllerImpl::OnKillFocus() {
  // Hide the location icon on phone.  A subsequent call to OnChanged() will
  // bring the icon back if needed.
  if (!IsIPadIdiom()) {
    [field_ hidePlaceholderImage];
    is_showing_placeholder_while_collapsed_ = false;
  }

  // Update the placeholder icon.
  const int resource_id =
      edit_view_->GetIcon(IsCurrentPageOffline(GetWebState()));
  [field_ setPlaceholderImage:resource_id];

  // Show the placeholder text on iPad.
  if (IsIPadIdiom()) {
    NSString* placeholderText = l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
    [field_ setPlaceholder:placeholderText];
  }

  UpdateRightDecorations();
  [delegate_ locationBarHasResignedFirstResponder];
}

void LocationBarControllerImpl::OnSetFocus() {
  // Show the location icon on phone.
  if (!IsIPadIdiom())
    [field_ showPlaceholderImage];

  // Update the placeholder icon.
  const int resource_id =
      edit_view_->GetIcon(IsCurrentPageOffline(GetWebState()));
  [field_ setPlaceholderImage:resource_id];

  // Hide the placeholder text on iPad.
  if (IsIPadIdiom()) {
    [field_ setPlaceholder:nil];
  }
  UpdateRightDecorations();
  [delegate_ locationBarHasBecomeFirstResponder];
}

const ToolbarModel* LocationBarControllerImpl::GetToolbarModel() const {
  return [delegate_ toolbarModel];
}

ToolbarModel* LocationBarControllerImpl::GetToolbarModel() {
  return [delegate_ toolbarModel];
}

web::WebState* LocationBarControllerImpl::GetWebState() {
  return [delegate_ getWebState];
}

void LocationBarControllerImpl::InstallLocationIcon() {
  page_info_bridge_ = [[PageInfoBridge alloc] init];
  page_info_bridge_.dispatcher = dispatcher_;
  // Set the placeholder for empty omnibox.
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  UIImage* image = NativeImage(IDR_IOS_OMNIBOX_SEARCH);
  [button setImage:image forState:UIControlStateNormal];
  [button setFrame:CGRectMake(0, 0, image.size.width, image.size.height)];
  [button addTarget:page_info_bridge_
                action:@selector(showPageInfoPopup:)
      forControlEvents:UIControlEventTouchUpInside];
  SetA11yLabelAndUiAutomationName(
      button, IDS_IOS_PAGE_INFO_SECURITY_BUTTON_ACCESSIBILITY_LABEL,
      @"Page Security Info");
  [button setIsAccessibilityElement:YES];

  // Set chip text options.
  [button setTitleColor:[UIColor colorWithWhite:0.631 alpha:1]
               forState:UIControlStateNormal];
  [button titleLabel].font = [[MDCTypography fontLoader] regularFontOfSize:12];
  [field_ setLeftView:button];

  // The placeholder image is only shown when in edit mode on iPhone, and always
  // shown on iPad.
  if (IsIPadIdiom())
    [field_ setLeftViewMode:UITextFieldViewModeAlways];
  else
    [field_ setLeftViewMode:UITextFieldViewModeNever];
}

void LocationBarControllerImpl::CreateClearTextIcon(bool is_incognito) {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  UIImage* omniBoxClearImage = is_incognito
                                   ? NativeImage(IDR_IOS_OMNIBOX_CLEAR_OTR)
                                   : NativeImage(IDR_IOS_OMNIBOX_CLEAR);
  UIImage* omniBoxClearPressedImage =
      is_incognito ? NativeImage(IDR_IOS_OMNIBOX_CLEAR_OTR_PRESSED)
                   : NativeImage(IDR_IOS_OMNIBOX_CLEAR_PRESSED);
  [button setImage:omniBoxClearImage forState:UIControlStateNormal];
  [button setImage:omniBoxClearPressedImage forState:UIControlStateHighlighted];

  CGRect frame = CGRectZero;
  frame.size = CGSizeMake(kClearTextButtonWidth, kClearTextButtonHeight);
  [button setFrame:frame];

  clear_button_bridge_ =
      [[OmniboxClearButtonBridge alloc] initWithOmniboxView:edit_view_.get()];
  [button addTarget:clear_button_bridge_
                action:@selector(clearText)
      forControlEvents:UIControlEventTouchUpInside];
  clear_text_button_ = button;

  SetA11yLabelAndUiAutomationName(clear_text_button_,
                                  IDS_IOS_ACCNAME_CLEAR_TEXT, @"Clear Text");
}

void LocationBarControllerImpl::UpdateRightDecorations() {
  DCHECK(clear_text_button_);
  if (!edit_view_->model()->has_focus()) {
    // Do nothing for iPhone. The right view will be set to nil after the
    // omnibox animation is completed.
    if (IsIPadIdiom())
      [field_ setRightView:nil];
  } else if ([field_ displayedText].empty()) {
    [field_ setRightView:nil];
  } else {
    [field_ setRightView:clear_text_button_];
    [clear_text_button_ setAlpha:1];
  }
}
