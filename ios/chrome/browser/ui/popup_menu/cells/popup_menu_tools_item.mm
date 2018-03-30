// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_tools_item.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kImageLength = 28;
const CGFloat kCellHeight = 44;
const CGFloat kImageTextMargin = 11;
const CGFloat kMargin = 15;
const CGFloat kImageTopMargin = 8;
}

@implementation PopupMenuToolsItem

@synthesize actionIdentifier = _actionIdentifier;
@synthesize image = _image;
@synthesize title = _title;
@synthesize enabled = _enabled;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PopupMenuToolsCell class];
    _enabled = YES;
  }
  return self;
}

- (void)configureCell:(PopupMenuToolsCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.titleLabel.text = self.title;
  cell.imageView.image = self.image;
  cell.userInteractionEnabled = self.enabled;
}

#pragma mark - PopupMenuItem

- (CGSize)cellSizeForWidth:(CGFloat)width {
  return [self.cellClass sizeForWidth:width title:self.title];
}

@end

#pragma mark - PopupMenuToolsCell

@interface PopupMenuToolsCell ()

// Title label for the cell, redefined as readwrite.
@property(nonatomic, strong, readwrite) UILabel* titleLabel;
// Image view for the cell, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIImageView* imageView;

@end

@implementation PopupMenuToolsCell

@synthesize imageView = _imageView;
@synthesize titleLabel = _titleLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.numberOfLines = 0;
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;

    _imageView = [[UIImageView alloc] init];
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;

    [self.contentView addSubview:_titleLabel];
    [self.contentView addSubview:_imageView];

    ApplyVisualConstraintsWithMetrics(
        @[
          @"H:|-(margin)-[image(imageSize)]-(textImage)-[text]-(margin)-|",
          @"V:|-(imageTopMargin)-[image(imageSize)]", @"V:|[text]|"
        ],
        @{@"image" : _imageView, @"text" : _titleLabel}, @{
          @"margin" : @(kMargin),
          @"imageSize" : @(kImageLength),
          @"textImage" : @(kImageTextMargin),
          @"imageTopMargin" : @(kImageTopMargin),
        });

    [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kCellHeight]
        .active = YES;
  }
  return self;
}

+ (CGSize)sizeForWidth:(CGFloat)width title:(NSString*)title {
  // This is not using a prototype cell and autolayout for performance reasons.
  CGFloat nonTitleElementWidth = kImageLength + 2 * kMargin + kImageTextMargin;
  // The width should be enough to contain more than the image.
  DCHECK(width > nonTitleElementWidth);

  CGSize titleSize = CGSizeMake(width - nonTitleElementWidth,
                                [UIScreen mainScreen].bounds.size.height);
  NSDictionary* attributes = @{NSFontAttributeName : [self cellFont]};
  CGRect rectForString =
      [title boundingRectWithSize:titleSize
                          options:NSStringDrawingUsesLineFragmentOrigin
                       attributes:attributes
                          context:nil];
  CGSize size = rectForString.size;
  size.height = MAX(size.height, kCellHeight);
  size.width += nonTitleElementWidth;
  return size;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.userInteractionEnabled = YES;
}

- (void)setUserInteractionEnabled:(BOOL)userInteractionEnabled {
  [super setUserInteractionEnabled:userInteractionEnabled];
  if (userInteractionEnabled) {
    self.titleLabel.textColor = self.tintColor;
    self.imageView.tintColor = self.tintColor;
  } else {
    self.titleLabel.textColor = [[self class] disabledColor];
    self.imageView.tintColor = [[self class] disabledColor];
  }
}

#pragma mark - Private

// Returns the font used by this cell's label.
+ (UIFont*)cellFont {
  static UIFont* font;
  if (!font) {
    PopupMenuToolsCell* cell =
        [[PopupMenuToolsCell alloc] initWithStyle:UITableViewCellStyleDefault
                                  reuseIdentifier:@"fakeID"];
    font = cell.titleLabel.font;
  }
  return font;
}

// Returns the color of the disabled button's title.
+ (UIColor*)disabledColor {
  static UIColor* systemTintColorForDisabled = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
    systemTintColorForDisabled =
        [button titleColorForState:UIControlStateDisabled];
  });
  return systemTintColorForDisabled;
}

@end
