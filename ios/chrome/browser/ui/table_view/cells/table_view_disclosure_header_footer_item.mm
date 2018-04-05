// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_disclosure_header_footer_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TableViewDisclosureHeaderFooterItem
@synthesize subtitleText = _subtitleText;
@synthesize text = _text;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewDisclosureHeaderFooterView class];
  }
  return self;
}

- (void)configureHeaderFooterView:(UITableViewHeaderFooterView*)headerFooter
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:headerFooter withStyler:styler];

  // Set the contentView backgroundColor, not the header's.
  headerFooter.contentView.backgroundColor = styler.tableViewBackgroundColor;

  TableViewDisclosureHeaderFooterView* header =
      base::mac::ObjCCastStrict<TableViewDisclosureHeaderFooterView>(
          headerFooter);
  header.titleLabel.text = self.text;
  header.subtitleLabel.text = self.subtitleText;
}

@end

#pragma mark - TableViewDisclosureHeaderFooterView

@interface TableViewDisclosureHeaderFooterView ()
// Animator that handles all cell animations.
@property(strong, nonatomic) UIViewPropertyAnimator* cellAnimator;
@end

@implementation TableViewDisclosureHeaderFooterView
@synthesize cellAnimator = _cellAnimator;
@synthesize subtitleLabel = _subtitleLabel;
@synthesize titleLabel = _titleLabel;

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    // Labels, set font sizes using dynamic type.
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    _subtitleLabel = [[UILabel alloc] init];
    _subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    _subtitleLabel.textColor = [UIColor lightGrayColor];

    // Vertical StackView.
    UIStackView* verticalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleLabel, _subtitleLabel ]];
    verticalStack.axis = UILayoutConstraintAxisVertical;
    verticalStack.spacing = kTableViewVerticalLabelStackSpacing;

    // Disclosure ImageView.
    UIImageView* disclosureImageView = [[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"table_view_cell_chevron"]];
    [disclosureImageView
        setContentHuggingPriority:UILayoutPriorityDefaultHigh
                          forAxis:UILayoutConstraintAxisHorizontal];

    // Horizontal StackView.
    UIStackView* horizontalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ verticalStack, disclosureImageView ]];
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

- (void)animateHighlight {
  UIColor* originalBackgroundColor = self.contentView.backgroundColor;
  self.cellAnimator = [[UIViewPropertyAnimator alloc]
      initWithDuration:kTableViewCellSelectionAnimationDuration
                 curve:UIViewAnimationCurveLinear
            animations:^{
              self.contentView.backgroundColor =
                  UIColorFromRGB(kTableViewHighlightedCellColor,
                                 kTableViewHighlightedCellColorAlpha);
            }];
  __weak TableViewDisclosureHeaderFooterView* weakSelf = self;
  [self.cellAnimator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    weakSelf.contentView.backgroundColor = originalBackgroundColor;
  }];
  [self.cellAnimator startAnimation];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  if (self.cellAnimator.isRunning)
    [self.cellAnimator stopAnimation:YES];
}

@end
