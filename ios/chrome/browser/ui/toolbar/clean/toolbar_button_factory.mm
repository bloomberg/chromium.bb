// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"

#include "base/ios/ios_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_visibility_configuration.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_tab_grid_button.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_base_feature.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_constants.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_resource_macros.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/images/branded_image_provider.h"
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
@synthesize dispatcher = _dispatcher;
@synthesize visibilityConfiguration = _visibilityConfiguration;

- (instancetype)initWithStyle:(ToolbarStyle)style {
  self = [super init];
  if (self) {
    _style = style;
    _toolbarConfiguration = [[ToolbarConfiguration alloc] initWithStyle:style];
  }
  return self;
}

#pragma mark - Buttons

- (ToolbarButton*)backButton {
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
  [self configureButton:backButton width:kToolbarButtonWidth];
  if (!IsIPadIdiom()) {
    backButton.imageEdgeInsets =
        UIEdgeInsetsMakeDirected(0, 0, 0, kBackButtonImageInset);
  }
  [backButton addTarget:self.dispatcher
                 action:@selector(goBack)
       forControlEvents:UIControlEventTouchUpInside];
  backButton.visibilityMask = self.visibilityConfiguration.backButtonVisibility;
  return backButton;
}

- (ToolbarButton*)leadingForwardButton {
  ToolbarButton* forwardButton = self.forwardButton;
  forwardButton.visibilityMask =
      self.visibilityConfiguration.leadingForwardButtonVisibility;
  return forwardButton;
}

- (ToolbarButton*)trailingForwardButton {
  ToolbarButton* forwardButton = self.forwardButton;
  forwardButton.visibilityMask =
      self.visibilityConfiguration.trailingForwardButtonVisibility;
  return forwardButton;
}

- (ToolbarTabGridButton*)tabGridButton {
  int tabGridButtonImages[styleCount][TOOLBAR_STATE_COUNT] =
      TOOLBAR_IDR_THREE_STATE(OVERVIEW);
  ToolbarTabGridButton* tabGridButton = [ToolbarTabGridButton
      toolbarButtonWithImageForNormalState:NativeImage(
                                               tabGridButtonImages[self.style]
                                                                  [DEFAULT])
                  imageForHighlightedState:NativeImage(
                                               tabGridButtonImages[self.style]
                                                                  [PRESSED])
                     imageForDisabledState:NativeImage(
                                               tabGridButtonImages[self.style]
                                                                  [DISABLED])];
  SetA11yLabelAndUiAutomationName(tabGridButton, IDS_IOS_TOOLBAR_SHOW_TABS,
                                  kToolbarStackButtonIdentifier);
  [tabGridButton
      setTitleColor:[self.toolbarConfiguration buttonTitleNormalColor]
           forState:UIControlStateNormal];
  [tabGridButton
      setTitleColor:[self.toolbarConfiguration buttonTitleHighlightedColor]
           forState:UIControlStateHighlighted];
  [self configureButton:tabGridButton width:kToolbarButtonWidth];

  // TODO(crbug.com/799601): Delete this once its not needed.
  if (base::FeatureList::IsEnabled(kMemexTabSwitcher)) {
    [tabGridButton addTarget:self.dispatcher
                      action:@selector(navigateToMemexTabSwitcher)
            forControlEvents:UIControlEventTouchUpInside];
  } else {
    [tabGridButton addTarget:self.dispatcher
                      action:@selector(displayTabSwitcher)
            forControlEvents:UIControlEventTouchUpInside];
  }

  tabGridButton.visibilityMask =
      self.visibilityConfiguration.tabGridButtonVisibility;
  return tabGridButton;
}

- (ToolbarButton*)tabSwitcherStripButton {
  return [self tabGridButton];
}

- (ToolbarButton*)tabSwitcherGridButton {
  ToolbarButton* tabSwitcherGridButton =
      [ToolbarButton toolbarButtonWithImageForNormalState:
                         [UIImage imageNamed:@"tabswitcher_tab_switcher_button"]
                                 imageForHighlightedState:nil
                                    imageForDisabledState:nil];
  tabSwitcherGridButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_SHOW_TAB_GRID);
  return tabSwitcherGridButton;
}

- (ToolbarToolsMenuButton*)toolsMenuButton {
  ToolbarControllerStyle style = self.style == NORMAL
                                     ? ToolbarControllerStyleLightMode
                                     : ToolbarControllerStyleIncognitoMode;
  ToolbarToolsMenuButton* toolsMenuButton =
      [[ToolbarToolsMenuButton alloc] initWithFrame:CGRectZero style:style];

  SetA11yLabelAndUiAutomationName(toolsMenuButton, IDS_IOS_TOOLBAR_SETTINGS,
                                  kToolbarToolsMenuButtonIdentifier);
  if (IsUIRefreshPhase1Enabled()) {
    [self configureButton:toolsMenuButton width:kToolbarButtonWidth];
    [toolsMenuButton.heightAnchor constraintEqualToConstant:kToolbarHeight]
        .active = YES;
  } else {
    [self configureButton:toolsMenuButton width:kToolsMenuButtonWidth];
  }
  [toolsMenuButton addTarget:self.dispatcher
                      action:@selector(showToolsMenu)
            forControlEvents:UIControlEventTouchUpInside];
  toolsMenuButton.visibilityMask =
      self.visibilityConfiguration.toolsMenuButtonVisibility;
  return toolsMenuButton;
}

- (ToolbarButton*)shareButton {
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
  SetA11yLabelAndUiAutomationName(shareButton, IDS_IOS_TOOLS_MENU_SHARE,
                                  kToolbarShareButtonIdentifier);
  [self configureButton:shareButton width:kToolbarButtonWidth];
  shareButton.titleLabel.text = @"Share";
  [shareButton addTarget:self.dispatcher
                  action:@selector(sharePage)
        forControlEvents:UIControlEventTouchUpInside];
  shareButton.visibilityMask =
      self.visibilityConfiguration.shareButtonVisibility;
  return shareButton;
}

- (ToolbarButton*)reloadButton {
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
  [self configureButton:reloadButton width:kToolbarButtonWidth];
  [reloadButton addTarget:self.dispatcher
                   action:@selector(reload)
         forControlEvents:UIControlEventTouchUpInside];
  reloadButton.visibilityMask =
      self.visibilityConfiguration.reloadButtonVisibility;
  return reloadButton;
}

- (ToolbarButton*)stopButton {
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
  [self configureButton:stopButton width:kToolbarButtonWidth];
  [stopButton addTarget:self.dispatcher
                 action:@selector(stopLoading)
       forControlEvents:UIControlEventTouchUpInside];
  stopButton.visibilityMask = self.visibilityConfiguration.stopButtonVisibility;
  return stopButton;
}

- (ToolbarButton*)bookmarkButton {
  int bookmarkButtonImages[styleCount][TOOLBAR_STATE_COUNT] =
      TOOLBAR_IDR_TWO_STATE(STAR);
  ToolbarButton* bookmarkButton = [ToolbarButton
      toolbarButtonWithImageForNormalState:NativeImage(
                                               bookmarkButtonImages[self.style]
                                                                   [DEFAULT])
                  imageForHighlightedState:NativeImage(
                                               bookmarkButtonImages[self.style]
                                                                   [PRESSED])
                     imageForDisabledState:nil];
  bookmarkButton.adjustsImageWhenHighlighted = NO;
  [bookmarkButton
      setImage:NativeImage(bookmarkButtonImages[self.style][PRESSED])
      forState:UIControlStateSelected];
  bookmarkButton.accessibilityLabel = l10n_util::GetNSString(IDS_TOOLTIP_STAR);
  [self configureButton:bookmarkButton width:kToolbarButtonWidth];
  [bookmarkButton addTarget:self.dispatcher
                     action:@selector(bookmarkPage)
           forControlEvents:UIControlEventTouchUpInside];

  bookmarkButton.visibilityMask =
      self.visibilityConfiguration.bookmarkButtonVisibility;
  return bookmarkButton;
}

- (ToolbarButton*)voiceSearchButton {
  NSArray<UIImage*>* images = [self voiceSearchImages];
  ToolbarButton* voiceSearchButton =
      [ToolbarButton toolbarButtonWithImageForNormalState:images[0]
                                 imageForHighlightedState:images[1]
                                    imageForDisabledState:nil];
  voiceSearchButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ACCNAME_VOICE_SEARCH);
  [self configureButton:voiceSearchButton width:kToolbarButtonWidth];
  voiceSearchButton.enabled = NO;
  voiceSearchButton.visibilityMask =
      self.visibilityConfiguration.voiceSearchButtonVisibility;
  return voiceSearchButton;
}

- (ToolbarButton*)contractButton {
  NSString* collapseName = _style ? @"collapse_incognito" : @"collapse";
  NSString* collapsePressedName =
      _style ? @"collapse_pressed_incognito" : @"collapse_pressed";
  ToolbarButton* contractButton = [ToolbarButton
      toolbarButtonWithImageForNormalState:[UIImage imageNamed:collapseName]
                  imageForHighlightedState:[UIImage
                                               imageNamed:collapsePressedName]
                     imageForDisabledState:nil];
  contractButton.accessibilityLabel = l10n_util::GetNSString(IDS_CANCEL);
  contractButton.alpha = 0;
  contractButton.hidden = YES;
  [self configureButton:contractButton width:kToolbarButtonWidth];
  [contractButton addTarget:self.dispatcher
                     action:@selector(cancelOmniboxEdit)
           forControlEvents:UIControlEventTouchUpInside];

  contractButton.visibilityMask =
      self.visibilityConfiguration.contractButtonVisibility;
  return contractButton;
}

- (ToolbarButton*)omniboxButton {
  ToolbarButton* omniboxButton = [ToolbarButton
      toolbarButtonWithImageForNormalState:NativeImage(IDR_IOS_OMNIBOX_SEARCH)
                  imageForHighlightedState:NativeImage(IDR_IOS_OMNIBOX_SEARCH)
                     imageForDisabledState:nil];
  [self configureButton:omniboxButton width:kToolbarButtonWidth];
  [omniboxButton addTarget:self.dispatcher
                    action:@selector(focusOmnibox)
          forControlEvents:UIControlEventTouchUpInside];

  omniboxButton.visibilityMask =
      self.visibilityConfiguration.omniboxButtonVisibility;
  return omniboxButton;
}

- (ToolbarButton*)locationBarLeadingButton {
  ToolbarButton* locationBarLeadingButton;
  if (self.style == INCOGNITO) {
    locationBarLeadingButton = [ToolbarButton
        toolbarButtonWithImageForNormalState:
            [UIImage imageNamed:@"incognito_marker_typing"]
                    imageForHighlightedState:nil
                       imageForDisabledState:
                           [UIImage imageNamed:@"incognito_marker_typing"]];
    locationBarLeadingButton.enabled = NO;
    [self configureButton:locationBarLeadingButton
                    width:kLeadingLocationBarButtonWidth];
    locationBarLeadingButton.imageEdgeInsets =
        UIEdgeInsetsMakeDirected(0, kLeadingLocationBarButtonImageInset, 0, 0);
  }
  locationBarLeadingButton.visibilityMask =
      self.visibilityConfiguration.locationBarLeadingButtonVisibility;

  return locationBarLeadingButton;
}

- (UIButton*)cancelButton {
  UIButton* cancelButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [cancelButton setTitle:l10n_util::GetNSString(IDS_CANCEL)
                forState:UIControlStateNormal];
  [cancelButton setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                  forAxis:UILayoutConstraintAxisHorizontal];
  [cancelButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  cancelButton.contentEdgeInsets = UIEdgeInsetsMake(
      0, kCancelButtonHorizontalInset, 0, kCancelButtonHorizontalInset);
  cancelButton.hidden = YES;
  [cancelButton addTarget:self.dispatcher
                   action:@selector(cancelOmniboxEdit)
         forControlEvents:UIControlEventTouchUpInside];
  return cancelButton;
}

#pragma mark - Helpers

// Sets the |button| width to |width| with a priority of
// UILayoutPriorityRequired - 1. If the priority is |UILayoutPriorityRequired|,
// there is a conflict when the buttons are hidden as the stack view is setting
// their width to 0. Setting the priority to UILayoutPriorityDefaultHigh doesn't
// work as they would have a lower priority than other elements.
- (void)configureButton:(UIView*)button width:(CGFloat)width {
  NSLayoutConstraint* constraint =
      [button.widthAnchor constraintEqualToConstant:width];
  constraint.priority = UILayoutPriorityRequired - 1;
  constraint.active = YES;
}

- (NSArray<UIImage*>*)voiceSearchImages {
  // The voice search images can be overridden by the branded image provider.
  int imageID;
  if (ios::GetChromeBrowserProvider()
          ->GetBrandedImageProvider()
          ->GetToolbarVoiceSearchButtonImageId(&imageID)) {
    return [NSArray
        arrayWithObjects:NativeImage(imageID), NativeImage(imageID), nil];
  }
  int voiceSearchImages[styleCount][TOOLBAR_STATE_COUNT] =
      TOOLBAR_IDR_TWO_STATE(VOICE);
  return [NSArray
      arrayWithObjects:NativeImage(voiceSearchImages[self.style][DEFAULT]),
                       NativeImage(voiceSearchImages[self.style][PRESSED]),
                       nil];
}

- (NSArray<UIImage*>*)TTSImages {
  int TTSImages[styleCount][TOOLBAR_STATE_COUNT] = TOOLBAR_IDR_TWO_STATE(TTS);
  return [NSArray arrayWithObjects:NativeImage(TTSImages[self.style][DEFAULT]),
                                   NativeImage(TTSImages[self.style][PRESSED]),
                                   nil];
}

// Returns a forward button without visibility mask configured.
- (ToolbarButton*)forwardButton {
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
  if (!IsIPadIdiom()) {
    forwardButton.imageEdgeInsets =
        UIEdgeInsetsMakeDirected(0, kForwardButtonImageInset, 0, 0);
  }
  [self configureButton:forwardButton width:kToolbarButtonWidth];
  [forwardButton addTarget:self.dispatcher
                    action:@selector(goForward)
          forControlEvents:UIControlEventTouchUpInside];
  return forwardButton;
}

@end
