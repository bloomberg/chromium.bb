// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_constants.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_resource_macros.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// State of the button, used as index.
typedef NS_ENUM(NSInteger, ToolbarButtonState) {
  DEFAULT = 0,
  PRESSED = 1,
  DISABLED = 2,
  TOOLBAR_STATE_COUNT
};

// Number of style used for the buttons.
const int styleCount = 2;
}  // namespace

@implementation ToolbarButtonFactory

@synthesize toolbarConfiguration = _toolbarConfiguration;
@synthesize style = _style;

- (instancetype)initWithStyle:(ToolbarStyle)style {
  self = [super init];
  if (self) {
    _style = style;
    _toolbarConfiguration = [[ToolbarConfiguration alloc] initWithStyle:style];
  }
  return self;
}

- (ToolbarButton*)backToolbarButton {
  int backButtonImages[styleCount][TOOLBAR_STATE_COUNT] =
      TOOLBAR_IDR_THREE_STATE(BACK);
  ToolbarButton* backButton = [ToolbarButton
      toolbarButtonWithImageForNormalState:NativeReversableImage(
                                               backButtonImages[self.style]
                                                               [DEFAULT],
                                               YES)
                  imageForHighlightedState:NativeReversableImage(
                                               backButtonImages[self.style]
                                                               [PRESSED],
                                               YES)
                     imageForDisabledState:NativeReversableImage(
                                               backButtonImages[self.style]
                                                               [DISABLED],
                                               YES)];
  backButton.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_BACK);
  return backButton;
}

- (ToolbarButton*)forwardToolbarButton {
  int forwardButtonImages[styleCount][TOOLBAR_STATE_COUNT] =
      TOOLBAR_IDR_THREE_STATE(FORWARD);
  ToolbarButton* forwardButton = [ToolbarButton
      toolbarButtonWithImageForNormalState:NativeReversableImage(
                                               forwardButtonImages[self.style]
                                                                  [DEFAULT],
                                               YES)
                  imageForHighlightedState:NativeReversableImage(
                                               forwardButtonImages[self.style]
                                                                  [PRESSED],
                                               YES)
                     imageForDisabledState:NativeReversableImage(
                                               forwardButtonImages[self.style]
                                                                  [DISABLED],
                                               YES)];
  forwardButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_ACCNAME_FORWARD);
  return forwardButton;
}

- (ToolbarButton*)tabSwitcherStripToolbarButton {
  int tabSwitcherButtonImages[styleCount][TOOLBAR_STATE_COUNT] =
      TOOLBAR_IDR_THREE_STATE(OVERVIEW);
  ToolbarButton* tabSwitcherStripButton = [ToolbarButton
      toolbarButtonWithImageForNormalState:
          NativeImage(tabSwitcherButtonImages[self.style][DEFAULT])
                  imageForHighlightedState:
                      NativeImage(tabSwitcherButtonImages[self.style][PRESSED])
                     imageForDisabledState:
                         NativeImage(
                             tabSwitcherButtonImages[self.style][DISABLED])];
  tabSwitcherStripButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_SHOW_TABS);
  [tabSwitcherStripButton
      setTitleColor:[self.toolbarConfiguration buttonTitleNormalColor]
           forState:UIControlStateNormal];
  [tabSwitcherStripButton
      setTitleColor:[self.toolbarConfiguration buttonTitleHighlightedColor]
           forState:UIControlStateHighlighted];
  return tabSwitcherStripButton;
}

- (ToolbarButton*)tabSwitcherGridToolbarButton {
  ToolbarButton* tabSwitcherGridButton =
      [ToolbarButton toolbarButtonWithImageForNormalState:
                         [UIImage imageNamed:@"tabswitcher_tab_switcher_button"]
                                 imageForHighlightedState:nil
                                    imageForDisabledState:nil];
  tabSwitcherGridButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_SHOW_TAB_GRID);
  return tabSwitcherGridButton;
}

- (ToolbarButton*)toolsMenuToolbarButton {
  int toolsMenuButtonImages[styleCount][TOOLBAR_STATE_COUNT] =
      TOOLBAR_IDR_TWO_STATE(TOOLS);
  ToolbarButton* toolsMenuButton = [ToolbarButton
      toolbarButtonWithImageForNormalState:NativeImage(
                                               toolsMenuButtonImages[self.style]
                                                                    [DEFAULT])
                  imageForHighlightedState:NativeImage(
                                               toolsMenuButtonImages[self.style]
                                                                    [PRESSED])
                     imageForDisabledState:nil];
  [toolsMenuButton setImageEdgeInsets:UIEdgeInsetsMakeDirected(0, -3, 0, 0)];
  toolsMenuButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_SETTINGS);
  return toolsMenuButton;
}

- (ToolbarButton*)shareToolbarButton {
  int shareButtonImages[styleCount][TOOLBAR_STATE_COUNT] =
      TOOLBAR_IDR_THREE_STATE(SHARE);
  ToolbarButton* shareButton = [ToolbarButton
      toolbarButtonWithImageForNormalState:NativeImage(
                                               shareButtonImages[self.style]
                                                                [DEFAULT])
                  imageForHighlightedState:NativeImage(
                                               shareButtonImages[self.style]
                                                                [PRESSED])
                     imageForDisabledState:NativeImage(
                                               shareButtonImages[self.style]
                                                                [DISABLED])];
  shareButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_SHARE);
  return shareButton;
}

- (ToolbarButton*)reloadToolbarButton {
  int reloadButtonImages[styleCount][TOOLBAR_STATE_COUNT] =
      TOOLBAR_IDR_THREE_STATE(RELOAD);
  ToolbarButton* reloadButton = [ToolbarButton
      toolbarButtonWithImageForNormalState:NativeReversableImage(
                                               reloadButtonImages[self.style]
                                                                 [DEFAULT],
                                               YES)
                  imageForHighlightedState:NativeReversableImage(
                                               reloadButtonImages[self.style]
                                                                 [PRESSED],
                                               YES)
                     imageForDisabledState:NativeReversableImage(
                                               reloadButtonImages[self.style]
                                                                 [DISABLED],
                                               YES)];
  reloadButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ACCNAME_RELOAD);
  return reloadButton;
}

- (ToolbarButton*)stopToolbarButton {
  int stopButtonImages[styleCount][TOOLBAR_STATE_COUNT] =
      TOOLBAR_IDR_THREE_STATE(STOP);
  ToolbarButton* stopButton = [ToolbarButton
      toolbarButtonWithImageForNormalState:NativeImage(
                                               stopButtonImages[self.style]
                                                               [DEFAULT])
                  imageForHighlightedState:NativeImage(
                                               stopButtonImages[self.style]
                                                               [PRESSED])
                     imageForDisabledState:NativeImage(
                                               stopButtonImages[self.style]
                                                               [DISABLED])];
  stopButton.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ACCNAME_STOP);
  return stopButton;
}

- (ToolbarButton*)starToolbarButton {
  int starButtonImages[styleCount][TOOLBAR_STATE_COUNT] =
      TOOLBAR_IDR_TWO_STATE(STAR);
  ToolbarButton* starButton = [ToolbarButton
      toolbarButtonWithImageForNormalState:NativeImage(
                                               starButtonImages[self.style]
                                                               [DEFAULT])
                  imageForHighlightedState:NativeImage(
                                               starButtonImages[self.style]
                                                               [PRESSED])
                     imageForDisabledState:nil];
  starButton.accessibilityLabel = l10n_util::GetNSString(IDS_TOOLTIP_STAR);
  return starButton;
}

@end
