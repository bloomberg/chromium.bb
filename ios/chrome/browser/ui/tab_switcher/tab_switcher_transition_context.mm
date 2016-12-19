// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/tab_switcher/tab_switcher_transition_context.h"

#include "base/mac/objc_property_releaser.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#include "ios/chrome/browser/ui/tab_switcher/tab_switcher_transition_context.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_controller+tab_switcher_animation.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_controller.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

@class BrowserViewController;

@interface TabSwitcherTransitionContextContent () {
  base::scoped_nsobject<TabSwitcherTabStripPlaceholderView>
      _tabStripPlaceholderView;
  base::WeakNSObject<BrowserViewController> _bvc;
}

@end

@implementation TabSwitcherTransitionContextContent {
  base::mac::ObjCPropertyReleaser
      _propertyReleaser_tabSwitcherTransitionContextContent;
}

+ (instancetype)tabSwitcherTransitionContextContentFromBVC:
    (BrowserViewController*)bvc {
  TabSwitcherTransitionContextContent* transitionContextContent =
      [[[TabSwitcherTransitionContextContent alloc] init] autorelease];

  Tab* currentTab = [[bvc tabModel] currentTab];
  transitionContextContent.initialSelectedTabIndex =
      [[bvc tabModel] indexOfTab:currentTab];

  if (![bvc isViewLoaded]) {
    [bvc ensureViewCreated];
    [bvc.view setFrame:[[UIScreen mainScreen] bounds]];
  }

  UIView* toolbarView = [[bvc toolbarController] view];
  base::scoped_nsobject<UIView> toolbarSnapshotView;
  if ([toolbarView window]) {
    toolbarSnapshotView.reset(
        [[toolbarView snapshotViewAfterScreenUpdates:NO] retain]);
  } else {
    toolbarSnapshotView.reset([[UIView alloc] initWithFrame:toolbarView.frame]);
    [toolbarSnapshotView layer].contents = static_cast<id>(
        CaptureViewWithOption(toolbarView, 1, kClientSideRendering).CGImage);
  }
  transitionContextContent.toolbarSnapshotView = toolbarSnapshotView;
  transitionContextContent->_bvc.reset(bvc);
  return transitionContextContent;
}

- (TabSwitcherTabStripPlaceholderView*)generateTabStripPlaceholderView {
  TabStripController* tsc = [_bvc tabStripController];
  return [tsc placeholderView];
}

@synthesize toolbarSnapshotView = _toolbarSnapshotView;
@synthesize initialSelectedTabIndex = _initialSelectedTabIndex;

- (instancetype)init {
  self = [super init];
  if (self) {
    _propertyReleaser_tabSwitcherTransitionContextContent.Init(
        self, [TabSwitcherTransitionContextContent class]);
  }
  return self;
}

@end

@implementation TabSwitcherTransitionContext {
  base::mac::ObjCPropertyReleaser
      _propertyReleaser_tabSwitcherTransitionContext;
}

+ (instancetype)
tabSwitcherTransitionContextWithCurrent:(BrowserViewController*)currentBVC
                                mainBVC:(BrowserViewController*)mainBVC
                                 otrBVC:(BrowserViewController*)otrBVC {
  TabSwitcherTransitionContext* transitionContext =
      [[[TabSwitcherTransitionContext alloc] init] autorelease];
  Tab* currentTab = [[currentBVC tabModel] currentTab];
  UIImage* tabSnapshotImage =
      [currentTab generateSnapshotWithOverlay:YES visibleFrameOnly:YES];
  [transitionContext setTabSnapshotImage:tabSnapshotImage];
  [transitionContext
      setIncognitoContent:
          [TabSwitcherTransitionContextContent
              tabSwitcherTransitionContextContentFromBVC:otrBVC]];
  [transitionContext
      setRegularContent:
          [TabSwitcherTransitionContextContent
              tabSwitcherTransitionContextContentFromBVC:mainBVC]];
  [transitionContext setInitialTabModel:currentBVC.tabModel];
  return transitionContext;
}

@synthesize tabSnapshotImage = _tabSnapshotImage;
@synthesize incognitoContent = _incognitoContent;
@synthesize regularContent = _regularContent;
@synthesize initialTabModel = _initialTabModel;

- (instancetype)init {
  self = [super init];
  if (self) {
    _propertyReleaser_tabSwitcherTransitionContext.Init(
        self, [TabSwitcherTransitionContext class]);
  }
  return self;
}

@end
