// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_text_field_item.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/ui/text_field_styling.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BookmarkTextFieldCell ()
@property(nonatomic, readwrite, strong)
    UITextField<TextFieldStyling>* textField;
@end

@implementation BookmarkTextFieldItem

@synthesize text = _text;
@synthesize placeholder = _placeholder;
@synthesize delegate = _delegate;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [BookmarkTextFieldCell class];
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureCell:(BookmarkTextFieldCell*)cell {
  [super configureCell:cell];
  cell.textField.text = self.text;
  cell.textField.placeholder = self.placeholder;
  cell.textField.tag = self.type;
  [cell.textField addTarget:self
                     action:@selector(textFieldDidChange:)
           forControlEvents:UIControlEventEditingChanged];
  cell.textField.delegate = self.delegate;
  cell.textField.accessibilityIdentifier =
      [NSString stringWithFormat:@"%@_textField", self.accessibilityIdentifier];
}

#pragma mark UIControlEventEditingChanged

- (void)textFieldDidChange:(UITextField*)textField {
  DCHECK_EQ(textField.tag, self.type);
  self.text = textField.text;
  [self.delegate textDidChangeForItem:self];
}

@end

@implementation BookmarkTextFieldCell

@synthesize textField = _textField;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;
    self.accessibilityTraits |= UIAccessibilityTraitButton;

    _textField =
        ios::GetChromeBrowserProvider()->CreateStyledTextField(CGRectZero);
    _textField.translatesAutoresizingMaskIntoConstraints = NO;
    _textField.textColor = bookmark_utils_ios::darkTextColor();
    _textField.clearButtonMode = UITextFieldViewModeWhileEditing;
    _textField.placeholderStyle =
        TextFieldStylingPlaceholderFloatingPlaceholder;
    [self.contentView addSubview:_textField];
    const CGFloat kHorizontalPadding = 15;
    const CGFloat kTopPadding = 8;
    [NSLayoutConstraint activateConstraints:@[
      [_textField.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                               constant:kHorizontalPadding],
      [_textField.topAnchor constraintEqualToAnchor:self.topAnchor
                                           constant:kTopPadding],
      [_textField.trailingAnchor constraintEqualToAnchor:self.trailingAnchor
                                                constant:-kHorizontalPadding],
    ]];

    self.shouldHideSeparator = YES;
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.textField resignFirstResponder];
  [self.textField removeTarget:nil
                        action:NULL
              forControlEvents:UIControlEventAllEvents];
  self.textField.delegate = nil;
  self.textField.text = nil;
  self.textField.textValidator = nil;
}

- (NSString*)accessibilityLabel {
  return self.textField.text;
}

@end
