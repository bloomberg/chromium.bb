// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_activity_indicator_header_footer_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TableViewActivityIndicatorHeaderFooterItem
@synthesize subtitleText = _subtitleText;
@synthesize text = _text;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewActivityIndicatorHeaderFooterView class];
  }
  return self;
}

- (void)configureHeaderFooterView:(UITableViewHeaderFooterView*)headerFooter
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:headerFooter withStyler:styler];

  // Set the contentView backgroundColor, not the header's.
  headerFooter.contentView.backgroundColor = styler.tableViewBackgroundColor;

  TableViewActivityIndicatorHeaderFooterView* header =
      base::mac::ObjCCastStrict<TableViewActivityIndicatorHeaderFooterView>(
          headerFooter);
  header.titleLabel.text = self.text;
  header.subtitleLabel.text = self.subtitleText;
}

@end

#pragma mark - TableViewActivityIndicatorHeaderFooterView

@implementation TableViewActivityIndicatorHeaderFooterView
@synthesize subtitleLabel = _subtitleLabel;
@synthesize titleLabel = titleLabel;

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    // Labels, set font sizes using dynamic type.
    self.titleLabel = [[UILabel alloc] init];
    self.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    self.subtitleLabel = [[UILabel alloc] init];
    self.subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    self.subtitleLabel.textColor = [UIColor lightGrayColor];

    // Vertical StackView.
    UIStackView* verticalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ self.titleLabel, self.subtitleLabel ]];
    verticalStack.axis = UILayoutConstraintAxisVertical;
    verticalStack.spacing = kTableViewVerticalLabelStackSpacing;

    // Activity Indicator.
    UIActivityIndicatorView* activityIndicator =
        [[UIActivityIndicatorView alloc]
            initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleWhite];
    activityIndicator.color = [UIColor blueColor];
    [activityIndicator startAnimating];
    [activityIndicator
        setContentHuggingPriority:UILayoutPriorityDefaultHigh
                          forAxis:UILayoutConstraintAxisHorizontal];

    // Horizontal StackView.
    UIStackView* horizontalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ verticalStack, activityIndicator ]];
    horizontalStack.axis = UILayoutConstraintAxisHorizontal;
    horizontalStack.spacing = kTableViewCellViewSpacing;
    horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;
    horizontalStack.alignment = UIStackViewAlignmentCenter;

    // Add subviews to View Hierarchy.
    [self.contentView addSubview:horizontalStack];

    // Set and activate constraints.
    [NSLayoutConstraint activateConstraints:@[
      // Horizontal Stack Constraints.
      [horizontalStack.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewCellViewSpacing],
      [horizontalStack.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewCellViewSpacing],
      [horizontalStack.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                      constant:kTableViewCellViewSpacing],
      [horizontalStack.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor
                                   constant:-kTableViewCellViewSpacing],
      [horizontalStack.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor]
    ]];
  }
  return self;
}

@end
