// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sad_tab/sad_tab_view.h"

#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/open_url_command.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#include "ios/web/public/interstitials/web_interstitial.h"
#include "ios/web/public/web_state/web_state.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// Color constants.
const CGFloat kBackgroundColorBrightness = 247.0f / 255.0f;
const CGFloat kTitleLabelTextColorBrightness = 22.0f / 255.0f;
const CGFloat kMessageLabelTextColorBrightness = 80.0f / 255.0f;
// Layout constants.
const UIEdgeInsets kLayoutInsets = {24.0f, 24.0f, 24.0f, 24.0f};
const CGFloat kLayoutBoundsMaxWidth = 600.0f;
const CGFloat kContainerViewLandscapeTopPadding = 22.0f;
const CGFloat kTitleLabelTopPadding = 26.0f;
const CGFloat kMessageLabelTopPadding = 16.0f;
const CGFloat kHelpLabelTopPadding = 16.0f;
const CGFloat kReloadButtonHeight = 48.0f;
const CGFloat kReloadButtonTopPadding = 16.0f;
// Label font sizes.
const CGFloat kTitleLabelFontSize = 23.0f;
const CGFloat kMessageLabelFontSize = 14.0f;
const CGFloat kHelpLabelFontSize = 14.0f;
}  // namespace

@interface SadTabView () {
  // The block called when |_reloadButton| is tapped.
  base::mac::ScopedBlock<ProceduralBlock> _reloadHandler;
  // Backing objects for properties of the same name.
  base::scoped_nsobject<UIView> _containerView;
  base::scoped_nsobject<UIImageView> _imageView;
  base::scoped_nsobject<UILabel> _titleLabel;
  base::scoped_nsobject<UILabel> _messageLabel;
  base::scoped_nsobject<UILabel> _helpLabel;
  base::scoped_nsobject<LabelLinkController> _helpLabelLinkController;
  base::scoped_nsobject<MDCButton> _reloadButton;
}

// Container view that displays all other subviews.
@property(nonatomic, readonly) UIView* containerView;
// Displays the Sad Tab face.
@property(nonatomic, readonly) UIImageView* imageView;
// Displays the Sad Tab title.
@property(nonatomic, readonly) UILabel* titleLabel;
// Displays the Sad Tab message.
@property(nonatomic, readonly) UILabel* messageLabel;
// Displays the Sad Tab help message.
@property(nonatomic, readonly) UILabel* helpLabel;
// Button used to trigger a reload.
@property(nonatomic, readonly) UIButton* reloadButton;

// The bounds of |containerView|, with a height updated to CGFLOAT_MAX to allow
// text to be laid out using as many lines as necessary.
@property(nonatomic, readonly) CGRect containerBounds;

// Subview layout methods.  Must be called in the following order, as subsequent
// layouts reference the values set in previous functions.
- (void)layoutImageView;
- (void)layoutTitleLabel;
- (void)layoutMessageLabel;
- (void)layoutHelpLabel;
- (void)layoutReloadButton;
- (void)layoutContainerView;

// The action selector for |_reloadButton|.
- (void)handleReloadButtonTapped;

// Returns the desired background color.
+ (UIColor*)sadTabBackgroundColor;

@end

#pragma mark - SadTabView

@implementation SadTabView

- (instancetype)initWithReloadHandler:(ProceduralBlock)reloadHandler {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(reloadHandler);
    _reloadHandler.reset([reloadHandler copy]);
    self.backgroundColor = [[self class] sadTabBackgroundColor];
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithFrame:(CGRect)frame {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  NOTREACHED();
  return nil;
}

#pragma mark Accessors

- (UIView*)containerView {
  if (!_containerView) {
    _containerView.reset([[UIView alloc] initWithFrame:CGRectZero]);
    [_containerView setBackgroundColor:self.backgroundColor];
  }
  return _containerView;
}

- (UIImageView*)imageView {
  if (!_imageView) {
    _imageView.reset(
        [[UIImageView alloc] initWithImage:NativeImage(IDR_CRASH_SAD_TAB)]);
    [_imageView setBackgroundColor:self.backgroundColor];
  }
  return _imageView.get();
}

- (UILabel*)titleLabel {
  if (!_titleLabel) {
    _titleLabel.reset([[UILabel alloc] initWithFrame:CGRectZero]);
    [_titleLabel setBackgroundColor:self.backgroundColor];
    [_titleLabel setText:base::SysUTF8ToNSString(
                             l10n_util::GetStringUTF8(IDS_SAD_TAB_TITLE))];
    [_titleLabel setLineBreakMode:NSLineBreakByWordWrapping];
    [_titleLabel setNumberOfLines:0];
    [_titleLabel
        setTextColor:[UIColor colorWithWhite:kTitleLabelTextColorBrightness
                                       alpha:1.0]];
    [_titleLabel setFont:[[MDFRobotoFontLoader sharedInstance]
                             regularFontOfSize:kTitleLabelFontSize]];
  }
  return _titleLabel.get();
}

- (UILabel*)messageLabel {
  if (!_messageLabel) {
    _messageLabel.reset([[UILabel alloc] initWithFrame:CGRectZero]);
    [_messageLabel setBackgroundColor:self.backgroundColor];
    std::string messageText = l10n_util::GetStringUTF8(IDS_SAD_TAB_MESSAGE);
    [_messageLabel setText:base::SysUTF8ToNSString(messageText)];
    [_messageLabel setLineBreakMode:NSLineBreakByWordWrapping];
    [_messageLabel setNumberOfLines:0];
    [_messageLabel
        setTextColor:[UIColor colorWithWhite:kMessageLabelTextColorBrightness
                                       alpha:1.0]];
    [_messageLabel setFont:[[MDFRobotoFontLoader sharedInstance]
                               regularFontOfSize:kMessageLabelFontSize]];
  }
  return _messageLabel.get();
}

- (UILabel*)helpLabel {
  if (!_helpLabel) {
    _helpLabel.reset([[UILabel alloc] initWithFrame:CGRectZero]);
    [_helpLabel setBackgroundColor:self.backgroundColor];
    [_helpLabel setNumberOfLines:0];
    [_helpLabel setFont:[[MDFRobotoFontLoader sharedInstance]
                            regularFontOfSize:kHelpLabelFontSize]];
    [_helpLabel
        setTextColor:[UIColor colorWithWhite:kMessageLabelTextColorBrightness
                                       alpha:1.0]];
    // Fetch help text.
    base::string16 helpLinkText(
        l10n_util::GetStringUTF16(IDS_SAD_TAB_HELP_LINK));
    NSString* helpText = base::SysUTF16ToNSString(
        l10n_util::GetStringFUTF16(IDS_SAD_TAB_HELP_MESSAGE, helpLinkText));
    [_helpLabel setText:helpText];
    // Create link controller.
    base::WeakNSObject<SadTabView> weakSelf(self);
    _helpLabelLinkController.reset([[LabelLinkController alloc]
        initWithLabel:_helpLabel
               action:^(const GURL& url) {
                 base::scoped_nsobject<OpenUrlCommand> openCommand(
                     [[OpenUrlCommand alloc] initWithURLFromChrome:url]);
                 [weakSelf chromeExecuteCommand:openCommand];
               }]);
    [_helpLabelLinkController
        setLinkFont:[[MDFRobotoFontLoader sharedInstance]
                        boldFontOfSize:kHelpLabelFontSize]];
    [_helpLabelLinkController setLinkUnderlineStyle:NSUnderlineStyleSingle];
    NSRange linkRange =
        [helpText rangeOfString:base::SysUTF16ToNSString(helpLinkText)];
    DCHECK_NE(linkRange.location, static_cast<NSUInteger>(NSNotFound));
    DCHECK_NE(linkRange.length, 0U);
    [_helpLabelLinkController addLinkWithRange:linkRange
                                           url:GURL(kCrashReasonURL)];
  }
  return _helpLabel.get();
}

- (UIButton*)reloadButton {
  if (!_reloadButton) {
    _reloadButton.reset([[MDCFlatButton alloc] init]);
    [_reloadButton setBackgroundColor:[[MDCPalette cr_bluePalette] tint500]
                             forState:UIControlStateNormal];
    [_reloadButton setBackgroundColor:[[MDCPalette greyPalette] tint500]
                             forState:UIControlStateDisabled];
    [_reloadButton setCustomTitleColor:[UIColor whiteColor]];
    [_reloadButton setUnderlyingColorHint:[UIColor blackColor]];
    [_reloadButton setInkColor:[UIColor colorWithWhite:1 alpha:0.2f]];
    NSString* title = base::SysUTF8ToNSString(
        l10n_util::GetStringUTF8(IDS_SAD_TAB_RELOAD_LABEL));
    [_reloadButton setTitle:title forState:UIControlStateNormal];
    [_reloadButton setTitleColor:[UIColor whiteColor]
                        forState:UIControlStateNormal];
    [_reloadButton addTarget:self
                      action:@selector(handleReloadButtonTapped)
            forControlEvents:UIControlEventTouchUpInside];
  }
  return _reloadButton.get();
}

- (CGRect)containerBounds {
  CGFloat containerWidth = std::min(
      CGRectGetWidth(self.bounds) - kLayoutInsets.left - kLayoutInsets.right,
      kLayoutBoundsMaxWidth);
  return CGRectMake(0.0, 0.0, containerWidth, CGFLOAT_MAX);
}

#pragma mark Layout

- (void)willMoveToSuperview:(nullable UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  if (self.containerView.superview) {
    DCHECK_EQ(self.containerView.superview, self);
    return;
  }

  [self addSubview:self.containerView];
  [self.containerView addSubview:self.imageView];
  [self.containerView addSubview:self.titleLabel];
  [self.containerView addSubview:self.messageLabel];
  [self.containerView addSubview:self.helpLabel];
}

- (void)layoutSubviews {
  [super layoutSubviews];

  [self layoutImageView];
  [self layoutTitleLabel];
  [self layoutMessageLabel];
  [self layoutHelpLabel];
  [self layoutReloadButton];
  [self layoutContainerView];
}

- (CGSize)sizeThatFits:(CGSize)size {
  return size;
}

- (void)layoutImageView {
  LayoutRect imageViewLayout = LayoutRectZero;
  imageViewLayout.boundingWidth = CGRectGetWidth(self.containerBounds);
  imageViewLayout.size = self.imageView.bounds.size;
  self.imageView.frame =
      AlignRectOriginAndSizeToPixels(LayoutRectGetRect(imageViewLayout));
}

- (void)layoutTitleLabel {
  CGRect containerBounds = self.containerBounds;
  LayoutRect titleLabelLayout = LayoutRectZero;
  titleLabelLayout.boundingWidth = CGRectGetWidth(containerBounds);
  titleLabelLayout.size = [self.titleLabel sizeThatFits:containerBounds.size];
  titleLabelLayout.position.originY =
      CGRectGetMaxY(self.imageView.frame) + kTitleLabelTopPadding;
  self.titleLabel.frame =
      AlignRectOriginAndSizeToPixels(LayoutRectGetRect(titleLabelLayout));
}

- (void)layoutMessageLabel {
  CGRect containerBounds = self.containerBounds;
  LayoutRect messageLabelLayout = LayoutRectZero;
  messageLabelLayout.boundingWidth = CGRectGetWidth(containerBounds);
  messageLabelLayout.size =
      [self.messageLabel sizeThatFits:containerBounds.size];
  messageLabelLayout.position.originY =
      CGRectGetMaxY(self.titleLabel.frame) + kMessageLabelTopPadding;
  self.messageLabel.frame =
      AlignRectOriginAndSizeToPixels(LayoutRectGetRect(messageLabelLayout));
}

- (void)layoutHelpLabel {
  CGRect containerBounds = self.containerBounds;
  LayoutRect helpLabelLayout = LayoutRectZero;
  helpLabelLayout.boundingWidth = CGRectGetWidth(containerBounds);
  helpLabelLayout.size = [self.helpLabel sizeThatFits:containerBounds.size];
  helpLabelLayout.position.originY =
      CGRectGetMaxY(self.messageLabel.frame) + kHelpLabelTopPadding;
  self.helpLabel.frame =
      AlignRectOriginAndSizeToPixels(LayoutRectGetRect(helpLabelLayout));
}

- (void)layoutReloadButton {
  CGRect containerBounds = self.containerBounds;
  BOOL isIPadIdiom = IsIPadIdiom();
  BOOL isPortrait = IsPortrait();
  BOOL shouldAddReloadButtonToContainer = isIPadIdiom || !isPortrait;
  LayoutRect reloadButtonLayout = LayoutRectZero;
  reloadButtonLayout.size =
      isIPadIdiom
          ? [self.reloadButton sizeThatFits:CGSizeZero]
          : CGSizeMake(CGRectGetWidth(containerBounds), kReloadButtonHeight);
  if (shouldAddReloadButtonToContainer) {
    // Right-align reloadButton and add it below helpLabel when adding it to
    // the containerView.
    if (self.reloadButton.superview != self.containerView)
      [self.containerView addSubview:self.reloadButton];
    reloadButtonLayout.boundingWidth = CGRectGetWidth(containerBounds);
    reloadButtonLayout.position = LayoutRectPositionMake(
        CGRectGetWidth(containerBounds) - reloadButtonLayout.size.width,
        CGRectGetMaxY(self.helpLabel.frame) + kReloadButtonTopPadding);
  } else {
    // Bottom-align the reloadButton with the bounds specified by kLayoutInsets.
    if (self.reloadButton.superview != self)
      [self addSubview:self.reloadButton];
    reloadButtonLayout.boundingWidth = CGRectGetWidth(self.bounds);
    reloadButtonLayout.position = LayoutRectPositionMake(
        UIEdgeInsetsGetLeading(kLayoutInsets),
        CGRectGetMaxY(self.bounds) - kLayoutInsets.bottom -
            reloadButtonLayout.size.height);
  }
  self.reloadButton.frame =
      AlignRectOriginAndSizeToPixels(LayoutRectGetRect(reloadButtonLayout));
}

- (void)layoutContainerView {
  UIView* bottomSubview = self.reloadButton.superview == self.containerView
                              ? self.reloadButton
                              : self.helpLabel;
  CGSize containerSize = CGSizeMake(CGRectGetWidth(self.containerBounds),
                                    CGRectGetMaxY(bottomSubview.frame));
  CGFloat containerOriginX =
      (CGRectGetWidth(self.bounds) - containerSize.width) / 2.0f;
  CGFloat containerOriginY = 0.0f;
  if (IsIPadIdiom()) {
    // Center the containerView on iPads.
    containerOriginY =
        (CGRectGetHeight(self.bounds) - containerSize.height) / 2.0f;
  } else if (IsPortrait()) {
    // Align containerView to a quarter of the view height on portrait iPhones.
    containerOriginY =
        (CGRectGetHeight(self.bounds) - containerSize.height) / 4.0f;
  } else {
    // Top-align containerView on landscape iPhones.
    containerOriginY = kContainerViewLandscapeTopPadding;
  }
  self.containerView.frame = AlignRectOriginAndSizeToPixels(
      CGRectMake(containerOriginX, containerOriginY, containerSize.width,
                 containerSize.height));
}

#pragma mark Util

- (void)handleHelpLabelLinkButtonTapped {
  base::scoped_nsobject<OpenUrlCommand> openCommand(
      [[OpenUrlCommand alloc] initWithURLFromChrome:GURL(kCrashReasonURL)]);
  [self chromeExecuteCommand:openCommand];
}

- (void)handleReloadButtonTapped {
  _reloadHandler.get()();
}

+ (UIColor*)sadTabBackgroundColor {
  return [UIColor colorWithWhite:kBackgroundColorBrightness alpha:1.0];
}

@end
