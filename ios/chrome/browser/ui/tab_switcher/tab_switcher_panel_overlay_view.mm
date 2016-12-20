// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_panel_overlay_view.h"

#import "base/mac/scoped_nsobject.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/material_components/activity_indicator.h"
#import "ios/chrome/browser/ui/sync/sync_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_model.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/third_party/material_components_ios/src/components/ActivityIndicator/src/MaterialActivityIndicator.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/ShadowElevations/src/MaterialShadowElevations.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

TabSwitcherPanelOverlayType PanelOverlayTypeFromSignInPanelsType(
    TabSwitcherSignInPanelsType signInPanelType) {
  switch (signInPanelType) {
    case TabSwitcherSignInPanelsType::PANEL_USER_SIGNED_OUT:
      return TabSwitcherPanelOverlayType::OVERLAY_PANEL_USER_SIGNED_OUT;
    case TabSwitcherSignInPanelsType::PANEL_USER_SIGNED_IN_SYNC_OFF:
      return TabSwitcherPanelOverlayType::OVERLAY_PANEL_USER_SIGNED_IN_SYNC_OFF;
    case TabSwitcherSignInPanelsType::PANEL_USER_SIGNED_IN_SYNC_IN_PROGRESS:
      return TabSwitcherPanelOverlayType::
          OVERLAY_PANEL_USER_SIGNED_IN_SYNC_IN_PROGRESS;
    case TabSwitcherSignInPanelsType::PANEL_USER_SIGNED_IN_SYNC_ON_NO_SESSIONS:
      return TabSwitcherPanelOverlayType::
          OVERLAY_PANEL_USER_SIGNED_IN_SYNC_ON_NO_SESSIONS;
    case TabSwitcherSignInPanelsType::NO_PANEL:
      return TabSwitcherPanelOverlayType::OVERLAY_PANEL_EMPTY;
  }
}

namespace {
const CGFloat kContainerOriginYOffset = -58.0;
const CGFloat kContainerWidth = 400.0;
const CGFloat kTitleMinimumLineHeight = 32.0;
const CGFloat kSubtitleMinimunLineHeight = 24.0;
}

@interface TabSwitcherPanelOverlayView ()

// Updates the texts of labels and button according to the current
// |overlayType|.
- (void)updateText;
// Updates the button target and tag according to the current |overlayType|.
- (void)updateButtonTarget;
// Sends a SignIn chrome command.
- (void)showSignIn;

@end

@implementation TabSwitcherPanelOverlayView {
  ios::ChromeBrowserState* _browserState;  // Weak.
  base::scoped_nsobject<UIView> _container;
  base::scoped_nsobject<UILabel> _titleLabel;
  base::scoped_nsobject<UILabel> _subtitleLabel;
  base::scoped_nsobject<MDCButton> _textButton;
  base::scoped_nsobject<MDCButton> _floatingButton;
  base::scoped_nsobject<MDCActivityIndicator> _activityIndicator;
  std::string _recordedMetricString;
}

@synthesize overlayType = _overlayType;

- (instancetype)initWithFrame:(CGRect)frame
                 browserState:(ios::ChromeBrowserState*)browserState {
  self = [super initWithFrame:frame];
  if (self) {
    _browserState = browserState;
    // Create and add container. Will be vertically and horizontally centered.
    _container.reset([[UIView alloc] initWithFrame:CGRectZero]);
    [_container setTranslatesAutoresizingMaskIntoConstraints:NO];
    [self addSubview:_container];

    // Create and add title label to the container.
    _titleLabel.reset([[UILabel alloc] initWithFrame:CGRectZero]);
    [_titleLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_titleLabel setFont:[MDCTypography headlineFont]];
    [_titleLabel
        setTextColor:[UIColor
                         colorWithWhite:1
                                  alpha:[MDCTypography headlineFontOpacity]]];
    [_titleLabel setLineBreakMode:NSLineBreakByWordWrapping];
    [_titleLabel setNumberOfLines:0];
    [_titleLabel setTextAlignment:NSTextAlignmentCenter];
    [_container addSubview:_titleLabel];

    // Create and add subtitle label to the container.
    _subtitleLabel.reset([[UILabel alloc] initWithFrame:CGRectZero]);
    [_subtitleLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_subtitleLabel setFont:[MDCTypography subheadFont]];
    [_subtitleLabel
        setTextColor:[UIColor
                         colorWithWhite:1
                                  alpha:[MDCTypography display1FontOpacity]]];
    [_subtitleLabel setNumberOfLines:0];
    [_container addSubview:_subtitleLabel];

    // Create and add button to the container.
    _textButton.reset([[MDCRaisedButton alloc] init]);
    [_textButton setElevation:MDCShadowElevationNone
                     forState:UIControlStateNormal];
    MDCPalette* buttonPalette = [MDCPalette cr_bluePalette];
    [_textButton
        setInkColor:[[buttonPalette tint300] colorWithAlphaComponent:0.5f]];
    [_textButton setBackgroundColor:[buttonPalette tint500]
                           forState:UIControlStateNormal];
    [_textButton setBackgroundColor:[UIColor colorWithWhite:0.8f alpha:1.0f]
                           forState:UIControlStateDisabled];
    [_textButton setTranslatesAutoresizingMaskIntoConstraints:NO];
    [[_textButton imageView] setTintColor:[UIColor whiteColor]];
    [_textButton setCustomTitleColor:[UIColor whiteColor]];
    [_container addSubview:_textButton];

    // Create and add floatingButton to the container.
    _floatingButton.reset([[MDCFloatingButton alloc] init]);
    [_floatingButton setTranslatesAutoresizingMaskIntoConstraints:NO];
    [[_floatingButton imageView] setTintColor:[UIColor whiteColor]];
    [_container addSubview:_floatingButton];

    // Create and add activity indicator to the container.
    _activityIndicator.reset(
        [[MDCActivityIndicator alloc] initWithFrame:CGRectZero]);
    [_activityIndicator setCycleColors:ActivityIndicatorBrandedCycleColors()];
    [_activityIndicator setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_container addSubview:_activityIndicator];

    // Set constraints on all of the container's subviews.
    AddSameCenterXConstraint(_container, _titleLabel);
    AddSameCenterXConstraint(_container, _subtitleLabel);
    NSDictionary* viewsDictionary = @{
      @"title" : _titleLabel.get(),
      @"subtitle" : _subtitleLabel.get(),
      @"button" : _textButton.get(),
      @"floatingButton" : _floatingButton.get(),
      @"activityIndicator" : _activityIndicator.get(),
    };
    AddSameCenterXConstraint(_container, _textButton);
    AddSameCenterXConstraint(_container, _floatingButton);
    AddSameCenterXConstraint(_container, _activityIndicator);
    NSArray* constraints = @[
      @"V:|-0-[title]-12-[subtitle]-48-[button]-0-|",
      @"V:[subtitle]-35-[floatingButton(==48)]-0-|",
      @"V:[subtitle]-24-[activityIndicator]", @"H:|-[title]-|",
      @"H:|-[subtitle]-|", @"H:[button(>=180)]", @"H:[floatingButton(==48)]"
    ];
    ApplyVisualConstraints(constraints, viewsDictionary, _container);

    // Sets the container's width relative to the parent.
    ApplyVisualConstraintsWithMetrics(
        @[
          @"H:|-(>=0)-[container(==containerWidth@999)]-(>=0)-|",
        ],
        @{ @"container" : _container.get() },
        @{ @"containerWidth" : @(kContainerWidth) }, self);
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  CGRect containerFrame = [_container frame];
  containerFrame.origin.x =
      (self.frame.size.width - containerFrame.size.width) / 2;
  containerFrame.origin.y =
      (self.frame.size.height - containerFrame.size.height) / 2 +
      kContainerOriginYOffset;
  [_container setFrame:containerFrame];
}

- (void)setOverlayType:(TabSwitcherPanelOverlayType)overlayType {
  _overlayType = overlayType;
  [self updateText];
  [self updateButtonTarget];
}

#pragma mark - Private

- (void)updateText {
  NSMutableAttributedString* titleString = nil;
  NSMutableAttributedString* subtitleString = nil;

  NSString* buttonTitle = nil;
  UIImage* buttonImage = nil;
  NSString* buttonAccessibilityLabel = nil;
  UIColor* floatingButtonNormalBackgroundColor = nil;
  UIColor* floatingButtonDisabledBackgroundColor = nil;
  UIColor* floatingButtonInkColor = nil;
  BOOL spinnerIsHidden = YES;
  switch (self.overlayType) {
    case TabSwitcherPanelOverlayType::OVERLAY_PANEL_EMPTY:
      break;
    case TabSwitcherPanelOverlayType::OVERLAY_PANEL_USER_SIGNED_OUT:
      titleString = [[[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_IOS_TAB_SWITCHER_SIGN_IN_ACCOUNT_TITLE)]
          autorelease];
      subtitleString = [[[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_IOS_TAB_SWITCHER_SIGN_IN_ACCOUNT_PROMO)]
          autorelease];
      buttonTitle =
          l10n_util::GetNSString(IDS_IOS_TAB_SWITCHER_SIGN_IN_ACCOUNT_BUTTON);
      break;
    case TabSwitcherPanelOverlayType::OVERLAY_PANEL_USER_SIGNED_IN_SYNC_OFF:
      titleString = [[[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_IOS_TAB_SWITCHER_ENABLE_SYNC_TITLE)]
          autorelease];
      subtitleString = [[[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_IOS_TAB_SWITCHER_SYNC_IS_OFF)] autorelease];
      buttonTitle =
          l10n_util::GetNSString(IDS_IOS_TAB_SWITCHER_ENABLE_SYNC_BUTTON);
      break;
    case TabSwitcherPanelOverlayType::
        OVERLAY_PANEL_USER_SIGNED_IN_SYNC_ON_NO_SESSIONS:
      titleString = [[[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_IOS_TAB_SWITCHER_NO_TABS_TO_SYNC_PROMO)]
          autorelease];
      subtitleString = [[[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_IOS_OPEN_TABS_NO_SESSION_INSTRUCTIONS)]
          autorelease];
      break;
    case TabSwitcherPanelOverlayType::
        OVERLAY_PANEL_USER_SIGNED_IN_SYNC_IN_PROGRESS:
      titleString = [[[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_IOS_TAB_SWITCHER_SYNC_IN_PROGRESS_PROMO)]
          autorelease];
      subtitleString = [[[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_IOS_TAB_SWITCHER_SYNC_IS_OFF)] autorelease];
      spinnerIsHidden = NO;
      break;
    case TabSwitcherPanelOverlayType::OVERLAY_PANEL_USER_NO_OPEN_TABS:
      titleString = [[[NSMutableAttributedString alloc]
          initWithString:
              l10n_util::GetNSString(
                  IDS_IOS_TAB_SWITCHER_NO_LOCAL_NON_INCOGNITO_TABS_TITLE)]
          autorelease];
      subtitleString = [[[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_IOS_TAB_SWITCHER_NO_LOCAL_NON_INCOGNITO_TABS)]
          autorelease];
      buttonImage = [UIImage imageNamed:@"tabswitcher_new_tab_fab"];
      buttonImage = [buttonImage
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      floatingButtonInkColor =
          [[[MDCPalette cr_bluePalette] tint300] colorWithAlphaComponent:0.5f];
      floatingButtonNormalBackgroundColor =
          [[MDCPalette cr_bluePalette] tint500];
      floatingButtonDisabledBackgroundColor =
          [UIColor colorWithWhite:0.8f alpha:1.0f];
      buttonAccessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_SWITCHER_CREATE_NEW_TAB);
      break;
    case TabSwitcherPanelOverlayType::OVERLAY_PANEL_USER_NO_INCOGNITO_TABS:
      titleString = [[[NSMutableAttributedString alloc]
          initWithString:
              l10n_util::GetNSString(
                  IDS_IOS_TAB_SWITCHER_NO_LOCAL_INCOGNITO_TABS_PROMO)]
          autorelease];
      subtitleString = [[[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_IOS_TAB_SWITCHER_NO_LOCAL_INCOGNITO_TABS)]
          autorelease];
      buttonImage = [UIImage imageNamed:@"tabswitcher_new_tab_fab"];
      buttonImage = [buttonImage
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

      floatingButtonInkColor =
          [[[MDCPalette greyPalette] tint300] colorWithAlphaComponent:0.25f];
      floatingButtonNormalBackgroundColor = [[MDCPalette greyPalette] tint500];
      floatingButtonDisabledBackgroundColor =
          [UIColor colorWithWhite:0.8f alpha:1.0f];
      buttonAccessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_SWITCHER_CREATE_NEW_INCOGNITO_TAB);
      break;
  };

  base::scoped_nsobject<NSMutableParagraphStyle> titleStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [titleStyle setMinimumLineHeight:kTitleMinimumLineHeight];
  [titleStyle setAlignment:NSTextAlignmentCenter];
  [titleStyle setLineBreakMode:NSLineBreakByWordWrapping];
  [titleString addAttribute:NSParagraphStyleAttributeName
                      value:titleStyle
                      range:NSMakeRange(0, [titleString length])];
  [_titleLabel setAttributedText:titleString];

  base::scoped_nsobject<NSMutableParagraphStyle> subtitleStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [subtitleStyle setMinimumLineHeight:kSubtitleMinimunLineHeight];
  [subtitleStyle setAlignment:NSTextAlignmentCenter];
  [subtitleStyle setLineBreakMode:NSLineBreakByWordWrapping];
  [subtitleString addAttribute:NSParagraphStyleAttributeName
                         value:subtitleStyle
                         range:NSMakeRange(0, [subtitleString length])];
  [_subtitleLabel setAttributedText:subtitleString];

  [_textButton setTitle:buttonTitle forState:UIControlStateNormal];
  [_textButton setImage:buttonImage forState:UIControlStateNormal];
  [_floatingButton setTitle:buttonTitle forState:UIControlStateNormal];
  [_floatingButton setImage:buttonImage forState:UIControlStateNormal];
  [_floatingButton setInkColor:floatingButtonInkColor];
  [_floatingButton setBackgroundColor:floatingButtonNormalBackgroundColor
                             forState:UIControlStateNormal];
  [_floatingButton setBackgroundColor:floatingButtonDisabledBackgroundColor
                             forState:UIControlStateDisabled];

  [_floatingButton setAccessibilityLabel:buttonAccessibilityLabel];
  [_activityIndicator setHidden:spinnerIsHidden];
  if (spinnerIsHidden) {
    [_activityIndicator stopAnimating];
  } else {
    [_activityIndicator startAnimating];
  }
}

- (void)updateButtonTarget {
  NSInteger tag = 0;
  SEL selector = nil;
  _recordedMetricString = "";

  BOOL shouldShowTextButton = YES;
  BOOL shouldShowFloatingButton = NO;
  switch (self.overlayType) {
    case TabSwitcherPanelOverlayType::OVERLAY_PANEL_EMPTY:
      shouldShowTextButton = NO;
      break;
    case TabSwitcherPanelOverlayType::OVERLAY_PANEL_USER_SIGNED_OUT:
      selector = @selector(showSignIn);
      _recordedMetricString = "MobileTabSwitcherSignIn";
      break;
    case TabSwitcherPanelOverlayType::OVERLAY_PANEL_USER_SIGNED_IN_SYNC_OFF:
      selector = @selector(showSyncSettings);
      _recordedMetricString = "MobileTabSwitcherEnableSync";
      break;
    case TabSwitcherPanelOverlayType::
        OVERLAY_PANEL_USER_SIGNED_IN_SYNC_ON_NO_SESSIONS:
      shouldShowTextButton = NO;
      break;
    case TabSwitcherPanelOverlayType::
        OVERLAY_PANEL_USER_SIGNED_IN_SYNC_IN_PROGRESS:
      shouldShowTextButton = NO;
      break;
    case TabSwitcherPanelOverlayType::OVERLAY_PANEL_USER_NO_OPEN_TABS:
      tag = IDC_NEW_TAB;
      selector = @selector(rootViewControllerChromeCommand:);
      _recordedMetricString = "MobileTabSwitcherCreateNonIncognitoTab";
      shouldShowTextButton = NO;
      shouldShowFloatingButton = YES;
      break;
    case TabSwitcherPanelOverlayType::OVERLAY_PANEL_USER_NO_INCOGNITO_TABS:
      tag = IDC_NEW_INCOGNITO_TAB;
      selector = @selector(rootViewControllerChromeCommand:);
      _recordedMetricString = "MobileTabSwitcherCreateIncognitoTab";
      shouldShowTextButton = NO;
      shouldShowFloatingButton = YES;
      break;
  }

  [_textButton setTag:tag];
  [_textButton addTarget:self
                  action:selector
        forControlEvents:UIControlEventTouchUpInside];
  [_textButton addTarget:self
                  action:@selector(recordMetrics)
        forControlEvents:UIControlEventTouchUpInside];
  [_textButton setHidden:!shouldShowTextButton];
  [_floatingButton setTag:tag];
  [_floatingButton addTarget:self
                      action:selector
            forControlEvents:UIControlEventTouchUpInside];
  [_floatingButton addTarget:self
                      action:@selector(recordMetrics)
            forControlEvents:UIControlEventTouchUpInside];
  [_floatingButton setHidden:!shouldShowFloatingButton];
}

- (void)showSignIn {
  base::RecordAction(base::UserMetricsAction("Signin_Signin_FromTabSwitcher"));
  base::scoped_nsobject<ShowSigninCommand> command([[ShowSigninCommand alloc]
      initWithOperation:AUTHENTICATION_OPERATION_SIGNIN
      signInAccessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_TAB_SWITCHER]);
  [self chromeExecuteCommand:command];
}

- (void)showSyncSettings {
  [self chromeExecuteCommand:ios_internal::sync::GetSyncCommandForBrowserState(
                                 _browserState)];
}

- (void)rootViewControllerChromeCommand:(id)command {
  [self chromeExecuteCommand:command];
}

- (void)recordMetrics {
  if (!_recordedMetricString.length())
    return;
  base::RecordAction(base::UserMetricsAction(_recordedMetricString.c_str()));
}

@end
