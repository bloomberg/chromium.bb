// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/search_widget_extension/search_widget_view.h"
#include "base/ios/ios_util.h"
#include "base/logging.h"
#import "ios/chrome/search_widget_extension/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kContentMargin = 16;
const CGFloat kURLButtonMargin = 10;
const CGFloat kActionButtonSize = 55;
const CGFloat kIconSize = 35;
const CGFloat kMaxContentSize = 421;
const CGFloat kIconSpacing = 5;

}  // namespace

@interface SearchWidgetView ()

// The target for actions in the view.
@property(nonatomic, weak) id<SearchWidgetViewActionTarget> target;
// The copied URL label containing the URL or a placeholder text.
@property(nonatomic, strong) UILabel* copiedURLLabel;
// The copued URL title label containing the title of the copied URL button.
@property(nonatomic, strong) UILabel* openCopiedURLTitleLabel;
// The hairline view shown between the action and copied URL views.
@property(nonatomic, strong) UIView* hairlineView;
// The button shown when there is a copied URL to open.
@property(nonatomic, strong) UIButton* copiedButtonView;
// The primary effect view of the widget. Add views here for a more opaque
// appearance.
@property(nonatomic, strong) UIVisualEffectView* primaryEffectView;
// The secondary effect view of the widget. Add views here for a more
// transparent appearance.
@property(nonatomic, strong) UIVisualEffectView* secondaryEffectView;
// The constraints to be activated when the copiedURL section is visible.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* visibleCopiedURLConstraints;
// The constraints to be activated when the copiedURL section is hidden.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* hiddenCopiedURLConstraints;

// Sets up the widget UI.
- (void)createUI;

// Creates the view for the action buttons.
- (UIView*)newActionsView;

// Creates the view for the copiedURL section.
- (void)initializeOpenCopiedURLSectionUsingTopAnchor:(NSLayoutAnchor*)topAnchor;

@end

@implementation SearchWidgetView

@synthesize target = _target;
@synthesize copiedURLVisible = _copiedURLVisible;
@synthesize copiedURLString = _copiedURLString;
@synthesize copiedURLLabel = _copiedURLLabel;
@synthesize openCopiedURLTitleLabel = _openCopiedURLTitleLabel;
@synthesize hairlineView = _hairlineView;
@synthesize copiedButtonView = _copiedButtonView;
@synthesize primaryEffectView = _primaryEffectView;
@synthesize secondaryEffectView = _secondaryEffectView;
@synthesize visibleCopiedURLConstraints = _visibleCopiedURLConstraints;
@synthesize hiddenCopiedURLConstraints = _hiddenCopiedURLConstraints;

- (instancetype)initWithActionTarget:(id<SearchWidgetViewActionTarget>)target
               primaryVibrancyEffect:(UIVibrancyEffect*)primaryVibrancyEffect
             secondaryVibrancyEffect:
                 (UIVibrancyEffect*)secondaryVibrancyEffect {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(target);
    _target = target;
    _primaryEffectView =
        [[UIVisualEffectView alloc] initWithEffect:primaryVibrancyEffect];
    _secondaryEffectView =
        [[UIVisualEffectView alloc] initWithEffect:secondaryVibrancyEffect];
    _copiedURLVisible = YES;
    [self createUI];
    [self updateCopiedURLUI];
  }
  return self;
}

#pragma mark - property overrides

- (void)setCopiedURLVisible:(BOOL)copiedURLVisible {
  _copiedURLVisible = copiedURLVisible;
  [self updateCopiedURLUI];
}

- (void)setCopiedURLString:(NSString*)copiedURL {
  _copiedURLString = copiedURL;
  [self updateCopiedURLUI];
}

#pragma mark - UI creation

- (void)createUI {
  for (UIVisualEffectView* effectView in
       @[ self.primaryEffectView, self.secondaryEffectView ]) {
    [self addSubview:effectView];
    effectView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint
        activateConstraints:ui_util::CreateSameConstraints(self, effectView)];
  }

  UIView* actionsView = [self newActionsView];
  [self initializeOpenCopiedURLSectionUsingTopAnchor:actionsView.bottomAnchor];
}

- (UIView*)newActionsView {
  // The use of vibrancy effects requires that the icons and circular buttons be
  // added to different parent views. This means that the constraints that
  // position them cannot be activated until all of the buttons are added to the
  // stack view that contains them, and that stack view itself is added to the
  // view hierarchy. In order to manage this, |constraints| is passed into each
  // invocation of the button creation method, so the constraints can be
  // collected for activation once the view hierarchy is complete.
  NSMutableArray<NSLayoutConstraint*>* constraints = [NSMutableArray array];

  UIStackView* actionRow = [[UIStackView alloc] initWithArrangedSubviews:@[
    [self newActionViewWithTitle:NSLocalizedString(@"IDS_IOS_NEW_SEARCH",
                                                   @"New Search")
                       imageName:@"quick_action_search"
                  actionSelector:@selector(openSearch:)
                     constraints:constraints],
    [self newActionViewWithTitle:NSLocalizedString(@"IDS_IOS_INCOGNITO_SEARCH",
                                                   @"Incognito Search")
                       imageName:@"quick_action_incognito_search"
                  actionSelector:@selector(openIncognito:)
                     constraints:constraints],
    [self newActionViewWithTitle:NSLocalizedString(@"IDS_IOS_VOICE_SEARCH",
                                                   @"Voice Search")
                       imageName:@"quick_action_voice_search"
                  actionSelector:@selector(openVoice:)
                     constraints:constraints],
    [self newActionViewWithTitle:NSLocalizedString(@"IDS_IOS_SCAN_QR_CODE",
                                                   @"Scan QR Code")
                       imageName:@"quick_action_camera_search"
                  actionSelector:@selector(openQRCode:)
                     constraints:constraints],
  ]];

  actionRow.axis = UILayoutConstraintAxisHorizontal;
  actionRow.alignment = UIStackViewAlignmentTop;
  actionRow.distribution = UIStackViewDistributionFillEqually;
  actionRow.spacing = kIconSpacing;
  actionRow.layoutMargins =
      UIEdgeInsetsMake(0, kContentMargin, 0, kContentMargin);
  actionRow.layoutMarginsRelativeArrangement = YES;
  actionRow.translatesAutoresizingMaskIntoConstraints = NO;

  [self.secondaryEffectView.contentView addSubview:actionRow];

  // These constraints stretch the action row to the full width of the widget.
  // Their priority is < UILayoutPriorityRequired so that they can break when
  // the view is larger than kMaxContentSize.
  NSLayoutConstraint* actionsLeftConstraint = [actionRow.leftAnchor
      constraintEqualToAnchor:self.secondaryEffectView.leftAnchor];
  actionsLeftConstraint.priority = UILayoutPriorityDefaultHigh;

  NSLayoutConstraint* actionsRightConstraint = [actionRow.rightAnchor
      constraintEqualToAnchor:self.secondaryEffectView.rightAnchor];
  actionsRightConstraint.priority = UILayoutPriorityDefaultHigh;

  // This constraint sets the top alignment for the action row. Its priority is
  // < UILayoutPriorityRequired so that it can break in favor of the
  // centerYAnchor rule (on the next line) when the copiedURL section is hidden.
  NSLayoutConstraint* actionsTopConstraint = [actionRow.topAnchor
      constraintEqualToAnchor:self.secondaryEffectView.topAnchor
                     constant:kContentMargin];
  actionsTopConstraint.priority = UILayoutPriorityDefaultHigh;

  self.hiddenCopiedURLConstraints = @[ [actionRow.centerYAnchor
      constraintEqualToAnchor:self.secondaryEffectView.centerYAnchor] ];

  [constraints addObjectsFromArray:@[
    [actionRow.centerXAnchor
        constraintEqualToAnchor:self.secondaryEffectView.centerXAnchor],
    [actionRow.widthAnchor constraintLessThanOrEqualToConstant:kMaxContentSize],
    actionsLeftConstraint,
    actionsRightConstraint,
    actionsTopConstraint,
  ]];

  [NSLayoutConstraint activateConstraints:constraints];

  return actionRow;
}

- (UIView*)newActionViewWithTitle:(NSString*)title
                        imageName:(NSString*)imageName
                   actionSelector:(SEL)actionSelector
                      constraints:
                          (NSMutableArray<NSLayoutConstraint*>*)constraints {
  UIView* circleView = [[UIView alloc] initWithFrame:CGRectZero];
  circleView.backgroundColor = base::ios::IsRunningOnIOS10OrLater()
                                   ? [UIColor colorWithWhite:0 alpha:0.05]
                                   : [UIColor whiteColor];
  circleView.layer.cornerRadius = kActionButtonSize / 2;

  [constraints addObjectsFromArray:@[
    [circleView.widthAnchor constraintEqualToConstant:kActionButtonSize],
    [circleView.heightAnchor constraintEqualToConstant:kActionButtonSize]
  ]];

  UILabel* labelView = [[UILabel alloc] initWithFrame:CGRectZero];
  labelView.text = title;
  labelView.numberOfLines = 0;
  labelView.textAlignment = NSTextAlignmentCenter;
  labelView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  labelView.isAccessibilityElement = NO;
  [labelView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];

  UIStackView* stack =
      [[UIStackView alloc] initWithArrangedSubviews:@[ circleView, labelView ]];
  stack.axis = UILayoutConstraintAxisVertical;
  stack.spacing = kIconSpacing;
  stack.alignment = UIStackViewAlignmentCenter;
  stack.translatesAutoresizingMaskIntoConstraints = NO;

  // A transparent button constrained to the same size as the stack is added to
  // handle taps on the stack view.
  UIButton* actionButton = [[UIButton alloc] initWithFrame:CGRectZero];
  actionButton.backgroundColor = [UIColor clearColor];
  [actionButton addTarget:self.target
                   action:actionSelector
         forControlEvents:UIControlEventTouchUpInside];
  actionButton.translatesAutoresizingMaskIntoConstraints = NO;
  actionButton.accessibilityLabel = title;
  [self addSubview:actionButton];
  [constraints
      addObjectsFromArray:ui_util::CreateSameConstraints(actionButton, stack)];

  UIImage* iconImage = [UIImage imageNamed:imageName];
  UIImageView* icon = [[UIImageView alloc] initWithImage:iconImage];
  icon.translatesAutoresizingMaskIntoConstraints = NO;

  [constraints addObjectsFromArray:@[
    [icon.widthAnchor constraintEqualToConstant:kIconSize],
    [icon.heightAnchor constraintEqualToConstant:kIconSize],
    [icon.centerXAnchor constraintEqualToAnchor:circleView.centerXAnchor],
    [icon.centerYAnchor constraintEqualToAnchor:circleView.centerYAnchor],
  ]];
  if (base::ios::IsRunningOnIOS10OrLater()) {
    [self.primaryEffectView.contentView addSubview:icon];
  } else {
    [self addSubview:icon];
  }
  return stack;
}

- (void)initializeOpenCopiedURLSectionUsingTopAnchor:
    (NSLayoutAnchor*)topAnchor {
  self.hairlineView = [[UIView alloc] initWithFrame:CGRectZero];
  self.hairlineView.backgroundColor =
      base::ios::IsRunningOnIOS10OrLater()
          ? [UIColor colorWithWhite:0 alpha:0.05]
          : [UIColor whiteColor];
  self.hairlineView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.secondaryEffectView.contentView addSubview:self.hairlineView];

  self.copiedButtonView = [[UIButton alloc] initWithFrame:CGRectZero];
  self.copiedButtonView.backgroundColor = [UIColor colorWithWhite:0 alpha:0.05];
  self.copiedButtonView.layer.cornerRadius = 5;
  self.copiedButtonView.translatesAutoresizingMaskIntoConstraints = NO;
  self.copiedButtonView.accessibilityLabel =
      NSLocalizedString(@"IDS_IOS_OPEN_COPIED_LINK", nil);
  [self.secondaryEffectView.contentView addSubview:self.copiedButtonView];
  [self.copiedButtonView addTarget:self.target
                            action:@selector(openCopiedURL:)
                  forControlEvents:UIControlEventTouchUpInside];

  self.openCopiedURLTitleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  self.openCopiedURLTitleLabel.textAlignment = NSTextAlignmentCenter;
  self.openCopiedURLTitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.openCopiedURLTitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  [self.primaryEffectView.contentView addSubview:self.openCopiedURLTitleLabel];

  self.copiedURLLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  self.copiedURLLabel.textAlignment = NSTextAlignmentCenter;
  self.copiedURLLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  self.copiedURLLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.secondaryEffectView.contentView addSubview:self.copiedURLLabel];

  self.visibleCopiedURLConstraints = @[
    [self.hairlineView.topAnchor constraintEqualToAnchor:topAnchor
                                                constant:kContentMargin],
    [self.hairlineView.leftAnchor
        constraintEqualToAnchor:self.secondaryEffectView.leftAnchor],
    [self.hairlineView.rightAnchor
        constraintEqualToAnchor:self.secondaryEffectView.rightAnchor],
    [self.hairlineView.heightAnchor constraintEqualToConstant:0.5],

    [self.copiedButtonView.centerXAnchor
        constraintEqualToAnchor:self.secondaryEffectView.centerXAnchor],
    [self.copiedButtonView.widthAnchor
        constraintEqualToAnchor:self.secondaryEffectView.widthAnchor
                       constant:-2 * kContentMargin],
    [self.copiedButtonView.topAnchor
        constraintEqualToAnchor:self.hairlineView.bottomAnchor
                       constant:12],
    [self.copiedButtonView.bottomAnchor
        constraintEqualToAnchor:self.secondaryEffectView.bottomAnchor
                       constant:-kContentMargin],

    [self.openCopiedURLTitleLabel.centerXAnchor
        constraintEqualToAnchor:self.primaryEffectView.centerXAnchor],
    [self.openCopiedURLTitleLabel.topAnchor
        constraintEqualToAnchor:self.copiedButtonView.topAnchor
                       constant:kURLButtonMargin],
    [self.openCopiedURLTitleLabel.widthAnchor
        constraintEqualToAnchor:self.copiedButtonView.widthAnchor
                       constant:-kContentMargin * 2],

    [self.copiedURLLabel.centerXAnchor
        constraintEqualToAnchor:self.primaryEffectView.centerXAnchor],
    [self.copiedURLLabel.topAnchor
        constraintEqualToAnchor:self.openCopiedURLTitleLabel.bottomAnchor],
    [self.copiedURLLabel.widthAnchor
        constraintEqualToAnchor:self.openCopiedURLTitleLabel.widthAnchor],
    [self.copiedURLLabel.bottomAnchor
        constraintEqualToAnchor:self.copiedButtonView.bottomAnchor
                       constant:-kURLButtonMargin],
  ];
}

- (void)addTapAction:(SEL)action toView:(UIView*)view {
  UIGestureRecognizer* tapRecognizer =
      [[UITapGestureRecognizer alloc] initWithTarget:self.target action:action];
  [view addGestureRecognizer:tapRecognizer];
}

- (void)updateCopiedURLUI {
  // If the copiedURL section is not visible, hide all the copiedURL section
  // views and activate the correct constraint set. If it is visible, show the
  // views in function of whether there is or not a copied URL to show.

  if (!self.copiedURLVisible) {
    self.copiedURLLabel.hidden = YES;
    self.openCopiedURLTitleLabel.hidden = YES;
    self.hairlineView.hidden = YES;
    self.copiedButtonView.hidden = YES;
    [NSLayoutConstraint deactivateConstraints:self.visibleCopiedURLConstraints];
    [NSLayoutConstraint activateConstraints:self.hiddenCopiedURLConstraints];
    return;
  }

  self.copiedURLLabel.hidden = NO;
  self.openCopiedURLTitleLabel.hidden = NO;

  [NSLayoutConstraint deactivateConstraints:self.hiddenCopiedURLConstraints];
  [NSLayoutConstraint activateConstraints:self.visibleCopiedURLConstraints];

  if (self.copiedURLString) {
    self.copiedButtonView.hidden = NO;
    self.hairlineView.hidden = YES;
    self.copiedURLLabel.text = self.copiedURLString;
    self.copiedURLLabel.accessibilityLabel = self.copiedURLString;
    self.openCopiedURLTitleLabel.alpha = 1;
    self.openCopiedURLTitleLabel.text =
        NSLocalizedString(@"IDS_IOS_OPEN_COPIED_LINK", nil);
    self.openCopiedURLTitleLabel.isAccessibilityElement = NO;
    self.copiedURLLabel.alpha = 1;
    return;
  }

  self.copiedButtonView.hidden = YES;
  self.hairlineView.hidden = NO;
  self.copiedURLLabel.text =
      NSLocalizedString(@"IDS_IOS_NO_COPIED_LINK_MESSAGE", nil);
  self.copiedURLLabel.accessibilityLabel =
      NSLocalizedString(@"IDS_IOS_NO_COPIED_LINK_MESSAGE", nil);
  self.openCopiedURLTitleLabel.text =
      NSLocalizedString(@"IDS_IOS_NO_COPIED_LINK_TITLE", nil);
  self.openCopiedURLTitleLabel.accessibilityLabel =
      NSLocalizedString(@"IDS_IOS_NO_COPIED_LINK_TITLE", nil);
  self.openCopiedURLTitleLabel.isAccessibilityElement = YES;

  if (base::ios::IsRunningOnIOS10OrLater()) {
    self.copiedURLLabel.alpha = 0.5;
    self.openCopiedURLTitleLabel.alpha = 0.5;
  }
}
@end
