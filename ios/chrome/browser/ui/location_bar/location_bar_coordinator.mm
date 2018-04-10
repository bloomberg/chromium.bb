// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"

#import <CoreLocation/CoreLocation.h>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/google/core/browser/google_util.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/search_engines/util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_constants.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_mediator.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_url_loader.h"
#include "ios/chrome/browser/ui/location_bar/location_bar_view_controller.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_coordinator.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_coordinator.h"
#include "ios/chrome/browser/ui/omnibox/web_omnibox_edit_controller_impl.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_coordinator_delegate.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "ios/web/public/referrer.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The histogram recording CLAuthorizationStatus for omnibox queries.
const char* const kOmniboxQueryLocationAuthorizationStatusHistogram =
    "Omnibox.QueryIosLocationAuthorizationStatus";
// The number of possible CLAuthorizationStatus values to report.
const int kLocationAuthorizationStatusCount = 4;
}  // namespace

@interface LocationBarCoordinator ()<LocationBarDelegate,
                                     LocationBarViewControllerDelegate,
                                     LocationBarConsumer> {
  std::unique_ptr<WebOmniboxEditControllerImpl> _editController;
}
// Coordinator for the omnibox popup.
@property(nonatomic, strong) OmniboxPopupCoordinator* omniboxPopupCoordinator;
// Coordinator for the omnibox.
@property(nonatomic, strong) OmniboxCoordinator* omniboxCoordinator;
@property(nonatomic, strong) LocationBarMediator* mediator;
@property(nonatomic, strong) LocationBarViewController* viewController;

@end

@implementation LocationBarCoordinator
@synthesize viewController = _viewController;
@synthesize mediator = _mediator;
@synthesize browserState = _browserState;
@synthesize dispatcher = _dispatcher;
@synthesize URLLoader = _URLLoader;
@synthesize delegate = _delegate;
@synthesize webStateList = _webStateList;
@synthesize omniboxPopupCoordinator = _omniboxPopupCoordinator;
@synthesize popupPositioner = _popupPositioner;
@synthesize omniboxCoordinator = _omniboxCoordinator;

#pragma mark - public

- (UIView*)view {
  return self.viewController.view;
}

- (void)start {
  BOOL isIncognito = self.browserState->IsOffTheRecord();

  UIColor* textColor =
      isIncognito
          ? [UIColor whiteColor]
          : [UIColor colorWithWhite:0 alpha:[MDCTypography body1FontOpacity]];
  UIColor* tintColor = isIncognito ? textColor : nil;
  self.viewController = [[LocationBarViewController alloc]
      initWithFrame:CGRectZero
               font:[MDCTypography subheadFont]
          textColor:textColor
          tintColor:tintColor];
  self.viewController.incognito = isIncognito;
  self.viewController.delegate = self;

  _editController = std::make_unique<WebOmniboxEditControllerImpl>(self);
  _editController->SetURLLoader(self);

  self.omniboxCoordinator = [[OmniboxCoordinator alloc] init];
  self.omniboxCoordinator.textField = self.viewController.textField;
  self.omniboxCoordinator.editController = _editController.get();
  self.omniboxCoordinator.browserState = self.browserState;
  self.omniboxCoordinator.dispatcher = self.dispatcher;
  [self.omniboxCoordinator start];

  self.omniboxPopupCoordinator =
      [self.omniboxCoordinator createPopupCoordinator:self.popupPositioner];
  self.omniboxPopupCoordinator.dispatcher = self.dispatcher;
  [self.omniboxPopupCoordinator start];

  self.mediator =
      [[LocationBarMediator alloc] initWithToolbarModel:[self toolbarModel]];
  self.mediator.webStateList = self.webStateList;
  self.mediator.consumer = self;
}

- (void)stop {
  // The popup has to be destroyed before the location bar.
  [self.omniboxPopupCoordinator stop];
  [self.omniboxCoordinator stop];
  _editController.reset();

  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;
}

- (BOOL)omniboxPopupHasAutocompleteResults {
  return self.omniboxPopupCoordinator.hasResults;
}

- (BOOL)showingOmniboxPopup {
  return self.omniboxPopupCoordinator.isOpen;
}

- (BOOL)isOmniboxFirstResponder {
  return [self.omniboxCoordinator isOmniboxFirstResponder];
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
    [self.omniboxCoordinator insertTextToOmnibox:result];
  }
}

#pragma mark - LocationBarURLLoader

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

#pragma mark - OmniboxFocuser

- (void)focusOmniboxFromSearchButton {
  [self.omniboxCoordinator setNextFocusSourceAsSearchButton];
  [self focusOmnibox];
}

- (void)focusOmnibox {
  [self.viewController switchToEditing:YES];
  [self.omniboxCoordinator focusOmnibox];
}

- (void)cancelOmniboxEdit {
  [self.omniboxCoordinator endEditing];
}

#pragma mark - LocationBarDelegate

- (void)locationBarHasBecomeFirstResponder {
  [self.delegate locationBarDidBecomeFirstResponder];
}

- (void)locationBarHasResignedFirstResponder {
  [self.delegate locationBarDidResignFirstResponder];
  [self.viewController switchToEditing:NO];
}

- (void)locationBarBeganEdit {
  [self.delegate locationBarBeganEdit];
}

- (web::WebState*)webState {
  return self.webStateList->GetActiveWebState();
}

- (ToolbarModel*)toolbarModel {
  return [self.delegate toolbarModel];
}

#pragma mark - LocationBarViewControllerDelegate

- (void)locationBarSteadyViewTapped {
  [self focusOmnibox];
}

#pragma mark - LocationBarConsumer

- (void)updateLocationText:(NSString*)text {
  [self.omniboxCoordinator updateOmniboxState];
  [self.viewController updateLocationText:text];
}

- (void)defocusOmnibox {
  [self cancelOmniboxEdit];
}

- (void)updateLocationIcon:(UIImage*)icon {
  [self.viewController updateLocationIcon:icon];
}

#pragma mark - private

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
