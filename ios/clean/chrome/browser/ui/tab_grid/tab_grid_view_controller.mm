// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/clean/chrome/browser/ui/actions/settings_actions.h"
#import "ios/clean/chrome/browser/ui/actions/tab_grid_actions.h"
#import "ios/clean/chrome/browser/ui/commands/settings_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tab_commands.h"
#import "ios/clean/chrome/browser/ui/commands/tab_grid_commands.h"
#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_tab_cell.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* const kTabGridCellIdentifier = @"tabGridCell";
const CGFloat kSpace = 20;
const CGFloat kTabSize = 150;
}

@interface TabGridViewController ()<SettingsActions,
                                    TabGridActions,
                                    UICollectionViewDataSource,
                                    UICollectionViewDelegate>
@property(nonatomic, weak) UICollectionView* grid;
@end

@implementation TabGridViewController

@synthesize dataSource = _dataSource;
@synthesize settingsCommandHandler = _settingsCommandHandler;
@synthesize tabGridCommandHandler = _tabGridCommandHandler;
@synthesize tabCommandHandler = _tabCommandHandler;
@synthesize grid = _grid;

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

  UICollectionViewFlowLayout* layout =
      [[UICollectionViewFlowLayout alloc] init];
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
  [self.grid registerClass:[TabGridTabCell class]
      forCellWithReuseIdentifier:kTabGridCellIdentifier];

  [NSLayoutConstraint activateConstraints:@[
    [self.grid.topAnchor constraintEqualToAnchor:stripe.bottomAnchor],
    [self.grid.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [self.grid.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [self.grid.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
  ]];
}

- (void)viewWillAppear:(BOOL)animated {
  [self.grid reloadData];
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
  TabGridTabCell* cell =
      base::mac::ObjCCastStrict<TabGridTabCell>([collectionView
          dequeueReusableCellWithReuseIdentifier:kTabGridCellIdentifier
                                    forIndexPath:indexPath]);
  cell.contentView.backgroundColor = [UIColor purpleColor];
  cell.selected = YES;
  cell.label.text = [self.dataSource titleAtIndex:indexPath.item];
  return cell;
}

#pragma mark - UICollectionViewDelegate methods

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [self.tabCommandHandler showTabAtIndexPath:indexPath];
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

@end
