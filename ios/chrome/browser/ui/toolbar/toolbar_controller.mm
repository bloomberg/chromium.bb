// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"

#include <QuartzCore/QuartzCore.h>

#include "base/format_macros.h"
#include "base/i18n/rtl.h"
#include "base/ios/ios_util.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/animation_util.h"
#include "ios/chrome/browser/ui/bubble/bubble_util.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/fullscreen_controller.h"
#import "ios/chrome/browser/ui/image_util.h"
#import "ios/chrome/browser/ui/reversed_animation.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller+protected.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_resource_macros.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/toolbar/tools_menu_button_observer_bridge.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_configuration.h"
#import "ios/chrome/browser/ui/tools_menu/tools_popup_controller.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/common/material_timing.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;
using ios::material::TimingFunction;

// Animation key used for stack view transition animations
NSString* const kToolbarTransitionAnimationKey = @"ToolbarTransitionAnimation";

// Externed max tab count.
const NSInteger kStackButtonMaxTabCount = 99;
// Font sizes for the button containing the tab count
const NSInteger kFontSizeFewerThanTenTabs = 11;
const NSInteger kFontSizeTenTabsOrMore = 9;

// The initial capacity used to construct |self.transitionLayers|.  The value
// is chosen because WebToolbarController animates 11 separate layers during
// transitions; this value should be updated if new subviews are animated in
// the future.
const NSUInteger kTransitionLayerCapacity = 11;

// Externed delay before non-initial button images are loaded.
const int64_t kNonInitialImageAdditionDelayNanosec = 500000LL;
NSString* const kMenuWillShowNotification = @"kMenuWillShowNotification";
NSString* const kMenuWillHideNotification = @"kMenuWillHideNotification";

NSString* const kToolbarIdentifier = @"kToolbarIdentifier";
NSString* const kIncognitoToolbarIdentifier = @"kIncognitoToolbarIdentifier";
NSString* const kToolbarToolsMenuButtonIdentifier =
    @"kToolbarToolsMenuButtonIdentifier";
NSString* const kToolbarStackButtonIdentifier =
    @"kToolbarStackButtonIdentifier";
NSString* const kToolbarShareButtonIdentifier =
    @"kToolbarShareButtonIdentifier";

// Macros for creating CGRects of height H, origin (0,0), with the portrait
// width of phone/pad devices.
// clang-format off
#define IPHONE_FRAME(H) { { 0, 0 }, { kPortraitWidth[IPHONE_IDIOM], H } }
#define IPAD_FRAME(H)   { { 0, 0 }, { kPortraitWidth[IPAD_IDIOM],   H } }

// Makes a two-element C array of CGRects as described above, one for each
// device idiom.
#define FRAME_PAIR(H) { IPHONE_FRAME(H), IPAD_FRAME(H) }
// clang-format on

const CGRect kToolbarFrame[INTERFACE_IDIOM_COUNT] = FRAME_PAIR(56);

namespace {

// Color constants for the stack button text, normal and pressed states.  These
// arrays are indexed by ToolbarControllerStyle enum values.
const CGFloat kStackButtonNormalColors[] = {
    85.0 / 255.0,   // ToolbarControllerStyleLightMode
    238.0 / 255.0,  // ToolbarControllerStyleDarkMode
    238.0 / 255.0,  // ToolbarControllerStyleIncognitoMode
};

const int kStackButtonHighlightedColors[] = {
    0x4285F4,  // ToolbarControllerStyleLightMode
    0x888a8c,  // ToolbarControllerStyleDarkMode
    0x888a8c,  // ToolbarControllerStyleIncognitoMode
};

// UI frames.  iPhone values followed by iPad values.
// Full-width frames that don't change for RTL languages.
const CGRect kBackgroundViewFrame[INTERFACE_IDIOM_COUNT] = FRAME_PAIR(56);
const CGRect kShadowViewFrame[INTERFACE_IDIOM_COUNT] = FRAME_PAIR(2);
// Full bleed shadow frame is iPhone-only
const CGRect kFullBleedShadowViewFrame = IPHONE_FRAME(10);

// Frames that change for RTL.
// clang-format off
const LayoutRect kStackButtonFrame =
  {kPortraitWidth[IPHONE_IDIOM], {230, 4}, {48, 48}};
const LayoutRect kShareMenuButtonFrame =
  {kPortraitWidth[IPAD_IDIOM], {680, 4}, {46, 48}};
const LayoutRect kToolsMenuButtonFrame[INTERFACE_IDIOM_COUNT] = {
  {kPortraitWidth[IPHONE_IDIOM], {276, 4}, {44, 48}},
  {kPortraitWidth[IPAD_IDIOM], {723, 4}, {46, 48}}
};
// clang-format on

// Distance to shift buttons when fading out.
const LayoutOffset kButtonFadeOutXOffset = 10;

// The amount of horizontal padding removed from a view's frame when presenting
// a popover anchored to it.
const CGFloat kPopoverAnchorHorizontalPadding = 10.0;

}  // namespace

// Helper class to display a UIButton with the image and text centered
// vertically and horizontally.
@interface ToolbarCenteredButton : UIButton {
}
@end

@implementation ToolbarCenteredButton

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.titleLabel.textAlignment = NSTextAlignmentCenter;
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  CGSize size = self.bounds.size;
  CGPoint center = CGPointMake(size.width / 2, size.height / 2);
  self.imageView.center = center;
  self.imageView.frame = AlignRectToPixel(self.imageView.frame);
  self.titleLabel.frame = self.bounds;
}

@end

@implementation ToolbarView

@synthesize delegate = delegate_;
@synthesize animatingTransition = animatingTransition_;
@synthesize hitTestBoundsContraintRelaxed = hitTestBoundsContraintRelaxed_;

// Some views added to the toolbar have bounds larger than the toolbar bounds
// and still needs to receive touches. The overscroll actions view is one of
// those. That method is overridden in order to still perform hit testing on
// subviews that resides outside the toolbar bounds.
- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
  UIView* hitView = [super hitTest:point withEvent:event];
  if (hitView || !self.hitTestBoundsContraintRelaxed)
    return hitView;

  for (UIView* view in [[self subviews] reverseObjectEnumerator]) {
    if (!view.userInteractionEnabled || [view isHidden] || [view alpha] < 0.01)
      continue;
    const CGPoint convertedPoint = [view convertPoint:point fromView:self];
    if ([view pointInside:convertedPoint withEvent:event]) {
      hitView = [view hitTest:convertedPoint withEvent:event];
      if (hitView)
        break;
    }
  }
  return hitView;
}

- (void)setFrame:(CGRect)frame {
  CGRect oldFrame = self.frame;
  [super setFrame:frame];
  [delegate_ frameDidChangeFrame:frame fromFrame:oldFrame];
}

- (void)didMoveToWindow {
  [super didMoveToWindow];
  [delegate_ windowDidChange];
}

- (id<CAAction>)actionForLayer:(CALayer*)layer forKey:(NSString*)event {
  // Don't allow UIView block-based animations if we're already performing
  // explicit transition animations.
  if (self.animatingTransition)
    return (id<CAAction>)[NSNull null];
  return [super actionForLayer:layer forKey:event];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [delegate_ traitCollectionDidChange];
}

@end

@interface ToolbarController () {
  // The shadow view. Only used on iPhone.
  UIImageView* fullBleedShadowView_;

  // The backing object for |self.transitionLayers|.
  NSMutableArray* transitionLayers_;

  ToolbarToolsMenuButton* toolsMenuButton_;
  UIButton* stackButton_;
  UIButton* shareButton_;
  NSArray* standardButtons_;
  ToolsMenuButtonObserverBridge* toolsMenuButtonObserverBridge_;
  ToolbarControllerStyle style_;

  // The following is nil if not visible.
  ToolsPopupController* toolsPopupController_;
}

// Returns the background image that should be used for |style|.
- (UIImage*)getBackgroundImageForStyle:(ToolbarControllerStyle)style;

// Whether the share button should be visible in the toolbar.
- (BOOL)shareButtonShouldBeVisible;

// Returns an animation for |button| for a toolbar transition animation with
// |style|.  |button|'s frame will be interpolated between its layout in the
// screen toolbar to the card's tab frame, and will be faded in for
// ToolbarTransitionStyleToStackView and faded out for
// ToolbarTransitionStyleToBVC.
- (CAAnimation*)transitionAnimationForButton:(UIButton*)button
                        containerBeginBounds:(CGRect)containerBeginBounds
                          containerEndBounds:(CGRect)containerEndBounds
                                   withStyle:(ToolbarTransitionStyle)style;
@end

@implementation ToolbarController

@synthesize readingListModel = readingListModel_;
@synthesize view = view_;
@synthesize backgroundView = backgroundView_;
@synthesize shadowView = shadowView_;
@synthesize toolsPopupController = toolsPopupController_;
@synthesize style = style_;
@synthesize dispatcher = dispatcher_;

- (instancetype)initWithStyle:(ToolbarControllerStyle)style
                   dispatcher:
                       (id<ApplicationCommands, BrowserCommands>)dispatcher {
  self = [super init];
  if (self) {
    style_ = style;
    dispatcher_ = dispatcher;
    DCHECK_LT(style_, ToolbarControllerStyleMaxStyles);

    InterfaceIdiom idiom = IsIPadIdiom() ? IPAD_IDIOM : IPHONE_IDIOM;
    CGRect viewFrame = kToolbarFrame[idiom];
    CGRect backgroundFrame = kBackgroundViewFrame[idiom];
    CGRect stackButtonFrame = LayoutRectGetRect(kStackButtonFrame);
    CGRect toolsMenuButtonFrame =
        LayoutRectGetRect(kToolsMenuButtonFrame[idiom]);

    if (idiom == IPHONE_IDIOM) {
      CGFloat statusBarOffset = [self statusBarOffset];
      viewFrame.size.height += statusBarOffset;
      backgroundFrame.size.height += statusBarOffset;
      stackButtonFrame.origin.y += statusBarOffset;
      toolsMenuButtonFrame.origin.y += statusBarOffset;
    }

    view_ = [[ToolbarView alloc] initWithFrame:viewFrame];
    backgroundView_ = [[UIImageView alloc] initWithFrame:backgroundFrame];
    toolsMenuButton_ =
        [[ToolbarToolsMenuButton alloc] initWithFrame:toolsMenuButtonFrame
                                                style:style_];
    [toolsMenuButton_ addTarget:self.dispatcher
                         action:@selector(showToolsMenu)
               forControlEvents:UIControlEventTouchUpInside];
    [toolsMenuButton_
        setAutoresizingMask:UIViewAutoresizingFlexibleLeadingMargin() |
                            UIViewAutoresizingFlexibleBottomMargin];

    [view_ addSubview:backgroundView_];
    [view_ addSubview:toolsMenuButton_];
    [view_ setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
    [backgroundView_ setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                         UIViewAutoresizingFlexibleHeight];

    if (idiom == IPAD_IDIOM) {
      CGRect shareButtonFrame = LayoutRectGetRect(kShareMenuButtonFrame);
      shareButton_ = [[UIButton alloc] initWithFrame:shareButtonFrame];
      [shareButton_
          setAutoresizingMask:UIViewAutoresizingFlexibleLeadingMargin() |
                              UIViewAutoresizingFlexibleBottomMargin];
      [self setUpButton:shareButton_
             withImageEnum:ToolbarButtonNameShare
           forInitialState:UIControlStateNormal
          hasDisabledImage:YES
             synchronously:NO];
      [shareButton_ addTarget:self.dispatcher
                       action:@selector(sharePage)
             forControlEvents:UIControlEventTouchUpInside];
      SetA11yLabelAndUiAutomationName(shareButton_, IDS_IOS_TOOLS_MENU_SHARE,
                                      kToolbarShareButtonIdentifier);
      [view_ addSubview:shareButton_];
    }

    CGRect shadowFrame = kShadowViewFrame[idiom];
    shadowFrame.origin.y = CGRectGetMaxY(backgroundFrame);
    shadowView_ = [[UIImageView alloc] initWithFrame:shadowFrame];
    [shadowView_ setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
    [shadowView_ setUserInteractionEnabled:NO];
    [view_ addSubview:shadowView_];
    [shadowView_ setImage:NativeImage(IDR_IOS_TOOLBAR_SHADOW)];

    if (idiom == IPHONE_IDIOM) {
      // iPad omnibox does not expand to full bleed.
      CGRect fullBleedShadowFrame = kFullBleedShadowViewFrame;
      fullBleedShadowFrame.origin.y = shadowFrame.origin.y;
      fullBleedShadowView_ =
          [[UIImageView alloc] initWithFrame:fullBleedShadowFrame];
      [fullBleedShadowView_
          setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
      [fullBleedShadowView_ setUserInteractionEnabled:NO];
      [fullBleedShadowView_ setAlpha:0];
      [view_ addSubview:fullBleedShadowView_];
      [fullBleedShadowView_
          setImage:NativeImage(IDR_IOS_TOOLBAR_SHADOW_FULL_BLEED)];
    }

    transitionLayers_ =
        [[NSMutableArray alloc] initWithCapacity:kTransitionLayerCapacity];

    // UIImageViews do not default to userInteractionEnabled:YES.
    [view_ setUserInteractionEnabled:YES];
    [backgroundView_ setUserInteractionEnabled:YES];

    UIImage* tile = [self getBackgroundImageForStyle:style];
    [[self backgroundView]
        setImage:StretchableImageFromUIImage(tile, 0.0, 3.0)];

    if (idiom == IPHONE_IDIOM) {
      stackButton_ =
          [[ToolbarCenteredButton alloc] initWithFrame:stackButtonFrame];
      [[stackButton_ titleLabel]
          setFont:[self fontForSize:kFontSizeFewerThanTenTabs]];
      [stackButton_
          setTitleColor:[UIColor colorWithWhite:kStackButtonNormalColors[style_]
                                          alpha:1.0]
               forState:UIControlStateNormal];
      UIColor* highlightColor =
          UIColorFromRGB(kStackButtonHighlightedColors[style_], 1.0);
      [stackButton_ setTitleColor:highlightColor
                         forState:UIControlStateHighlighted];

      [stackButton_
          setAutoresizingMask:UIViewAutoresizingFlexibleLeadingMargin() |
                              UIViewAutoresizingFlexibleBottomMargin];

      [self setUpButton:stackButton_
             withImageEnum:ToolbarButtonNameStack
           forInitialState:UIControlStateNormal
          hasDisabledImage:NO
             synchronously:NO];
      [view_ addSubview:stackButton_];
    }
    [self registerEventsForButton:toolsMenuButton_];

    self.view.accessibilityIdentifier =
        style == ToolbarControllerStyleIncognitoMode
            ? kIncognitoToolbarIdentifier
            : kToolbarIdentifier;
    SetA11yLabelAndUiAutomationName(stackButton_, IDS_IOS_TOOLBAR_SHOW_TABS,
                                    kToolbarStackButtonIdentifier);
    SetA11yLabelAndUiAutomationName(toolsMenuButton_, IDS_IOS_TOOLBAR_SETTINGS,
                                    kToolbarToolsMenuButtonIdentifier);
    [self updateStandardButtons];

    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(applicationDidEnterBackground:)
                          name:UIApplicationDidEnterBackgroundNotification
                        object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [toolsPopupController_ setDelegate:nil];
}

#pragma mark - Public API

- (void)applicationDidEnterBackground:(NSNotification*)notify {
  if (toolsPopupController_) {
    // Dismiss the tools popup menu without animation.
    [toolsMenuButton_ setToolsMenuIsVisible:NO];
    toolsPopupController_ = nil;
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kMenuWillHideNotification
                      object:nil];
  }
}

- (void)setReadingListModel:(ReadingListModel*)readingListModel {
  readingListModel_ = readingListModel;
  if (readingListModel_) {
    toolsMenuButtonObserverBridge_ =
        [[ToolsMenuButtonObserverBridge alloc] initWithModel:readingListModel_
                                               toolbarButton:toolsMenuButton_];
  }
}

#pragma mark ToolsMenuPopUp

- (void)showToolsMenuPopupWithConfiguration:
    (ToolsMenuConfiguration*)configuration {
  // Because an animation hides and shows the tools popup menu it is possible to
  // tap the tools button multiple times before the tools menu is shown. Ignore
  // repeated taps between animations.
  if (toolsPopupController_)
    return;

  base::RecordAction(UserMetricsAction("ShowAppMenu"));

  // Keep the button pressed.
  [toolsMenuButton_ setToolsMenuIsVisible:YES];

  [configuration setToolsMenuButton:toolsMenuButton_];
  toolsPopupController_ =
      [[ToolsPopupController alloc] initWithConfiguration:configuration
                                               dispatcher:self.dispatcher];

  [toolsPopupController_ setDelegate:self];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kMenuWillShowNotification
                    object:nil];
}

- (void)dismissToolsMenuPopup {
  if (!toolsPopupController_)
    return;
  ToolsPopupController* tempTPC = toolsPopupController_;
  [tempTPC containerView].userInteractionEnabled = NO;
  [tempTPC dismissAnimatedWithCompletion:^{
    // Unpress the tools menu button by restoring the normal and
    // highlighted images to their usual state.
    [toolsMenuButton_ setToolsMenuIsVisible:NO];
    // Reference tempTPC so the block retains it.
    [tempTPC self];
  }];
  // reset tabHistoryPopupController_ to prevent -applicationDidEnterBackground
  // from posting another kMenuWillHideNotification.
  toolsPopupController_ = nil;

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kMenuWillHideNotification
                    object:nil];
}

#pragma mark Appearance

- (void)setBackgroundAlpha:(CGFloat)alpha {
  [backgroundView_ setAlpha:alpha];
  [shadowView_ setAlpha:alpha];
}

- (void)setTabCount:(NSInteger)tabCount {
  if (!stackButton_)
    return;
  // Enable or disable the stack view icon based on the number of tabs. This
  // locks the user in the stack view when there are no tabs.
  [stackButton_ setEnabled:tabCount > 0 ? YES : NO];

  // Update the text shown in the |stackButton_|. Note that the button's title
  // may be empty or contain an easter egg, but the accessibility value will
  // always be equal to |tabCount|. Also, the text of |stackButton_| is shifted
  // up, via |kEasterEggTitleInsets|, to avoid overlapping with the button's
  // outline.
  NSString* stackButtonValue =
      [NSString stringWithFormat:@"%" PRIdNS, tabCount];
  NSString* stackButtonTitle;
  if (tabCount <= 0) {
    stackButtonTitle = @"";
  } else if (tabCount > kStackButtonMaxTabCount) {
    stackButtonTitle = @":)";
    [[stackButton_ titleLabel]
        setFont:[self fontForSize:kFontSizeFewerThanTenTabs]];
  } else {
    stackButtonTitle = stackButtonValue;
    if (tabCount < 10) {
      [[stackButton_ titleLabel]
          setFont:[self fontForSize:kFontSizeFewerThanTenTabs]];
    } else {
      [[stackButton_ titleLabel]
          setFont:[self fontForSize:kFontSizeTenTabsOrMore]];
    }
  }

  [stackButton_ setTitle:stackButtonTitle forState:UIControlStateNormal];
  [stackButton_ setAccessibilityValue:stackButtonValue];
}

- (void)setShareButtonEnabled:(BOOL)enabled {
  [shareButton_ setEnabled:enabled];
}

- (void)setUpButton:(UIButton*)button
       withImageEnum:(int)imageEnum
     forInitialState:(UIControlState)initialState
    hasDisabledImage:(BOOL)hasDisabledImage
       synchronously:(BOOL)synchronously {
  [self registerEventsForButton:button];
  // Add the non-initial images after a slight delay, to help performance
  // and responsiveness on startup.
  dispatch_time_t addImageDelay =
      dispatch_time(DISPATCH_TIME_NOW, kNonInitialImageAdditionDelayNanosec);

  void (^normalImageBlock)(void) = ^{
    UIImage* image =
        [self imageForImageEnum:imageEnum forState:ToolbarButtonUIStateNormal];
    [button setImage:image forState:UIControlStateNormal];
  };
  if (synchronously || initialState == UIControlStateNormal)
    normalImageBlock();
  else
    dispatch_after(addImageDelay, dispatch_get_main_queue(), normalImageBlock);

  void (^pressedImageBlock)(void) = ^{
    UIImage* image =
        [self imageForImageEnum:imageEnum forState:ToolbarButtonUIStatePressed];
    [button setImage:image forState:UIControlStateHighlighted];
  };
  if (synchronously || initialState == UIControlStateHighlighted)
    pressedImageBlock();
  else
    dispatch_after(addImageDelay, dispatch_get_main_queue(), pressedImageBlock);

  if (hasDisabledImage) {
    void (^disabledImageBlock)(void) = ^{
      UIImage* image = [self imageForImageEnum:imageEnum
                                      forState:ToolbarButtonUIStateDisabled];
      [button setImage:image forState:UIControlStateDisabled];
    };
    if (synchronously || initialState == UIControlStateDisabled) {
      disabledImageBlock();
    } else {
      dispatch_after(addImageDelay, dispatch_get_main_queue(),
                     disabledImageBlock);
    }
  }
}

- (BOOL)imageShouldFlipForRightToLeftLayoutDirection:(int)imageEnum {
  // None of the images this class knows about should flip.
  return NO;
}

- (void)hideViewsForNewTabPage:(BOOL)hide {
  DCHECK(!IsIPadIdiom());
  [shadowView_ setHidden:hide];
}

#pragma mark Animations

- (void)animateTransitionWithBeginFrame:(CGRect)beginFrame
                               endFrame:(CGRect)endFrame
                        transitionStyle:(ToolbarTransitionStyle)style {
  // Animation values.
  DCHECK(!self.transitionLayers.count);
  BOOL transitioningToStackView =
      (style == TOOLBAR_TRANSITION_STYLE_TO_STACK_VIEW);
  CAAnimation* frameAnimation = nil;
  CAMediaTimingFunction* frameTiming =
      TimingFunction(ios::material::CurveEaseInOut);
  CFTimeInterval frameDuration = ios::material::kDuration1;
  CGRect beginBounds = {CGPointZero, beginFrame.size};
  CGRect endBounds = {CGPointZero, endFrame.size};

  // Update layer geometry.
  frameAnimation = FrameAnimationMake(self.view.layer, beginFrame, endFrame);
  frameAnimation.duration = frameDuration;
  frameAnimation.timingFunction = frameTiming;
  [self.transitionLayers addObject:self.view.layer];
  [self.view.layer addAnimation:frameAnimation
                         forKey:kToolbarTransitionAnimationKey];

  // Hide background view using CAAnimation so it can be unhidden when the
  // animations are removed in |-cleanUpTransitionAnimations|.
  CAAnimation* hideAnimation = OpacityAnimationMake(0.0, 0.0);
  [self.transitionLayers addObject:self.backgroundView.layer];
  [self.backgroundView.layer addAnimation:hideAnimation
                                   forKey:kToolbarTransitionAnimationKey];

  // Update shadow.  When transitioning to the stack view, hide the shadow.
  // When transitioning to the BVC, animate its frame while fading in.
  CAAnimation* shadowAnimation = nil;
  if (transitioningToStackView) {
    shadowAnimation = hideAnimation;
  } else {
    InterfaceIdiom idiom = IsIPadIdiom() ? IPAD_IDIOM : IPHONE_IDIOM;
    CGFloat shadowHeight = kShadowViewFrame[idiom].size.height;
    CGFloat shadowVerticalOffset = 0.0;
    beginFrame = CGRectOffset(beginBounds, 0.0,
                              beginBounds.size.height - shadowVerticalOffset);
    beginFrame.size.height = shadowHeight;
    endFrame = CGRectOffset(endBounds, 0.0,
                            endBounds.size.height - shadowVerticalOffset);
    endFrame.size.height = shadowHeight;
    frameAnimation =
        FrameAnimationMake([shadowView_ layer], beginFrame, endFrame);
    frameAnimation.duration = frameDuration;
    frameAnimation.timingFunction = frameTiming;
    CAAnimation* fadeAnimation = OpacityAnimationMake(0.0, 1.0);
    fadeAnimation.timingFunction = TimingFunction(ios::material::CurveEaseOut);
    fadeAnimation.duration = ios::material::kDuration3;
    shadowAnimation = AnimationGroupMake(@[ frameAnimation, fadeAnimation ]);
  }
  [self.transitionLayers addObject:[shadowView_ layer]];
  [[shadowView_ layer] addAnimation:shadowAnimation
                             forKey:kToolbarTransitionAnimationKey];

  // Animate toolbar buttons
  [self animateTransitionForButtonsInView:self.view
                     containerBeginBounds:beginBounds
                       containerEndBounds:endBounds
                          transitionStyle:style];
}

- (void)reverseTransitionAnimations {
  ReverseAnimationsForKeyForLayers(kToolbarTransitionAnimationKey,
                                   [self transitionLayers]);
}

- (void)cleanUpTransitionAnimations {
  RemoveAnimationForKeyFromLayers(kToolbarTransitionAnimationKey,
                                  self.transitionLayers);
  [self.transitionLayers removeAllObjects];
}

- (void)triggerToolsMenuButtonAnimation {
  [toolsMenuButton_ triggerAnimation];
}

#pragma mark - Protected API

- (void)updateStandardButtons {
  BOOL shareButtonShouldBeVisible = [self shareButtonShouldBeVisible];
  [shareButton_ setHidden:!shareButtonShouldBeVisible];
  NSMutableArray* standardButtons = [NSMutableArray array];
  [standardButtons addObject:toolsMenuButton_];
  if (stackButton_)
    [standardButtons addObject:stackButton_];
  if (shareButtonShouldBeVisible)
    [standardButtons addObject:shareButton_];
  standardButtons_ = standardButtons;
}

- (CGFloat)statusBarOffset {
  return StatusBarHeight();
}

- (IBAction)recordUserMetrics:(id)sender {
  if (sender == toolsMenuButton_)
    base::RecordAction(UserMetricsAction("MobileToolbarShowMenu"));
  else if (sender == stackButton_)
    base::RecordAction(UserMetricsAction("MobileToolbarShowStackView"));
  else if (sender == shareButton_)
    base::RecordAction(UserMetricsAction("MobileToolbarShareMenu"));
  else
    NOTREACHED();
}

- (UIButton*)stackButton {
  return stackButton_;
}

- (CGRect)specificControlsArea {
  // Return the rect to the leading side of the leading-most trailing control.
  UIView* trailingControl = toolsMenuButton_;
  if (!IsIPadIdiom())
    trailingControl = stackButton_;
  if ([self shareButtonShouldBeVisible])
    trailingControl = shareButton_;
  LayoutRect trailing =
      LayoutRectForRectInBoundingRect(trailingControl.frame, self.view.bounds);
  LayoutRect controlsArea = LayoutRectGetLeadingLayout(trailing);
  controlsArea.size.height = self.view.bounds.size.height;
  controlsArea.position.originY = self.view.bounds.origin.y;
  CGRect controlsFrame = LayoutRectGetRect(controlsArea);

  if (!IsIPadIdiom()) {
    controlsFrame.origin.y += StatusBarHeight();
    controlsFrame.size.height -= StatusBarHeight();
  }
  return controlsFrame;
}

- (void)setStandardControlsVisible:(BOOL)visible {
  if (visible) {
    for (UIButton* button in standardButtons_) {
      [button setAlpha:1.0];
    }
  } else {
    for (UIButton* button in standardButtons_) {
      [button setAlpha:0.0];
    }
  }
}

- (void)setStandardControlsAlpha:(CGFloat)alpha {
  for (UIButton* button in standardButtons_) {
    if (![button isHidden])
      [button setAlpha:alpha];
  }
}

- (UIImage*)imageForImageEnum:(int)imageEnum
                     forState:(ToolbarButtonUIState)state {
  int imageID =
      [self imageIdForImageEnum:imageEnum style:[self style] forState:state];
  return NativeReversableImage(
      imageID, [self imageShouldFlipForRightToLeftLayoutDirection:imageEnum]);
}

- (int)imageEnumForButton:(UIButton*)button {
  if (button == stackButton_)
    return ToolbarButtonNameStack;
  return NumberOfToolbarButtonNames;
}

- (int)imageIdForImageEnum:(int)index
                     style:(ToolbarControllerStyle)style
                  forState:(ToolbarButtonUIState)state {
  DCHECK(index < NumberOfToolbarButtonNames);
  DCHECK(style < ToolbarControllerStyleMaxStyles);
  DCHECK(state < NumberOfToolbarButtonUIStates);
  // Incognito mode gets dark buttons.
  if (style == ToolbarControllerStyleIncognitoMode)
    style = ToolbarControllerStyleDarkMode;

  // Name, style [light, dark], UIControlState [normal, pressed, disabled]
  static int buttonImageIds[NumberOfToolbarButtonNames][2]
                           [NumberOfToolbarButtonUIStates] = {
                               TOOLBAR_IDR_THREE_STATE(OVERVIEW),
                               TOOLBAR_IDR_THREE_STATE(SHARE),
                           };

  DCHECK(buttonImageIds[index][style][state]);
  return buttonImageIds[index][style][state];
}

- (uint32_t)snapshotHash {
  // Only the 3 lowest bits are used by UIControlState.
  uint32_t hash = [toolsMenuButton_ state] & 0x07;
  // When the tools popup controller is valid, it means that the images
  // representing the tools menu button have been swapped. Factor that in by
  // adding in whether or not the tools popup menu is a valid object, rather
  // than trying to figure out which image is currently visible.
  hash |= toolsPopupController_ ? (1 << 4) : 0;
  // The label of the stack button changes with the number of tabs open.
  hash ^= [[stackButton_ titleForState:UIControlStateNormal] hash];
  return hash;
}

- (NSMutableArray*)transitionLayers {
  return transitionLayers_;
}

#pragma mark Animations

- (void)animateTransitionForButtonsInView:(UIView*)containerView
                     containerBeginBounds:(CGRect)containerBeginBounds
                       containerEndBounds:(CGRect)containerEndBounds
                          transitionStyle:(ToolbarTransitionStyle)style {
  [containerView.subviews enumerateObjectsUsingBlock:^(
                              UIButton* button, NSUInteger idx, BOOL* stop) {
    if ([button isKindOfClass:[UIButton class]] && button.alpha > 0.0) {
      CAAnimation* buttonAnimation =
          [self transitionAnimationForButton:button
                        containerBeginBounds:containerBeginBounds
                          containerEndBounds:containerEndBounds
                                   withStyle:style];
      [self.transitionLayers addObject:button.layer];
      [button.layer addAnimation:buttonAnimation
                          forKey:kToolbarTransitionAnimationKey];
    }
  }];
}

- (void)fadeInView:(UIView*)view
    fromLeadingOffset:(LayoutOffset)leadingOffset
         withDuration:(NSTimeInterval)duration
           afterDelay:(NSTimeInterval)delay {
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  [CATransaction setCompletionBlock:^{
    [view.layer removeAnimationForKey:@"fadeIn"];
  }];
  view.alpha = 1.0;

  // Animate the position of |view| |leadingOffset| pixels after |delay|.
  CGRect shiftedFrame = CGRectLayoutOffset(view.frame, leadingOffset);
  CAAnimation* shiftAnimation =
      FrameAnimationMake(view.layer, shiftedFrame, view.frame);
  shiftAnimation.duration = duration;
  shiftAnimation.beginTime = delay;
  shiftAnimation.timingFunction = TimingFunction(ios::material::CurveEaseInOut);

  // Animate the opacity of |view| to 1 after |delay|.
  CAAnimation* fadeAnimation = OpacityAnimationMake(0.0, 1.0);
  fadeAnimation.duration = duration;
  fadeAnimation.beginTime = delay;
  shiftAnimation.timingFunction = TimingFunction(ios::material::CurveEaseInOut);

  // Add group animation to layer.
  CAAnimation* group = AnimationGroupMake(@[ shiftAnimation, fadeAnimation ]);
  [view.layer addAnimation:group forKey:@"fadeIn"];

  [CATransaction commit];
}

- (void)animateStandardControlsForOmniboxExpansion:(BOOL)growOmnibox {
  if (growOmnibox)
    [self fadeOutStandardControls];
  else
    [self fadeInStandardControls];
}

#pragma mark - Private Methods
#pragma mark Animations
- (void)fadeOutStandardControls {
  // The opacity animation has a different duration from the position animation.
  // Thus they require separate CATransations.

  // Animate the opacity of the buttons to 0.
  [CATransaction begin];
  [CATransaction setAnimationDuration:ios::material::kDuration2];
  [CATransaction
      setAnimationTimingFunction:TimingFunction(ios::material::CurveEaseIn)];
  CABasicAnimation* fadeButtons =
      [CABasicAnimation animationWithKeyPath:@"opacity"];
  fadeButtons.fromValue = @1;
  fadeButtons.toValue = @0;

  for (UIButton* button in standardButtons_) {
    if (![button isHidden]) {
      [button layer].opacity = 0;
      [[button layer] addAnimation:fadeButtons forKey:@"fade"];
    }
  }
  [CATransaction commit];

  // Animate the buttons 10 pixels in the leading-to-trailing direction
  [CATransaction begin];
  [CATransaction setAnimationDuration:ios::material::kDuration1];
  [CATransaction
      setAnimationTimingFunction:TimingFunction(ios::material::CurveEaseIn)];

  for (UIButton* button in standardButtons_) {
    CABasicAnimation* shiftButton =
        [CABasicAnimation animationWithKeyPath:@"position"];
    CGPoint startPosition = [button layer].position;
    CGPoint endPosition =
        CGPointLayoutOffset(startPosition, kButtonFadeOutXOffset);
    shiftButton.fromValue = [NSValue valueWithCGPoint:startPosition];
    shiftButton.toValue = [NSValue valueWithCGPoint:endPosition];
    [[button layer] addAnimation:shiftButton forKey:@"shiftButton"];
  }

  [CATransaction commit];

  // Fade to the full bleed shadow.
  [UIView animateWithDuration:ios::material::kDuration1
                   animations:^{
                     [shadowView_ setAlpha:0];
                     [fullBleedShadowView_ setAlpha:1];
                   }];
}

- (void)fadeInStandardControls {
  for (UIButton* button in standardButtons_) {
    [self fadeInView:button
        fromLeadingOffset:10
             withDuration:ios::material::kDuration2
               afterDelay:ios::material::kDuration1];
  }

  // Fade to the normal shadow.
  [UIView animateWithDuration:ios::material::kDuration1
                   animations:^{
                     [shadowView_ setAlpha:self.backgroundView.alpha];
                     [fullBleedShadowView_ setAlpha:0];
                   }];
}

- (CAAnimation*)transitionAnimationForButton:(UIButton*)button
                        containerBeginBounds:(CGRect)containerBeginBounds
                          containerEndBounds:(CGRect)containerEndBounds
                                   withStyle:(ToolbarTransitionStyle)style {
  BOOL toStackView = style == TOOLBAR_TRANSITION_STYLE_TO_STACK_VIEW;
  CGRect cardBounds = toStackView ? containerEndBounds : containerBeginBounds;
  CGRect toolbarBounds =
      toStackView ? containerBeginBounds : containerEndBounds;

  // |button|'s model layer frame is the button's frame within |toolbarBounds|.
  CGRect toolbarButtonFrame = button.layer.frame;
  LayoutRect toolbarButtonLayout =
      LayoutRectForRectInBoundingRect(toolbarButtonFrame, toolbarBounds);

  // |button|'s leading or trailing padding is maintained depending on its
  // resizing mask.  Its vertical positioning should be centered within the
  // container view's card bounds.
  LayoutRect cardButtonLayout = toolbarButtonLayout;
  cardButtonLayout.boundingWidth = CGRectGetWidth(cardBounds);
  BOOL flexibleLeading =
      button.autoresizingMask & UIViewAutoresizingFlexibleLeadingMargin();
  if (flexibleLeading) {
    CGFloat trailingPadding =
        LayoutRectGetTrailingLayout(toolbarButtonLayout).size.width;
    cardButtonLayout.position.leading = cardButtonLayout.boundingWidth -
                                        trailingPadding -
                                        cardButtonLayout.size.width;
  }
  cardButtonLayout.position.originY =
      CGRectGetMidY(cardBounds) - 0.5 * cardButtonLayout.size.height;
  cardButtonLayout.position =
      AlignLayoutRectPositionToPixel(cardButtonLayout.position);
  CGRect cardButtonFrame = LayoutRectGetRect(cardButtonLayout);

  CGRect beginFrame = toStackView ? toolbarButtonFrame : cardButtonFrame;
  CGRect endFrame = toStackView ? cardButtonFrame : toolbarButtonFrame;

  // Create animations.
  CAAnimation* frameAnimation =
      FrameAnimationMake(button.layer, beginFrame, endFrame);
  frameAnimation.duration = ios::material::kDuration1;
  frameAnimation.timingFunction = TimingFunction(ios::material::CurveEaseInOut);
  CAAnimation* fadeAnimation =
      OpacityAnimationMake(toStackView ? 1.0 : 0.0, toStackView ? 0.0 : 1.0);
  fadeAnimation.duration = ios::material::kDuration8;
  fadeAnimation.timingFunction = TimingFunction(ios::material::CurveEaseIn);
  return AnimationGroupMake(@[ frameAnimation, fadeAnimation ]);
}

#pragma mark Helpers

- (UIFont*)fontForSize:(NSInteger)size {
  return [[MDCTypography fontLoader] boldFontOfSize:size];
}

- (BOOL)shareButtonShouldBeVisible {
  // The share button only exists on iPad, and when some tabs are visible
  // (i.e. when not in DarkMode), and when the width is greater than
  // the tablet mini view.
  if (!IsIPadIdiom() || style_ == ToolbarControllerStyleDarkMode ||
      IsCompactTablet(self.view))
    return NO;
  return YES;
}

- (void)standardButtonPressed:(UIButton*)sender {
  // This check for valid button images assumes that the buttons all have a
  // different image for the highlighted state as for the normal state.
  // Currently, that assumption is true.
  if ([sender imageForState:UIControlStateHighlighted] ==
      [sender imageForState:UIControlStateNormal]) {
    // Update the button images synchronously - somehow the button was pressed
    // before the dispatched task completed.
    [self setUpButton:sender
           withImageEnum:[self imageEnumForButton:sender]
         forInitialState:UIControlStateNormal
        hasDisabledImage:NO
           synchronously:YES];
  }
}

- (void)registerEventsForButton:(UIButton*)button {
  if (button != toolsMenuButton_) {
    // |target| must be |self| (as opposed to |nil|) because |self| isn't in the
    // responder chain.
    [button addTarget:self
                  action:@selector(standardButtonPressed:)
        forControlEvents:UIControlEventTouchUpInside];
  }
  [button addTarget:self
                action:@selector(recordUserMetrics:)
      forControlEvents:UIControlEventTouchUpInside];
}

- (UIImage*)getBackgroundImageForStyle:(ToolbarControllerStyle)style {
  int backgroundImageID;
  if (style == ToolbarControllerStyleLightMode)
    backgroundImageID = IDR_IOS_TOOLBAR_LIGHT_BACKGROUND;
  else
    backgroundImageID = IDR_IOS_TOOLBAR_DARK_BACKGROUND;

  return NativeImage(backgroundImageID);
}

#pragma mark - ActivityServicePositioner

- (CGRect)shareButtonAnchorRect {
  // Shrink the padding around the shareButton so the popovers are anchored
  // correctly.
  return CGRectInset([shareButton_ bounds], kPopoverAnchorHorizontalPadding, 0);
}

- (UIView*)shareButtonView {
  return shareButton_;
}

#pragma mark - PopupMenuDelegate methods.

- (void)dismissPopupMenu:(PopupMenuController*)controller {
  if ([controller isKindOfClass:[ToolsPopupController class]] &&
      (ToolsPopupController*)controller == toolsPopupController_)
    [self dismissToolsMenuPopup];
}

#pragma mark - BubbleViewAnchorPointProvider methods.

- (CGPoint)anchorPointForTabSwitcherButton:(BubbleArrowDirection)direction {
  // Shrink the padding around the tab switcher button so popovers are anchored
  // correctly.
  CGRect unpaddedRect =
      CGRectInset(stackButton_.frame, kPopoverAnchorHorizontalPadding, 0.0);
  CGPoint anchorPoint = bubble_util::AnchorPoint(unpaddedRect, direction);
  return [stackButton_.superview convertPoint:anchorPoint
                                       toView:stackButton_.window];
}

- (CGPoint)anchorPointForToolsMenuButton:(BubbleArrowDirection)direction {
  // Shrink the padding around the tools menu button so popovers are anchored
  // correctly.
  CGRect unpaddedRect =
      CGRectInset(toolsMenuButton_.frame, kPopoverAnchorHorizontalPadding, 0.0);
  CGPoint anchorPoint = bubble_util::AnchorPoint(unpaddedRect, direction);
  return [toolsMenuButton_.superview convertPoint:anchorPoint
                                           toView:toolsMenuButton_.window];
}

#pragma mark - CAAnimationDelegate
// WebToolbarController conforms to CAAnimationDelegate.
- (void)animationDidStart:(CAAnimation*)anim {
  // Once the buttons start fading in, set their opacity to 1 so there's no
  // flicker at the end of the animation.
  for (UIButton* button in standardButtons_) {
    if (anim == [[button layer] animationForKey:@"fadeIn"]) {
      [button layer].opacity = 1;
      return;
    }
  }
}

@end
