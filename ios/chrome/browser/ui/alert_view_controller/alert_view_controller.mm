// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/alert_view_controller/alert_view_controller.h"

#import "ios/chrome/browser/ui/elements/gray_highlight_button.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The alpha of the black chrome behind the alert view.
constexpr CGFloat kBackgroundAlpha = 0.4;

// Properties of the alert shadow.
constexpr CGFloat kShadowOffsetX = 0;
constexpr CGFloat kShadowOffsetY = 15;
constexpr CGFloat kShadowRadius = 13;
constexpr float kShadowOpacity = 0.12;

// Properties of the alert view.
constexpr CGFloat kCornerRadius = 14;
constexpr CGFloat kAlertWidth = 270;
constexpr CGFloat kMinimumHeight = 30;
constexpr CGFloat kMinimumMargin = 4;

// Inset for the content in the alert view.
constexpr CGFloat kTitleInsetTop = 20;
constexpr CGFloat kTitleInsetLeading = 20;
constexpr CGFloat kTitleInsetBottom = 4;
constexpr CGFloat kTitleInsetTrailing = 20;
constexpr CGFloat kMessageInsetTop = 4;
constexpr CGFloat kMessageInsetLeading = 20;
constexpr CGFloat kMessageInsetBottom = 20;
constexpr CGFloat kMessageInsetTrailing = 20;
constexpr CGFloat kButtonInsetTop = 20;
constexpr CGFloat kButtonInsetLeading = 20;
constexpr CGFloat kButtonInsetBottom = 20;
constexpr CGFloat kButtonInsetTrailing = 20;

// Colors for the action buttons.
constexpr int kButtonTextDefaultColor = 0x0579ff;
constexpr int kButtonTextDestructiveColor = 0xdf322f;

}  // namespace

@interface AlertAction ()

@property(nonatomic, readwrite) NSString* title;
@property(nonatomic, readwrite) UIAlertActionStyle style;

// The unique identifier for the actions created.
@property(nonatomic) NSInteger uniqueIdentifier;

// Block to be called when this action is triggered.
@property(nonatomic, copy) void (^handler)(AlertAction* action);

@end

@implementation AlertAction

+ (instancetype)actionWithTitle:(NSString*)title
                          style:(UIAlertActionStyle)style
                        handler:(void (^)(AlertAction* action))handler {
  AlertAction* action = [[AlertAction alloc] init];
  static NSInteger actionIdentifier = 0;
  action.uniqueIdentifier = ++actionIdentifier;
  action.title = title;
  action.handler = handler;
  action.style = style;
  return action;
}

@end

@interface AlertViewController ()
@property(nonatomic, readwrite) NSArray<AlertAction*>* actions;
@property(nonatomic, readwrite) NSArray<UITextField*>* textFields;

// This maps UIButtons' tags with AlertActions' uniqueIdentifiers.
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, AlertAction*>* buttonAlertActionsDictionary;

// This is the view with the shadow, white background and round corners.
// Everything will be added here.
@property(nonatomic, strong) UIView* contentView;

@end

@implementation AlertViewController

@dynamic title;

- (void)addAction:(AlertAction*)action {
  if (!self.actions) {
    self.actions = @[ action ];
    return;
  }
  self.actions = [self.actions arrayByAddingObject:action];
}

- (void)addTextFieldWithConfigurationHandler:
    (void (^)(UITextField* textField))configurationHandler {
  // TODO(crbug.com/951303): Implement support.
}

- (void)loadView {
  [super loadView];
  self.view.backgroundColor =
      [[UIColor blackColor] colorWithAlphaComponent:kBackgroundAlpha];

  self.contentView = [[UIView alloc] init];
  self.contentView.clipsToBounds = YES;
  self.contentView.backgroundColor = [UIColor whiteColor];
  self.contentView.layer.cornerRadius = kCornerRadius;
  self.contentView.layer.shadowOffset =
      CGSizeMake(kShadowOffsetX, kShadowOffsetY);
  self.contentView.layer.shadowRadius = kShadowRadius;
  self.contentView.layer.shadowOpacity = kShadowOpacity;
  self.contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.contentView];
  [NSLayoutConstraint activateConstraints:@[
    // Centering
    [self.contentView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [self.contentView.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],

    // Width
    [self.contentView.widthAnchor constraintEqualToConstant:kAlertWidth],

    // Minimum Size
    [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kMinimumHeight],

    // Maximum Size
    [self.contentView.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                                 .topAnchor
                                    constant:kMinimumMargin],
    [self.contentView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .bottomAnchor
                                 constant:-kMinimumMargin],
    [self.contentView.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .trailingAnchor
                                 constant:-kMinimumMargin],
    [self.contentView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                                 .leadingAnchor
                                    constant:kMinimumMargin],

  ]];

  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:stackView];
  AddSameConstraints(stackView, self.contentView);

  if (self.title.length) {
    UILabel* titleLabel = [[UILabel alloc] init];
    titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    titleLabel.textAlignment = NSTextAlignmentCenter;
    titleLabel.text = self.title;
    titleLabel.translatesAutoresizingMaskIntoConstraints = NO;

    UIView* titleContainer = [[UIView alloc] init];
    [titleContainer addSubview:titleLabel];
    titleContainer.translatesAutoresizingMaskIntoConstraints = NO;
    [stackView addArrangedSubview:titleContainer];

    ChromeDirectionalEdgeInsets contentInsets =
        ChromeDirectionalEdgeInsetsMake(kTitleInsetTop, kTitleInsetLeading,
                                        kTitleInsetBottom, kTitleInsetTrailing);
    AddSameConstraintsWithInsets(titleLabel, titleContainer, contentInsets);
    AddSameConstraintsToSides(titleContainer, self.contentView,
                              LayoutSides::kTrailing | LayoutSides::kLeading);
  }

  if (self.message.length) {
    UILabel* messageLabel = [[UILabel alloc] init];
    messageLabel.numberOfLines = 0;
    messageLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    messageLabel.textAlignment = NSTextAlignmentCenter;
    messageLabel.text = self.message;
    messageLabel.translatesAutoresizingMaskIntoConstraints = NO;

    UIView* messageContainer = [[UIView alloc] init];
    [messageContainer addSubview:messageLabel];
    messageContainer.translatesAutoresizingMaskIntoConstraints = NO;
    [stackView addArrangedSubview:messageContainer];

    ChromeDirectionalEdgeInsets contentInsets = ChromeDirectionalEdgeInsetsMake(
        kMessageInsetTop, kMessageInsetLeading, kMessageInsetBottom,
        kMessageInsetTrailing);
    AddSameConstraintsWithInsets(messageLabel, messageContainer, contentInsets);
    AddSameConstraintsToSides(messageContainer, self.contentView,
                              LayoutSides::kTrailing | LayoutSides::kLeading);
  }

  self.buttonAlertActionsDictionary = [[NSMutableDictionary alloc] init];
  for (AlertAction* action in self.actions) {
    GrayHighlightButton* button = [[GrayHighlightButton alloc] init];
    UIFont* font = nil;
    UIColor* textColor = nil;
    if (action.style == UIAlertActionStyleDefault) {
      font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
      textColor = UIColorFromRGB(kButtonTextDefaultColor);
    } else if (action.style == UIAlertActionStyleCancel) {
      font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
      textColor = UIColorFromRGB(kButtonTextDefaultColor);
    } else {  // Style is UIAlertActionStyleDestructive
      font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
      textColor = UIColorFromRGB(kButtonTextDestructiveColor);
    }
    [button.titleLabel setFont:font];
    [button setTitleColor:textColor forState:UIControlStateNormal];
    [button setTitle:action.title forState:UIControlStateNormal];

    button.contentHorizontalAlignment =
        UIControlContentHorizontalAlignmentCenter;
    [button addTarget:self
                  action:@selector(didSelectActionForButton:)
        forControlEvents:UIControlEventTouchUpInside];
    button.translatesAutoresizingMaskIntoConstraints = NO;
    button.contentEdgeInsets =
        UIEdgeInsetsMake(kButtonInsetTop, kButtonInsetLeading,
                         kButtonInsetBottom, kButtonInsetTrailing);

    UIView* hairline = [[UIView alloc] init];
    hairline.backgroundColor = [UIColor lightGrayColor];
    hairline.translatesAutoresizingMaskIntoConstraints = NO;

    UIView* buttonContainer = [[UIView alloc] init];
    [buttonContainer addSubview:button];
    [buttonContainer addSubview:hairline];
    buttonContainer.translatesAutoresizingMaskIntoConstraints = NO;
    [stackView addArrangedSubview:buttonContainer];

    CGFloat pixelHeight = 1.0 / [UIScreen mainScreen].scale;
    [hairline.heightAnchor constraintEqualToConstant:pixelHeight].active = YES;
    AddSameConstraintsToSides(
        hairline, buttonContainer,
        (LayoutSides::kTrailing | LayoutSides::kTop | LayoutSides::kLeading));

    AddSameConstraints(button, buttonContainer);
    AddSameConstraintsToSides(buttonContainer, self.contentView,
                              LayoutSides::kTrailing | LayoutSides::kLeading);

    button.tag = action.uniqueIdentifier;
    self.buttonAlertActionsDictionary[@(action.uniqueIdentifier)] = action;
  }
}

#pragma mark - Private

- (void)didSelectActionForButton:(UIButton*)button {
  AlertAction* action = self.buttonAlertActionsDictionary[@(button.tag)];
  if (action.handler) {
    action.handler(action);
  }
}

@end
