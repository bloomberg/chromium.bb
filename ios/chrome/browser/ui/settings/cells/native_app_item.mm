// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/native_app_item.h"

#import <QuartzCore/QuartzCore.h>

#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;

// Icon rounded corner radius.
const CGFloat kIconCornerRadius = 5.088;

// Icon size.
const CGFloat kIconSize = 29;

// Placeholder icon for native app cells.
UIImage* PlaceholderIcon() {
  return [UIImage imageNamed:@"app_icon_placeholder"];
}

}  // namespace

@implementation NativeAppItem

@synthesize name = _name;
@synthesize icon = _icon;
@synthesize state = _state;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [NativeAppCell class];
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureCell:(NativeAppCell*)cell {
  [super configureCell:cell];
  cell.nameLabel.text = self.name;
  if (self.icon) {
    cell.iconImageView.image = self.icon;
  }
  [cell updateWithState:self.state];
}

@end

@implementation NativeAppCell

@synthesize nameLabel = _nameLabel;
@synthesize iconImageView = _iconImageView;
@synthesize switchControl = _switchControl;
@synthesize installButton = _installButton;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    UIView* contentView = self.contentView;
    self.isAccessibilityElement = YES;

    _iconImageView = [[UIImageView alloc] init];
    _iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _iconImageView.image = PlaceholderIcon();
    _iconImageView.layer.cornerRadius = kIconCornerRadius;
    _iconImageView.layer.masksToBounds = YES;
    self.layer.shouldRasterize = YES;
    self.layer.rasterizationScale = [[UIScreen mainScreen] scale];
    [contentView addSubview:_iconImageView];

    _nameLabel = [[UILabel alloc] init];
    _nameLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_nameLabel];

    _nameLabel.font =
        [[MDFRobotoFontLoader sharedInstance] mediumFontOfSize:14];
    _nameLabel.textColor = [[MDCPalette greyPalette] tint900];

    _switchControl = [[UISwitch alloc] init];
    _switchControl.onTintColor = [[MDCPalette cr_bluePalette] tint500];

    _installButton = [[MDCFlatButton alloc] init];
    _installButton.translatesAutoresizingMaskIntoConstraints = NO;
    _installButton.customTitleColor = [[MDCPalette cr_bluePalette] tint500];
    [_installButton
        setTitle:l10n_util::GetNSString(IDS_IOS_GOOGLE_APPS_INSTALL_BUTTON)
        forState:UIControlStateNormal];
    [_installButton setTitle:@"" forState:UIControlStateDisabled];
    _installButton.accessibilityHint = l10n_util::GetNSString(
        IDS_IOS_GOOGLE_APPS_INSTALL_BUTTON_ACCESSIBILITY_HINT);

    // Set up the constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_iconImageView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_iconImageView.trailingAnchor
          constraintEqualToAnchor:_nameLabel.leadingAnchor
                         constant:-kHorizontalPadding],
      [_iconImageView.widthAnchor constraintEqualToConstant:kIconSize],
      [_iconImageView.heightAnchor constraintEqualToConstant:kIconSize],
      [_nameLabel.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalPadding],
      [_iconImageView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_nameLabel.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_installButton.widthAnchor constraintGreaterThanOrEqualToConstant:79],
      [_installButton.widthAnchor constraintLessThanOrEqualToConstant:127],
    ]];
  }
  return self;
}

- (void)updateWithState:(NativeAppItemState)state {
  switch (state) {
    case NativeAppItemSwitchOn:
      self.switchControl.on = YES;
      self.accessoryView = self.switchControl;
      break;
    case NativeAppItemSwitchOff:
      self.switchControl.on = NO;
      self.accessoryView = self.switchControl;
      break;
    case NativeAppItemInstall:
      self.accessoryView = self.installButton;
      break;
  }
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.switchControl removeTarget:nil
                            action:nil
                  forControlEvents:[self.switchControl allControlEvents]];
  self.switchControl.tag = 0;
  [self.installButton removeTarget:nil
                            action:nil
                  forControlEvents:[self.installButton allControlEvents]];
  self.iconImageView.image = PlaceholderIcon();
  self.iconImageView.tag = 0;
}

#pragma mark - UIAccessibility

- (CGPoint)accessibilityActivationPoint {
  // Activate over the accessory view.
  CGRect accessoryViewFrame = UIAccessibilityConvertFrameToScreenCoordinates(
      [self.accessoryView frame], self);
  return CGPointMake(CGRectGetMidX(accessoryViewFrame),
                     CGRectGetMidY(accessoryViewFrame));
}

- (NSString*)accessibilityHint {
  return [self.accessoryView accessibilityHint];
}

- (NSString*)accessibilityValue {
  if (self.accessoryView == self.switchControl) {
    if (self.switchControl.on) {
      return [NSString
          stringWithFormat:@"%@, %@", self.nameLabel.text,
                           l10n_util::GetNSString(IDS_IOS_SETTING_ON)];
    } else {
      return [NSString
          stringWithFormat:@"%@, %@", self.nameLabel.text,
                           l10n_util::GetNSString(IDS_IOS_SETTING_OFF)];
    }
  }

  return [NSString stringWithFormat:@"%@, %@", self.nameLabel.text,
                                    l10n_util::GetNSString(
                                        IDS_IOS_GOOGLE_APPS_INSTALL_BUTTON)];
}

@end
