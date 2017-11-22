// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_coordinator.h"

#import <CoreLocation/CoreLocation.h>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/google/core/browser/google_util.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/search_engines/util.h"
#include "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_controller.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_controller_impl.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_positioner.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_popup_view_ios.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_coordinator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_mediator.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_style.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/public/web_toolbar_controller_constants.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_ios.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarCoordinator ()<LocationBarDelegate, OmniboxPopupPositioner> {
  std::unique_ptr<LocationBarControllerImpl> _locationBar;
  std::unique_ptr<OmniboxPopupViewIOS> _popupView;
}

// The View Controller managed by this coordinator.
@property(nonatomic, strong) ToolbarViewController* toolbarViewController;
// The mediator owned by this coordinator.
@property(nonatomic, strong) ToolbarMediator* mediator;
// LocationBarView containing the omnibox. At some point, this property and the
// |_locationBar| should become a LocationBarCoordinator.
@property(nonatomic, strong) LocationBarView* locationBarView;

@end

@implementation ToolbarCoordinator
@synthesize delegate = _delegate;
@synthesize browserState = _browserState;
@synthesize dispatcher = _dispatcher;
@synthesize locationBarView = _locationBarView;
@synthesize mediator = _mediator;
@synthesize URLLoader = _URLLoader;
@synthesize toolbarViewController = _toolbarViewController;
@synthesize webStateList = _webStateList;

- (instancetype)init {
  if ((self = [super init])) {
    _mediator = [[ToolbarMediator alloc] init];
  }
  return self;
}

#pragma mark - Properties

- (UIViewController*)viewController {
  return self.toolbarViewController;
}

#pragma mark - BrowserCoordinator

- (void)start {
  BOOL isIncognito = self.browserState->IsOffTheRecord();
  // TODO(crbug.com/785253): Move this to the LocationBarCoordinator once it is
  // created.
  UIColor* textColor =
      isIncognito
          ? [UIColor whiteColor]
          : [UIColor colorWithWhite:0 alpha:[MDCTypography body1FontOpacity]];
  UIColor* tintColor = isIncognito ? textColor : nil;
  self.locationBarView =
      [[LocationBarView alloc] initWithFrame:CGRectZero
                                        font:[MDCTypography subheadFont]
                                   textColor:textColor
                                   tintColor:tintColor];
  _locationBar = base::MakeUnique<LocationBarControllerImpl>(
      self.locationBarView, self.browserState, self, self.dispatcher);
  _popupView = _locationBar->CreatePopupView(self);
  // End of TODO(crbug.com/785253):.

  ToolbarStyle style = isIncognito ? INCOGNITO : NORMAL;
  ToolbarButtonFactory* factory =
      [[ToolbarButtonFactory alloc] initWithStyle:style];

  self.toolbarViewController =
      [[ToolbarViewController alloc] initWithDispatcher:self.dispatcher
                                          buttonFactory:factory];
  self.toolbarViewController.locationBarView = self.locationBarView;

  self.mediator.consumer = self.toolbarViewController;
  self.mediator.webStateList = self.webStateList;
}

- (void)stop {
  [self.mediator disconnect];
  // The popup has to be destroyed before the location bar.
  _popupView.reset();
  _locationBar.reset();
  self.locationBarView = nil;
}

#pragma mark - Public

- (void)updateToolbarState {
  // TODO(crbug.com/784911): This function should probably triggers something in
  // the mediator. Investigate how to handle it.
}

- (void)updateToolbarForSideSwipeSnapshot:(web::WebState*)webState {
  web::NavigationItem* item =
      webState->GetNavigationManager()->GetVisibleItem();
  GURL URL = item ? item->GetURL().GetOrigin() : GURL::EmptyGURL();
  BOOL isNTP = URL == GURL(kChromeUINewTabURL);

  // Don't do anything for a live non-ntp tab.
  if (webState == [self getWebState] && !isNTP) {
    [_locationBarView setHidden:NO];
    return;
  }

  self.viewController.view.hidden = NO;
  [_locationBarView setHidden:YES];
  [self.mediator updateConsumerForWebState:webState];
  [self.toolbarViewController updateForSideSwipeSnapshotOnNTP:isNTP];
}

- (void)resetToolbarAfterSideSwipeSnapshot {
  [self.mediator updateConsumerForWebState:[self getWebState]];
  [_locationBarView setHidden:NO];
  [self.toolbarViewController resetAfterSideSwipeSnapshot];
}

// TODO(crbug.com/786940): This protocol should move to the ViewController
// owning the Toolbar. This can wait until the omnibox and toolbar refactoring
// is more advanced.
#pragma mark OmniboxPopupPositioner methods.

- (UIView*)popupAnchorView {
  return self.toolbarViewController.view;
}

- (CGRect)popupFrame:(CGFloat)height {
  UIView* parent = [[self popupAnchorView] superview];
  CGRect frame = [parent bounds];

  if (IsIPadIdiom()) {
    // For iPad, the omnibox is displayed between the location bar and the
    // bottom of the toolbar.
    CGRect fieldFrame = [parent convertRect:[_locationBarView bounds]
                                   fromView:_locationBarView];
    CGFloat maxY = CGRectGetMaxY(fieldFrame);
    frame.origin.y = maxY + kiPadOmniboxPopupVerticalOffset;
    frame.size.height = height;
  } else {
    // For iPhone place the popup just below the toolbar.
    CGRect fieldFrame =
        [parent convertRect:[self.toolbarViewController.view bounds]
                   fromView:self.toolbarViewController.view];
    frame.origin.y = CGRectGetMaxY(fieldFrame);
    frame.size.height = CGRectGetMaxY([parent bounds]) - frame.origin.y;
  }
  return frame;
}

#pragma mark - LocationBarDelegate

- (void)loadGURLFromLocationBar:(const GURL&)url
                     transition:(ui::PageTransition)transition {
  if (url.SchemeIs(url::kJavaScriptScheme)) {
    // Evaluate the URL as JavaScript if its scheme is JavaScript.
    NSString* jsToEval = [base::SysUTF8ToNSString(url.GetContent())
        stringByRemovingPercentEncoding];
    [self.URLLoader loadJavaScriptFromLocationBar:jsToEval];
  } else {
    // When opening a URL, force the omnibox to resign first responder.  This
    // will also close the popup.

    // TODO(crbug.com/785244): Is it ok to call |cancelOmniboxEdit| after
    // |loadURL|?  It doesn't seem to be causing major problems.  If we call
    // cancel before load, then any prerendered pages get destroyed before the
    // call to load.
    [self.URLLoader loadURL:url
                   referrer:web::Referrer()
                 transition:transition
          rendererInitiated:NO];

    if (google_util::IsGoogleSearchUrl(url)) {
      UMA_HISTOGRAM_ENUMERATION(
          kOmniboxQueryLocationAuthorizationStatusHistogram,
          [CLLocationManager authorizationStatus],
          kLocationAuthorizationStatusCount);
    }
  }
  [self cancelOmniboxEdit];
}

- (void)locationBarHasBecomeFirstResponder {
  [self.delegate locationBarDidBecomeFirstResponder];
  if (@available(iOS 10, *)) {
    [self.toolbarViewController expandOmniboxAnimated:YES];
  }
}

- (void)locationBarHasResignedFirstResponder {
  [self.delegate locationBarDidResignFirstResponder];
  if (@available(iOS 10, *)) {
    [self.toolbarViewController contractOmnibox];
  }
}

- (void)locationBarBeganEdit {
  [self.delegate locationBarBeganEdit];
}

- (web::WebState*)getWebState {
  return self.webStateList->GetActiveWebState();
}

- (ToolbarModel*)toolbarModel {
  ToolbarModelIOS* toolbarModelIOS = [self.delegate toolbarModelIOS];
  return toolbarModelIOS ? toolbarModelIOS->GetToolbarModel() : nullptr;
}

#pragma mark - OmniboxFocuser

- (void)focusOmnibox {
  if (!self.viewController.view.hidden)
    [_locationBarView.textField becomeFirstResponder];
}

- (void)cancelOmniboxEdit {
  _locationBar->HideKeyboardAndEndEditing();
  [self updateToolbarState];
}

- (void)focusFakebox {
  if (IsIPadIdiom()) {
    OmniboxEditModel* model = _locationBar->GetLocationEntry()->model();
    // Setting the caret visibility to false causes OmniboxEditModel to indicate
    // that omnibox interaction was initiated from the fakebox. Note that
    // SetCaretVisibility is a no-op unless OnSetFocus is called first.  Only
    // set fakebox on iPad, where there is a distinction between the omnibox
    // and the fakebox on the NTP.  On iPhone there is no visible omnibox, so
    // there's no need to indicate interaction was initiated from the fakebox.
    model->OnSetFocus(false);
    model->SetCaretVisibility(false);
  } else {
    [self.toolbarViewController expandOmniboxAnimated:NO];
  }

  [self focusOmnibox];
}

- (void)onFakeboxBlur {
  DCHECK(!IsIPadIdiom());
  // Hide the toolbar if the NTP is currently displayed.
  web::WebState* webState = [self getWebState];
  if (webState && (webState->GetVisibleURL() == GURL(kChromeUINewTabURL))) {
    self.viewController.view.hidden = YES;
  }
}

- (void)onFakeboxAnimationComplete {
  DCHECK(!IsIPadIdiom());
  self.viewController.view.hidden = NO;
}

#pragma mark - VoiceSearchControllerDelegate

- (void)receiveVoiceSearchResult:(NSString*)result {
  DCHECK(result);
  [self loadURLForQuery:result];
}

#pragma mark - QRScannerResultLoading

- (void)receiveQRScannerResult:(NSString*)result loadImmediately:(BOOL)load {
  DCHECK(result);
  if (load) {
    [self loadURLForQuery:result];
  } else {
    [self focusOmnibox];
    [_locationBarView.textField insertTextWhileEditing:result];
    // The call to |setText| shouldn't be needed, but without it the "Go" button
    // of the keyboard is disabled.
    [_locationBarView.textField setText:result];
    // Notify the accessibility system to start reading the new contents of the
    // Omnibox.
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                    _locationBarView.textField);
  }
}

#pragma mark - Private

// Navigate to |query| from omnibox.
- (void)loadURLForQuery:(NSString*)query {
  GURL searchURL;
  metrics::OmniboxInputType type = AutocompleteInput::Parse(
      base::SysNSStringToUTF16(query), std::string(),
      AutocompleteSchemeClassifierImpl(), nullptr, nullptr, &searchURL);
  if (type != metrics::OmniboxInputType::URL || !searchURL.is_valid()) {
    searchURL = GetDefaultSearchURLForSearchTerms(
        ios::TemplateURLServiceFactory::GetForBrowserState(self.browserState),
        base::SysNSStringToUTF16(query));
  }
  if (searchURL.is_valid()) {
    // It is necessary to include PAGE_TRANSITION_FROM_ADDRESS_BAR in the
    // transition type is so that query-in-the-omnibox is triggered for the
    // URL.
    ui::PageTransition transition = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    [self.URLLoader loadURL:GURL(searchURL)
                   referrer:web::Referrer()
                 transition:transition
          rendererInitiated:NO];
  }
}

@end
