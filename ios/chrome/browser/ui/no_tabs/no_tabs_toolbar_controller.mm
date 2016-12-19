// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/no_tabs/no_tabs_toolbar_controller.h"

#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/toolbar/new_tab_button.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ui/gfx/ios/uikit_util.h"

namespace {

const CGFloat kNewTabButtonLeading = 8;
const CGFloat kModeToggleWidth = 34;
const CGFloat kModeToggleHeight = 38;

}  // end namespace

@interface NoTabsToolbarController () {
  // Top-level view for notabs-specific content.
  base::scoped_nsobject<UIView> _noTabsToolbar;
  base::scoped_nsobject<UIButton> _buttonNewTab;
  base::scoped_nsobject<UIButton> _modeToggleButton;
}

@end

@implementation NoTabsToolbarController

- (instancetype)initWithNoTabs {
  self = [super initWithStyle:ToolbarControllerStyleDarkMode];
  if (self) {
    _noTabsToolbar.reset([[UIView alloc] initWithFrame:self.view.bounds]);
    [_noTabsToolbar setBackgroundColor:[UIColor clearColor]];
    [_noTabsToolbar setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                        UIViewAutoresizingFlexibleBottomMargin];

    // Resize the container to match the available area.
    // Do this before the layouts of subviews are computed.
    [_noTabsToolbar setFrame:[self specificControlsArea]];

    CGFloat boundingHeight = [_noTabsToolbar bounds].size.height;
    CGFloat boundingWidth = [_noTabsToolbar bounds].size.width;
    LayoutRect newTabButtonLayout = LayoutRectMake(
        kNewTabButtonLeading, boundingWidth, 0, boundingHeight, boundingHeight);
    _buttonNewTab.reset([[NewTabButton alloc]
        initWithFrame:LayoutRectGetRect(newTabButtonLayout)]);
    [_buttonNewTab
        setAutoresizingMask:UIViewAutoresizingFlexibleTrailingMargin()];

    LayoutRect modeToggleButtonLayout = LayoutRectMake(
        boundingWidth - kModeToggleWidth - boundingHeight, boundingWidth,
        ui::AlignValueToUpperPixel((boundingHeight - kModeToggleHeight) / 2),
        kModeToggleWidth, kModeToggleHeight);

    _modeToggleButton.reset([[UIButton alloc]
        initWithFrame:LayoutRectGetRect(modeToggleButtonLayout)]);
    [_modeToggleButton setHidden:YES];
    [_modeToggleButton
        setAutoresizingMask:UIViewAutoresizingFlexibleLeadingMargin()];
    [_modeToggleButton setImageEdgeInsets:UIEdgeInsetsMakeDirected(7, 5, 7, 5)];
    [_modeToggleButton setImage:[UIImage imageNamed:@"tabstrip_switch"]
                       forState:UIControlStateNormal];
    [_modeToggleButton setTag:IDC_SWITCH_BROWSER_MODES];
    [_modeToggleButton addTarget:self
                          action:@selector(recordUserMetrics:)
                forControlEvents:UIControlEventTouchUpInside];
    [_modeToggleButton addTarget:_modeToggleButton
                          action:@selector(chromeExecuteCommand:)
                forControlEvents:UIControlEventTouchUpInside];

    // The toolbar background is not supposed to show in the no-tabs UI.
    [self.backgroundView setHidden:YES];

    [self.view addSubview:_noTabsToolbar];
    [_noTabsToolbar addSubview:_buttonNewTab];
    [_noTabsToolbar addSubview:_modeToggleButton];

    self.shadowView.hidden = YES;
  }
  return self;
}

// Applies the given transform to this toolbar's controls.
- (void)setControlsTransform:(CGAffineTransform)transform {
  [self setStandardControlsTransform:transform];
  [_buttonNewTab setTransform:transform];
  [_modeToggleButton setTransform:transform];
}

// Shows or hides the mode toggle switch.
- (void)setHasModeToggleSwitch:(BOOL)hasModeToggle {
  [_modeToggleButton setHidden:!hasModeToggle];
}

// Called when a button is pressed.
- (void)recordUserMetrics:(id)sender {
  if (sender == _buttonNewTab.get()) {
    // TODO(rohitrao): Record metrics. http://crbug.com/437418
  } else if (sender == _modeToggleButton.get()) {
    // TODO(rohitrao): Record metrics. http://crbug.com/437418
  } else {
    [super recordUserMetrics:sender];
  }
}

- (UIButton*)modeToggleButton {
  return _modeToggleButton;
}

@end
