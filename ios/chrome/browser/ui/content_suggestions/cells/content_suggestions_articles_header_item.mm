// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_articles_header_item.h"

#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Leading and trailing margin for label and button.
const CGFloat kTextMargin = 13;
// Color for label text.
const int kLabelColorRGB = 0x6D6D72;
}

#pragma mark - ContentSuggestionsArticlesHeaderItem

@interface ContentSuggestionsArticlesHeaderItem ()

@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy) void (^callback)();

@end

@implementation ContentSuggestionsArticlesHeaderItem

@synthesize expanded = _expanded;
@synthesize title = _title;
@synthesize callback = _callback;

- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                    callback:(void (^)())callback {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsArticlesHeaderCell class];
    _title = [title copy];
    _callback = [callback copy];
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsArticlesHeaderCell*)cell {
  [super configureCell:cell];
  [cell.button
      setTitle:self.expanded
                   ? l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_HIDE)
                   : l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_SHOW)
      forState:UIControlStateNormal];
  cell.label.text = [self.title uppercaseString];
  cell.delegate = self;
}

#pragma mark ContentSuggestionsArticlesHeaderCellDelegate

- (void)cellButtonTapped:(ContentSuggestionsArticlesHeaderCell*)cell {
  if (self.callback) {
    self.callback();
  }
}

@end

#pragma mark - ContentSuggestionsArticlesHeaderCell

@implementation ContentSuggestionsArticlesHeaderCell

@synthesize button = _button;
@synthesize delegate = _delegate;
@synthesize label = _label;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _label = [[UILabel alloc] init];
    _label.translatesAutoresizingMaskIntoConstraints = NO;
    _label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _label.textColor = UIColorFromRGB(kLabelColorRGB, 1.0);
    _label.adjustsFontSizeToFitWidth = YES;

    _button = [UIButton buttonWithType:UIButtonTypeSystem];
    _button.translatesAutoresizingMaskIntoConstraints = NO;
    _button.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    [_button addTarget:self
                  action:@selector(buttonTapped)
        forControlEvents:UIControlEventTouchUpInside];

    [self.contentView addSubview:_button];
    [self.contentView addSubview:_label];
    [NSLayoutConstraint activateConstraints:@[
      [_label.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTextMargin],
      [_label.topAnchor constraintEqualToAnchor:self.contentView.topAnchor],
      [_label.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor],
      [_button.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTextMargin],
      [_button.topAnchor constraintEqualToAnchor:self.contentView.topAnchor],
      [_button.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor],
      [_label.trailingAnchor
          constraintLessThanOrEqualToAnchor:_button.leadingAnchor]
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.delegate = nil;
}

#pragma mark Private

// Callback for the button action.
- (void)buttonTapped {
  [self.delegate cellButtonTapped:self];
}

@end
