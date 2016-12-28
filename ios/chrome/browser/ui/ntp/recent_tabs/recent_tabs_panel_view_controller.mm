// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_panel_view_controller.h"

#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_panel_controller.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/views/panel_bar_view.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

// A UIViewController that forces the status bar to be visible.
@interface RecentTabsWrapperViewController : UIViewController
@end

@implementation RecentTabsWrapperViewController

- (BOOL)prefersStatusBarHidden {
  return NO;
}

@end

@implementation RecentTabsPanelViewController {
  base::scoped_nsobject<RecentTabsPanelController> _recentTabsController;
  base::scoped_nsobject<PanelBarView> _panelBarView;
}

+ (UIViewController*)controllerToPresentForBrowserState:
                         (ios::ChromeBrowserState*)browserState
                                                 loader:(id<UrlLoader>)loader {
  UIViewController* controller =
      [[[RecentTabsWrapperViewController alloc] init] autorelease];
  RecentTabsPanelViewController* rtpvc = [[[RecentTabsPanelViewController alloc]
      initWithLoader:loader
        browserState:browserState] autorelease];
  [controller addChildViewController:rtpvc];

  PanelBarView* panelBarView = [[[PanelBarView alloc] init] autorelease];
  rtpvc->_panelBarView.reset([panelBarView retain]);
  [panelBarView setCloseTarget:rtpvc action:@selector(didFinish)];
  base::scoped_nsobject<UIImageView> shadow(
      [[UIImageView alloc] initWithImage:NativeImage(IDR_IOS_TOOLBAR_SHADOW)]);

  [panelBarView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [rtpvc.view setTranslatesAutoresizingMaskIntoConstraints:NO];
  [shadow setTranslatesAutoresizingMaskIntoConstraints:NO];

  [controller.view addSubview:panelBarView];
  [controller.view addSubview:rtpvc.view];
  [controller.view addSubview:shadow];

  NSDictionary* viewsDictionary =
      @{ @"bar" : panelBarView,
         @"table" : rtpvc.view,
         @"shadow" : shadow };
  // clang-format off
  NSArray* constraints = @[
    @"V:|-0-[bar]-0-[table]-0-|",
    @"V:[bar]-0-[shadow]",
    @"H:|-0-[bar]-0-|",
    @"H:|-0-[table]-0-|",
    @"H:|-0-[shadow]-0-|"
  ];
  // clang-format on
  ApplyVisualConstraints(constraints, viewsDictionary, controller.view);
  return controller;
}

- (void)dealloc {
  [_recentTabsController dismissKeyboard];
  [_recentTabsController dismissModals];
  [super dealloc];
}

- (instancetype)initWithLoader:(id<UrlLoader>)loader
                  browserState:(ios::ChromeBrowserState*)browserState {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _recentTabsController.reset([[RecentTabsPanelController alloc]
        initWithLoader:loader
          browserState:browserState]);
    if ([self respondsToSelector:@selector(edgesForExtendedLayout)])
      self.edgesForExtendedLayout = UIRectEdgeNone;
  }
  return self;
}

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  NOTREACHED();
  return nil;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  CGRect frame = self.view.bounds;
  [_recentTabsController view].frame = frame;
  [self.view addSubview:[_recentTabsController view]];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [_panelBarView setNeedsUpdateConstraints];
}

- (BOOL)prefersStatusBarHidden {
  return NO;
}

#pragma mark Accessibility

- (BOOL)accessibilityPerformEscape {
  [self didFinish];
  return YES;
}

#pragma mark Actions

- (void)didFinish {
  [self dismissViewControllerAnimated:YES
                           completion:^{
                           }];
}

@end
