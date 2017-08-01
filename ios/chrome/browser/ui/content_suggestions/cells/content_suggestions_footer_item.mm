// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_footer_item.h"

#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kButtonMargin = 4;
const CGFloat kButtonPadding = 16;
}

#pragma mark - ContentSuggestionsFooterItem

@interface ContentSuggestionsFooterItem ()

@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy) ProceduralBlock block;

@end

@implementation ContentSuggestionsFooterItem

@synthesize title = _title;
@synthesize block = _block;

- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                       block:(ProceduralBlock)block {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsFooterCell class];
    _title = [title copy];
    _block = [block copy];
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsFooterCell*)cell {
  [super configureCell:cell];
  [cell.button setTitle:self.title forState:UIControlStateNormal];
  [cell.button addTarget:self
                  action:@selector(executeBlock)
        forControlEvents:UIControlEventTouchUpInside];
}

#pragma mark - Private

// Executes the |_block| if not nil.
- (void)executeBlock {
  if (self.block) {
    self.block();
  }
}

@end

#pragma mark - ContentSuggestionsFooterCell

@implementation ContentSuggestionsFooterCell

@synthesize button = _button;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _button = [UIButton buttonWithType:UIButtonTypeSystem];
    _button.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_button];
    _button.contentEdgeInsets =
        UIEdgeInsetsMake(0, kButtonPadding, 0, kButtonPadding);
    ApplyVisualConstraintsWithMetrics(
        @[ @"V:|-(margin)-[button]-(margin)-|", @"H:|-(margin)-[button]" ],
        @{@"button" : _button},
        @{ @"margin" : @(kButtonMargin) });
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.button removeTarget:nil
                     action:NULL
           forControlEvents:UIControlEventAllEvents];
}

@end
