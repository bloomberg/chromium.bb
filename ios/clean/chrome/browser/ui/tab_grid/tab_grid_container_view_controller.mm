// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_container_view_controller.h"

#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_toolbar.h"
#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_toolbar_commands.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabGridContainerViewController ()
// Toolbar managing the tab grid.
@property(nonatomic, weak) TabGridToolbar* toolbar;
@end

@implementation TabGridContainerViewController

@synthesize toolbar = _toolbar;
@synthesize dispatcher = _dispatcher;
@synthesize tabGrid = _tabGrid;
@synthesize incognito = _incognito;

- (void)setTabGrid:(UIViewController*)tabGrid {
  [_tabGrid willMoveToParentViewController:nil];
  [_tabGrid.view removeFromSuperview];
  [_tabGrid removeFromParentViewController];

  _tabGrid = tabGrid;

  if (!tabGrid)
    return;

  [self addChildViewController:tabGrid];

  [self.view addSubview:tabGrid.view];
  tabGrid.view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [tabGrid.view.topAnchor constraintEqualToAnchor:self.toolbar.bottomAnchor],
    [tabGrid.view.leftAnchor constraintEqualToAnchor:self.view.leftAnchor],
    [tabGrid.view.rightAnchor constraintEqualToAnchor:self.view.rightAnchor],
    [tabGrid.view.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
  ]];

  [tabGrid didMoveToParentViewController:self];
}

- (void)setDispatcher:(id<TabGridToolbarCommands>)dispatcher {
  _dispatcher = dispatcher;
  self.toolbar.dispatcher = dispatcher;
}

- (void)setIncognito:(BOOL)incognito {
  _incognito = incognito;
  [self.toolbar setIncognito:incognito];
}

#pragma mark - View lifecyle

- (void)viewDidLoad {
  [super viewDidLoad];

  TabGridToolbar* toolbar = [[TabGridToolbar alloc] init];
  self.toolbar = toolbar;
  self.toolbar.dispatcher = self.dispatcher;
  [self.view addSubview:self.toolbar];
  self.toolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.toolbar.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [self.toolbar.heightAnchor
        constraintEqualToAnchor:self.topLayoutGuide.heightAnchor
                       constant:self.toolbar.intrinsicContentSize.height],
    [self.toolbar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.toolbar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor]
  ]];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  // We need to dismiss the ToolsMenu every time the Toolbar frame changes
  // (e.g. Size changes, rotation changes, etc.)
  [self.dispatcher closeToolsMenu];
}

#pragma mark - ZoomTransitionDelegate methods

- (CGRect)rectForZoomWithKey:(NSObject*)key inView:(UIView*)view {
  // If key is nil we return a CGRect based on the toolbar position, if not it
  // was set by the TabGrid and we return a CGRect based on the IndexPath of the
  // key.
  if (!key) {
    return [self.toolbar rectForZoomWithKey:key inView:view];
  }

  if ([self.tabGrid respondsToSelector:@selector(rectForZoomWithKey:inView:)]) {
    return [static_cast<id<ZoomTransitionDelegate>>(self.tabGrid)
        rectForZoomWithKey:key
                    inView:view];
  }
  return CGRectNull;
}

@end
