// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/translate/language_selection_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/translate/language_selection_delegate.h"
#import "ios/chrome/browser/translate/language_selection_handler.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter.h"
#import "ios/chrome/browser/ui/presenters/vertical_animation_container.h"
#import "ios/chrome/browser/ui/translate/language_selection_mediator.h"
#import "ios/chrome/browser/ui/translate/language_selection_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LanguageSelectionCoordinator () <
    LanguageSelectionHandler,
    LanguageSelectionViewControllerDelegate>

// The WebStateList being observed.
@property(nonatomic) WebStateList* webStateList;
// Presenter to use to for presenting the view controller.
@property(nonatomic) id<ContainedPresenter> presenter;
// The view controller this coordinator manages.
@property(nonatomic) LanguageSelectionViewController* selectionViewController;
// A mediator to interoperate with the translation model.
@property(nonatomic) LanguageSelectionMediator* selectionMediator;
// A delegate, provided by showLanguageSelectorWithContext:delegate:.
@property(nonatomic, weak) id<LanguageSelectionDelegate> selectionDelegate;
// YES if the coordinator has been started.
@property(nonatomic) BOOL started;

@end

@implementation LanguageSelectionCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                              webStateList:(WebStateList*)webStateList {
  DCHECK(webStateList);
  self = [super initWithBaseViewController:viewController
                              browserState:browserState];
  if (self) {
    _webStateList = webStateList;
  }
  return self;
}

#pragma mark - private methods

- (void)start {
  if (self.started)
    return;

  self.selectionMediator =
      [[LanguageSelectionMediator alloc] initWithLanguageSelectionHandler:self];
  self.selectionMediator.webStateList = self.webStateList;

  self.started = YES;
}

- (void)stop {
  if (!self.started)
    return;

  [self dismissLanguageSelector];
  [self.selectionMediator disconnect];
  self.selectionMediator = nil;
  self.presenter = nil;
  self.selectionViewController = nil;
  self.selectionDelegate = nil;

  self.started = NO;
}

#pragma mark - LanguageSelectionHandler

- (void)showLanguageSelectorWithContext:(LanguageSelectionContext*)context
                               delegate:
                                   (id<LanguageSelectionDelegate>)delegate {
  self.selectionDelegate = delegate;

  self.selectionMediator.context = context;

  self.selectionViewController = [[LanguageSelectionViewController alloc] init];
  self.selectionViewController.delegate = self;
  self.selectionMediator.consumer = self.selectionViewController;

  self.presenter = [[VerticalAnimationContainer alloc] init];
  self.presenter.baseViewController = self.baseViewController;
  self.presenter.presentedViewController = self.selectionViewController;
  [self.presenter prepareForPresentation];
  [self.presenter presentAnimated:YES];
}

- (void)dismissLanguageSelector {
  [self.selectionDelegate languageSelectorClosedWithoutSelection];
  [self.presenter dismissAnimated:NO];
}

#pragma mark - LanguageSelectionViewControllerDelegate

- (void)languageSelectedAtIndex:(int)index {
  [self.selectionDelegate
      languageSelectorSelectedLanguage:
          [self.selectionMediator languageCodeForLanguageAtIndex:index]];
  [self.presenter dismissAnimated:NO];
}

- (void)languageSelectionCanceled {
  [self.selectionDelegate languageSelectorClosedWithoutSelection];
  [self.presenter dismissAnimated:NO];
}

@end
