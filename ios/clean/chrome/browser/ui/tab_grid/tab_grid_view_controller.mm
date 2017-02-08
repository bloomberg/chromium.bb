// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_panel_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_panel_collection_view_layout.h"
#import "ios/clean/chrome/browser/ui/actions/settings_actions.h"
#import "ios/clean/chrome/browser/ui/actions/tab_grid_actions.h"
#import "ios/clean/chrome/browser/ui/commands/settings_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tab_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tab_grid_commands.h"
#import "ios/clean/chrome/browser/ui/tab_grid/mdc_floating_button+cr_tab_grid.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kSpace = 20;
const CGFloat kTabSize = 150;
}

@interface TabGridViewController ()<SettingsActions,
                                    TabGridActions,
                                    UICollectionViewDataSource,
                                    UICollectionViewDelegate,
                                    SessionCellDelegate>
@property(nonatomic, weak) UICollectionView* grid;
@property(nonatomic, strong) MDCFloatingButton* floatingNewTabButton;
@end

@implementation TabGridViewController

@synthesize dataSource = _dataSource;
@synthesize settingsCommandHandler = _settingsCommandHandler;
@synthesize tabGridCommandHandler = _tabGridCommandHandler;
@synthesize tabCommandHandler = _tabCommandHandler;
@synthesize grid = _grid;
@synthesize floatingNewTabButton = _floatingNewTabButton;

- (void)viewDidLoad {
  // Placeholder dark grey stripe at the top of the view.
  UIView* stripe = [[UIView alloc] initWithFrame:CGRectZero];
  stripe.translatesAutoresizingMaskIntoConstraints = NO;
  stripe.backgroundColor = [UIColor darkGrayColor];
  [self.view addSubview:stripe];

  // Placeholder settings button at in the stripe.
  UIButton* settings = [UIButton buttonWithType:UIButtonTypeSystem];
  settings.translatesAutoresizingMaskIntoConstraints = NO;
  [settings setTitle:@"⚙" forState:UIControlStateNormal];
  settings.titleLabel.font = [UIFont systemFontOfSize:24.0];
  [settings addTarget:nil
                action:@selector(showSettings:)
      forControlEvents:UIControlEventTouchUpInside];
  [stripe addSubview:settings];

  [NSLayoutConstraint activateConstraints:@[
    [stripe.heightAnchor constraintEqualToConstant:64.0],
    [stripe.widthAnchor constraintEqualToAnchor:self.view.widthAnchor],
    [stripe.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [stripe.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
    [settings.leadingAnchor
        constraintEqualToAnchor:stripe.layoutMarginsGuide.leadingAnchor],
    [settings.centerYAnchor constraintEqualToAnchor:stripe.centerYAnchor]
  ]];

  TabSwitcherPanelCollectionViewLayout* layout =
      [[TabSwitcherPanelCollectionViewLayout alloc] init];
  layout.minimumLineSpacing = kSpace;
  layout.minimumInteritemSpacing = kSpace;
  layout.sectionInset = UIEdgeInsetsMake(kSpace, kSpace, kSpace, kSpace);
  layout.itemSize = CGSizeMake(kTabSize, kTabSize);

  UICollectionView* grid = [[UICollectionView alloc] initWithFrame:CGRectZero
                                              collectionViewLayout:layout];
  grid.translatesAutoresizingMaskIntoConstraints = NO;
  grid.backgroundColor = [UIColor blackColor];

  [self.view addSubview:grid];
  self.grid = grid;
  self.grid.dataSource = self;
  self.grid.delegate = self;
  [self.grid registerClass:[TabSwitcherLocalSessionCell class]
      forCellWithReuseIdentifier:[TabSwitcherLocalSessionCell identifier]];

  [NSLayoutConstraint activateConstraints:@[
    [self.grid.topAnchor constraintEqualToAnchor:stripe.bottomAnchor],
    [self.grid.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [self.grid.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [self.grid.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
  ]];
}

- (void)viewWillAppear:(BOOL)animated {
  [self.grid reloadData];
  self.floatingNewTabButton = [MDCFloatingButton cr_tabGridNewTabButton];
  [self.floatingNewTabButton
      setFrame:[MDCFloatingButton
                   cr_frameForTabGridNewTabButtonInRect:self.view.bounds]];
  [self.view addSubview:self.floatingNewTabButton];
}

#pragma mark - UICollectionViewDataSource methods

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  return 1;
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return [self.dataSource numberOfTabsInTabGrid];
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(nonnull NSIndexPath*)indexPath {
  TabSwitcherLocalSessionCell* cell =
      base::mac::ObjCCastStrict<TabSwitcherLocalSessionCell>([collectionView
          dequeueReusableCellWithReuseIdentifier:
                              [TabSwitcherLocalSessionCell identifier]
                                    forIndexPath:indexPath]);
  cell.delegate = self;
  [cell setSessionType:TabSwitcherSessionType::REGULAR_SESSION];
  [cell setAppearanceForTabTitle:[self.dataSource titleAtIndex:indexPath.item]
                         favicon:nil
                        cellSize:CGSizeZero];
  return cell;
}

#pragma mark - ZoomTransitionDelegate methods

- (CGRect)rectForZoomWithKey:(NSObject*)key inView:(UIView*)view {
  NSIndexPath* cellPath = base::mac::ObjCCastStrict<NSIndexPath>(key);
  if (!key)
    return CGRectNull;
  UICollectionViewCell* cell = [self.grid cellForItemAtIndexPath:cellPath];
  return [view convertRect:cell.bounds fromView:cell];
}

#pragma mark - SettingsActions

- (void)showSettings:(id)sender {
  [self.settingsCommandHandler showSettings];
}

#pragma mark - TabGridActions

- (void)showTabGrid:(id)sender {
  [self.tabGridCommandHandler showTabGrid];
}

#pragma mark - SessionCellDelegate

- (TabSwitcherCache*)tabSwitcherCache {
  // PLACEHOLDER: return image cache.
  return nil;
}

- (void)cellPressed:(UICollectionViewCell*)cell {
  [self.tabCommandHandler showTabAtIndexPath:[self.grid indexPathForCell:cell]];
}

- (void)deleteButtonPressedForCell:(UICollectionViewCell*)cell {
  // PLACEHOLDER: handle close tab button.
}

@end
