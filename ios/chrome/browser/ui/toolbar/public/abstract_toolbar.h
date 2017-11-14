// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_ABSTRACT_TOOLBAR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_ABSTRACT_TOOLBAR_H_

#import <Foundation/Foundation.h>

@class ToolsPopupController;
@class ToolsMenuConfiguration;

// ToolbarController public interface.
@protocol AbstractToolbar<NSObject>
// Sets whether the share button is enabled or not.
- (void)setShareButtonEnabled:(BOOL)enabled;
// Triggers an animation on the tools menu button to draw the user's
// attention.
- (void)triggerToolsMenuButtonAnimation;
// Adjusts the height of the Toolbar to match the current UI layout.
- (void)adjustToolbarHeight;
// Sets the background to a particular alpha value. Intended for use by
// subcleasses that need to set the opacity of the entire toolbar.
- (void)setBackgroundAlpha:(CGFloat)alpha;
// Updates the tab stack button (if there is one) based on the given tab
// count. If |tabCount| > |kStackButtonMaxTabCount|, an easter egg is shown
// instead of the actual number of tabs.
- (void)setTabCount:(NSInteger)tabCount;
// Activates constraints to simulate a safe area with |fakeSafeAreaInsets|
// insets. The insets will be used as leading/trailing wrt RTL. Those
// constraints have a higher priority than the one used to respect the safe
// area. They need to be deactivated for the toolbar to respect the safe area
// again. The fake safe area can be bigger or smaller than the real safe area.
- (void)activateFakeSafeAreaInsets:(UIEdgeInsets)fakeSafeAreaInsets;
// Deactivates the constraints used to create a fake safe area.
- (void)deactivateFakeSafeAreaInsets;
// The view for the toolbar background image. This is a subview of |view| to
// allow clients to alter the transparency of the background image without
// affecting the other components of the toolbar.
@property(nonatomic, readonly, strong) UIImageView* backgroundView;

// Following methods will be removed shortly by CL 741466.
#pragma mark - ToolsMenu
- (void)showToolsMenuPopupWithConfiguration:
    (ToolsMenuConfiguration*)configuration;
- (void)dismissToolsMenuPopup;
@property(nonatomic, readonly, strong)
    ToolsPopupController* toolsPopupController;
@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_ABSTRACT_TOOLBAR_H_
