// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sad_tab/sad_tab_view.h"

#include "base/strings/sys_string_conversions.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/commands/open_url_command.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#include "ios/web/public/navigation_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
const CGFloat kFooterLabelTopPadding = 16.0f;
const CGFloat kActionButtonHeight = 48.0f;
const CGFloat kActionButtonTopPadding = 16.0f;
// Label font sizes.
const CGFloat kTitleLabelFontSize = 23.0f;
const CGFloat kMessageLabelFontSize = 14.0f;
const CGFloat kFooterLabelFontSize = 14.0f;
// String constants for formatting bullets.
// "<5xSpace><Bullet><4xSpace><Content>".
NSString* const kMessageLabelBulletPrefix = @"     \u2022    ";
// "<newline>".
NSString* const kMessageLabelBulletSuffix = @"\n";
// "<RTL Begin Indicator><NSString Token><RTL End Indicator>".
NSString* const kMessageLabelBulletRTLFormat = @"\u202E%@\u202C";
}  // namespace

@interface SadTabView ()

// Container view that displays all other subviews.
@property(nonatomic, readonly, strong) UIView* containerView;
// Displays the Sad Tab face.
@property(nonatomic, readonly, strong) UIImageView* imageView;
// Displays the Sad Tab title.
@property(nonatomic, readonly, strong) UILabel* titleLabel;
// Displays the Sad Tab message.
@property(nonatomic, readonly, strong) UILabel* messageLabel;
// Displays the Sad Tab footer message (including a link to more help).
@property(nonatomic, readonly, strong) UILabel* footerLabel;
// Provides Link functionality to the footerLabel.
@property(nonatomic, readonly, strong)
    LabelLinkController* footerLabelLinkController;
// Triggers a reload or feedback action.
@property(nonatomic, readonly, strong) MDCFlatButton* actionButton;
// The bounds of |containerView|, with a height updated to CGFLOAT_MAX to allow
// text to be laid out using as many lines as necessary.
@property(nonatomic, readonly) CGRect containerBounds;
// Allows this view to perform navigation actions such as reloading.
@property(nonatomic, readonly) web::NavigationManager* navigationManager;

// Subview layout methods.  Must be called in the following order, as subsequent
// layouts reference the values set in previous functions.
- (void)layoutImageView;
- (void)layoutTitleLabel;
- (void)layoutMessageLabel;
- (void)layoutFooterLabel;
- (void)layoutActionButton;
- (void)layoutContainerView;

// Takes an array of strings and bulletizes them into a single multi-line string
// for display.
+ (nonnull NSString*)bulletedStringFromStrings:
    (nonnull NSArray<NSString*>*)strings;

// Returns the appropriate title for the view, e.g. 'Aw Snap!'.
- (nonnull NSString*)titleLabelText;
// Returns the appropriate message label body for the view, this will typically
// be a larger body of explanation or help text.
- (nonnull NSString*)messageLabelText;
// Returns the full footer string containing a link, intended to be the last
// piece of text.
- (nonnull NSString*)footerLabelText;
// Returns the substring of the footer string which is to be the underlined link
// text. (May be the entire footer label string).
- (nonnull NSString*)footerLinkText;
// Returns the string to be used for the main action button.
- (nonnull NSString*)buttonText;

// Attaches a link controller to |label|, finding the |linkString|
// within the |label| text to use as the link.
- (void)attachLinkControllerToLabel:(nonnull UILabel*)label
                        forLinkText:(nonnull NSString*)linkText;

// The action selector for |_actionButton|.
- (void)handleActionButtonTapped:(id)sender;

// Returns the desired background color.
+ (UIColor*)sadTabBackgroundColor;

@end

#pragma mark - SadTabView

@implementation SadTabView

@synthesize imageView = _imageView;
@synthesize containerView = _containerView;
@synthesize titleLabel = _titleLabel;
@synthesize messageLabel = _messageLabel;
@synthesize footerLabel = _footerLabel;
@synthesize footerLabelLinkController = _footerLabelLinkController;
@synthesize actionButton = _actionButton;
@synthesize mode = _mode;
@synthesize navigationManager = _navigationManager;

- (instancetype)initWithMode:(SadTabViewMode)mode
           navigationManager:(web::NavigationManager*)navigationManager {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _mode = mode;
    _navigationManager = navigationManager;
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

#pragma mark - Text Utilities

+ (nonnull NSString*)bulletedStringFromStrings:
    (nonnull NSArray<NSString*>*)strings {
  // Ensures the bullet string is appropriately directional.
  NSString* directionalBulletPrefix =
      base::i18n::IsRTL()
          ? [NSString stringWithFormat:kMessageLabelBulletRTLFormat,
                                       kMessageLabelBulletPrefix]
          : kMessageLabelBulletPrefix;
  NSMutableString* bulletedString = [NSMutableString string];
  for (NSString* string in strings) {
    // If content line has been added to the bulletedString already, ensure the
    // suffix is applied, otherwise don't (e.g. don't for the first item).
    NSArray* newStringArray =
        bulletedString.length
            ? @[ kMessageLabelBulletSuffix, directionalBulletPrefix, string ]
            : @[ directionalBulletPrefix, string ];
    [bulletedString appendString:[newStringArray componentsJoinedByString:@""]];
  }
  DCHECK(bulletedString);
  return bulletedString;
}

#pragma mark - Label Text

- (nonnull NSString*)titleLabelText {
  NSString* label = nil;
  switch (self.mode) {
    case SadTabViewMode::RELOAD:
      label = l10n_util::GetNSString(IDS_SAD_TAB_TITLE);
      break;
    case SadTabViewMode::FEEDBACK:
      label = l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_TITLE);
      break;
  }
  DCHECK(label);
  return label;
}

- (nonnull NSString*)messageLabelText {
  NSString* label = nil;
  switch (self.mode) {
    case SadTabViewMode::RELOAD:
      label = l10n_util::GetNSString(IDS_SAD_TAB_MESSAGE);
      break;
    case SadTabViewMode::FEEDBACK:
      label = l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_TRY);
      label = [label
          stringByAppendingFormat:
              @"\n\n%@", [[self class] bulletedStringFromStrings:@[
                l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_CLOSE_NOTABS),
                l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_INCOGNITO),
                l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_RESTART_BROWSER),
                l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_RESTART_DEVICE)
              ]]];

      break;
  }
  DCHECK(label);
  return label;
}

- (nonnull NSString*)footerLabelText {
  NSString* label = nil;
  switch (self.mode) {
    case SadTabViewMode::RELOAD: {
      base::string16 footerLinkText(
          l10n_util::GetStringUTF16(IDS_SAD_TAB_HELP_LINK));
      label = base::SysUTF16ToNSString(
          l10n_util::GetStringFUTF16(IDS_SAD_TAB_HELP_MESSAGE, footerLinkText));
    } break;
    case SadTabViewMode::FEEDBACK:
      label = l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_LEARN_MORE);
      break;
  }
  DCHECK(label);
  return label;
}

- (nonnull NSString*)footerLinkText {
  NSString* label = nil;
  switch (self.mode) {
    case SadTabViewMode::RELOAD: {
      base::string16 footerLinkText(
          l10n_util::GetStringUTF16(IDS_SAD_TAB_HELP_LINK));
      label = base::SysUTF16ToNSString(footerLinkText);
    } break;
    case SadTabViewMode::FEEDBACK:
      label = l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_LEARN_MORE);
      break;
  }
  DCHECK(label);
  return label;
}

- (nonnull NSString*)buttonText {
  NSString* label = nil;
  switch (self.mode) {
    case SadTabViewMode::RELOAD:
      label = l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_LABEL);
      break;
    case SadTabViewMode::FEEDBACK:
      label = l10n_util::GetNSString(IDS_SAD_TAB_SEND_FEEDBACK_LABEL);
      break;
  }
  DCHECK(label);
  return label;
}

- (void)attachLinkControllerToLabel:(nonnull UILabel*)label
                        forLinkText:(nonnull NSString*)linkText {
  __weak __typeof(self) weakSelf = self;
  _footerLabelLinkController = [[LabelLinkController alloc]
      initWithLabel:label
             action:^(const GURL& URL) {
               OpenUrlCommand* command =
                   [[OpenUrlCommand alloc] initWithURLFromChrome:URL];
               [weakSelf chromeExecuteCommand:command];
             }];

  _footerLabelLinkController.linkFont =
      [MDFRobotoFontLoader.sharedInstance boldFontOfSize:kFooterLabelFontSize];
  _footerLabelLinkController.linkUnderlineStyle = NSUnderlineStyleSingle;
  NSRange linkRange = [label.text rangeOfString:linkText];
  DCHECK(linkRange.location != NSNotFound);
  DCHECK(linkRange.length > 0);
  [_footerLabelLinkController addLinkWithRange:linkRange
                                           url:GURL(kCrashReasonURL)];
}

#pragma mark Accessors

- (UIView*)containerView {
  if (!_containerView) {
    _containerView = [[UIView alloc] initWithFrame:CGRectZero];
    [_containerView setBackgroundColor:self.backgroundColor];
  }
  return _containerView;
}

- (UIImageView*)imageView {
  if (!_imageView) {
    _imageView =
        [[UIImageView alloc] initWithImage:NativeImage(IDR_CRASH_SAD_TAB)];
    [_imageView setBackgroundColor:self.backgroundColor];
  }
  return _imageView;
}

- (UILabel*)titleLabel {
  if (!_titleLabel) {
    _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    [_titleLabel setBackgroundColor:self.backgroundColor];
    [_titleLabel setText:[self titleLabelText]];
    [_titleLabel setLineBreakMode:NSLineBreakByWordWrapping];
    [_titleLabel setNumberOfLines:0];
    [_titleLabel
        setTextColor:[UIColor colorWithWhite:kTitleLabelTextColorBrightness
                                       alpha:1.0]];
    [_titleLabel setFont:[[MDFRobotoFontLoader sharedInstance]
                             regularFontOfSize:kTitleLabelFontSize]];
  }
  return _titleLabel;
}

- (UILabel*)messageLabel {
  if (!_messageLabel) {
    _messageLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    [_messageLabel setBackgroundColor:self.backgroundColor];
    [_messageLabel setText:[self messageLabelText]];
    [_messageLabel setLineBreakMode:NSLineBreakByWordWrapping];
    [_messageLabel setNumberOfLines:0];
    [_messageLabel
        setTextColor:[UIColor colorWithWhite:kMessageLabelTextColorBrightness
                                       alpha:1.0]];
    [_messageLabel setFont:[[MDFRobotoFontLoader sharedInstance]
                               regularFontOfSize:kMessageLabelFontSize]];
  }
  return _messageLabel;
}

- (UILabel*)footerLabel {
  if (!_footerLabel) {
    _footerLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    [_footerLabel setBackgroundColor:self.backgroundColor];
    [_footerLabel setNumberOfLines:0];
    [_footerLabel setFont:[[MDFRobotoFontLoader sharedInstance]
                              regularFontOfSize:kFooterLabelFontSize]];
    [_footerLabel
        setTextColor:[UIColor colorWithWhite:kMessageLabelTextColorBrightness
                                       alpha:1.0]];

    [_footerLabel setText:[self footerLabelText]];
    [self attachLinkControllerToLabel:_footerLabel
                          forLinkText:[self footerLinkText]];
  }
  return _footerLabel;
}

- (UIButton*)actionButton {
  if (!_actionButton) {
    _actionButton = [[MDCFlatButton alloc] init];
    [_actionButton setBackgroundColor:[[MDCPalette cr_bluePalette] tint500]
                             forState:UIControlStateNormal];
    [_actionButton setBackgroundColor:[[MDCPalette greyPalette] tint500]
                             forState:UIControlStateDisabled];
    [_actionButton setCustomTitleColor:[UIColor whiteColor]];
    [_actionButton setUnderlyingColorHint:[UIColor blackColor]];
    [_actionButton setInkColor:[UIColor colorWithWhite:1 alpha:0.2f]];

    [_actionButton setTitle:[self buttonText] forState:UIControlStateNormal];
    [_actionButton setTitleColor:[UIColor whiteColor]
                        forState:UIControlStateNormal];
    [_actionButton addTarget:self
                      action:@selector(handleActionButtonTapped:)
            forControlEvents:UIControlEventTouchUpInside];
  }
  return _actionButton;
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
  [self.containerView addSubview:self.footerLabel];
}

- (void)layoutSubviews {
  [super layoutSubviews];

  [self layoutImageView];
  [self layoutTitleLabel];
  [self layoutMessageLabel];
  [self layoutFooterLabel];
  [self layoutActionButton];
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

- (void)layoutFooterLabel {
  CGRect containerBounds = self.containerBounds;
  LayoutRect footerLabelLayout = LayoutRectZero;
  footerLabelLayout.boundingWidth = CGRectGetWidth(containerBounds);
  footerLabelLayout.size = [self.footerLabel sizeThatFits:containerBounds.size];
  footerLabelLayout.position.originY =
      CGRectGetMaxY(self.messageLabel.frame) + kFooterLabelTopPadding;
  self.footerLabel.frame =
      AlignRectOriginAndSizeToPixels(LayoutRectGetRect(footerLabelLayout));
}

- (void)layoutActionButton {
  CGRect containerBounds = self.containerBounds;
  BOOL isIPadIdiom = IsIPadIdiom();
  BOOL isPortrait = IsPortrait();
  BOOL shouldAddActionButtonToContainer = isIPadIdiom || !isPortrait;
  LayoutRect actionButtonLayout = LayoutRectZero;
  actionButtonLayout.size =
      isIPadIdiom
          ? [self.actionButton sizeThatFits:CGSizeZero]
          : CGSizeMake(CGRectGetWidth(containerBounds), kActionButtonHeight);
  if (shouldAddActionButtonToContainer) {
    // Right-align actionButton and add it below helpLabel when adding it to
    // the containerView.
    if (self.actionButton.superview != self.containerView)
      [self.containerView addSubview:self.actionButton];
    actionButtonLayout.boundingWidth = CGRectGetWidth(containerBounds);
    actionButtonLayout.position = LayoutRectPositionMake(
        CGRectGetWidth(containerBounds) - actionButtonLayout.size.width,
        CGRectGetMaxY(self.footerLabel.frame) + kActionButtonTopPadding);
  } else {
    // Bottom-align the actionButton with the bounds specified by kLayoutInsets.
    if (self.actionButton.superview != self)
      [self addSubview:self.actionButton];
    actionButtonLayout.boundingWidth = CGRectGetWidth(self.bounds);
    actionButtonLayout.position = LayoutRectPositionMake(
        UIEdgeInsetsGetLeading(kLayoutInsets),
        CGRectGetMaxY(self.bounds) - kLayoutInsets.bottom -
            actionButtonLayout.size.height);
  }
  self.actionButton.frame =
      AlignRectOriginAndSizeToPixels(LayoutRectGetRect(actionButtonLayout));
}

- (void)layoutContainerView {
  UIView* bottomSubview = self.actionButton.superview == self.containerView
                              ? self.actionButton
                              : self.footerLabel;
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

- (void)handleActionButtonTapped:(id)sender {
  switch (self.mode) {
    case SadTabViewMode::RELOAD:
      self.navigationManager->Reload(web::ReloadType::NORMAL, true);
      break;
    case SadTabViewMode::FEEDBACK: {
      GenericChromeCommand* command =
          [[GenericChromeCommand alloc] initWithTag:IDC_REPORT_AN_ISSUE];
      [self chromeExecuteCommand:command];
      break;
    }
  };
}

+ (UIColor*)sadTabBackgroundColor {
  return [UIColor colorWithWhite:kBackgroundColorBrightness alpha:1.0];
}

@end
