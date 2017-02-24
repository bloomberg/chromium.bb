// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_button+factory.h"

#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ToolbarButton (Factory)

#pragma mark - ToolbarButton Setup

+ (instancetype)backToolbarButton {
  ToolbarButton* backButton = [self
      toolbarButtonWithImageForNormalState:NativeReversableImage(
                                               IDR_IOS_TOOLBAR_LIGHT_BACK, YES)
                  imageForHighlightedState:
                      NativeReversableImage(IDR_IOS_TOOLBAR_LIGHT_BACK_PRESSED,
                                            YES)
                     imageForDisabledState:
                         NativeReversableImage(
                             IDR_IOS_TOOLBAR_LIGHT_BACK_DISABLED, YES)];
  return backButton;
}

+ (instancetype)forwardToolbarButton {
  ToolbarButton* forwardButton = [self
      toolbarButtonWithImageForNormalState:NativeReversableImage(
                                               IDR_IOS_TOOLBAR_LIGHT_FORWARD,
                                               YES)
                  imageForHighlightedState:
                      NativeReversableImage(
                          IDR_IOS_TOOLBAR_LIGHT_FORWARD_PRESSED, YES)
                     imageForDisabledState:
                         NativeReversableImage(
                             IDR_IOS_TOOLBAR_LIGHT_FORWARD_DISABLED, YES)];
  return forwardButton;
}

+ (instancetype)tabSwitcherStripToolbarButton {
  return [self toolbarButtonWithImageForNormalState:
                   [UIImage imageNamed:@"tabswitcher_open_tabs"]
                           imageForHighlightedState:nil
                              imageForDisabledState:nil];
}

+ (instancetype)tabSwitcherGridToolbarButton {
  return [self toolbarButtonWithImageForNormalState:
                   [UIImage imageNamed:@"tabswitcher_tab_switcher_button"]
                           imageForHighlightedState:nil
                              imageForDisabledState:nil];
}

+ (instancetype)toolsMenuToolbarButton {
  ToolbarButton* toolsMenuButton = [self
      toolbarButtonWithImageForNormalState:[UIImage
                                               imageNamed:@"tabswitcher_menu"]
                  imageForHighlightedState:nil
                     imageForDisabledState:nil];
  [toolsMenuButton setImageEdgeInsets:UIEdgeInsetsMakeDirected(0, -3, 0, 0)];
  return toolsMenuButton;
}

+ (instancetype)shareToolbarButton {
  ToolbarButton* shareButton = [self
      toolbarButtonWithImageForNormalState:NativeImage(
                                               IDR_IOS_TOOLBAR_LIGHT_SHARE)
                  imageForHighlightedState:
                      NativeImage(IDR_IOS_TOOLBAR_LIGHT_SHARE_PRESSED)
                     imageForDisabledState:
                         NativeImage(IDR_IOS_TOOLBAR_LIGHT_SHARE_DISABLED)];
  return shareButton;
}

+ (instancetype)reloadToolbarButton {
  ToolbarButton* reloadButton = [self
      toolbarButtonWithImageForNormalState:NativeReversableImage(
                                               IDR_IOS_TOOLBAR_LIGHT_RELOAD,
                                               YES)
                  imageForHighlightedState:
                      NativeReversableImage(
                          IDR_IOS_TOOLBAR_LIGHT_RELOAD_PRESSED, YES)
                     imageForDisabledState:
                         NativeReversableImage(
                             IDR_IOS_TOOLBAR_LIGHT_RELOAD_DISABLED, YES)];
  return reloadButton;
}

+ (instancetype)stopToolbarButton {
  ToolbarButton* stopButton = [self
      toolbarButtonWithImageForNormalState:NativeImage(
                                               IDR_IOS_TOOLBAR_LIGHT_STOP)
                  imageForHighlightedState:
                      NativeImage(IDR_IOS_TOOLBAR_LIGHT_STOP_PRESSED)
                     imageForDisabledState:
                         NativeImage(IDR_IOS_TOOLBAR_LIGHT_STOP_DISABLED)];
  return stopButton;
}

@end
