// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_stack_item.h"

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_stack_item_actions.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kStackSpacing = 8;
}

@implementation ContentSuggestionsStackItem {
  NSString* _title;
  NSString* _subtitle;
}

- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsStackCell class];
    _title = [title copy];
    _subtitle = [subtitle copy];
  }
  return self;
}

#pragma mark - CollectionViewItem

- (void)configureCell:(ContentSuggestionsStackCell*)cell {
  [super configureCell:cell];
  [cell.titleButton setTitle:_title forState:UIControlStateNormal];
  cell.detailTextLabel.text = _subtitle;
}

@end

@implementation ContentSuggestionsStackCell

@synthesize titleButton = _titleButton;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor clearColor];
    self.contentView.backgroundColor = [UIColor clearColor];
    self.backgroundView.backgroundColor = [UIColor clearColor];

    UIControl* itemDisplay = [[UIControl alloc] init];
    itemDisplay.layer.borderColor = [UIColor blackColor].CGColor;
    itemDisplay.layer.borderWidth = 1;
    itemDisplay.backgroundColor = [UIColor whiteColor];
    [itemDisplay addTarget:nil
                    action:@selector(openReadingListFirstItem:)
          forControlEvents:UIControlEventTouchUpInside];
    itemDisplay.translatesAutoresizingMaskIntoConstraints = NO;

    _titleButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _detailTextLabel = [[UILabel alloc] init];
    _titleButton.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;

    [itemDisplay addSubview:_titleButton];
    [itemDisplay addSubview:_detailTextLabel];

    // Placeholder. This view is only used to display a border below the item.
    UIView* secondView = [[UIView alloc] init];
    secondView.backgroundColor = [UIColor whiteColor];
    secondView.translatesAutoresizingMaskIntoConstraints = NO;
    secondView.layer.borderColor = [UIColor blackColor].CGColor;
    secondView.layer.borderWidth = 1;

    // Placeholder. This view is only used to display a border below the item.
    UIView* thirdView = [[UIView alloc] init];
    thirdView.backgroundColor = [UIColor whiteColor];
    thirdView.translatesAutoresizingMaskIntoConstraints = NO;
    thirdView.layer.borderColor = [UIColor blackColor].CGColor;
    thirdView.layer.borderWidth = 1;

    // Placeholder. This view is only used to display a border below the item.
    UIView* fourthView = [[UIView alloc] init];
    fourthView.backgroundColor = [UIColor whiteColor];
    fourthView.translatesAutoresizingMaskIntoConstraints = NO;
    fourthView.layer.borderColor = [UIColor blackColor].CGColor;
    fourthView.layer.borderWidth = 1;

    [self.contentView addSubview:itemDisplay];
    [self.contentView insertSubview:secondView belowSubview:itemDisplay];
    [self.contentView insertSubview:thirdView belowSubview:secondView];
    [self.contentView insertSubview:fourthView belowSubview:thirdView];

    [NSLayoutConstraint activateConstraints:@[
      [itemDisplay.widthAnchor constraintEqualToAnchor:secondView.widthAnchor],
      [itemDisplay.heightAnchor
          constraintEqualToAnchor:secondView.heightAnchor],
      [itemDisplay.widthAnchor constraintEqualToAnchor:thirdView.widthAnchor],
      [itemDisplay.heightAnchor constraintEqualToAnchor:thirdView.heightAnchor],
      [itemDisplay.widthAnchor constraintEqualToAnchor:fourthView.widthAnchor],
      [itemDisplay.heightAnchor
          constraintEqualToAnchor:fourthView.heightAnchor],
    ]];

    ApplyVisualConstraintsWithMetrics(
        @[
          @"H:|-[title]-|",
          @"H:|-[text]-|",
          @"H:|[item]-(fourSpace)-|",
          @"H:|-(oneSpace)-[second]-(threeSpace)-|",
          @"H:|-(twoSpace)-[third]-(twoSpace)-|",
          @"H:|-(threeSpace)-[fourth]-(oneSpace)-|",
          @"V:|-[title]-[text]-|",
          @"V:|[item]",
          @"V:|-(oneSpace)-[second]",
          @"V:|-(twoSpace)-[third]",
          @"V:|-(threeSpace)-[fourth]|",
        ],
        @{
          @"title" : _titleButton,
          @"text" : _detailTextLabel,
          @"item" : itemDisplay,
          @"second" : secondView,
          @"third" : thirdView,
          @"fourth" : fourthView,
        },
        @{
          @"oneSpace" : @(kStackSpacing * 1),
          @"twoSpace" : @(kStackSpacing * 2),
          @"threeSpace" : @(kStackSpacing * 3),
          @"fourSpace" : @(kStackSpacing * 4)
        });
  }
  return self;
}

@end
