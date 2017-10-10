// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_table_cell.h"

#include "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Preferred image size in points.
const CGFloat kBookmarkTableCellDefaultImageSize = 16.0;

// Padding in table cell.
const CGFloat kBookmarkTableCellImagePadding = 16.0;
}  // namespace

@interface BookmarkTableCell ()<UITextFieldDelegate>

// The label, that displays placeholder text when favicon is missing.
@property(nonatomic, strong) UILabel* placeholderLabel;

// The title text.
@property(nonatomic, strong) UITextField* titleText;

@end

@implementation BookmarkTableCell
@synthesize placeholderLabel = _placeholderLabel;
@synthesize titleText = _titleText;
@synthesize textDelegate = _textDelegate;

#pragma mark - Initializer

- (instancetype)initWithReuseIdentifier:(NSString*)bookmarkCellIdentifier {
  self = [super initWithStyle:UITableViewCellStyleDefault
              reuseIdentifier:bookmarkCellIdentifier];
  if (self) {
    _titleText = [[UITextField alloc] initWithFrame:CGRectZero];
    _titleText.textColor = [[MDCPalette greyPalette] tint900];
    _titleText.font = [MDCTypography subheadFont];
    _titleText.userInteractionEnabled = NO;
    [self.contentView addSubview:_titleText];

    self.imageView.clipsToBounds = YES;
    [self.imageView setHidden:NO];

    _placeholderLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _placeholderLabel.textAlignment = NSTextAlignmentCenter;
    _placeholderLabel.font = [MDCTypography captionFont];

    [_placeholderLabel setHidden:YES];
    [self.contentView addSubview:_placeholderLabel];
  }
  return self;
}

#pragma mark - Public

- (void)setNode:(const bookmarks::BookmarkNode*)node {
  self.titleText.text = bookmark_utils_ios::TitleForBookmarkNode(node);
  self.titleText.accessibilityIdentifier = self.titleText.text;

  self.imageView.image = [UIImage imageNamed:@"bookmark_gray_folder_new"];
  if (node->is_folder()) {
    [self setAccessoryType:UITableViewCellAccessoryDisclosureIndicator];
  } else {
    [self setAccessoryType:UITableViewCellAccessoryNone];
  }
}

- (void)startEdit {
  // TODO(crbug.com/695749): Prevent the keyboard from overlapping the editing
  // cell.
  // The folder name will be submitted only when the return key is pressed.
  // It will revert to 'New Folder' when the following happen:
  // 1. Click on 'Done' or 'Back' of the navigation bar.
  // 2. Click on 'New Folder' or 'Select' of the context bar.
  // 3. Click on other folder or bookmark.
  self.titleText.userInteractionEnabled = YES;
  self.titleText.enablesReturnKeyAutomatically = YES;
  self.titleText.keyboardType = UIKeyboardTypeDefault;
  self.titleText.returnKeyType = UIReturnKeyDone;
  self.titleText.accessibilityIdentifier = @"bookmark_editing_text";
  self.accessoryType = UITableViewCellAccessoryNone;
  [self.titleText becomeFirstResponder];
  // selectAll doesn't work immediately after calling becomeFirstResponder.
  // Do selectAll on the next run loop.
  dispatch_async(dispatch_get_main_queue(), ^{
    if ([self.titleText isFirstResponder]) {
      [self.titleText selectAll:nil];
    }
  });
  self.titleText.delegate = self;
}

- (void)stopEdit {
  [self.textDelegate textDidChangeTo:self.titleText.text];
  self.titleText.userInteractionEnabled = NO;
  [self.titleText endEditing:YES];
}

+ (NSString*)reuseIdentifier {
  return @"BookmarkTableCellIdentifier";
}

+ (CGFloat)preferredImageSize {
  return kBookmarkTableCellDefaultImageSize;
}

- (void)setImage:(UIImage*)image {
  [self.imageView setHidden:NO];
  [self.placeholderLabel setHidden:YES];

  [self.imageView setImage:image];
  [self setNeedsLayout];
}

- (void)setPlaceholderText:(NSString*)text
                 textColor:(UIColor*)textColor
           backgroundColor:(UIColor*)backgroundColor {
  [self.imageView setHidden:YES];
  [self.placeholderLabel setHidden:NO];

  self.placeholderLabel.backgroundColor = backgroundColor;
  self.placeholderLabel.textColor = textColor;
  self.placeholderLabel.text = text;
  [self.placeholderLabel sizeToFit];
  [self setNeedsLayout];
}

#pragma mark - Layout

- (void)prepareForReuse {
  self.imageView.image = nil;
  self.placeholderLabel.hidden = YES;
  self.imageView.hidden = NO;
  self.titleText.text = nil;
  self.titleText.accessibilityIdentifier = nil;
  self.titleText.userInteractionEnabled = NO;
  self.textDelegate = nil;
  [super prepareForReuse];
}

- (void)layoutSubviews {
  [super layoutSubviews];
  CGFloat titleTextStart =
      kBookmarkTableCellDefaultImageSize + kBookmarkTableCellImagePadding * 2;
  // TODO(crbug.com/695749): Investigate why this separator inset is not
  // set correctly in landscape mode.
  [self setSeparatorInset:UIEdgeInsetsMake(0, titleTextStart, 0, 0)];

  // TODO(crbug.com/695749): Investigate using constraints instead of manual
  // layout.
  self.imageView.contentMode = UIViewContentModeScaleAspectFill;
  CGRect frame = CGRectMake(kBookmarkTableCellImagePadding, 0,
                            kBookmarkTableCellDefaultImageSize,
                            kBookmarkTableCellDefaultImageSize);
  self.imageView.frame = frame;
  self.imageView.center =
      CGPointMake(self.imageView.center.x, self.contentView.center.y);

  self.placeholderLabel.frame = frame;
  self.placeholderLabel.center =
      CGPointMake(self.placeholderLabel.center.x, self.contentView.center.y);

  self.titleText.frame = CGRectMake(
      titleTextStart, 0,
      self.contentView.frame.size.width - titleTextStart -
          self.accessoryView.bounds.size.width - kBookmarkTableCellImagePadding,
      self.contentView.frame.size.height);
}

#pragma mark - UITextFieldDelegate

// This method hides the keyboard when the return key is pressed.
- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [self.textDelegate textDidChangeTo:self.titleText.text];
  self.titleText.userInteractionEnabled = NO;
  [textField endEditing:YES];
  return YES;
}

@end
