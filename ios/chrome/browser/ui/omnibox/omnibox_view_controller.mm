// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_view_controller.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_container_view.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Font size used in the omnibox.
const CGFloat kFontSize = 19.0f;

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
  UIColor* textFieldTintColor =
      self.incognito ? [UIColor whiteColor] : [UIColor blackColor];
  UIColor* iconTintColor = self.incognito
                               ? [UIColor whiteColor]
                               : [UIColor colorWithWhite:0 alpha:0.7];

  self.view = [[OmniboxContainerView alloc]
      initWithFrame:CGRectZero
               font:[UIFont systemFontOfSize:kFontSize]
          textColor:textColor
      textFieldTint:textFieldTintColor
           iconTint:iconTintColor];
  self.view.incognito = self.incognito;

  SetA11yLabelAndUiAutomationName(self.textField, IDS_ACCNAME_LOCATION,
                                  @"Address");
}

- (void)viewDidLoad {
  [super viewDidLoad];
  if (self.incognito) {
    self.textField.placeholderTextColor = [UIColor colorWithWhite:1 alpha:0.5];
  } else {
    self.textField.placeholderTextColor = [UIColor colorWithWhite:0 alpha:0.3];
  }
  self.textField.placeholder = l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
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

@end
