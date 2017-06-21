// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_button+factory.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

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
  backButton.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_BACK);
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
  forwardButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_ACCNAME_FORWARD);
  return forwardButton;
}

+ (instancetype)tabSwitcherStripToolbarButton {
  ToolbarButton* tabSwitcherStripButton = [self
      toolbarButtonWithImageForNormalState:NativeImage(
                                               IDR_IOS_TOOLBAR_LIGHT_OVERVIEW)
                  imageForHighlightedState:
                      NativeImage(IDR_IOS_TOOLBAR_LIGHT_OVERVIEW_PRESSED)
                     imageForDisabledState:
                         NativeImage(IDR_IOS_TOOLBAR_LIGHT_OVERVIEW_DISABLED)];
  tabSwitcherStripButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_SHOW_TABS);
  return tabSwitcherStripButton;
}

+ (instancetype)tabSwitcherGridToolbarButton {
  ToolbarButton* tabSwitcherGridButton =
      [self toolbarButtonWithImageForNormalState:
                [UIImage imageNamed:@"tabswitcher_tab_switcher_button"]
                        imageForHighlightedState:nil
                           imageForDisabledState:nil];
  tabSwitcherGridButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_SHOW_TAB_GRID);
  return tabSwitcherGridButton;
}

+ (instancetype)toolsMenuToolbarButton {
  ToolbarButton* toolsMenuButton = [self
      toolbarButtonWithImageForNormalState:NativeImage(
                                               IDR_IOS_TOOLBAR_LIGHT_TOOLS)
                  imageForHighlightedState:
                      NativeImage(IDR_IOS_TOOLBAR_LIGHT_TOOLS_PRESSED)
                     imageForDisabledState:nil];
  [toolsMenuButton setImageEdgeInsets:UIEdgeInsetsMakeDirected(0, -3, 0, 0)];
  toolsMenuButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_SETTINGS);
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
  shareButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_SHARE);
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
  reloadButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ACCNAME_RELOAD);
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
  stopButton.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ACCNAME_STOP);
  return stopButton;
}

+ (instancetype)starToolbarButton {
  ToolbarButton* starButton =
      [self toolbarButtonWithImageForNormalState:NativeImage(
                                                     IDR_IOS_TOOLBAR_LIGHT_STAR)
                        imageForHighlightedState:
                            NativeImage(IDR_IOS_TOOLBAR_LIGHT_STAR_PRESSED)
                           imageForDisabledState:nil];
  starButton.accessibilityLabel = l10n_util::GetNSString(IDS_TOOLTIP_STAR);
  return starButton;
}

@end
