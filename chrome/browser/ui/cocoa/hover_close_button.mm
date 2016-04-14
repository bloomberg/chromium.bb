// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/hover_close_button.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_view.h"
#include "chrome/grit/generated_resources.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMKeyValueAnimation.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/resources/grit/ui_resources.h"

namespace  {
const CGFloat kFramesPerSecond = 16; // Determined experimentally to look good.
const CGFloat kCloseAnimationDuration = 0.1;
const int kTabCloseButtonSize = 16;

// Strings that are used for all close buttons. Set up in +initialize.
NSString* gBasicAccessibilityTitle = nil;
NSString* gTooltip = nil;

// If this string is changed, the setter (currently setFadeOutValue:) must
// be changed as well to match.
NSString* const kFadeOutValueKeyPath = @"fadeOutValue";
}  // namespace

@interface HoverCloseButton ()

// Common initialization routine called from initWithFrame and awakeFromNib.
- (void)commonInit;

// Called by |fadeOutAnimation_| when animated value changes.
- (void)setFadeOutValue:(CGFloat)value;

// Gets the image for the given hover state.
- (NSImage*)imageForHoverState:(HoverState)hoverState;

@end

@implementation HoverCloseButton

+ (void)initialize {
  // Grab some strings that are used by all close buttons.
  if (!gBasicAccessibilityTitle) {
    gBasicAccessibilityTitle = [l10n_util::GetNSStringWithFixup(
        IDS_ACCNAME_CLOSE) copy];
  }
  if (!gTooltip)
    gTooltip = [l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_CLOSE_TAB) copy];
}

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    [self commonInit];
  }
  return self;
}

- (void)awakeFromNib {
  [super awakeFromNib];
  [self commonInit];
}

- (void)removeFromSuperview {
  // -stopAnimation will call the animationDidStop: delegate method
  // which will release our animation.
  [fadeOutAnimation_ stopAnimation];
  [super removeFromSuperview];
}

- (void)animationDidStop:(NSAnimation*)animation {
  DCHECK(animation == fadeOutAnimation_);
  [fadeOutAnimation_ setDelegate:nil];
  [fadeOutAnimation_ release];
  fadeOutAnimation_ = nil;
}

- (void)animationDidEnd:(NSAnimation*)animation {
  [self animationDidStop:animation];
}

- (void)drawRect:(NSRect)dirtyRect {
  NSImage* image = [self imageForHoverState:[self hoverState]];

  // Close boxes align left horizontally, and align center vertically.
  // http:crbug.com/14739 requires this.
  NSRect imageRect = NSZeroRect;
  imageRect.size = [image size];

  NSRect destRect = [self bounds];
  destRect.origin.y = floor((NSHeight(destRect) / 2)
                            - (NSHeight(imageRect) / 2));
  destRect.size = imageRect.size;

  switch(self.hoverState) {
    case kHoverStateMouseOver:
    case kHoverStateMouseDown:
      [image drawInRect:destRect
               fromRect:imageRect
              operation:NSCompositeSourceOver
               fraction:1.0
         respectFlipped:YES
                  hints:nil];
      break;

    case kHoverStateNone: {
      CGFloat value = 1.0;
      if (fadeOutAnimation_) {
        value = [fadeOutAnimation_ currentValue];
        NSImage* previousImage = nil;
        if (previousState_ == kHoverStateMouseOver)
          previousImage = [self imageForHoverState:kHoverStateMouseOver];
        else
          previousImage = [self imageForHoverState:kHoverStateMouseDown];
        [previousImage drawInRect:destRect
                         fromRect:imageRect
                        operation:NSCompositeSourceOver
                         fraction:1.0 - value
                   respectFlipped:YES
                            hints:nil];
      }
      [image drawInRect:destRect
               fromRect:imageRect
              operation:NSCompositeSourceOver
               fraction:value
         respectFlipped:YES
                  hints:nil];
      break;
    }
  }
}

- (void)drawFocusRingMask {
  // Match the hover image's shape.
  NSRect circleRect = NSInsetRect([self bounds], 2, 2);
  [[NSBezierPath bezierPathWithOvalInRect:circleRect] fill];
}

- (void)setFadeOutValue:(CGFloat)value {
  [self setNeedsDisplay];
}

- (TabView *)tabView {
  return base::mac::ObjCCast<TabView>([self superview]);
}

- (SkColor)iconColor {
  if ([[self window] hasDarkTheme]) {
    return SkColorSetARGB(0xFF, 0xC4, 0xC4, 0xC4);
  }

  const ui::ThemeProvider* themeProvider = [[self window] themeProvider];
  if (themeProvider) {
    TabView* tabView = [self tabView];
    bool use_active_tab_text_color = !tabView || [tabView isActiveTab];

    const SkColor titleColor = use_active_tab_text_color ?
        themeProvider->GetColor(ThemeProperties::COLOR_TAB_TEXT) :
        themeProvider->GetColor(ThemeProperties::COLOR_BACKGROUND_TAB_TEXT);
    return SkColorSetA(titleColor, 0xA0);
  }

  // Return the default COLOR_TAB_TEXT color.
  return SkColorSetARGB(0xA0, 0x00, 0x00, 0x00);
}

- (NSImage*)imageForHoverState:(HoverState)hoverState {
  int imageID = IDR_CLOSE_1;

  if (!ui::MaterialDesignController::IsModeMaterial()) {
    switch (hoverState) {
      case kHoverStateNone:
        imageID = IDR_CLOSE_1;
        break;
      case kHoverStateMouseOver:
        imageID = IDR_CLOSE_1_H;
        break;
      case kHoverStateMouseDown:
        imageID = IDR_CLOSE_1_P;
        break;
    }
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    return bundle.GetNativeImageNamed(imageID).ToNSImage();
  }

  gfx::VectorIconId vectorIconID;
  SkColor vectorIconColor = gfx::kPlaceholderColor;

  switch (hoverState) {
    case kHoverStateNone:
      vectorIconID = gfx::VectorIconId::TAB_CLOSE_NORMAL;
      vectorIconColor = [self iconColor];
      break;
    case kHoverStateMouseOver:
      // For mouse over, the icon color is the fill color of the circle.
      vectorIconID = gfx::VectorIconId::TAB_CLOSE_HOVERED_PRESSED;
      vectorIconColor = SkColorSetARGB(0xFF, 0xDB, 0x44, 0x37);
      break;
    case kHoverStateMouseDown:
      // For mouse pressed, the icon color is the fill color of the circle.
      vectorIconID = gfx::VectorIconId::TAB_CLOSE_HOVERED_PRESSED;
      vectorIconColor = SkColorSetARGB(0xFF, 0xA8, 0x35, 0x2A);
      break;
  }

  return NSImageFromImageSkia(
      gfx::CreateVectorIcon(vectorIconID, kTabCloseButtonSize,
          vectorIconColor));
}

- (void)setHoverState:(HoverState)state {
  if (state != self.hoverState) {
    previousState_ = self.hoverState;
    [super setHoverState:state];
    // Only animate the HoverStateNone case and only if this is still in the
    // view hierarchy.
    if (state == kHoverStateNone && [self superview] != nil) {
      DCHECK(fadeOutAnimation_ == nil);
      fadeOutAnimation_ =
          [[GTMKeyValueAnimation alloc] initWithTarget:self
                                               keyPath:kFadeOutValueKeyPath];
      [fadeOutAnimation_ setDuration:kCloseAnimationDuration];
      [fadeOutAnimation_ setFrameRate:kFramesPerSecond];
      [fadeOutAnimation_ setDelegate:self];
      [fadeOutAnimation_ startAnimation];
    } else {
      // -stopAnimation will call the animationDidStop: delegate method
      // which will clean up the animation.
      [fadeOutAnimation_ stopAnimation];
    }
  }
}

- (void)commonInit {
  [self setAccessibilityTitle:nil];

  // Add a tooltip. Using 'owner:self' means that
  // -view:stringForToolTip:point:userData: will be called to provide the
  // tooltip contents immediately before showing it.
  [self addToolTipRect:[self bounds] owner:self userData:NULL];

  // Initialize previousState.
  previousState_ = kHoverStateNone;
}

// Called each time a tooltip is about to be shown.
- (NSString*)view:(NSView*)view
 stringForToolTip:(NSToolTipTag)tag
            point:(NSPoint)point
         userData:(void*)userData {
  if (self.hoverState == kHoverStateMouseOver) {
    // In some cases (e.g. the download tray), the button is still in the
    // hover state, but is outside the bounds of its parent and not visible.
    // Don't show the tooltip in that case.
    NSRect buttonRect = [self frame];
    NSRect parentRect = [[self superview] bounds];
    if (NSIntersectsRect(buttonRect, parentRect))
      return gTooltip;
  }

  return nil;  // Do not show the tooltip.
}

- (void)setAccessibilityTitle:(NSString*)accessibilityTitle {
  if (!accessibilityTitle) {
    [super setAccessibilityTitle:gBasicAccessibilityTitle];
    return;
  }

  NSString* extendedTitle = l10n_util::GetNSStringFWithFixup(
      IDS_ACCNAME_CLOSE_TAB,
      base::SysNSStringToUTF16(accessibilityTitle));
  [super setAccessibilityTitle:extendedTitle];
}

@end

@implementation WebUIHoverCloseButton

- (NSImage*)imageForHoverState:(HoverState)hoverState {
  int imageID = IDR_CLOSE_DIALOG;
  switch (hoverState) {
    case kHoverStateNone:
      imageID = IDR_CLOSE_DIALOG;
      break;
    case kHoverStateMouseOver:
      imageID = IDR_CLOSE_DIALOG_H;
      break;
    case kHoverStateMouseDown:
      imageID = IDR_CLOSE_DIALOG_P;
      break;
  }
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  return bundle.GetNativeImageNamed(imageID).ToNSImage();
}

@end
