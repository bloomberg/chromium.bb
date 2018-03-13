// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The inner insets of the View content.
const CGFloat kMargin = 16;
}

@implementation TableViewTextHeaderFooterItem
@synthesize text = _text;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewTextHeaderFooterView class];
  }
  return self;
}

- (void)configureHeaderFooterView:(UITableViewHeaderFooterView*)headerFooter
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:headerFooter withStyler:styler];

  // Set the contentView backgroundColor, not the header's.
  headerFooter.contentView.backgroundColor = styler.tableViewBackgroundColor;

  TableViewTextHeaderFooterView* header =
      base::mac::ObjCCastStrict<TableViewTextHeaderFooterView>(headerFooter);
  header.textLabel.text = self.text;
  header.textLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
}

@end

#pragma mark - TableViewTextHeaderFooter

@implementation TableViewTextHeaderFooterView
@synthesize textLabel = _textLabel;

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    // Text Label, set font sizes using dynamic type.
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;

    // Container View.
    UIView* containerView = [[UIView alloc] init];
    containerView.translatesAutoresizingMaskIntoConstraints = NO;

    // Add subviews to View Hierarchy.
    [containerView addSubview:_textLabel];
    [self.contentView addSubview:containerView];

    // Set and activate constraints.
    [NSLayoutConstraint activateConstraints:@[
      // Container Constraints.
      [containerView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kMargin],
      [containerView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kMargin],
      [containerView.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                      constant:kMargin],
      [containerView.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor
                                   constant:-kMargin],
      [containerView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      // Title Label Constraints.
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:containerView.leadingAnchor],
      [_textLabel.topAnchor constraintEqualToAnchor:containerView.topAnchor],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:containerView.bottomAnchor],
    ]];
  }
  return self;
}

@end
