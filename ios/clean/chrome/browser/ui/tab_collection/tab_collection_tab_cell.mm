// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_tab_cell.h"

#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_button.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kBorderMargin = 6.0f;
const CGFloat kSelectedBorderCornerRadius = 8.0f;
const CGFloat kSelectedBorderWidth = 4.0f;
}

@interface TabCollectionTabCell ()
@property(nonatomic, strong) TabCollectionItem* item;
@property(nonatomic, strong) UILabel* titleLabel;
@property(nonatomic, strong) UIImageView* favicon;
@property(nonatomic, strong) TabSwitcherButton* snapshotButton;
@end

@implementation TabCollectionTabCell
@synthesize item = _item;
@dynamic titleLabel;
@dynamic favicon;
@dynamic snapshotButton;

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    [self setupSelectedBackgroundView];
  }
  return self;
}

#pragma mark - Cell lifecycle

- (void)prepareForReuse {
  [super prepareForReuse];
  self.item = nil;
}

#pragma mark - Public methods

- (void)configureCell:(TabCollectionItem*)item
        snapshotCache:(SnapshotCache*)snapshotCache {
  DCHECK(item);
  self.item = item;
  self.titleLabel.text = item.title;
  self.snapshotButton.accessibilityIdentifier =
      [NSString stringWithFormat:@"%@_button", item.title];
  self.contentView.accessibilityLabel = item.title;
  self.favicon.image = NativeImage(IDR_IOS_OMNIBOX_HTTP);
  __weak TabCollectionTabCell* weakSelf = self;
  [snapshotCache
      retrieveImageForSessionID:self.item.tabID
                       callback:^(UIImage* snapshot) {
                         // PLACEHOLDER: This operation will be cancellable.
                         if ([weakSelf.item.tabID isEqualToString:item.tabID]) {
                           weakSelf.snapshot = snapshot;
                         }
                       }];
}

#pragma mark - Private methods

- (void)setupSelectedBackgroundView {
  self.selectedBackgroundView = [[UIView alloc] init];
  self.selectedBackgroundView.backgroundColor = [UIColor blackColor];

  UIView* border = [[UIView alloc] init];
  border.translatesAutoresizingMaskIntoConstraints = NO;
  border.backgroundColor = [UIColor blackColor];
  border.layer.cornerRadius = kSelectedBorderCornerRadius;
  border.layer.borderWidth = kSelectedBorderWidth;
  border.layer.borderColor = self.tintColor.CGColor;
  [self.selectedBackgroundView addSubview:border];
  [NSLayoutConstraint activateConstraints:@[
    [border.topAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.topAnchor
                       constant:-kBorderMargin],
    [border.leadingAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.leadingAnchor
                       constant:-kBorderMargin],
    [border.trailingAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.trailingAnchor
                       constant:kBorderMargin],
    [border.bottomAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.bottomAnchor
                       constant:kBorderMargin]
  ]];
}

@end
