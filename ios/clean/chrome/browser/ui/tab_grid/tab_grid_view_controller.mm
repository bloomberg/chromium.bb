// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_panel_overlay_view.h"
#import "ios/clean/chrome/browser/ui/actions/tab_grid_actions.h"
#import "ios/clean/chrome/browser/ui/commands/tab_grid_commands.h"
#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_tab_cell.h"
#import "ios/clean/chrome/browser/ui/tab_grid/mdc_floating_button+cr_tab_grid.h"
#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_collection_view_layout.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kTopInset = 20;
}  // namespace

@interface TabGridViewController ()<TabGridActions>
@property(nonatomic, weak) UIView* noTabsOverlay;
@property(nonatomic, weak) MDCFloatingButton* floatingNewTabButton;
@end

@implementation TabGridViewController
@synthesize dispatcher = _dispatcher;
@synthesize noTabsOverlay = _noTabsOverlay;
@synthesize floatingNewTabButton = _floatingNewTabButton;

#pragma mark - Required subclass override

- (UICollectionViewLayout*)collectionViewLayout {
  return [[TabGridCollectionViewLayout alloc] init];
}

- (void)showTabAtIndex:(int)index {
  [self.dispatcher showTabGridTabAtIndex:index];
}

- (void)closeTabAtIndex:(int)index {
  [self.dispatcher closeTabGridTabAtIndex:index];
}

#pragma mark - View lifecyle

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tabs.contentInset = UIEdgeInsetsMake(kTopInset, 0, 0, 0);
  if (self.items.count == 0) {
    [self addNoTabsOverlay];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  MDCFloatingButton* floatingNewTabButton =
      [MDCFloatingButton cr_tabGridNewTabButton];
  self.floatingNewTabButton = floatingNewTabButton;
  [self.floatingNewTabButton
      setFrame:[MDCFloatingButton
                   cr_frameForTabGridNewTabButtonInRect:self.view.bounds]];
  [self.view addSubview:self.floatingNewTabButton];
}


#pragma mark - TabGridActions

- (void)showTabGrid:(id)sender {
  [self.dispatcher showTabGrid];
}

- (void)createNewTab:(id)sender {
  [self.dispatcher createAndShowNewTabInTabGrid];
}

#pragma mark - ZoomTransitionDelegate methods

- (CGRect)rectForZoomWithKey:(NSObject*)key inView:(UIView*)view {
  NSIndexPath* cellPath = base::mac::ObjCCastStrict<NSIndexPath>(key);
  UICollectionViewCell* cell = [self.tabs cellForItemAtIndexPath:cellPath];
  return [view convertRect:cell.bounds fromView:cell];
}

#pragma mark - TabGridConsumer methods

- (void)addNoTabsOverlay {
  // PLACEHOLDER: The new tab grid will have a completely different zero tab
  // overlay from the tab switcher. Also, the overlay will be above the recent
  // tabs section.
  TabSwitcherPanelOverlayView* overlayView =
      [[TabSwitcherPanelOverlayView alloc] initWithFrame:self.tabs.bounds
                                            browserState:nil
                                               presenter:nil
                                              dispatcher:nil];
  overlayView.overlayType =
      TabSwitcherPanelOverlayType::OVERLAY_PANEL_USER_NO_OPEN_TABS;
  overlayView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self.tabs addSubview:overlayView];
  self.noTabsOverlay = overlayView;
}

- (void)removeNoTabsOverlay {
  [self.noTabsOverlay removeFromSuperview];
}

@end
