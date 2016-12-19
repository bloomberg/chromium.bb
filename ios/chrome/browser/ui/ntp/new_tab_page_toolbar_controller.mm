// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_toolbar_controller.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/strings/grit/components_strings.h"
#include "components/toolbar/toolbar_model.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_model_ios.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_resource_macros.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;

namespace {

const CGFloat kButtonYOffset = 4.0;
const CGFloat kBackButtonLeading = 0;
const CGFloat kForwardButtonLeading = 48;
const CGFloat kOmniboxFocuserLeading = 96;
const CGSize kBackButtonSize = {48, 48};
const CGSize kForwardButtonSize = {48, 48};
const CGSize kOmniboxFocuserSize = {128, 48};

enum {
  NTPToolbarButtonNameBack = NumberOfToolbarButtonNames,
  NTPToolbarButtonNameForward,
  NumberOfNTPToolbarButtonNames,
};

}  // namespace

@interface NewTabPageToolbarController () {
  base::scoped_nsobject<UIButton> _backButton;
  base::scoped_nsobject<UIButton> _forwardButton;
  base::scoped_nsobject<UIButton> _omniboxFocuser;
  id<WebToolbarDelegate> _delegate;

  // Delegate to focus and blur the omnibox.
  base::WeakNSProtocol<id<OmniboxFocuser>> _focuser;
}

@end

@implementation NewTabPageToolbarController

- (instancetype)initWithToolbarDelegate:(id<WebToolbarDelegate>)delegate
                                focuser:(id<OmniboxFocuser>)focuser {
  self = [super initWithStyle:ToolbarControllerStyleLightMode];
  if (self) {
    _delegate = delegate;
    _focuser.reset(focuser);
    [self.backgroundView setHidden:YES];

    CGFloat boundingWidth = self.view.bounds.size.width;
    LayoutRect backButtonLayout =
        LayoutRectMake(kBackButtonLeading, boundingWidth, kButtonYOffset,
                       kBackButtonSize.width, kBackButtonSize.height);
    _backButton.reset(
        [[UIButton alloc] initWithFrame:LayoutRectGetRect(backButtonLayout)]);
    [_backButton
        setAutoresizingMask:UIViewAutoresizingFlexibleTrailingMargin() |
                            UIViewAutoresizingFlexibleBottomMargin];
    LayoutRect forwardButtonLayout =
        LayoutRectMake(kForwardButtonLeading, boundingWidth, kButtonYOffset,
                       kForwardButtonSize.width, kForwardButtonSize.height);
    _forwardButton.reset([[UIButton alloc]
        initWithFrame:LayoutRectGetRect(forwardButtonLayout)]);
    [_forwardButton
        setAutoresizingMask:UIViewAutoresizingFlexibleTrailingMargin() |
                            UIViewAutoresizingFlexibleBottomMargin];
    LayoutRect omniboxFocuserLayout =
        LayoutRectMake(kOmniboxFocuserLeading, boundingWidth, kButtonYOffset,
                       kOmniboxFocuserSize.width, kOmniboxFocuserSize.height);
    _omniboxFocuser.reset([[UIButton alloc]
        initWithFrame:LayoutRectGetRect(omniboxFocuserLayout)]);
    [_omniboxFocuser
        setAccessibilityLabel:l10n_util::GetNSString(IDS_ACCNAME_LOCATION)];

    [_omniboxFocuser setAutoresizingMask:UIViewAutoresizingFlexibleWidth];

    [self.view addSubview:_backButton];
    [self.view addSubview:_forwardButton];
    [self.view addSubview:_omniboxFocuser];
    [_backButton setImageEdgeInsets:UIEdgeInsetsMakeDirected(0, 0, 0, -10)];
    [_forwardButton setImageEdgeInsets:UIEdgeInsetsMakeDirected(0, -7, 0, 0)];

    // Set up the button images.
    [self setUpButton:_backButton
           withImageEnum:NTPToolbarButtonNameBack
         forInitialState:UIControlStateDisabled
        hasDisabledImage:YES
           synchronously:NO];
    [self setUpButton:_forwardButton
           withImageEnum:NTPToolbarButtonNameForward
         forInitialState:UIControlStateDisabled
        hasDisabledImage:YES
           synchronously:NO];

    base::scoped_nsobject<UILongPressGestureRecognizer> backLongPress(
        [[UILongPressGestureRecognizer alloc]
            initWithTarget:self
                    action:@selector(handleLongPress:)]);
    [_backButton addGestureRecognizer:backLongPress];
    base::scoped_nsobject<UILongPressGestureRecognizer> forwardLongPress(
        [[UILongPressGestureRecognizer alloc]
            initWithTarget:self
                    action:@selector(handleLongPress:)]);
    [_forwardButton addGestureRecognizer:forwardLongPress];
    [_backButton setTag:IDC_BACK];
    [_forwardButton setTag:IDC_FORWARD];

    [_omniboxFocuser addTarget:self
                        action:@selector(focusOmnibox:)
              forControlEvents:UIControlEventTouchUpInside];

    SetA11yLabelAndUiAutomationName(_backButton, IDS_ACCNAME_BACK, @"Back");
    SetA11yLabelAndUiAutomationName(_forwardButton, IDS_ACCNAME_FORWARD,
                                    @"Forward");
  }
  return self;
}

- (CGFloat)statusBarOffset {
  return 0;
}

- (BOOL)imageShouldFlipForRightToLeftLayoutDirection:(int)imageEnum {
  DCHECK(imageEnum < NumberOfNTPToolbarButtonNames);
  if (imageEnum < NumberOfToolbarButtonNames)
    return [super imageShouldFlipForRightToLeftLayoutDirection:imageEnum];
  if (imageEnum == NTPToolbarButtonNameBack ||
      imageEnum == NTPToolbarButtonNameForward) {
    return YES;
  }
  return NO;
}

- (int)imageEnumForButton:(UIButton*)button {
  if (button == _backButton.get())
    return NTPToolbarButtonNameBack;
  if (button == _forwardButton.get())
    return NTPToolbarButtonNameForward;
  return [super imageEnumForButton:button];
}

- (int)imageIdForImageEnum:(int)index
                     style:(ToolbarControllerStyle)style
                  forState:(ToolbarButtonUIState)state {
  DCHECK(style < ToolbarControllerStyleMaxStyles);
  DCHECK(state < NumberOfToolbarButtonUIStates);

  if (index >= NumberOfNTPToolbarButtonNames)
    NOTREACHED();
  if (index < NumberOfToolbarButtonNames)
    return [super imageIdForImageEnum:index style:style forState:state];

  index -= NumberOfToolbarButtonNames;

  const int numberOfAddedNames =
      NumberOfNTPToolbarButtonNames - NumberOfToolbarButtonNames;
  // Name, style [light, dark], UIControlState [normal, pressed, disabled]
  static int
      buttonImageIds[numberOfAddedNames][2][NumberOfToolbarButtonUIStates] = {
          TOOLBAR_IDR_THREE_STATE(BACK), TOOLBAR_IDR_THREE_STATE(FORWARD),
      };
  return buttonImageIds[index][style][state];
}

- (IBAction)recordUserMetrics:(id)sender {
  if (sender == _backButton.get()) {
    base::RecordAction(UserMetricsAction("MobileToolbarBack"));
  } else if (sender == _forwardButton.get()) {
    base::RecordAction(UserMetricsAction("MobileToolbarForward"));
  } else {
    [super recordUserMetrics:sender];
  }
}

- (void)handleLongPress:(UILongPressGestureRecognizer*)gesture {
  if (gesture.state != UIGestureRecognizerStateBegan)
    return;

  if (gesture.view == _backButton.get()) {
    base::scoped_nsobject<GenericChromeCommand> command(
        [[GenericChromeCommand alloc] initWithTag:IDC_SHOW_BACK_HISTORY]);
    [_backButton chromeExecuteCommand:command];
  } else if (gesture.view == _forwardButton.get()) {
    base::scoped_nsobject<GenericChromeCommand> command(
        [[GenericChromeCommand alloc] initWithTag:IDC_SHOW_FORWARD_HISTORY]);
    [_forwardButton chromeExecuteCommand:command];
  }
}

- (void)hideViewsForNewTabPage:(BOOL)hide {
  [super hideViewsForNewTabPage:hide];
  // Show the back/forward buttons if there is forward history.
  ToolbarModelIOS* toolbarModelIOS = [_delegate toolbarModelIOS];
  BOOL forwardEnabled = toolbarModelIOS->CanGoForward();
  [_backButton setHidden:!forwardEnabled && hide];
  [_backButton setEnabled:toolbarModelIOS->CanGoBack()];
  [_forwardButton setHidden:!forwardEnabled && hide];
}

- (void)focusOmnibox:(id)sender {
  [_focuser focusFakebox];
}

- (IBAction)stackButtonTouchDown:(id)sender {
  [_delegate prepareToEnterTabSwitcher:self];
}

@end
