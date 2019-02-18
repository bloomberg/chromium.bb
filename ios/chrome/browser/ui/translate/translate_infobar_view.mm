// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/translate/translate_infobar_view.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/procedural_block_types.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/translate/translate_infobar_language_tab_strip_view.h"
#import "ios/chrome/browser/ui/translate/translate_infobar_language_tab_strip_view_delegate.h"
#import "ios/chrome/browser/ui/translate/translate_infobar_view_delegate.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const CGFloat kInfobarHeight = 54;

namespace {

// Size of the infobar buttons.
const CGFloat kButtonSize = 44;

// Size of the infobar icons.
const CGFloat kIconSize = 24;

// Leading margin for the translate icon.
const CGFloat kIconLeadingMargin = 16;

// Trailing margin for the translate icon.
const CGFloat kIconTrailingMargin = 12;

}  // namespace

@interface TranslateInfobarView () <
    FullscreenUIElement,
    TranslateInfobarLanguageTabStripViewDelegate>

// Translate icon view.
@property(nonatomic, weak) UIImageView* iconView;

// Scrollable tab strip holding the source and the target language tabs.
@property(nonatomic, weak) TranslateInfobarLanguageTabStripView* languagesView;

// Options button. Presents the options popup menu when tapped.
@property(nonatomic, weak) ToolbarButton* optionsButton;

// Dismiss button.
@property(nonatomic, weak) ToolbarButton* dismissButton;

// Toolbar configuration object used for |optionsButton| and |dismissButton|.
@property(nonatomic, strong) ToolbarConfiguration* toolbarConfiguration;

// Constraint used to add bottom margin to the view.
@property(nonatomic, weak) NSLayoutConstraint* bottomAnchorConstraint;

// Last recorded fullscreen progress.
@property(nonatomic, assign) CGFloat previousFullscreenProgress;

@end

@implementation TranslateInfobarView

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // Create and add subviews the first time this moves to a superview.
  if (newSuperview && !self.subviews.count) {
    [self setupSubviews];
    // Lower constraint's priority to avoid breaking other constraints while
    // |newSuperview| is animating.
    // TODO(crbug.com/904521): Investigate why this is needed.
    self.bottomAnchorConstraint.priority = UILayoutPriorityDefaultLow;
  }
  [super willMoveToSuperview:newSuperview];
}

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  if (!self.superview)
    return;

  [NamedGuide guideWithName:kTranslateInfobarOptionsGuide
                       view:self.optionsButton]
      .constrainedView = self.optionsButton;

  // The initial bottom padding should be the current height of the secondary
  // toolbar or the bottom safe area inset, whichever is greater.
  UILayoutGuide* secondaryToolbarGuide =
      [NamedGuide guideWithName:kSecondaryToolbarGuide view:self];
  DCHECK(secondaryToolbarGuide);
  self.bottomAnchorConstraint.constant =
      MAX(secondaryToolbarGuide.layoutFrame.size.height,
          self.superview.safeAreaInsets.bottom);

  // Increase constraint's priority after the view was added to its superview.
  // TODO(crbug.com/904521): Investigate why this is needed.
  self.bottomAnchorConstraint.priority = UILayoutPriorityDefaultHigh;
}

- (CGSize)sizeThatFits:(CGSize)size {
  // Now that the constraint constant has been set calculate the fitting size.
  CGSize computedSize = [self systemLayoutSizeFittingSize:size];
  return CGSizeMake(size.width, computedSize.height);
}

#pragma mark - Properties

- (void)setSourceLanguage:(NSString*)sourceLanguage {
  _sourceLanguage = sourceLanguage;
  self.languagesView.sourceLanguage = sourceLanguage;
}

- (void)setTargetLanguage:(NSString*)targetLanguage {
  _targetLanguage = targetLanguage;
  self.languagesView.targetLanguage = targetLanguage;
}

- (void)setState:(TranslateInfobarViewState)state {
  _state = state;
  self.languagesView.sourceLanguageTabState =
      [self sourceLanguageTabStateFromTranslateInfobarViewState:state];
  self.languagesView.targetLanguageTabState =
      [self targetLanguageTabStateFromTranslateInfobarViewState:state];
}

#pragma mark - Public

- (void)updateUIForPopUpMenuDisplayed:(BOOL)displayed {
  self.optionsButton.spotlighted = displayed;
  self.optionsButton.dimmed = displayed;
  self.dismissButton.dimmed = displayed;
}

#pragma mark - FullscreenUIElement methods

- (void)updateForFullscreenProgress:(CGFloat)progress {
  // The maximum bottom padding is the maximum height of the secondary toolbar
  // or the bottom safe area inset, whichever is greater.
  UILayoutGuide* secondaryToolbarNoFullscreenGuide =
      [NamedGuide guideWithName:kSecondaryToolbarNoFullscreenGuide view:self];
  DCHECK(secondaryToolbarNoFullscreenGuide);
  CGFloat maxBottomPadding =
      MAX(secondaryToolbarNoFullscreenGuide.layoutFrame.size.height,
          self.safeAreaInsets.bottom);

  // Calculate the appropriate bottom padding given the fullscreen progress.
  // Bottom padding can range from the negative value of the infobar's height
  // in fullscreen mode (i.e., progress == 0), thus hiding the infobar, to the
  // maximum bottom padding calculated above.
  CGFloat bottomPadding =
      progress * (maxBottomPadding + kInfobarHeight) - kInfobarHeight;

  // If the fullscreen progress is greater than the previous progress, i.e., we
  // are exiting the fullscreen mode, update the bottom padding only if the
  // calculated value is greater than the current bottom padding. Otherwise,
  // the infobar will initially hide and then start to fully appear again.
  self.bottomAnchorConstraint.constant =
      (progress > self.previousFullscreenProgress)
          ? MAX(bottomPadding, self.bottomAnchorConstraint.constant)
          : bottomPadding;

  self.previousFullscreenProgress = progress;
}

#pragma mark - TranslateInfobarLanguageTabStripViewDelegate

- (void)translateInfobarTabStripViewDidTapSourceLangugage:
    (TranslateInfobarLanguageTabStripView*)sender {
  [self.delegate translateInfobarViewDidTapSourceLangugage:self];
}

- (void)translateInfobarTabStripViewDidTapTargetLangugage:
    (TranslateInfobarLanguageTabStripView*)sender {
  [self.delegate translateInfobarViewDidTapTargetLangugage:self];
}

#pragma mark - Private

- (void)setupSubviews {
  [self setAccessibilityViewIsModal:YES];
  if (IsUIRefreshPhase1Enabled()) {
    self.backgroundColor = UIColorFromRGB(kInfobarBackgroundColor);
  } else {
    self.backgroundColor = [UIColor whiteColor];
  }
  id<LayoutGuideProvider> safeAreaLayoutGuide = self.safeAreaLayoutGuide;

  // The Content view. Holds all the other subviews.
  UIView* contentView = [[UIView alloc] init];
  contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:contentView];
  self.bottomAnchorConstraint =
      [self.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor];
  [NSLayoutConstraint activateConstraints:@[
    [safeAreaLayoutGuide.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor],
    [safeAreaLayoutGuide.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor],
    [self.topAnchor constraintEqualToAnchor:contentView.topAnchor],
    self.bottomAnchorConstraint
  ]];

  UIImage* icon = [[UIImage imageNamed:@"translate_icon"]
      resizableImageWithCapInsets:UIEdgeInsetsZero
                     resizingMode:UIImageResizingModeStretch];
  UIImageView* iconView = [[UIImageView alloc] initWithImage:icon];
  self.iconView = iconView;
  self.iconView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:self.iconView];

  TranslateInfobarLanguageTabStripView* languagesView =
      [[TranslateInfobarLanguageTabStripView alloc] init];
  self.languagesView = languagesView;
  self.languagesView.translatesAutoresizingMaskIntoConstraints = NO;
  self.languagesView.sourceLanguage = self.sourceLanguage;
  self.languagesView.sourceLanguageTabState =
      [self sourceLanguageTabStateFromTranslateInfobarViewState:self.state];
  self.languagesView.targetLanguage = self.targetLanguage;
  self.languagesView.targetLanguageTabState =
      [self targetLanguageTabStateFromTranslateInfobarViewState:self.state];
  self.languagesView.delegate = self;
  [contentView addSubview:self.languagesView];

  self.toolbarConfiguration =
      [[ToolbarConfiguration alloc] initWithStyle:NORMAL];

  ToolbarButton* optionsButton =
      [self toolbarButtonWithImageNamed:@"translate_options"
                                 target:self
                                 action:@selector(showOptions)];
  self.optionsButton = optionsButton;
  [contentView addSubview:self.optionsButton];

  ToolbarButton* dismissButton =
      [self toolbarButtonWithImageNamed:@"translate_dismiss"
                                 target:self
                                 action:@selector(dismiss)];
  self.dismissButton = dismissButton;
  [contentView addSubview:self.dismissButton];

  ApplyVisualConstraintsWithMetrics(
      @[
        @"H:|-(iconLeadingMargin)-[icon(iconSize)]-(iconTrailingMargin)-[languages][options(buttonSize)][dismiss(buttonSize)]|",
        @"V:|[languages(infobarHeight)]|",
        @"V:[icon(iconSize)]",
        @"V:[options(buttonSize)]",
        @"V:[dismiss(buttonSize)]",
      ],
      @{
        @"icon" : self.iconView,
        @"languages" : self.languagesView,
        @"options" : self.optionsButton,
        @"dismiss" : self.dismissButton,
      },
      @{
        @"iconSize" : @(kIconSize),
        @"iconLeadingMargin" : @(kIconLeadingMargin),
        @"iconTrailingMargin" : @(kIconTrailingMargin),
        @"infobarHeight" : @(kInfobarHeight),
        @"buttonSize" : @(kButtonSize),
      });
  AddSameCenterYConstraint(contentView, self.iconView);
  AddSameCenterYConstraint(contentView, self.optionsButton);
  AddSameCenterYConstraint(contentView, self.dismissButton);
}

// Returns the source language tab view state for the given infobar view state.
- (TranslateInfobarLanguageTabViewState)
    sourceLanguageTabStateFromTranslateInfobarViewState:
        (TranslateInfobarViewState)state {
  switch (state) {
    case TranslateInfobarViewStateBeforeTranslate:
      return TranslateInfobarLanguageTabViewStateSelected;
    case TranslateInfobarViewStateTranslating:
      return TranslateInfobarLanguageTabViewStateDefault;
    case TranslateInfobarViewStateAfterTranslate:
      return TranslateInfobarLanguageTabViewStateDefault;
  }
}

// Returns the target language tab view state for the given infobar view state.
- (TranslateInfobarLanguageTabViewState)
    targetLanguageTabStateFromTranslateInfobarViewState:
        (TranslateInfobarViewState)state {
  switch (state) {
    case TranslateInfobarViewStateBeforeTranslate:
      return TranslateInfobarLanguageTabViewStateDefault;
    case TranslateInfobarViewStateTranslating:
      return TranslateInfobarLanguageTabViewStateLoading;
    case TranslateInfobarViewStateAfterTranslate:
      return TranslateInfobarLanguageTabViewStateSelected;
  }
}

- (ToolbarButton*)toolbarButtonWithImageNamed:(NSString*)name
                                       target:(id)target
                                       action:(SEL)action {
  UIImage* image = [[UIImage imageNamed:name]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  ToolbarButton* button = [ToolbarButton toolbarButtonWithImage:image];
  [button addTarget:target
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  const CGFloat kButtonPadding = (kButtonSize - kIconSize) / 2;
  button.contentEdgeInsets = UIEdgeInsetsMake(kButtonPadding, kButtonPadding,
                                              kButtonPadding, kButtonPadding);
  button.configuration = self.toolbarConfiguration;
  return button;
}

- (void)showOptions {
  [self.delegate translateInfobarViewDidTapOptions:self];
}

- (void)dismiss {
  [self.delegate translateInfobarViewDidTapDismiss:self];
}

@end
