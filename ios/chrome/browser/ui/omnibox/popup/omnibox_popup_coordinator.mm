// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_coordinator.h"

#import "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_mediator.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_presenter.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxPopupCoordinator ()

@property(nonatomic, strong) OmniboxPopupViewController* popupViewController;
@property(nonatomic, strong) OmniboxPopupMediator* mediator;
@property(nonatomic, strong) OmniboxPopupPresenter* presenter;

@end

@implementation OmniboxPopupCoordinator

@synthesize browserState = _browserState;
@synthesize mediator = _mediator;
@synthesize mediatorDelegate = _mediatorDelegate;
@synthesize open = _open;
@synthesize popupViewController = _popupViewController;
@synthesize positioner = _positioner;
@synthesize presenter = _presenter;

#pragma mark - Public

- (void)start {
  self.open = NO;
  std::unique_ptr<image_fetcher::IOSImageDataFetcherWrapper> imageFetcher =
      std::make_unique<image_fetcher::IOSImageDataFetcherWrapper>(
          self.browserState->GetRequestContext());

  self.mediator =
      [[OmniboxPopupMediator alloc] initWithFetcher:std::move(imageFetcher)
                                           delegate:self.mediatorDelegate];
  self.popupViewController = [[OmniboxPopupViewController alloc] init];
  self.popupViewController.incognito = self.browserState->IsOffTheRecord();

  self.mediator.incognito = self.browserState->IsOffTheRecord();
  self.mediator.consumer = self.popupViewController;
  self.popupViewController.imageRetriever = self.mediator;
  self.popupViewController.delegate = self.mediator;

  self.presenter = [[OmniboxPopupPresenter alloc]
      initWithPopupPositioner:self.positioner
          popupViewController:self.popupViewController];
}

- (void)updateWithResults:(const AutocompleteResult&)result {
  if (!self.open && !result.empty()) {
    // The popup is not currently open and there are results to display. Update
    // and animate the cells
    [self.mediator updateMatches:result withAnimation:YES];
  } else {
    // The popup is already displayed or there are no results to display. Update
    // the cells without animating.
    [self.mediator updateMatches:result withAnimation:NO];
  }
  self.open = !result.empty();

  if (self.open) {
    [self.presenter updateHeightAndAnimateAppearanceIfNecessary];
  } else {
    [self.presenter animateCollapse];
  }
}

- (void)setTextAlignment:(NSTextAlignment)alignment {
  [self.popupViewController setTextAlignment:alignment];
}

@end
