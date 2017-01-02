// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/share_extension/share_extension_view.h"

#include "base/logging.h"
#import "ios/chrome/share_extension/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kCornerRadius = 6;
// Minimum size around the widget
const CGFloat kDividerHeight = 0.5;
const CGFloat kShareExtensionPadding = 16;
const CGFloat kButtonHeight = 44;
const CGFloat kLowAlpha = 0.3;

// Size of the icon if present.
const CGFloat kScreenshotSize = 80;

// Size for the buttons font.
const CGFloat kButtonFontSize = 17;

}  // namespace

#pragma mark - Share Extension Button

// UIButton with the background color changing when it is highlighted.
@interface ShareExtensionButton : UIButton
@end

@implementation ShareExtensionButton

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];

  if (highlighted)
    self.backgroundColor = [UIColor colorWithWhite:217.0f / 255.0f alpha:1];
  else
    self.backgroundColor = [UIColor clearColor];
}

@end

#pragma mark - Share Extension View

@interface ShareExtensionView () {
  __weak id<ShareExtensionViewActionTarget> _target;
}

// Keep strong references of the views that need to be updated.
@property(nonatomic, strong) UILabel* titleLabel;
@property(nonatomic, strong) UILabel* URLLabel;
@property(nonatomic, strong) UIImageView* screenshotView;
@property(nonatomic, strong) UIStackView* itemStack;

// View creation helpers.
// Returns a view containing the shared items (title, URL, screenshot). This
// method will set the ivars.
- (UIView*)sharedItemView;

// Returns a button containing the with title |title| and action |selector| on
// |_target|.
- (UIButton*)buttonWithTitle:(NSString*)title selector:(SEL)selector;

// Returns a view containing a divider with vibrancy effect.
- (UIView*)dividerViewWithVibrancy:(UIVisualEffect*)vibrancyEffect;

// Returns a navigationBar.
- (UINavigationBar*)navigationBar;

@end

@implementation ShareExtensionView

@synthesize titleLabel = _titleLabel;
@synthesize URLLabel = _URLLabel;
@synthesize screenshotView = _screenshotView;
@synthesize itemStack = _itemStack;

#pragma mark - Lifecycle

- (instancetype)initWithActionTarget:
    (id<ShareExtensionViewActionTarget>)target {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(target);
    _target = target;

    [self.layer setCornerRadius:kCornerRadius];
    [self setClipsToBounds:YES];
    UIBlurEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleExtraLight];
    UIVibrancyEffect* vibrancyEffect =
        [UIVibrancyEffect effectForBlurEffect:blurEffect];

    self.backgroundColor = [UIColor colorWithWhite:242.0f / 255.0f alpha:1];

    // Add the blur effect to the whole widget.
    UIVisualEffectView* blurringView =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    [self addSubview:blurringView];
    ui_util::ConstrainAllSidesOfViewToView(self, blurringView);
    [[blurringView layer] setCornerRadius:kCornerRadius];
    [blurringView setClipsToBounds:YES];

    NSString* addToReadingListTitle = NSLocalizedString(
        @"IDS_IOS_ADD_READING_LIST_SHARE_EXTENSION",
        @"The add to reading list button text in share extension.");
    UIButton* readingListButton = [self
        buttonWithTitle:addToReadingListTitle
               selector:@selector(
                            shareExtensionViewDidSelectAddToReadingList:)];

    NSString* addToBookmarksTitle = NSLocalizedString(
        @"IDS_IOS_ADD_BOOKMARKS_SHARE_EXTENSION",
        @"The Add to bookmarks button text in share extension.");
    UIButton* bookmarksButton = [self
        buttonWithTitle:addToBookmarksTitle
               selector:@selector(shareExtensionViewDidSelectAddToBookmarks:)];

    UIStackView* contentStack = [[UIStackView alloc] initWithArrangedSubviews:@[
      [self navigationBar], [self dividerViewWithVibrancy:vibrancyEffect],
      [self sharedItemView], [self dividerViewWithVibrancy:vibrancyEffect],
      readingListButton, [self dividerViewWithVibrancy:vibrancyEffect],
      bookmarksButton
    ]];
    [contentStack setAxis:UILayoutConstraintAxisVertical];
    [[blurringView contentView] addSubview:contentStack];

    [blurringView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [contentStack setTranslatesAutoresizingMaskIntoConstraints:NO];

    ui_util::ConstrainAllSidesOfViewToView([blurringView contentView],
                                           contentStack);
  }
  return self;
}

#pragma mark Init helpers

- (UIView*)sharedItemView {
  // Title label. Text will be filled by |setTitle:| when available.
  _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  [_titleLabel setFont:[UIFont boldSystemFontOfSize:16]];
  [_titleLabel setTranslatesAutoresizingMaskIntoConstraints:NO];

  // URL label. Text will be filled by |setURL:| when available.
  _URLLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  [_URLLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_URLLabel setNumberOfLines:3];
  [_URLLabel setLineBreakMode:NSLineBreakByWordWrapping];
  [_URLLabel setFont:[UIFont systemFontOfSize:12]];

  // Screenshot view. Image will be filled by |setScreenshot:| when available.
  _screenshotView = [[UIImageView alloc] initWithFrame:CGRectZero];
  [_screenshotView.widthAnchor
      constraintLessThanOrEqualToConstant:kScreenshotSize]
      .active = YES;
  [_screenshotView.heightAnchor
      constraintEqualToAnchor:_screenshotView.widthAnchor]
      .active = YES;
  [_screenshotView setContentMode:UIViewContentModeScaleAspectFill];
  [_screenshotView setClipsToBounds:YES];
  [_screenshotView setHidden:YES];

  // |_screenshotView| should take as much space as needed. Lower compression
  // resistance of the other elements.
  [_titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_titleLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                 forAxis:UILayoutConstraintAxisVertical];
  [_URLLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                               forAxis:UILayoutConstraintAxisVertical];

  [_URLLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];

  UIStackView* titleURLStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _titleLabel, _URLLabel ]];
  [titleURLStack setAxis:UILayoutConstraintAxisVertical];

  UIView* titleURLContainer = [[UIView alloc] initWithFrame:CGRectZero];
  [titleURLContainer setTranslatesAutoresizingMaskIntoConstraints:NO];
  [titleURLContainer addSubview:titleURLStack];
  [[titleURLStack topAnchor]
      constraintEqualToAnchor:[titleURLContainer topAnchor]
                     constant:kShareExtensionPadding]
      .active = YES;
  [[titleURLStack bottomAnchor]
      constraintEqualToAnchor:[titleURLContainer bottomAnchor]
                     constant:-kShareExtensionPadding]
      .active = YES;

  [titleURLStack.centerYAnchor
      constraintEqualToAnchor:titleURLContainer.centerYAnchor]
      .active = YES;
  [titleURLStack.centerXAnchor
      constraintEqualToAnchor:titleURLContainer.centerXAnchor]
      .active = YES;
  [titleURLStack.widthAnchor
      constraintEqualToAnchor:titleURLContainer.widthAnchor]
      .active = YES;
  [titleURLStack
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisVertical];

  _itemStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[ titleURLContainer ]];
  [_itemStack setAxis:UILayoutConstraintAxisHorizontal];
  [_itemStack setLayoutMargins:UIEdgeInsetsMake(kShareExtensionPadding,
                                                kShareExtensionPadding,
                                                kShareExtensionPadding,
                                                kShareExtensionPadding)];
  [_itemStack setLayoutMarginsRelativeArrangement:YES];
  [_itemStack setSpacing:kShareExtensionPadding];

  [_titleLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_URLLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_screenshotView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [titleURLStack setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_itemStack setTranslatesAutoresizingMaskIntoConstraints:NO];

  return _itemStack;
}

- (UIView*)dividerViewWithVibrancy:(UIVisualEffect*)vibrancyEffect {
  UIVisualEffectView* dividerVibrancy =
      [[UIVisualEffectView alloc] initWithEffect:vibrancyEffect];
  UIView* divider = [[UIView alloc] initWithFrame:CGRectZero];
  [divider setBackgroundColor:[UIColor colorWithWhite:0 alpha:kLowAlpha]];
  [[dividerVibrancy contentView] addSubview:divider];
  [dividerVibrancy setTranslatesAutoresizingMaskIntoConstraints:NO];
  [divider setTranslatesAutoresizingMaskIntoConstraints:NO];
  ui_util::ConstrainAllSidesOfViewToView([dividerVibrancy contentView],
                                         divider);
  CGFloat slidingConstant = ui_util::AlignValueToPixel(kDividerHeight);
  [[dividerVibrancy heightAnchor] constraintEqualToConstant:slidingConstant]
      .active = YES;
  return dividerVibrancy;
}

- (UIButton*)buttonWithTitle:(NSString*)title selector:(SEL)selector {
  UIButton* systemButton = [UIButton buttonWithType:UIButtonTypeSystem];
  UIColor* systemColor = [systemButton titleColorForState:UIControlStateNormal];

  UIButton* button = [[ShareExtensionButton alloc] initWithFrame:CGRectZero];
  [button setTitle:title forState:UIControlStateNormal];
  [button setTitleColor:systemColor forState:UIControlStateNormal];
  [[button titleLabel] setFont:[UIFont systemFontOfSize:kButtonFontSize]];
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];
  [button addTarget:_target
                action:selector
      forControlEvents:UIControlEventTouchUpInside];
  [button.heightAnchor constraintEqualToConstant:kButtonHeight].active = YES;
  return button;
}

- (UINavigationBar*)navigationBar {
  // Create the navigation bar.
  UINavigationBar* navigationBar =
      [[UINavigationBar alloc] initWithFrame:CGRectZero];
  [[navigationBar layer] setCornerRadius:kCornerRadius];
  [navigationBar setClipsToBounds:YES];

  // Create an empty image to replace the standard gray background of the
  // UINavigationBar.
  UIImage* emptyImage = [[UIImage alloc] init];
  [navigationBar setBackgroundImage:emptyImage
                      forBarMetrics:UIBarMetricsDefault];
  [navigationBar setShadowImage:emptyImage];
  [navigationBar setTranslucent:YES];
  [navigationBar setTranslatesAutoresizingMaskIntoConstraints:NO];

  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:_target
                           action:@selector(
                                      shareExtensionViewDidSelectCancel:)];

  NSString* appName =
      [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleDisplayName"];
  UINavigationItem* titleItem =
      [[UINavigationItem alloc] initWithTitle:appName];
  [titleItem setLeftBarButtonItem:cancelButton];
  [titleItem setHidesBackButton:YES];
  [navigationBar pushNavigationItem:titleItem animated:NO];
  return navigationBar;
}

#pragma mark - Content getters and setters.

- (void)setURL:(NSURL*)URL {
  [[self URLLabel] setText:[URL absoluteString]];
}

- (void)setTitle:(NSString*)title {
  [[self titleLabel] setText:title];
}

- (void)setScreenshot:(UIImage*)screenshot {
  [[self screenshotView] setHidden:NO];
  [[self screenshotView] setImage:screenshot];
  [[self itemStack] addArrangedSubview:[self screenshotView]];
}

@end
