// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_text_item.h"

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_text_item_actions.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSuggestionsTextItem ()

@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy) NSString* subtitle;

@end

@implementation ContentSuggestionsTextItem

@synthesize title = _title;
@synthesize subtitle = _subtitle;

- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsTextCell class];
    _title = [title copy];
    _subtitle = [subtitle copy];
  }
  return self;
}

#pragma mark - CollectionViewItem

- (void)configureCell:(ContentSuggestionsTextCell*)cell {
  [super configureCell:cell];
  [cell.titleButton setTitle:self.title forState:UIControlStateNormal];
  cell.detailTextLabel.text = self.subtitle;
}

@end

#pragma mark - ContentSuggestionsTextCell

@implementation ContentSuggestionsTextCell

@synthesize titleButton = _titleButton;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    id<MDCTypographyFontLoading> fontLoader = [MDCTypography fontLoader];
    _titleButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _titleButton.titleLabel.font = [fontLoader mediumFontOfSize:16];
    _titleButton.titleLabel.textColor = [[MDCPalette greyPalette] tint900];
    [_titleButton addTarget:nil
                     action:@selector(addNewTextItem:)
           forControlEvents:UIControlEventTouchUpInside];

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.font = [fontLoader mediumFontOfSize:14];
    _detailTextLabel.textColor = [[MDCPalette greyPalette] tint500];

    UIStackView* labelsStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleButton, _detailTextLabel ]];
    labelsStack.axis = UILayoutConstraintAxisVertical;

    [self.contentView addSubview:labelsStack];
    labelsStack.layoutMarginsRelativeArrangement = YES;
    labelsStack.layoutMargins = UIEdgeInsetsMake(16, 16, 16, 16);
    labelsStack.alignment = UIStackViewAlignmentCenter;

    labelsStack.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [self.contentView.leadingAnchor
          constraintEqualToAnchor:labelsStack.leadingAnchor],
      [self.contentView.trailingAnchor
          constraintEqualToAnchor:labelsStack.trailingAnchor],
      [self.contentView.topAnchor
          constraintEqualToAnchor:labelsStack.topAnchor],
      [self.contentView.bottomAnchor
          constraintEqualToAnchor:labelsStack.bottomAnchor]
    ]];

    self.backgroundColor = [UIColor whiteColor];
  }
  return self;
}

@end
