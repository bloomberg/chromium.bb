// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_view_controller.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#include "ios/chrome/browser/ui/location_bar/location_bar_steady_view.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(int, TrailingButtonState) {
  kNoButton = 0,
  kShareButton,
  kVoiceSearchButton,
};

}  // namespace

@interface LocationBarViewController ()
// The injected edit view.
@property(nonatomic, strong) UIView* editView;

// The view that displays current location when the omnibox is not focused.
@property(nonatomic, strong) LocationBarSteadyView* locationBarSteadyView;

@property(nonatomic, assign) TrailingButtonState trailingButtonState;

// When this flag is YES, the share button will not be displayed in situations
// when it normally is shown. Setting it triggers a refresh of the button
// visibility.
@property(nonatomic, assign) BOOL hideShareButtonWhileOnIncognitoNTP;

// Starts voice search, updating the NamedGuide to be constrained to the
// trailing button.
- (void)startVoiceSearch;

@end

@implementation LocationBarViewController
@synthesize editView = _editView;
@synthesize locationBarSteadyView = _locationBarSteadyView;
@synthesize incognito = _incognito;
@synthesize delegate = _delegate;
@synthesize dispatcher = _dispatcher;
@synthesize voiceSearchEnabled = _voiceSearchEnabled;
@synthesize trailingButtonState = _trailingButtonState;
@synthesize hideShareButtonWhileOnIncognitoNTP =
    _hideShareButtonWhileOnIncognitoNTP;

#pragma mark - public

- (instancetype)initWithFrame:(CGRect)frame
                         font:(UIFont*)font
                    textColor:(UIColor*)textColor
                    tintColor:(UIColor*)tintColor {
  self = [super init];
  if (self) {
    _locationBarSteadyView = [[LocationBarSteadyView alloc] init];
    [_locationBarSteadyView addTarget:self
                               action:@selector(locationBarSteadyViewTapped)
                     forControlEvents:UIControlEventTouchUpInside];
  }
  return self;
}

- (void)setEditView:(UIView*)editView {
  DCHECK(!self.editView);
  _editView = editView;
}

- (void)switchToEditing:(BOOL)editing {
  self.editView.hidden = !editing;
  self.locationBarSteadyView.hidden = editing;
}

- (void)setIncognito:(BOOL)incognito {
  _incognito = incognito;
  [self.locationBarSteadyView
      setColorScheme:incognito
                         ? [LocationBarSteadyViewColorScheme incognitoScheme]
                         : [LocationBarSteadyViewColorScheme standardScheme]];
}

- (void)setDispatcher:
    (id<ActivityServiceCommands, BrowserCommands, ApplicationCommands>)
        dispatcher {
  _dispatcher = dispatcher;
}

- (void)setVoiceSearchEnabled:(BOOL)enabled {
  if (_voiceSearchEnabled == enabled) {
    return;
  }
  _voiceSearchEnabled = enabled;
  [self updateTrailingButtonState];
}

- (void)setHideShareButtonWhileOnIncognitoNTP:(BOOL)hide {
  if (_hideShareButtonWhileOnIncognitoNTP == hide) {
    return;
  }
  _hideShareButtonWhileOnIncognitoNTP = hide;
  [self updateTrailingButton];
}

- (void)updateTrailingButtonState {
  BOOL shouldShowVoiceSearch =
      self.traitCollection.horizontalSizeClass ==
          UIUserInterfaceSizeClassRegular ||
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;

  if (shouldShowVoiceSearch) {
    if (self.voiceSearchEnabled) {
      self.trailingButtonState = kVoiceSearchButton;
    } else {
      self.trailingButtonState = kNoButton;
    }
  } else {
    self.trailingButtonState = kShareButton;
  }
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  DCHECK(self.editView) << "The edit view must be set at this point";

  [self.view addSubview:self.editView];
  self.editView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.editView, self.view);

  [self.view addSubview:self.locationBarSteadyView];
  self.locationBarSteadyView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.locationBarSteadyView, self.view);

  [self switchToEditing:NO];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [self updateTrailingButtonState];
  [super traitCollectionDidChange:previousTraitCollection];
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  CGFloat alphaValue = fmax((progress - 0.85) / 0.15, 0);
  CGFloat scaleValue = 0.79 + 0.21 * progress;
  self.locationBarSteadyView.trailingButton.alpha = alphaValue;
  self.locationBarSteadyView.transform =
      CGAffineTransformMakeScale(scaleValue, scaleValue);
}

- (void)updateForFullscreenEnabled:(BOOL)enabled {
  if (!enabled)
    [self updateForFullscreenProgress:1.0];
}

- (void)finishFullscreenScrollWithAnimator:(FullscreenAnimator*)animator {
  [self addFullscreenAnimationsToAnimator:animator];
}

- (void)scrollFullscreenToTopWithAnimator:(FullscreenAnimator*)animator {
  [self addFullscreenAnimationsToAnimator:animator];
}

- (void)showToolbarWithAnimator:(FullscreenAnimator*)animator {
  [self addFullscreenAnimationsToAnimator:animator];
}

- (void)addFullscreenAnimationsToAnimator:(FullscreenAnimator*)animator {
  CGFloat finalProgress = animator.finalProgress;
  [animator addAnimations:^{
    [self updateForFullscreenProgress:finalProgress];
  }];
}

#pragma mark - LocationBarConsumer

- (void)updateLocationText:(NSString*)text {
  self.locationBarSteadyView.locationLabel.text = text;
}

- (void)updateLocationIcon:(UIImage*)icon {
  self.locationBarSteadyView.locationIconImageView.image =
      [icon imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

- (void)updateForIncognitoNTP:(BOOL)isIncognitoNTP {
  if (isIncognitoNTP) {
    // Display a fake "placeholder".
    NSString* placeholderString =
        l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
    UIColor* placeholderColor =
        [LocationBarSteadyViewColorScheme incognitoScheme].placeholderColor;
    self.locationBarSteadyView.locationLabel.attributedText = [
        [NSAttributedString alloc]
        initWithString:placeholderString
            attributes:@{NSForegroundColorAttributeName : placeholderColor}];
  }
  self.hideShareButtonWhileOnIncognitoNTP = isIncognitoNTP;
}

#pragma mark - private

- (void)locationBarSteadyViewTapped {
  [self.delegate locationBarSteadyViewTapped];
}

- (void)updateTrailingButton {
  // Stop constraining the voice guide to the trailing button if transitioning
  // from kVoiceSearchButton.
  NamedGuide* voiceGuide =
      [NamedGuide guideWithName:kVoiceSearchButtonGuide
                           view:self.locationBarSteadyView];
  if (voiceGuide.constrainedView == self.locationBarSteadyView.trailingButton)
    voiceGuide.constrainedView = nil;


  // Cancel previous possible state.
  [self.locationBarSteadyView.trailingButton
          removeTarget:nil
                action:nil
      forControlEvents:UIControlEventAllEvents];
  self.locationBarSteadyView.trailingButton.hidden = NO;

  TrailingButtonState state = self.trailingButtonState;
  if (state == kShareButton && self.hideShareButtonWhileOnIncognitoNTP) {
    state = kNoButton;
  }

  switch (state) {
    case kNoButton: {
      self.locationBarSteadyView.trailingButton.hidden = YES;
      break;
    };
    case kShareButton: {
      [self.locationBarSteadyView.trailingButton
                 addTarget:self.dispatcher
                    action:@selector(sharePage)
          forControlEvents:UIControlEventTouchUpInside];

      [self.locationBarSteadyView.trailingButton
          setImage:
              [[UIImage imageNamed:@"location_bar_share"]
                  imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
          forState:UIControlStateNormal];
      break;
    };
    case kVoiceSearchButton: {
      [self.locationBarSteadyView.trailingButton
                 addTarget:self.dispatcher
                    action:@selector(preloadVoiceSearch)
          forControlEvents:UIControlEventTouchDown];
      [self.locationBarSteadyView.trailingButton
                 addTarget:self
                    action:@selector(startVoiceSearch)
          forControlEvents:UIControlEventTouchUpInside];
      [self.locationBarSteadyView.trailingButton
          setImage:
              [[UIImage imageNamed:@"location_bar_voice"]
                  imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
          forState:UIControlStateNormal];
    }
  }
}

- (void)setTrailingButtonState:(TrailingButtonState)state {
  if (_trailingButtonState == state) {
    return;
  }
  _trailingButtonState = state;

  [self updateTrailingButton];
}

- (void)startVoiceSearch {
  [NamedGuide guideWithName:kVoiceSearchButtonGuide view:self.view]
      .constrainedView = self.locationBarSteadyView.trailingButton;
  [self.dispatcher startVoiceSearch];
}

@end
