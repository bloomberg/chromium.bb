// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/popup_menu_commands.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_presenter.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PopupMenuCoordinator ()<PopupMenuCommands>

// Presenter for the popup menu, managing the animations.
@property(nonatomic, strong) PopupMenuPresenter* presenter;

@end

@implementation PopupMenuCoordinator

@synthesize dispatcher = _dispatcher;
@synthesize presenter = _presenter;

#pragma mark - ChromeCoordinator

- (void)start {
  [self.dispatcher startDispatchingToTarget:self
                                forProtocol:@protocol(PopupMenuCommands)];
}

- (void)stop {
  [self.dispatcher stopDispatchingToTarget:self];
}

#pragma mark - PopupMenuCommands

- (void)showNavigationHistoryBackPopupMenu {
  UIViewController* viewController = [[UIViewController alloc] init];
  UILabel* label = [[UILabel alloc] init];
  label.text = @"Back";
  viewController.view = label;
  // TODO(crbug.com/804779): Use the Navigation menu instead of a label.
  [self presentPopupForContent:viewController fromNamedGuide:kBackButtonGuide];
}

- (void)showNavigationHistoryForwardPopupMenu {
  UIViewController* viewController = [[UIViewController alloc] init];
  UILabel* label = [[UILabel alloc] init];
  label.text = @"Forward";
  viewController.view = label;
  // TODO(crbug.com/804779): Use the Navigation menu instead of a label.
  [self presentPopupForContent:viewController
                fromNamedGuide:kForwardButtonGuide];
}

- (void)showToolsMenuPopup {
  UIViewController* viewController = [[UIViewController alloc] init];
  UILabel* label = [[UILabel alloc] init];
  label.text = @"Tools menu";
  viewController.view = label;
  // TODO(crbug.com/804773): Use the tools menu instead of a label.
  [self presentPopupForContent:viewController fromNamedGuide:kToolsMenuGuide];
}

- (void)showTabGridButtonPopup {
  UIViewController* viewController = [[UIViewController alloc] init];
  UILabel* label = [[UILabel alloc] init];
  label.text = @"TabGrid";
  viewController.view = label;
  // TODO(crbug.com/821560): Use the tab grid menu instead of a label.
  [self presentPopupForContent:viewController fromNamedGuide:kTabSwitcherGuide];
}

- (void)searchButtonPopup {
  UIViewController* viewController = [[UIViewController alloc] init];
  UILabel* label = [[UILabel alloc] init];
  label.text = @"Search";
  viewController.view = label;
  // TODO(crbug.com/821560): Use the search menu instead of a label.
  [self presentPopupForContent:viewController fromNamedGuide:nil];
}

- (void)dismissPopupMenu {
  [self.presenter dismissAnimated:YES];
  self.presenter = nil;
}

#pragma mark - Private

// Presents the |content| with an animation starting from |guideName|.
- (void)presentPopupForContent:(UIViewController*)content
                fromNamedGuide:(GuideName*)guideName {
  DCHECK(!self.presenter);
  self.presenter = [[PopupMenuPresenter alloc] init];
  self.presenter.baseViewController = self.baseViewController;
  self.presenter.commandHandler = self;
  self.presenter.presentedViewController = content;
  self.presenter.guideName = guideName;

  [self.presenter prepareForPresentation];
  [self.presenter presentAnimated:YES];
  return;
}

@end
