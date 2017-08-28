// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/md_hover_button.h"

#import <QuartzCore/QuartzCore.h>

#import "chrome/browser/ui/cocoa/themed_window.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#import "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {
constexpr int kDefaultIconSize = 16;
constexpr CGFloat kHoverAnimationDuration = 0.25;

NSColor* GetHoveringColor(BOOL dark_theme) {
  return [NSColor colorWithWhite:dark_theme ? 1 : 0 alpha:0.08];
}

NSColor* GetActiveColor(BOOL dark_theme) {
  return [NSColor colorWithWhite:dark_theme ? 1 : 0 alpha:0.12];
}

}  // namespace

@implementation MDHoverButton {
  const gfx::VectorIcon* icon_;
}
@synthesize icon = icon_;
@synthesize iconSize = iconSize_;
@synthesize hoverSuppressed = hoverSuppressed_;

- (instancetype)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    iconSize_ = kDefaultIconSize;
    self.bezelStyle = NSRoundedBezelStyle;
    self.bordered = NO;
    self.wantsLayer = YES;
    self.layer.cornerRadius = 2;
  }
  return self;
}

- (void)setIcon:(const gfx::VectorIcon*)icon {
  icon_ = icon;
  self.needsDisplay = YES;
}

- (void)setIconSize:(int)iconSize {
  iconSize_ = iconSize;
  self.needsDisplay = YES;
}

- (void)setHoverSuppressed:(BOOL)hoverSuppressed {
  hoverSuppressed_ = hoverSuppressed;
  [self updateHoverButtonAppearanceAnimated:YES];
}

- (SkColor)iconColor {
  const ui::ThemeProvider* provider = [[self window] themeProvider];
  return [[self window] hasDarkTheme]
             ? SK_ColorWHITE
             : provider ? provider->ShouldIncreaseContrast()
                              ? SK_ColorBLACK
                              : gfx::kChromeIconGrey
                        : gfx::kPlaceholderColor;
}

- (void)updateHoverButtonAppearanceAnimated:(BOOL)animated {
  const BOOL darkTheme = [[self window] hasDarkTheme];
  CGColorRef targetBackgroundColor = nil;
  if (!hoverSuppressed_) {
    switch (self.hoverState) {
      case kHoverStateMouseDown:
        targetBackgroundColor = GetActiveColor(darkTheme).CGColor;
      case kHoverStateMouseOver:
        targetBackgroundColor = GetHoveringColor(darkTheme).CGColor;
      case kHoverStateNone:
        break;
    }
  }
  if (CGColorEqualToColor(targetBackgroundColor, self.layer.backgroundColor)) {
    return;
  }
  if (!animated) {
    [self.layer removeAnimationForKey:@"hoverButtonAppearance"];
    self.layer.backgroundColor = targetBackgroundColor;
    return;
  }
  [NSAnimationContext runAnimationGroup:^(NSAnimationContext* context) {
    context.duration = kHoverAnimationDuration;
    CABasicAnimation* animation =
        [CABasicAnimation animationWithKeyPath:@"backgroundColor"];
    self.layer.backgroundColor = targetBackgroundColor;
    [self.layer addAnimation:animation forKey:@"hoverButtonAppearance"];
  }
                      completionHandler:nil];
}

// HoverButton overrides.

- (void)setHoverState:(HoverState)state {
  if (state == hoverState_)
    return;
  const BOOL animated =
      state != kHoverStateMouseDown && hoverState_ != kHoverStateMouseDown;
  [super setHoverState:state];
  [self updateHoverButtonAppearanceAnimated:animated];
}

// NSView overrides.

- (void)viewWillDraw {
  if (!icon_ || icon_->is_empty() || iconSize_ == 0) {
    self.image = nil;
    return;
  }
  const SkColor iconColor =
      color_utils::DeriveDefaultIconColor([self iconColor]);
  self.image =
      NSImageFromImageSkia(gfx::CreateVectorIcon(*icon_, iconSize_, iconColor));
  [super viewWillDraw];
}

- (void)drawFocusRingMask {
  CGFloat radius = self.layer.cornerRadius;
  [[NSBezierPath bezierPathWithRoundedRect:self.bounds
                                   xRadius:radius
                                   yRadius:radius] fill];
}

@end
