// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_collection_cells.h"

#import <QuartzCore/QuartzCore.h>

#include "base/logging.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_extended_button.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/third_party/material_components_ios/src/components/Ink/src/MaterialInk.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"
#include "url/gurl.h"

namespace {
const CGFloat kBookmarkItemCellDefaultImageSize = 40.0;
const CGFloat kBookmarkFolderCellDefaultImageSize = 24.0;
}  // namespace

@interface BookmarkCell () {
 @protected
  // Subclasses should set these in the constructor with the wanted values.
  CGFloat _imageSize;

 @private
  base::mac::ObjCPropertyReleaser _propertyReleaser_BookmarkCell;
}
// Redefined to be read-write.
@property(nonatomic, retain) UILabel* titleLabel;
// Redefined to readwrite.
@property(nonatomic, retain) UIImageView* imageView;
// Label to show placeholder text when there is no image displayed.
@property(nonatomic, retain) UILabel* placeholderLabel;
// When the cell is selected for editing, a cover is shown with a blue color.
// Subclasses should insert new views below this view.
@property(nonatomic, retain) UIView* highlightCover;
// Lists the accessibility elements that are to be seen by UIAccessibility.
@property(nonatomic, readonly) NSMutableArray* accessibilityElements;
// Location of the last touch on the cell.
@property(nonatomic, assign) CGPoint touchLocation;
// The view doing the highlight animation. Only set while the cell is
// highlighted.
@property(nonatomic, retain) MDCInkView* touchFeedbackView;
@property(nonatomic, retain) BookmarkExtendedButton* button;
@property(nonatomic, assign) SEL buttonAction;
@property(nonatomic, assign) id buttonTarget;
@end

@implementation BookmarkCell
@synthesize accessibilityElements = _accessibilityElements;
@synthesize titleLabel = _titleLabel;
@synthesize imageView = _imageView;
@synthesize highlightCover = _highlightCover;
@synthesize shouldAnimateImageChanges = _shouldAnimateImageChanges;
@synthesize touchLocation = _touchLocation;
@synthesize touchFeedbackView = _touchFeedbackView;
@synthesize button = _button;
@synthesize buttonAction = _buttonAction;
@synthesize buttonTarget = _buttonTarget;
@synthesize placeholderLabel = _placeholderLabel;

+ (NSString*)reuseIdentifier {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _propertyReleaser_BookmarkCell.Init(self, [BookmarkCell class]);
    self.exclusiveTouch = YES;
    self.backgroundColor = [UIColor whiteColor];

    _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _titleLabel.textColor = bookmark_utils_ios::darkTextColor();
    self.titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_titleLabel];

    _imageView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _imageView.clipsToBounds = YES;
    self.imageView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_imageView];

    _placeholderLabel = [[UILabel alloc] init];
    _placeholderLabel.textAlignment = NSTextAlignmentCenter;
    [self.contentView addSubview:_placeholderLabel];

    _highlightCover = [[UIView alloc] initWithFrame:CGRectZero];
    _highlightCover.backgroundColor =
        [bookmark_utils_ios::blueColor() colorWithAlphaComponent:0.7];
    _highlightCover.hidden = YES;
    _highlightCover.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_highlightCover];

    self.button = base::scoped_nsobject<BookmarkExtendedButton>(
        [[BookmarkExtendedButton alloc] init]);
    self.button.contentMode = UIViewContentModeCenter;
    self.button.backgroundColor = [UIColor clearColor];
    [self.button addTarget:self
                    action:@selector(buttonTapped:)
          forControlEvents:UIControlEventTouchUpInside];
    self.button.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView insertSubview:self.button
                       belowSubview:self.highlightCover];

    _accessibilityElements = [[NSMutableArray alloc] init];
    self.contentView.isAccessibilityElement = YES;
    self.contentView.accessibilityTraits |= UIAccessibilityTraitButton;
    [_accessibilityElements addObject:self.contentView];
    [self.accessibilityElements addObject:self.button];
  }
  return self;
}

+ (BOOL)requiresConstraintBasedLayout {
  return YES;
}

- (void)updateConstraints {
  if (_imageSize) {
    // Create constraints.

    // Align all the views on the same horizontal line.
    AddSameCenterYConstraint(self.contentView, self.titleLabel);
    AddSameCenterYConstraint(self.contentView, self.imageView);
    AddSameCenterYConstraint(self.contentView, self.button);

    // clang-format off
    ApplyVisualConstraintsWithMetrics(
        @[
          @"H:|-leadingImageMargin-[image(imageSize)]",
          @"H:|-leadingMargin-[title]-[button(buttonSize)]|",
          @"V:[image(imageSize)]",
          @"V:[button(buttonSize)]",
          @"H:|[highlight]|",
          @"V:|[highlight]|"
        ],
        @{
          @"title" : self.titleLabel,
          @"image" : self.imageView,
          @"button" : self.button,
          @"highlight" : self.highlightCover
        },
        @{
          @"buttonSize" : [NSNumber numberWithFloat:32.0],
          @"leadingImageMargin" : [NSNumber numberWithFloat:16.0],
          @"leadingMargin" : [NSNumber numberWithFloat:64.0],
          @"imageSize" : [NSNumber numberWithFloat:_imageSize],
        },
        self.contentView);
    // clang-format on
  }

  [NSLayoutConstraint activateConstraints:@[
    [self.placeholderLabel.leadingAnchor
        constraintEqualToAnchor:self.imageView.leadingAnchor],
    [self.placeholderLabel.trailingAnchor
        constraintEqualToAnchor:self.imageView.trailingAnchor],
    [self.placeholderLabel.topAnchor
        constraintEqualToAnchor:self.imageView.topAnchor],
    [self.placeholderLabel.bottomAnchor
        constraintEqualToAnchor:self.imageView.bottomAnchor]
  ]];
  self.placeholderLabel.translatesAutoresizingMaskIntoConstraints = NO;

  [super updateConstraints];
}

- (void)setSelectedForEditing:(BOOL)selected animated:(BOOL)animated {
  self.highlightCover.hidden = !selected;
  if (selected)
    self.contentView.accessibilityTraits |= UIAccessibilityTraitSelected;
  else
    self.contentView.accessibilityTraits &= ~UIAccessibilityTraitSelected;
}

- (void)setImage:(UIImage*)image {
  self.imageView.image = image;
  self.placeholderLabel.hidden = YES;
  self.imageView.hidden = NO;

  if (!self.shouldAnimateImageChanges)
    return;

  CATransition* transition = [CATransition animation];
  transition.duration = 0.2f;
  transition.timingFunction = [CAMediaTimingFunction
      functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
  transition.type = kCATransitionFade;
  [self.imageView.layer addAnimation:transition forKey:nil];
}

- (void)setPlaceholderText:(NSString*)text
                 textColor:(UIColor*)textColor
           backgroundColor:(UIColor*)backgroundColor {
  self.placeholderLabel.hidden = NO;
  self.imageView.hidden = YES;

  self.placeholderLabel.backgroundColor = backgroundColor;
  self.placeholderLabel.textColor = textColor;
  self.placeholderLabel.text = text;
}

- (void)prepareForReuse {
  self.imageView.image = nil;
  self.imageView.backgroundColor = [UIColor whiteColor];
  self.highlightCover.hidden = YES;
  self.shouldAnimateImageChanges = NO;
  [self.touchFeedbackView cancelAllAnimationsAnimated:NO];
  [self.touchFeedbackView removeFromSuperview];
  self.touchFeedbackView = nil;
  [self updateWithTitle:nil];
  self.placeholderLabel.hidden = YES;
  self.imageView.hidden = NO;

  [super prepareForReuse];
}

- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  UITouch* touch = [touches anyObject];
  self.touchLocation = [touch locationInView:self];
  [super touchesBegan:touches withEvent:event];
}

- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
  UITouch* touch = [touches anyObject];
  self.touchLocation = [touch locationInView:self];
  [super touchesEnded:touches withEvent:event];
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];

  // To prevent selection messing up with ink, immediately remove ink.
  [self.touchFeedbackView removeFromSuperview];
  self.touchFeedbackView = nil;

  if (highlighted) {
    // Creates an ink feedback and animates it.
    base::scoped_nsobject<MDCInkView> touchFeedbackView(
        [[MDCInkView alloc] initWithFrame:self.bounds]);
    [self addSubview:touchFeedbackView];
    self.touchFeedbackView = touchFeedbackView;
    [self.touchFeedbackView startTouchBeganAnimationAtPoint:self.touchLocation
                                                 completion:nil];
  } else {
    [self.touchFeedbackView startTouchEndedAnimationAtPoint:self.touchLocation
                                                 completion:nil];
  }
}

- (void)updateWithTitle:(NSString*)title {
  self.titleLabel.text = title;
  [self setNeedsLayout];
  [self updateAccessibilityValues];
}

#pragma mark - Button
- (void)setButtonTarget:(id)target action:(SEL)action {
  self.buttonTarget = target;
  self.buttonAction = action;
}

- (void)buttonTapped:(id)target {
  [self.buttonTarget performSelector:self.buttonAction
                          withObject:self
                          withObject:target];
}

- (void)showButtonOfType:(bookmark_cell::ButtonType)buttonType
                animated:(BOOL)animated {
  switch (buttonType) {
    case bookmark_cell::ButtonNone:
      [self removeButtonAnimated:animated];
      break;

    case bookmark_cell::ButtonMenu: {
      [self addButtonWithImage:NativeImage(IDR_IOS_TOOLBAR_LIGHT_TOOLS)
                      animated:animated];
      break;
    }
  }
}

- (void)addButtonWithImage:(UIImage*)image animated:(BOOL)animated {
  // Set the image.
  [self.button setImage:image forState:UIControlStateNormal];

  // Animates the button in.
  [UIView animateWithDuration:animated ? 0.15 : 0
      delay:0
      options:UIViewAnimationOptionBeginFromCurrentState
      animations:^{
        self.button.alpha = 1.0;
      }
      completion:^(BOOL finished) {
        if (finished) {
          // If this method is called twice in succession, the button should
          // only be added once to |self.accessibilityElements|.
          if (![self.accessibilityElements containsObject:self.button])
            [self.accessibilityElements addObject:self.button];
        }
      }];
}

- (void)removeButtonAnimated:(BOOL)animated {
  if (!self.button)
    return;

  [UIView animateWithDuration:animated ? 0.15 : 0
      delay:0
      options:UIViewAnimationOptionBeginFromCurrentState
      animations:^{
        self.button.alpha = 0.0;
      }
      completion:^(BOOL finished) {
        if (finished) {
          [self.accessibilityElements removeObject:self.button];
        }
      }];
}

#pragma mark - Accessibility

- (void)updateAccessibilityValues {
  self.button.accessibilityIdentifier =
      [NSString stringWithFormat:@"%@ %@", self.titleLabel.text, @"Info"];
  self.button.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_BOOKMARK_NEW_MORE_BUTTON_ACCESSIBILITY_LABEL);
}

- (NSInteger)accessibilityElementCount {
  return [self.accessibilityElements count];
}

- (id)accessibilityElementAtIndex:(NSInteger)index {
  return [self.accessibilityElements objectAtIndex:index];
}

- (NSInteger)indexOfAccessibilityElement:(id)element {
  return [self.accessibilityElements indexOfObject:element];
}

@end

#pragma mark - BookmarkItemCell

@interface BookmarkItemCell () {
  base::mac::ObjCPropertyReleaser _propertyReleaser_BookmarkItemCell;
}
@end

@implementation BookmarkItemCell

+ (NSString*)reuseIdentifier {
  return @"BookmarkItemCell";
}

+ (CGFloat)preferredImageSize {
  return kBookmarkItemCellDefaultImageSize;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _propertyReleaser_BookmarkItemCell.Init(self, [BookmarkItemCell class]);

    // Set the non-layout properties of the titles.
    UIFont* font = [MDCTypography subheadFont];
    self.titleLabel.font = font;
    self.titleLabel.backgroundColor = [UIColor clearColor];
    self.titleLabel.numberOfLines = 1;

    _imageSize = kBookmarkItemCellDefaultImageSize;
  }
  return self;
}

- (void)updateWithTitle:(NSString*)title {
  [super updateWithTitle:title];
  [self updateAccessibilityValues];
}

- (void)updateAccessibilityValues {
  [super updateAccessibilityValues];
  self.contentView.accessibilityLabel =
      [NSString stringWithFormat:@"%@", self.titleLabel.text];
  self.contentView.accessibilityIdentifier = self.titleLabel.text;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self updateAccessibilityValues];
}

@end

#pragma mark - BookmarkFolderCell

@interface BookmarkFolderCell () {
  base::mac::ObjCPropertyReleaser _propertyReleaser_BookmarkFolderCell;
}

@end

@implementation BookmarkFolderCell

+ (NSString*)reuseIdentifier {
  return @"BookmarkFolderCell";
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _propertyReleaser_BookmarkFolderCell.Init(self, [BookmarkFolderCell class]);

    self.imageView.image = [UIImage imageNamed:@"bookmark_gray_folder"];
    self.titleLabel.font = [MDCTypography subheadFont];

    _imageSize = kBookmarkFolderCellDefaultImageSize;
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self setImage:[UIImage imageNamed:@"bookmark_gray_folder"]];
}

- (void)updateWithTitle:(NSString*)title {
  [super updateWithTitle:title];

  self.contentView.accessibilityLabel = title;
  self.contentView.accessibilityIdentifier = title;
}

@end

#pragma mark - BookmarkHeaderView

@interface BookmarkHeaderView () {
  base::mac::ObjCPropertyReleaser _propertyReleaser_BookmarkHeaderView;
}
@property(nonatomic, retain) UILabel* titleLabel;
@end

@implementation BookmarkHeaderView
@synthesize titleLabel = _titleLabel;
+ (NSString*)reuseIdentifier {
  return @"BookmarkHeaderView";
}

+ (CGFloat)handsetHeight {
  return 47.0;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _propertyReleaser_BookmarkHeaderView.Init(self, [BookmarkHeaderView class]);
    base::scoped_nsobject<UILabel> titleLabel(
        [[UILabel alloc] initWithFrame:CGRectZero]);
    self.titleLabel = titleLabel;
    UIFont* font = [MDCTypography body2Font];
    self.titleLabel.font = font;
    self.titleLabel.textColor = bookmark_utils_ios::lightTextColor();
    self.titleLabel.backgroundColor = [UIColor clearColor];
    self.titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;
    [self addSubview:self.titleLabel];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];

  // On handsets the title is aligned with the icon in the navigation bar.
  CGFloat margin = 16.0;
  if (IsIPadIdiom())
    margin = 24.0;  // On tablets the alignment is the same as the tiles.
  self.titleLabel.frame =
      CGRectMake(margin, 0, self.bounds.size.width - margin * 2.0,
                 self.bounds.size.height);
}

- (void)setTitle:(NSString*)title {
  self.titleLabel.text = title;
}

- (void)updateLayoutWithLeftMargin:(CGFloat)leftMargin {
  self.titleLabel.frame =
      CGRectMake(leftMargin, 0, self.bounds.size.width - leftMargin,
                 self.bounds.size.height);
}

@end

#pragma mark - BookmarkHeaderSeparatorView

@interface BookmarkHeaderSeparatorView () {
  base::mac::ObjCPropertyReleaser _propertyReleaser_BookmarkHeaderSeparatorView;
}
// The bottom separator line.
@property(nonatomic, retain) UIView* lineView;
@end

@implementation BookmarkHeaderSeparatorView

@synthesize lineView = _lineView;

+ (NSString*)reuseIdentifier {
  return NSStringFromClass(self);
}

+ (CGFloat)preferredHeight {
  return 16;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _propertyReleaser_BookmarkHeaderSeparatorView.Init(
        self, [BookmarkHeaderSeparatorView class]);
    _lineView = [[UIView alloc] init];
    _lineView.backgroundColor = bookmark_utils_ios::separatorColor();
    [self addSubview:_lineView];
  }
  return self;
}

- (void)layoutSubviews {
  CGFloat lineHeight = 1;
  CGFloat lineY = self.bounds.size.height - lineHeight - 8;
  _lineView.frame = CGRectMake(0, lineY, self.bounds.size.width, lineHeight);
}

@end
