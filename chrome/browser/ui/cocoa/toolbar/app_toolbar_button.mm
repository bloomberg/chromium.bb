// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/toolbar/app_toolbar_button.h"

#include "base/macros.h"
#include "chrome/app/vector_icons/vector_icons.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"

@interface AppToolbarButton ()
- (void)commonInit;
@end

@implementation AppToolbarButton

- (instancetype)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    [self commonInit];
  }
  return self;
}

- (void)awakeFromNib {
  [self commonInit];
}

- (void)commonInit {
  view_id_util::SetID(self, VIEW_ID_APP_MENU);
  severity_ = AppMenuIconController::Severity::NONE;
  type_ = AppMenuIconController::IconType::NONE;
}

- (const gfx::VectorIcon*)vectorIcon {
  switch (type_) {
    case AppMenuIconController::IconType::NONE:
      DCHECK_EQ(severity_, AppMenuIconController::Severity::NONE);
      return &kBrowserToolsIcon;
    case AppMenuIconController::IconType::UPGRADE_NOTIFICATION:
      return &kBrowserToolsUpdateIcon;
    case AppMenuIconController::IconType::GLOBAL_ERROR:
    case AppMenuIconController::IconType::INCOMPATIBILITY_WARNING:
      return &kBrowserToolsErrorIcon;
  }

  return nullptr;
}

- (SkColor)vectorIconColor:(BOOL)themeIsDark {
  const ui::ThemeProvider* provider = [[self window] themeProvider];
  switch (severity_) {
    case AppMenuIconController::Severity::NONE:
      return themeIsDark ? SK_ColorWHITE
                         : (provider && provider->ShouldIncreaseContrast()
                                ? SK_ColorBLACK
                                : gfx::kChromeIconGrey);
      break;

    case AppMenuIconController::Severity::LOW:
      return themeIsDark ? gfx::kGoogleGreen300 : gfx::kGoogleGreen700;
      break;

    case AppMenuIconController::Severity::MEDIUM:
      return themeIsDark ? gfx::kGoogleYellow300 : gfx::kGoogleYellow700;
      break;

    case AppMenuIconController::Severity::HIGH:
      return themeIsDark ? gfx::kGoogleRed300 : gfx::kGoogleRed700;
      break;

    default:
      break;
  }
}

- (void)setSeverity:(AppMenuIconController::Severity)severity
           iconType:(AppMenuIconController::IconType)type
      shouldAnimate:(BOOL)shouldAnimate {
  if (severity != severity_ || type != type_) {
    severity_ = severity;
    type_ = type;
    // Update the button state images with the new severity color or icon type.
    [self resetButtonStateImages];
  }
}

@end
