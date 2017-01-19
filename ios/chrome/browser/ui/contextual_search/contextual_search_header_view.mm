// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/contextual_search/contextual_search_header_view.h"

#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/contextual_search/contextual_search_panel_view.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/common/material_timing.h"
#include "ios/chrome/common/string_util.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/images/branded_image_provider.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

namespace {
const CGFloat kHorizontalMargin = 24.0;
const CGFloat kHorizontalLayoutGap = 16.0;

const NSTimeInterval kTextTransformAnimationDuration =
    ios::material::kDuration1;
const NSTimeInterval kLogoIrisAnimationDuration = ios::material::kDuration1;
}  // namespace

// An image that can "iris" in/out. Assumes a square image and will do a stupid-
// looking eliptical iris otherwise.
@interface IrisingImageView : UIImageView
// |iris| is the degree that the logo is irised; a value of 0.0 indicates
// the logo is completly invisible, a 1.0 indicates it is completely visible,
// and 0.5 indicates the iris is open to show a diameter half of the image size.
// |iris| has an initial value of 1.0.
// |iris| is animatable, in that setting in inside an animation block will
// cause the transition to be animated.
@property(nonatomic, assign) CGFloat iris;
@end

@implementation IrisingImageView {
  CGFloat _iris;
}

@synthesize iris = _iris;

// Create a mask layer for the iris effect
- (instancetype)initWithImage:(UIImage*)image {
  if ((self = [super initWithImage:image])) {
    CAShapeLayer* maskLayer = [CAShapeLayer layer];
    maskLayer.bounds = self.bounds;
    base::ScopedCFTypeRef<CGPathRef> path(
        CGPathCreateWithEllipseInRect(maskLayer.bounds, NULL));
    maskLayer.path = path;
    maskLayer.fillColor = [UIColor whiteColor].CGColor;
    maskLayer.position =
        CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
    self.layer.mask = maskLayer;
    self.iris = 1.0;
    [self setContentHuggingPriority:UILayoutPriorityDefaultHigh
                            forAxis:UILayoutConstraintAxisVertical];
    [self setContentHuggingPriority:UILayoutPriorityDefaultHigh
                            forAxis:UILayoutConstraintAxisHorizontal];
  }
  return self;
}

- (void)setIris:(CGFloat)iris {
  _iris = iris;
  // Transform the (0.0 ... 1.0) iris value so that the area covered appears
  // to change linearly. A value of 0.5 should cover half the area of the image,
  // so the radius of the circle should be sqrt(0.5); so, use sqrt(_iris).
  // At a scale of 1.0, the iris should totally expose the image, so the iris
  // diameter must be enough to encompass a square the size of the image; for a
  // unit square, that's sqrt(2).
  CGFloat scale = sqrt(_iris) * sqrt(2.0);
  [self.layer.mask setAffineTransform:CGAffineTransformMakeScale(scale, scale)];
}

@end

// Button subclass whose intrinsic content size is always large enough to be
// easily tappable.
@interface TappableButton : UIButton
@end

@implementation TappableButton

- (CGSize)intrinsicContentSize {
  CGSize contentSize = [super intrinsicContentSize];
  contentSize.height = MAX(contentSize.height, 44.0);
  contentSize.width = MAX(contentSize.width, 44.0);
  return contentSize;
}

@end

@implementation ContextualSearchHeaderView {
  CGFloat _height;
  // Circular logo positioned leading side.
  __unsafe_unretained IrisingImageView* _logo;
  // Up/down caret positioned trailing side.
  __unsafe_unretained UIImageView* _caret;
  // Close control position identically to the caret.
  __unsafe_unretained TappableButton* _closeButton;
  // Label showing the text the user tapped on in the web page, and any
  // additional context that will be displayed.
  __unsafe_unretained UILabel* _textLabel;
  base::WeakNSProtocol<id<ContextualSearchPanelTapHandler>> _tapHandler;
  base::scoped_nsobject<UIGestureRecognizer> _tapRecognizer;
}

+ (BOOL)requiresConstraintBasedLayout {
  return YES;
}

- (instancetype)initWithHeight:(CGFloat)height {
  if (!(self = [super initWithFrame:CGRectZero]))
    return nil;

  DCHECK(height > 0);
  _height = height;

  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.backgroundColor = [UIColor whiteColor];
  _tapRecognizer.reset([[UITapGestureRecognizer alloc] init]);
  [self addGestureRecognizer:_tapRecognizer];
  [_tapRecognizer addTarget:self action:@selector(panelWasTapped:)];

  UIImage* logoImage = ios::GetChromeBrowserProvider()
                           ->GetBrandedImageProvider()
                           ->GetContextualSearchHeaderImage();
  _logo = [[[IrisingImageView alloc] initWithImage:logoImage] autorelease];
  _logo.translatesAutoresizingMaskIntoConstraints = NO;
  _logo.iris = 0.0;

  _caret = [[[UIImageView alloc]
      initWithImage:[UIImage imageNamed:@"expand_less"]] autorelease];
  _caret.translatesAutoresizingMaskIntoConstraints = NO;
  [_caret setContentHuggingPriority:UILayoutPriorityDefaultHigh
                            forAxis:UILayoutConstraintAxisVertical];
  [_caret setContentHuggingPriority:UILayoutPriorityDefaultHigh
                            forAxis:UILayoutConstraintAxisHorizontal];

  _closeButton =
      [[[TappableButton alloc] initWithFrame:CGRectZero] autorelease];
  _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_closeButton setImage:[UIImage imageNamed:@"card_close_button"]
                forState:UIControlStateNormal];
  [_closeButton setImage:[UIImage imageNamed:@"card_close_button_pressed"]
                forState:UIControlStateHighlighted];
  [_closeButton setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                  forAxis:UILayoutConstraintAxisVertical];
  [_closeButton setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                  forAxis:UILayoutConstraintAxisHorizontal];
  _closeButton.alpha = 0;

  _textLabel = [[[UILabel alloc] initWithFrame:CGRectZero] autorelease];
  _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _textLabel.font = [MDCTypography subheadFont];
  _textLabel.textAlignment = NSTextAlignmentNatural;
  _textLabel.lineBreakMode = NSLineBreakByCharWrapping;
  // Ensure that |_textLabel| doesn't expand past the space defined for it
  // regardless of how long its text is.
  [_textLabel setContentHuggingPriority:UILayoutPriorityDefaultLow
                                forAxis:UILayoutConstraintAxisHorizontal];

  [self setAccessibilityIdentifier:@"header"];
  [_logo setAccessibilityIdentifier:@"logo"];
  [_caret setAccessibilityIdentifier:@"caret"];
  [_closeButton setAccessibilityIdentifier:@"close"];
  [_textLabel setAccessibilityIdentifier:@"selectedText"];

  [self addSubview:_logo];
  [self addSubview:_caret];
  [self addSubview:_textLabel];
  [self addSubview:_closeButton];

  [self setLayoutMargins:UIEdgeInsetsMake(0, kHorizontalMargin, 0,
                                          kHorizontalMargin)];

  [NSLayoutConstraint activateConstraints:@[
    // Horizontal layout:
    // Logo is at the leading margin:
    [_logo.leadingAnchor
        constraintEqualToAnchor:self.layoutMarginsGuide.leadingAnchor],
    // Caret is at the trailing margin:
    [_caret.trailingAnchor
        constraintEqualToAnchor:self.layoutMarginsGuide.trailingAnchor],
    // Close button is centered over the caret:
    [_closeButton.centerXAnchor constraintEqualToAnchor:_caret.centerXAnchor],
    // The available space for the text label is the space (minus
    // |kHorizontalLayoutGap| on each side) between the logo and the caret:
    [_textLabel.leadingAnchor constraintEqualToAnchor:_logo.trailingAnchor
                                             constant:kHorizontalLayoutGap],
    [_textLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_caret.leadingAnchor
                                 constant:-kHorizontalLayoutGap],
    // Vertical layout:
    // Everything is center-aligned to |self|.
    [_logo.centerYAnchor
        constraintEqualToAnchor:self.layoutMarginsGuide.centerYAnchor],
    [_textLabel.centerYAnchor
        constraintEqualToAnchor:self.layoutMarginsGuide.centerYAnchor],
    [_caret.centerYAnchor
        constraintEqualToAnchor:self.layoutMarginsGuide.centerYAnchor],
    [_closeButton.centerYAnchor constraintEqualToAnchor:_caret.centerYAnchor],
  ]];

  return self;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE {
  NOTREACHED();
  return nil;
}

#pragma mark - property implementation.

- (void)setTapHandler:(id<ContextualSearchPanelTapHandler>)tapHandler {
  if (_tapHandler) {
    [_closeButton removeTarget:_tapHandler
                        action:@selector(closePanel)
              forControlEvents:UIControlEventTouchUpInside];
  }
  _tapHandler.reset(tapHandler);
  if (_tapHandler) {
    [_closeButton addTarget:_tapHandler
                     action:@selector(closePanel)
           forControlEvents:UIControlEventTouchUpInside];
  }
}

- (id<ContextualSearchPanelTapHandler>)tapHandler {
  return _tapHandler;
}

- (void)panelWasTapped:(UIGestureRecognizer*)gestureRecognizer {
  for (NSUInteger touchIndex = 0;
       touchIndex < gestureRecognizer.numberOfTouches; touchIndex++) {
    if (!CGRectContainsPoint(
            self.frame,
            [gestureRecognizer locationOfTouch:touchIndex inView:self])) {
      return;
    }
  }
  [_tapHandler panelWasTapped:gestureRecognizer];
}

#pragma mark - UIView layout methods

- (CGSize)intrinsicContentSize {
  // This view's height is always |_height|.
  return CGSizeMake(UIViewNoIntrinsicMetric, _height);
}

#pragma mark - ContextualSearchPanelMotionObserver

- (void)panel:(ContextualSearchPanelView*)panel
    didMoveWithMotion:(ContextualSearch::PanelMotion)motion {
  if (motion.state == ContextualSearch::PREVIEWING) {
    [self setCloseButtonTransition:1.0];
  }
  if (motion.state == ContextualSearch::PEEKING) {
    _caret.alpha = 1.0;
    [self setCloseButtonTransition:motion.gradation];
  }
}

- (void)panelWillPromote:(ContextualSearchPanelView*)panel {
  // Disable tap handling.
  self.tapHandler = nil;
}

- (void)panelIsPromoting:(ContextualSearchPanelView*)panel {
  self.alpha = 0.0;
  [panel removeMotionObserver:self];
}

#pragma mark - Subview update

- (void)setCloseButtonTransition:(CGFloat)gradation {
  // Crossfade the caret into the close button by fading the caret all the way
  // out, and then fading the close button in.
  // As the overall gradation moves from 0 to 0.5, the caret's alpha moves
  // from 1.0 to 0, and as the gradation continues from 0.5 to 1.0, the
  // close button's alpha moves from 0 to 1.0.
  CGFloat scaledGradation = 1 - (2 * gradation);      // [0, 1] -> [1, -1]
  CGFloat caretGradation = MAX(scaledGradation, 0);   // [1.0 .. 0.0 .. 0.0]
  CGFloat closeGradation = MAX(-scaledGradation, 0);  // [0.0 .. 0.0 .. 1.0]
  _caret.alpha = caretGradation * caretGradation;
  _closeButton.alpha = closeGradation * closeGradation;
}

#pragma mark - Animated transitions

- (void)setText:(NSString*)text
    followingTextRange:(NSRange)followingTextRange
              animated:(BOOL)animated {
  NSMutableAttributedString* styledText =
      [[[NSMutableAttributedString alloc] initWithString:text] autorelease];
  [styledText addAttribute:NSForegroundColorAttributeName
                     value:[UIColor colorWithWhite:0 alpha:0.71f]
                     range:followingTextRange];

  void (^transform)(void) = ^{
    _textLabel.attributedText = styledText;
  };
  void (^complete)(BOOL) = ^(BOOL finished) {
    [self showLogoAnimated:animated];
  };

  if (animated) {
    UIViewAnimationOptions options =
        UIViewAnimationOptionTransitionCrossDissolve;
    [UIView cr_transitionWithView:self
                         duration:kTextTransformAnimationDuration
                            curve:ios::material::CurveEaseOut
                          options:options
                       animations:transform
                       completion:complete];
  } else {
    transform();
    complete(NO);
  }
}

- (void)setSearchTerm:(NSString*)searchTerm animated:(BOOL)animated {
  void (^transform)(void) = ^{
    _textLabel.text = searchTerm;
  };
  void (^complete)(BOOL) = ^(BOOL finished) {
    [self showLogoAnimated:animated];
  };

  if (animated) {
    UIViewAnimationOptions options =
        UIViewAnimationOptionTransitionCrossDissolve;
    [UIView cr_transitionWithView:self
                         duration:kTextTransformAnimationDuration
                            curve:ios::material::CurveEaseInOut
                          options:options
                       animations:transform
                       completion:complete];
  } else {
    transform();
    complete(NO);
  }
}

- (void)showLogoAnimated:(BOOL)animated {
  // Since the logo is round, we only need to animate to 1/sqrt(2) to display
  // the whole thing.
  if ([_logo iris] > 0.0)
    return;

  void (^transform)(void) = ^{
    [_logo setIris:(1.0 / sqrt(2.0))];
  };
  if (animated) {
    [UIView cr_animateWithDuration:kLogoIrisAnimationDuration
                             delay:0
                             curve:ios::material::CurveEaseIn
                           options:0
                        animations:transform
                        completion:nil];
  } else {
    transform();
  }
}

- (void)hideLogoAnimated:(BOOL)animated {
  if ([_logo iris] == 0.0)
    return;

  void (^transform)(void) = ^{
    [_logo setIris:0.0];
  };
  if (animated) {
    [UIView cr_animateWithDuration:kLogoIrisAnimationDuration
                             delay:0
                             curve:ios::material::CurveEaseOut
                           options:0
                        animations:transform
                        completion:nil];
  } else {
    transform();
  }
}

@end
