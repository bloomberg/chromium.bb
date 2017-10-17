// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_view.h"

#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/QuartzCore.h>

#include "base/format_macros.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/infobars/infobar_view_delegate.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"
#import "ui/gfx/ios/uikit_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kChromeInfobarURL[] = "chromeinternal://infobar/";

// UX configuration for the layout of items.
const CGFloat kLeftMarginOnFirstLineWhenIconAbsent = 20.0;
const CGFloat kMinimumSpaceBetweenRightAndLeftAlignedWidgets = 30.0;
const CGFloat kRightMargin = 10.0;
const CGFloat kSpaceBetweenWidgets = 10.0;
const CGFloat kCloseButtonInnerPadding = 16.0;
const CGFloat kButtonHeight = 36.0;
const CGFloat kButtonMargin = 16.0;
const CGFloat kExtraButtonMarginOnSingleLine = 8.0;
const CGFloat kButtonSpacing = 8.0;
const CGFloat kButtonWidthUnits = 8.0;
const CGFloat kButtonsTopMargin = kCloseButtonInnerPadding;
const CGFloat kCloseButtonLeftMargin = 16.0;
const CGFloat kLabelLineSpacing = 5.0;
const CGFloat kLabelMarginBottom = 22.0;
const CGFloat kExtraMarginBetweenLabelAndButton = 8.0;
const CGFloat kLabelMarginTop = kButtonsTopMargin + 5.0;  // Baseline lowered.
const CGFloat kMinimumInfobarHeight = 68.0;
const CGFloat kHorizontalSpaceBetweenIconAndText = 16.0;

const int kButton2TitleColor = 0x4285f4;

enum InfoBarButtonPosition { ON_FIRST_LINE, CENTER, LEFT, RIGHT };

}  // namespace

// UIView containing a switch and a label.
@interface SwitchView : BidiContainerView

// Initialize the view's label with |labelText|.
- (id)initWithLabel:(NSString*)labelText isOn:(BOOL)isOn;

// Specifies the object, action, and tag used when the switch is toggled.
- (void)setTag:(NSInteger)tag target:(id)target action:(SEL)action;

// Returns the height taken by the view constrained by a width of |width|.
// If |layout| is yes, it sets the frame of the label and the switch to fit
// |width|.
- (CGFloat)heightRequiredForSwitchWithWidth:(CGFloat)width layout:(BOOL)layout;

// Returns the preferred width. A smaller width requires eliding the text.
- (CGFloat)preferredWidth;

@end

@implementation SwitchView {
  UILabel* label_;
  UISwitch* switch_;
  CGFloat preferredTotalWidth_;
  CGFloat preferredLabelWidth_;
}

- (id)initWithLabel:(NSString*)labelText isOn:(BOOL)isOn {
  // Creates switch and label.
  UILabel* tempLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  [tempLabel setTextAlignment:NSTextAlignmentNatural];
  [tempLabel setFont:[MDCTypography body1Font]];
  [tempLabel setText:labelText];
  [tempLabel setBackgroundColor:[UIColor clearColor]];
  [tempLabel setLineBreakMode:NSLineBreakByWordWrapping];
  [tempLabel setNumberOfLines:0];
  [tempLabel setAdjustsFontSizeToFitWidth:NO];
  UISwitch* tempSwitch = [[UISwitch alloc] initWithFrame:CGRectZero];
  [tempSwitch setExclusiveTouch:YES];
  [tempSwitch setAccessibilityLabel:labelText];
  [tempSwitch setOnTintColor:[[MDCPalette cr_bluePalette] tint500]];
  [tempSwitch setOn:isOn];

  // Computes the size and initializes the view.
  CGSize maxSize = CGSizeMake(CGFLOAT_MAX, CGFLOAT_MAX);
  CGSize labelSize =
      [[tempLabel text] cr_boundingSizeWithSize:maxSize font:[tempLabel font]];
  CGSize switchSize = [tempSwitch frame].size;
  CGRect frameRect = CGRectMake(
      0, 0, labelSize.width + kSpaceBetweenWidgets + switchSize.width,
      std::max(labelSize.height, switchSize.height));
  self = [super initWithFrame:frameRect];
  if (!self)
    return nil;
  label_ = tempLabel;
  switch_ = tempSwitch;

  // Sets the position of the label and the switch. The label is left aligned
  // and the switch is right aligned. Both are vertically centered.
  CGRect labelFrame =
      CGRectMake(0, (self.frame.size.height - labelSize.height) / 2,
                 labelSize.width, labelSize.height);
  CGRect switchFrame =
      CGRectMake(self.frame.size.width - switchSize.width,
                 (self.frame.size.height - switchSize.height) / 2,
                 switchSize.width, switchSize.height);

  labelFrame = AlignRectOriginAndSizeToPixels(labelFrame);
  switchFrame = AlignRectOriginAndSizeToPixels(switchFrame);

  [label_ setFrame:labelFrame];
  [switch_ setFrame:switchFrame];
  preferredTotalWidth_ = CGRectGetMaxX(switchFrame);
  preferredLabelWidth_ = CGRectGetMaxX(labelFrame);

  [self addSubview:label_];
  [self addSubview:switch_];
  return self;
}

- (void)setTag:(NSInteger)tag target:(id)target action:(SEL)action {
  [switch_ setTag:tag];
  [switch_ addTarget:target
                action:action
      forControlEvents:UIControlEventValueChanged];
}

- (CGFloat)heightRequiredForSwitchWithWidth:(CGFloat)width layout:(BOOL)layout {
  CGFloat widthLeftForLabel =
      width - [switch_ frame].size.width - kSpaceBetweenWidgets;
  CGSize maxSize = CGSizeMake(widthLeftForLabel, CGFLOAT_MAX);
  CGSize labelSize =
      [[label_ text] cr_boundingSizeWithSize:maxSize font:[label_ font]];
  CGFloat viewHeight = std::max(labelSize.height, [switch_ frame].size.height);
  if (layout) {
    // Lays out the label and the switch to fit in {width, viewHeight}.
    CGRect newLabelFrame;
    newLabelFrame.origin.x = 0;
    newLabelFrame.origin.y = (viewHeight - labelSize.height) / 2;
    newLabelFrame.size = labelSize;
    newLabelFrame = AlignRectOriginAndSizeToPixels(newLabelFrame);
    [label_ setFrame:newLabelFrame];
    CGRect newSwitchFrame;
    newSwitchFrame.origin.x =
        CGRectGetMaxX(newLabelFrame) + kSpaceBetweenWidgets;
    newSwitchFrame.origin.y = (viewHeight - [switch_ frame].size.height) / 2;
    newSwitchFrame.size = [switch_ frame].size;
    newSwitchFrame = AlignRectOriginAndSizeToPixels(newSwitchFrame);
    [switch_ setFrame:newSwitchFrame];
  }
  return viewHeight;
}

- (CGFloat)preferredWidth {
  return preferredTotalWidth_;
}

@end

@interface InfoBarView (Testing)
// Returns the buttons' height.
- (CGFloat)buttonsHeight;
// Returns the button margin applied in some views.
- (CGFloat)buttonMargin;
// Returns the height of the infobar, and lays out the subviews if |layout| is
// YES.
- (CGFloat)computeRequiredHeightAndLayoutSubviews:(BOOL)layout;
// Returns the height of the laid out buttons when not on the first line.
// Either the buttons are narrow enough and they are on a single line next to
// each other, or they are supperposed on top of each other.
// Also lays out the buttons when |layout| is YES, in which case it uses
// |heightOfFirstLine| to compute their vertical position.
- (CGFloat)heightThatFitsButtonsUnderOtherWidgets:(CGFloat)heightOfFirstLine
                                           layout:(BOOL)layout;
// The |button| is positioned with the right edge at the specified y-axis
// position |rightEdge| and the top row at |y|.
// Returns the left edge of the newly-positioned button.
- (CGFloat)layoutWideButtonAlignRight:(UIButton*)button
                            rightEdge:(CGFloat)rightEdge
                                    y:(CGFloat)y;
// Returns the minimum height of infobars.
- (CGFloat)minimumInfobarHeight;
// Returns |string| stripped of the markers specifying the links and fills
// |linkRanges_| with the ranges of the enclosed links.
- (NSString*)stripMarkersFromString:(NSString*)string;
// Returns the ranges of the links and the associated tags.
- (const std::vector<std::pair<NSUInteger, NSRange>>&)linkRanges;
@end

@interface InfoBarView ()

// Returns the marker delimiting the start of a link.
+ (NSString*)openingMarkerForLink;
// Returns the marker delimiting the end of a link.
+ (NSString*)closingMarkerForLink;

@end

@implementation InfoBarView {
  // Delegates UIView events.
  InfoBarViewDelegate* delegate_;  // weak.
  // The current height of this infobar (used for animations where part of the
  // infobar is hidden).
  CGFloat visibleHeight_;
  // The height of this infobar when fully visible.
  CGFloat targetHeight_;
  // View containing the icon.
  UIImageView* imageView_;
  // Close button.
  UIButton* closeButton_;
  // View containing the switch and its label.
  SwitchView* switchView_;
  // We are using a LabelLinkController with an UILabel to be able to have
  // parts of the label underlined and clickable. This label_ may be nil if
  // the delegate returns an empty string for GetMessageText().
  LabelLinkController* labelLinkController_;
  UILabel* label_;
  // Array of range information. The first element of the pair is the tag of
  // the action and the second element is the range defining the link.
  std::vector<std::pair<NSUInteger, NSRange>> linkRanges_;
  // Text for the label with link markers included.
  NSString* markedLabel_;
  // Buttons.
  // button1_ is tagged with ConfirmInfoBarDelegate::BUTTON_OK .
  // button2_ is tagged with ConfirmInfoBarDelegate::BUTTON_CANCEL .
  UIButton* button1_;
  UIButton* button2_;
  // Drop shadow.
  UIImageView* shadow_;
}

@synthesize visibleHeight = visibleHeight_;

- (instancetype)initWithFrame:(CGRect)frame
                     delegate:(InfoBarViewDelegate*)delegate {
  self = [super initWithFrame:frame];
  if (self) {
    delegate_ = delegate;
    // Make the drop shadow.
    UIImage* shadowImage = [UIImage imageNamed:@"infobar_shadow"];
    shadow_ = [[UIImageView alloc] initWithImage:shadowImage];
    [self addSubview:shadow_];
    [self setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                              UIViewAutoresizingFlexibleHeight];
    [self setAccessibilityViewIsModal:YES];
  }
  return self;
}


- (NSString*)markedLabel {
  return markedLabel_;
}

- (void)resetDelegate {
  delegate_ = NULL;
}

// Returns the width reserved for the icon.
- (CGFloat)leftMarginOnFirstLine {
  CGFloat leftMargin = 0;
  if (imageView_) {
    leftMargin += CGRectGetMaxX([self frameOfIcon]);
    leftMargin += kHorizontalSpaceBetweenIconAndText;
  } else {
    leftMargin += kLeftMarginOnFirstLineWhenIconAbsent;
    leftMargin += SafeAreaInsetsForView(self).left;
  }
  return leftMargin;
}

// Returns the width reserved for the close button.
- (CGFloat)rightMarginOnFirstLine {
  return [closeButton_ imageView].image.size.width +
         kCloseButtonInnerPadding * 2 + SafeAreaInsetsForView(self).right;
}

// Returns the horizontal space available between the icon and the close
// button.
- (CGFloat)horizontalSpaceAvailableOnFirstLine {
  return [self frame].size.width - [self leftMarginOnFirstLine] -
         [self rightMarginOnFirstLine];
}

// Returns the height taken by a label constrained by a width of |width|.
- (CGFloat)heightRequiredForLabelWithWidth:(CGFloat)width {
  return [label_ sizeThatFits:CGSizeMake(width, CGFLOAT_MAX)].height;
}

// Returns the width required by a label if it was displayed on a single line.
- (CGFloat)widthOfLabelOnASingleLine {
  // |label_| can be nil when delegate returns "" for GetMessageText().
  if (!label_)
    return 0.0;
  CGSize rect = [[label_ text] cr_pixelAlignedSizeWithFont:[label_ font]];
  return rect.width;
}

// Returns the minimum size required by |button| to be properly displayed.
- (CGFloat)narrowestWidthOfButton:(UIButton*)button {
  if (!button)
    return 0;
  // The button itself is queried for the size. The width is rounded up to be a
  // multiple of 8 to fit Material grid spacing requirements.
  CGFloat labelWidth =
      [button sizeThatFits:CGSizeMake(CGFLOAT_MAX, CGFLOAT_MAX)].width;
  return ceil(labelWidth / kButtonWidthUnits) * kButtonWidthUnits;
}

// Returns the width of the buttons if they are laid out on the first line.
- (CGFloat)widthOfButtonsOnFirstLine {
  CGFloat width = [self narrowestWidthOfButton:button1_] +
                  [self narrowestWidthOfButton:button2_];
  if (button1_ && button2_) {
    width += kSpaceBetweenWidgets;
  }
  return width;
}

// Returns the width needed for the switch.
- (CGFloat)preferredWidthOfSwitch {
  return [switchView_ preferredWidth];
}

// Returns the space required to separate the left aligned widgets (label) from
// the right aligned widgets (switch, buttons), assuming they fit on one line.
- (CGFloat)widthToSeparateRightAndLeftWidgets {
  BOOL leftWidgetsArePresent = (label_ != nil);
  BOOL rightWidgetsArePresent = button1_ || button2_ || switchView_;
  if (!leftWidgetsArePresent || !rightWidgetsArePresent)
    return 0;
  return kMinimumSpaceBetweenRightAndLeftAlignedWidgets;
}

// Returns the space required to separate the switch and the buttons.
- (CGFloat)widthToSeparateSwitchAndButtons {
  BOOL buttonsArePresent = button1_ || button2_;
  BOOL switchIsPresent = (switchView_ != nil);
  if (!buttonsArePresent || !switchIsPresent)
    return 0;
  return kSpaceBetweenWidgets;
}

// Lays out |button| at the height |y| and in the position |position|.
// Must only be used for wide buttons, i.e. buttons not on the first line.
- (void)layoutWideButton:(UIButton*)button
                       y:(CGFloat)y
                position:(InfoBarButtonPosition)position {
  CGFloat screenWidth = [self frame].size.width;
  CGFloat startPercentage = 0.0;
  CGFloat endPercentage = 0.0;
  switch (position) {
    case LEFT:
      startPercentage = 0.0;
      endPercentage = 0.5;
      break;
    case RIGHT:
      startPercentage = 0.5;
      endPercentage = 1.0;
      break;
    case CENTER:
      startPercentage = 0.0;
      endPercentage = 1.0;
      break;
    case ON_FIRST_LINE:
      NOTREACHED();
  }
  DCHECK(startPercentage >= 0.0 && startPercentage <= 1.0);
  DCHECK(endPercentage >= 0.0 && endPercentage <= 1.0);
  DCHECK(startPercentage < endPercentage);
  // In Material the button is not stretched to fit the available space. It is
  // placed centrally in the allotted space.
  CGFloat minX = screenWidth * startPercentage;
  CGFloat maxX = screenWidth * endPercentage;
  CGFloat midpoint = (minX + maxX) / 2;
  CGFloat minWidth =
      std::min([self narrowestWidthOfButton:button], maxX - minX);
  CGFloat left = midpoint - minWidth / 2;
  CGRect frame = CGRectMake(left, y, minWidth, kButtonHeight);
  frame = AlignRectOriginAndSizeToPixels(frame);
  [button setFrame:frame];
}

- (CGFloat)layoutWideButtonAlignRight:(UIButton*)button
                            rightEdge:(CGFloat)rightEdge
                                    y:(CGFloat)y {
  CGFloat width = [self narrowestWidthOfButton:button];
  CGFloat leftEdge = rightEdge - width;
  CGRect frame = CGRectMake(leftEdge, y, width, kButtonHeight);
  frame = AlignRectOriginAndSizeToPixels(frame);
  [button setFrame:frame];
  return leftEdge;
}

- (CGFloat)heightThatFitsButtonsUnderOtherWidgets:(CGFloat)heightOfFirstLine
                                           layout:(BOOL)layout {
  if (button1_ && button2_) {
    CGFloat halfWidthOfScreen = [self frame].size.width / 2.0;
    if ([self narrowestWidthOfButton:button1_] <= halfWidthOfScreen &&
        [self narrowestWidthOfButton:button2_] <= halfWidthOfScreen) {
      // Each button can fit in half the screen's width.
      if (layout) {
        // When there are two buttons on one line, they are positioned aligned
        // right in the available space, spaced apart by kButtonSpacing.
        CGFloat leftOfRightmostButton =
            [self layoutWideButtonAlignRight:button1_
                                   rightEdge:CGRectGetWidth(self.bounds) -
                                             kButtonMargin -
                                             SafeAreaInsetsForView(self).right
                                           y:heightOfFirstLine];
        [self layoutWideButtonAlignRight:button2_
                               rightEdge:leftOfRightmostButton - kButtonSpacing
                                       y:heightOfFirstLine];
      }
      return kButtonHeight;
    } else {
      // At least one of the two buttons is larger than half the screen's width,
      // so |button2_| is placed underneath |button1_|.
      if (layout) {
        [self layoutWideButton:button1_ y:heightOfFirstLine position:CENTER];
        [self layoutWideButton:button2_
                             y:heightOfFirstLine + kButtonHeight
                      position:CENTER];
      }
      return 2 * kButtonHeight;
    }
  }
  // There is at most 1 button to layout.
  UIButton* button = button1_ ? button1_ : button2_;
  if (button) {
    if (layout) {
      // Where is there is just one button it is positioned aligned right in the
      // available space.
      [self layoutWideButtonAlignRight:button
                             rightEdge:CGRectGetWidth(self.bounds) -
                                       kButtonMargin -
                                       SafeAreaInsetsForView(self).right
                                     y:heightOfFirstLine];
    }
    return kButtonHeight;
  }
  return 0;
}

- (CGFloat)computeRequiredHeightAndLayoutSubviews:(BOOL)layout {
  CGFloat requiredHeight = 0;
  CGFloat widthOfLabel = [self widthOfLabelOnASingleLine] +
                         [self widthToSeparateRightAndLeftWidgets];
  CGFloat widthOfButtons = [self widthOfButtonsOnFirstLine];
  CGFloat preferredWidthOfSwitch = [self preferredWidthOfSwitch];
  CGFloat widthOfScreen = [self frame].size.width;
  CGFloat rightMarginOnFirstLine = [self rightMarginOnFirstLine];
  CGFloat spaceAvailableOnFirstLine =
      [self horizontalSpaceAvailableOnFirstLine];
  CGFloat widthOfButtonAndSwitch = widthOfButtons +
                                   [self widthToSeparateSwitchAndButtons] +
                                   preferredWidthOfSwitch;
  // Tests if the label, switch, and buttons can fit on a single line.
  if (widthOfLabel + widthOfButtonAndSwitch < spaceAvailableOnFirstLine) {
    // The label, switch, and buttons can fit on a single line.
    requiredHeight = kMinimumInfobarHeight;
    if (layout) {
      // Lays out the close button.
      CGRect buttonFrame = [self frameOfCloseButton:YES];
      [closeButton_ setFrame:buttonFrame];
      // Lays out the label.
      CGFloat labelHeight = [self heightRequiredForLabelWithWidth:widthOfLabel];
      CGRect frame = CGRectMake([self leftMarginOnFirstLine],
                                (kMinimumInfobarHeight - labelHeight) / 2,
                                [self widthOfLabelOnASingleLine], labelHeight);
      frame = AlignRectOriginAndSizeToPixels(frame);
      [label_ setFrame:frame];
      // Layouts the buttons.
      CGFloat buttonMargin =
          rightMarginOnFirstLine + kExtraButtonMarginOnSingleLine;
      if (button1_) {
        CGFloat width = [self narrowestWidthOfButton:button1_];
        CGFloat offset = width;
        frame = CGRectMake(widthOfScreen - buttonMargin - offset,
                           (kMinimumInfobarHeight - kButtonHeight) / 2, width,
                           kButtonHeight);
        frame = AlignRectOriginAndSizeToPixels(frame);
        [button1_ setFrame:frame];
      }
      if (button2_) {
        CGFloat width = [self narrowestWidthOfButton:button2_];
        CGFloat offset = widthOfButtons;
        frame = CGRectMake(widthOfScreen - buttonMargin - offset,
                           (kMinimumInfobarHeight - kButtonHeight) / 2, width,
                           frame.size.height = kButtonHeight);
        frame = AlignRectOriginAndSizeToPixels(frame);
        [button2_ setFrame:frame];
      }
      // Lays out the switch view to the left of the buttons.
      if (switchView_) {
        frame = CGRectMake(
            widthOfScreen - buttonMargin - widthOfButtonAndSwitch,
            (kMinimumInfobarHeight - [switchView_ frame].size.height) / 2.0,
            preferredWidthOfSwitch, [switchView_ frame].size.height);
        frame = AlignRectOriginAndSizeToPixels(frame);
        [switchView_ setFrame:frame];
      }
    }
  } else {
    // The widgets (label, switch, buttons) can't fit on a single line. Attempts
    // to lay out the label and switch on the first line, and the buttons
    // underneath.
    CGFloat heightOfLabelAndSwitch = 0;

    if (layout) {
      // Lays out the close button.
      CGRect buttonFrame = [self frameOfCloseButton:NO];
      [closeButton_ setFrame:buttonFrame];
    }
    if (widthOfLabel + preferredWidthOfSwitch < spaceAvailableOnFirstLine) {
      // The label and switch can fit on the first line.
      heightOfLabelAndSwitch = kMinimumInfobarHeight;
      if (layout) {
        CGFloat labelHeight =
            [self heightRequiredForLabelWithWidth:widthOfLabel];
        CGRect labelFrame =
            CGRectMake([self leftMarginOnFirstLine],
                       (heightOfLabelAndSwitch - labelHeight) / 2,
                       [self widthOfLabelOnASingleLine], labelHeight);
        labelFrame = AlignRectOriginAndSizeToPixels(labelFrame);
        [label_ setFrame:labelFrame];
        if (switchView_) {
          CGRect switchRect = CGRectMake(
              widthOfScreen - rightMarginOnFirstLine - preferredWidthOfSwitch,
              (heightOfLabelAndSwitch - [switchView_ frame].size.height) / 2,
              preferredWidthOfSwitch, [switchView_ frame].size.height);
          switchRect = AlignRectOriginAndSizeToPixels(switchRect);
          [switchView_ setFrame:switchRect];
        }
      }
    } else {
      // The label and switch can't fit on the first line, so lay them out on
      // different lines.
      // Computes the height of the label, and optionally lays it out.
      CGFloat labelMarginBottom = kLabelMarginBottom;
      if (button1_ || button2_) {
        // Material features more padding between the label and the button than
        // the label and the bottom of the dialog when there is no button.
        labelMarginBottom += kExtraMarginBetweenLabelAndButton;
      }
      CGFloat heightOfLabelWithPadding =
          [self heightRequiredForLabelWithWidth:spaceAvailableOnFirstLine] +
          kLabelMarginTop + labelMarginBottom;
      if (layout) {
        CGRect labelFrame = CGRectMake(
            [self leftMarginOnFirstLine], kLabelMarginTop,
            spaceAvailableOnFirstLine,
            heightOfLabelWithPadding - kLabelMarginTop - labelMarginBottom);
        labelFrame = AlignRectOriginAndSizeToPixels(labelFrame);
        [label_ setFrame:labelFrame];
      }
      // Computes the height of the switch view (if any), and optionally lays it
      // out.
      CGFloat heightOfSwitchWithPadding = 0;
      if (switchView_ != nil) {
        // The switch view is aligned with the first line's label, hence the
        // call to |leftMarginOnFirstLine|.
        CGFloat widthAvailableForSwitchView = [self frame].size.width -
                                              [self leftMarginOnFirstLine] -
                                              kRightMargin;
        CGFloat heightOfSwitch = [switchView_
            heightRequiredForSwitchWithWidth:widthAvailableForSwitchView
                                      layout:layout];
        // If there are buttons underneath the switch, add padding.
        if (button1_ || button2_) {
          heightOfSwitchWithPadding = heightOfSwitch + kSpaceBetweenWidgets +
                                      kExtraMarginBetweenLabelAndButton;
        } else {
          heightOfSwitchWithPadding = heightOfSwitch;
        }
        if (layout) {
          CGRect switchRect =
              CGRectMake([self leftMarginOnFirstLine], heightOfLabelWithPadding,
                         widthAvailableForSwitchView, heightOfSwitch);
          switchRect = AlignRectOriginAndSizeToPixels(switchRect);
          [switchView_ setFrame:switchRect];
        }
      }
      heightOfLabelAndSwitch =
          std::max(heightOfLabelWithPadding + heightOfSwitchWithPadding,
                   kMinimumInfobarHeight);
    }
    // Lays out the button(s) under the label and switch.
    CGFloat heightOfButtons =
        [self heightThatFitsButtonsUnderOtherWidgets:heightOfLabelAndSwitch
                                              layout:layout];
    requiredHeight = heightOfLabelAndSwitch;
    if (heightOfButtons > 0)
      requiredHeight += heightOfButtons + kButtonMargin;
  }
  // Take into account the bottom safe area.
  // The top safe area is ignored because at rest (i.e. not during animations)
  // the infobar is aligned to the bottom of the screen, and thus should not
  // have its top intersect whith any safe area.
  CGFloat bottomSafeAreaInset = SafeAreaInsetsForView(self).bottom;
  requiredHeight += bottomSafeAreaInset;

  return requiredHeight;
}

- (CGSize)sizeThatFits:(CGSize)size {
  CGFloat requiredHeight = [self computeRequiredHeightAndLayoutSubviews:NO];
  return CGSizeMake([self frame].size.width, requiredHeight);
}

- (void)layoutSubviews {
  // Lays out the position of the icon.
  [imageView_ setFrame:[self frameOfIcon]];
  targetHeight_ = [self computeRequiredHeightAndLayoutSubviews:YES];

  if (delegate_)
    delegate_->SetInfoBarTargetHeight(targetHeight_);
  [self resetBackground];

  // Asks the BidiContainerView to reposition of all the subviews.
  for (UIView* view in [self subviews])
    [self setSubviewNeedsAdjustmentForRTL:view];
  [super layoutSubviews];
}

- (void)resetBackground {
  UIColor* color = [UIColor whiteColor];
  [self setBackgroundColor:color];
  CGFloat shadowY = 0;
  shadowY = -[shadow_ image].size.height;  // Shadow above the infobar.
  [shadow_ setFrame:CGRectMake(0, shadowY, self.bounds.size.width,
                               [shadow_ image].size.height)];
  [shadow_ setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
}

- (void)addCloseButtonWithTag:(NSInteger)tag
                       target:(id)target
                       action:(SEL)action {
  DCHECK(!closeButton_);
  // TODO(crbug/228611): Add IDR_ constant and use GetNativeImageNamed().
  UIImage* image = [UIImage imageNamed:@"infobar_close"];
  closeButton_ = [UIButton buttonWithType:UIButtonTypeCustom];
  [closeButton_ setExclusiveTouch:YES];
  [closeButton_ setImage:image forState:UIControlStateNormal];
  [closeButton_ addTarget:target
                   action:action
         forControlEvents:UIControlEventTouchUpInside];
  [closeButton_ setTag:tag];
  [closeButton_ setAccessibilityLabel:l10n_util::GetNSString(IDS_CLOSE)];
  [self addSubview:closeButton_];
}

- (void)addSwitchWithLabel:(NSString*)label
                      isOn:(BOOL)isOn
                       tag:(NSInteger)tag
                    target:(id)target
                    action:(SEL)action {
  switchView_ = [[SwitchView alloc] initWithLabel:label isOn:isOn];
  [switchView_ setTag:tag target:target action:action];
  [self addSubview:switchView_];
}

- (void)addLeftIcon:(UIImage*)image {
  if (imageView_) {
    [imageView_ removeFromSuperview];
  }
  imageView_ = [[UIImageView alloc] initWithImage:image];
  [self addSubview:imageView_];
}

- (NSString*)stripMarkersFromString:(NSString*)string {
  linkRanges_.clear();
  for (;;) {
    // Find the opening marker, followed by the tag between parentheses.
    NSRange startingRange =
        [string rangeOfString:[[InfoBarView openingMarkerForLink]
                                  stringByAppendingString:@"("]];
    if (!startingRange.length)
      return [string copy];
    // Read the tag.
    NSUInteger beginTag = NSMaxRange(startingRange);
    NSRange closingParenthesis = [string
        rangeOfString:@")"
              options:NSLiteralSearch
                range:NSMakeRange(beginTag, [string length] - beginTag)];
    if (closingParenthesis.location == NSNotFound)
      return [string copy];
    NSInteger tag = [[string
        substringWithRange:NSMakeRange(beginTag, closingParenthesis.location -
                                                     beginTag)] integerValue];
    // If the parsing fails, |tag| is 0. Negative values are not allowed.
    if (tag <= 0)
      return [string copy];
    // Find the closing marker.
    startingRange.length =
        closingParenthesis.location - startingRange.location + 1;
    NSRange endingRange =
        [string rangeOfString:[InfoBarView closingMarkerForLink]];
    DCHECK(endingRange.length);
    // Compute range of link in stripped string and add it to the array.
    NSRange rangeOfLinkInStrippedString =
        NSMakeRange(startingRange.location,
                    endingRange.location - NSMaxRange(startingRange));
    linkRanges_.push_back(std::make_pair(tag, rangeOfLinkInStrippedString));
    // Creates a new string without the markers.
    NSString* beforeLink = [string substringToIndex:startingRange.location];
    NSRange rangeOfLink =
        NSMakeRange(NSMaxRange(startingRange),
                    endingRange.location - NSMaxRange(startingRange));
    NSString* link = [string substringWithRange:rangeOfLink];
    NSString* afterLink = [string substringFromIndex:NSMaxRange(endingRange)];
    string = [NSString stringWithFormat:@"%@%@%@", beforeLink, link, afterLink];
  }
}

- (void)addLabel:(NSString*)label {
  [self addLabel:label action:nil];
}

- (void)addLabel:(NSString*)text action:(void (^)(NSUInteger))action {
  markedLabel_ = [text copy];
  if (action)
    text = [self stripMarkersFromString:text];
  if ([label_ superview]) {
    [label_ removeFromSuperview];
  }

  label_ = [[UILabel alloc] initWithFrame:CGRectZero];

  UIFont* font = [MDCTypography subheadFont];

  [label_ setBackgroundColor:[UIColor clearColor]];

  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.lineBreakMode = NSLineBreakByWordWrapping;
  paragraphStyle.lineSpacing = kLabelLineSpacing;
  NSDictionary* attributes = @{
    NSParagraphStyleAttributeName : paragraphStyle,
    NSFontAttributeName : font,
  };
  [label_ setNumberOfLines:0];

  [label_
      setAttributedText:[[NSAttributedString alloc] initWithString:text
                                                        attributes:attributes]];

  [self addSubview:label_];

  if (linkRanges_.empty())
    return;

  labelLinkController_ = [[LabelLinkController alloc]
      initWithLabel:label_
             action:^(const GURL& gurl) {
               if (action) {
                 NSUInteger actionTag = [base::SysUTF8ToNSString(
                     gurl.ExtractFileName()) integerValue];
                 action(actionTag);
               }
             }];

  [labelLinkController_ setLinkUnderlineStyle:NSUnderlineStyleSingle];
  [labelLinkController_ setLinkColor:[UIColor blackColor]];

  std::vector<std::pair<NSUInteger, NSRange>>::const_iterator it;
  for (it = linkRanges_.begin(); it != linkRanges_.end(); ++it) {
    // The last part of the URL contains the tag, so it can be retrieved in the
    // callback. This tag is generally a command ID.
    std::string url = std::string(kChromeInfobarURL) +
                      std::string(std::to_string((int)it->first));
    [labelLinkController_ addLinkWithRange:it->second url:GURL(url)];
  }
}

- (void)addButton1:(NSString*)title1
              tag1:(NSInteger)tag1
           button2:(NSString*)title2
              tag2:(NSInteger)tag2
            target:(id)target
            action:(SEL)action {
  button1_ = [self infoBarButton:title1
                         palette:[MDCPalette cr_bluePalette]
                customTitleColor:[UIColor whiteColor]
                             tag:tag1
                          target:target
                          action:action];
  [self addSubview:button1_];

  button2_ = [self infoBarButton:title2
                         palette:nil
                customTitleColor:UIColorFromRGB(kButton2TitleColor)
                             tag:tag2
                          target:target
                          action:action];
  [self addSubview:button2_];
}

- (void)addButton:(NSString*)title
              tag:(NSInteger)tag
           target:(id)target
           action:(SEL)action {
  if (![title length])
    return;
  button1_ = [self infoBarButton:title
                         palette:[MDCPalette cr_bluePalette]
                customTitleColor:[UIColor whiteColor]
                             tag:tag
                          target:target
                          action:action];
  [self addSubview:button1_];
}

// Initializes and returns a button for the infobar, with the specified
// |message| and colors.
- (UIButton*)infoBarButton:(NSString*)message
                   palette:(MDCPalette*)palette
          customTitleColor:(UIColor*)customTitleColor
                       tag:(NSInteger)tag
                    target:(id)target
                    action:(SEL)action {
  MDCFlatButton* button = [[MDCFlatButton alloc] init];
  button.inkColor = [[palette tint300] colorWithAlphaComponent:0.5f];
  [button setBackgroundColor:[palette tint500] forState:UIControlStateNormal];
  [button setBackgroundColor:[UIColor colorWithWhite:0.8f alpha:1.0f]
                    forState:UIControlStateDisabled];
  if (palette)
    button.hasOpaqueBackground = YES;
  if (customTitleColor) {
    button.tintAdjustmentMode = UIViewTintAdjustmentModeNormal;
    [button setTitleColor:customTitleColor forState:UIControlStateNormal];
  }
  button.titleLabel.adjustsFontSizeToFitWidth = YES;
  button.titleLabel.minimumScaleFactor = 0.6f;
  [button setTitle:message forState:UIControlStateNormal];
  [button setTag:tag];
  [button addTarget:target
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  // Without the call to layoutIfNeeded, |button| returns an incorrect
  // titleLabel the first time it is accessed in |narrowestWidthOfButton|.
  [button layoutIfNeeded];
  return button;
}

- (CGRect)frameOfCloseButton:(BOOL)singleLineMode {
  DCHECK(closeButton_);
  // Add padding to increase the touchable area.
  CGSize closeButtonSize = [closeButton_ imageView].image.size;
  closeButtonSize.width += kCloseButtonInnerPadding * 2;
  closeButtonSize.height += kCloseButtonInnerPadding * 2;
  CGFloat x = CGRectGetMaxX(self.frame) - closeButtonSize.width -
              SafeAreaInsetsForView(self).right;
  // Aligns the close button at the top (height includes touch padding).
  CGFloat y = 0;
  if (singleLineMode) {
    // On single-line mode the button is centered vertically.
    y = ui::AlignValueToUpperPixel(
        (kMinimumInfobarHeight - closeButtonSize.height) * 0.5);
  }
  return CGRectMake(x, y, closeButtonSize.width, closeButtonSize.height);
}

- (CGRect)frameOfIcon {
  CGSize iconSize = [imageView_ image].size;
  CGFloat y = kButtonsTopMargin;
  CGFloat x = kCloseButtonLeftMargin + SafeAreaInsetsForView(self).left;
  return CGRectMake(AlignValueToPixel(x), AlignValueToPixel(y), iconSize.width,
                    iconSize.height);
}

+ (NSString*)openingMarkerForLink {
  return @"$LINK_START";
}

+ (NSString*)closingMarkerForLink {
  return @"$LINK_END";
}

+ (NSString*)stringAsLink:(NSString*)string tag:(NSUInteger)tag {
  DCHECK_NE(0u, tag);
  return [NSString stringWithFormat:@"%@(%" PRIuNS ")%@%@",
                                    [InfoBarView openingMarkerForLink], tag,
                                    string, [InfoBarView closingMarkerForLink]];
}

#pragma mark - Testing

- (CGFloat)minimumInfobarHeight {
  return kMinimumInfobarHeight;
}

- (CGFloat)buttonsHeight {
  return kButtonHeight;
}

- (CGFloat)buttonMargin {
  return kButtonMargin;
}

- (const std::vector<std::pair<NSUInteger, NSRange>>&)linkRanges {
  return linkRanges_;
}

@end
