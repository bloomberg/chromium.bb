// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_view_controller.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_container_view.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_constants.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kClearButtonSize = 28.0f;

}  // namespace

@interface OmniboxViewController ()

// Override of UIViewController's view with a different type.
@property(nonatomic, strong) OmniboxContainerView* view;

@property(nonatomic, assign) BOOL incognito;

@end

@implementation OmniboxViewController
@synthesize incognito = _incognito;
@dynamic view;

- (instancetype)initWithIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    _incognito = isIncognito;
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  UIColor* textColor = self.incognito ? [UIColor whiteColor]
                                      : [UIColor colorWithWhite:0 alpha:0.7];
  UIColor* textFieldTintColor = self.incognito
                                    ? [UIColor whiteColor]
                                    : UIColorFromRGB(kLocationBarTintBlue);
  UIColor* iconTintColor = self.incognito
                               ? [UIColor whiteColor]
                               : [UIColor colorWithWhite:0 alpha:0.7];

  self.view = [[OmniboxContainerView alloc]
      initWithFrame:CGRectZero
               font:[UIFont systemFontOfSize:kLocationBarFontSize]
         largerFont:[UIFont systemFontOfSize:kLocationBarRegularRegularFontSize]
          textColor:textColor
      textFieldTint:textFieldTintColor
           iconTint:iconTintColor];
  self.view.incognito = self.incognito;

  SetA11yLabelAndUiAutomationName(self.textField, IDS_ACCNAME_LOCATION,
                                  @"Address");
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.textField.placeholderTextColor = [self placeholderAndClearButtonColor];
  self.textField.placeholder = l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  [self setupClearButton];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateLeadingImageVisibility];
}

#pragma mark - public methods

- (OmniboxTextFieldIOS*)textField {
  return self.view.textField;
}

#pragma mark - OmniboxConsumer

- (void)updateAutocompleteIcon:(UIImage*)icon {
  [self.view setLeadingImage:icon];
}

#pragma mark - private

- (void)updateLeadingImageVisibility {
  [self.view setLeadingImageHidden:!IsRegularXRegularSizeClass(self)];
}

// Tint color for the textfield placeholder and the clear button.
- (UIColor*)placeholderAndClearButtonColor {
  return self.incognito ? [UIColor colorWithWhite:1 alpha:0.5]
                        : [UIColor colorWithWhite:0 alpha:0.3];
}

#pragma mark clear button

// Omnibox uses a custom clear button. It has a custom tint and image, but
// otherwise it should act exactly like a system button. To achieve this, a
// custom button is used as the |rightView|. Textfield's setRightViewMode: is
// used to make the button invisible when the textfield is empty; the visibility
// is updated on textfield text changes and clear button presses.
- (void)setupClearButton {
  // Do not use the system clear button. Use a custom "right view" instead.
  // Note that |rightView| is an incorrect name, it's really a trailing view.
  [self.textField setClearButtonMode:UITextFieldViewModeNever];
  [self.textField setRightViewMode:UITextFieldViewModeAlways];

  UIButton* clearButton = [UIButton buttonWithType:UIButtonTypeSystem];
  clearButton.frame = CGRectMake(0, 0, kClearButtonSize, kClearButtonSize);
  [clearButton setImage:[self clearButtonIcon] forState:UIControlStateNormal];
  [clearButton addTarget:self
                  action:@selector(clearButtonPressed)
        forControlEvents:UIControlEventTouchUpInside];
  self.textField.rightView = clearButton;

  clearButton.tintColor = [self placeholderAndClearButtonColor];
  SetA11yLabelAndUiAutomationName(clearButton, IDS_IOS_ACCNAME_CLEAR_TEXT,
                                  @"Clear Text");

  // Observe text changes to show the clear button when there is text and hide
  // it when the textfield is empty.
  [self.textField addTarget:self
                     action:@selector(textFieldDidChange:)
           forControlEvents:UIControlEventEditingChanged];
}

- (UIImage*)clearButtonIcon {
  UIImage* image = [[UIImage imageNamed:@"omnibox_clear_icon"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

  return image;
}

- (void)clearButtonPressed {
  // Emulate a system button clear callback.
  BOOL shouldClear =
      [self.textField.delegate textFieldShouldClear:self.textField];
  if (shouldClear) {
    [self.textField setText:@""];
    // Calling setText: does not trigger UIControlEventEditingChanged, so update
    // the clear button visibility manually.
    [self updateClearButtonVisibility];
  }
}

// Called on textField's UIControlEventEditingChanged.
- (void)textFieldDidChange:(UITextField*)textField {
  [self updateClearButtonVisibility];
}

// Hides the clear button if the textfield is empty; shows it otherwise.
- (void)updateClearButtonVisibility {
  BOOL hasText = self.textField.text.length > 0;
  [self.textField setRightViewMode:hasText ? UITextFieldViewModeAlways
                                           : UITextFieldViewModeNever];
}

@end
