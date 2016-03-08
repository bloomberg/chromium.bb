// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/toolbar/toolbar_button_cocoa.h"

#include "base/mac/foundation_util.h"
#include "base/mac/sdk_forward_declarations.h"
#import "chrome/browser/ui/cocoa/image_button_cell.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// The bounds of toolbar buttons in Material Design.
const NSRect kMDButtonBounds = NSMakeRect(0, 0, 28, 28);

// The size of a toolbar button icon in Material Design. A toolbar button image
// consists of a border and background, with a centered icon.
const NSSize kMDButtonIconSize = NSMakeSize(16, 16);

}  // namespace

// An NSCustomImageRep subclass that creates the "three dots" image of the
// Material Design browser tools icon.
@interface BrowserToolsImageRep : NSCustomImageRep
@property (retain, nonatomic) NSColor* fillColor;
// NSCustomImageRep delegate method that performs the drawing.
+ (void)drawBrowserToolsIcon:(BrowserToolsImageRep*)imageRep;
@end

@implementation BrowserToolsImageRep

@synthesize fillColor = fillColor_;

- (void)dealloc {
  [fillColor_ release];
  [super dealloc];
}

+ (void)drawBrowserToolsIcon:(BrowserToolsImageRep*)imageRep {
  [imageRep.fillColor set];
  NSBezierPath* dotPath =
      [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(6.5, 1.5, 3, 3)];
  CGContextRef context = static_cast<CGContextRef>(
      [[NSGraphicsContext currentContext] graphicsPort]);
  // Draw the three dots by drawing |dotPath| in three different locations.
  for (NSUInteger i = 0; i < 3; i++) {
    [dotPath fill];
    CGContextTranslateCTM(context, 0, 5);
  }
}

@end

// An NSCustomImageRep subclass that draws a Material Design background behind
// and border around a centered icon image.
@interface ToolbarButtonImageRep : NSCustomImageRep
@property (retain, nonatomic) NSImage* icon;
@property (assign, nonatomic) ToolbarButtonImageBackgroundStyle style;
// NSCustomImageRep delegate method that performs the drawing.
+ (void)drawImage:(ToolbarButtonImageRep*)imageRep;
@end

@implementation ToolbarButtonImageRep

@synthesize icon = icon_;
@synthesize style = style_;

- (void)dealloc {
  [icon_ release];
  [super dealloc];
}

+ (void)drawImage:(ToolbarButtonImageRep*)imageRep {
  // Create the path used for the background fill.
  NSRect destRect = NSInsetRect(kMDButtonBounds, 2, 2);
  NSBezierPath* roundedRectPath =
      [NSBezierPath bezierPathWithRoundedRect:destRect xRadius:2 yRadius:2];

  // Determine the fill color.
  NSColor* fillColor = nil;
  switch (imageRep.style) {
    case ToolbarButtonImageBackgroundStyle::HOVER:
      fillColor = [NSColor colorWithCalibratedWhite:0 alpha:0.08];
      break;
    case ToolbarButtonImageBackgroundStyle::HOVER_THEMED:
      fillColor = [NSColor colorWithCalibratedWhite:1 alpha:0.08];
      break;
    case ToolbarButtonImageBackgroundStyle::PRESSED:
      fillColor = [NSColor colorWithCalibratedWhite:0 alpha:0.12];
      break;
    case ToolbarButtonImageBackgroundStyle::PRESSED_THEMED:
      fillColor = [NSColor colorWithCalibratedWhite:1 alpha:0.12];
      break;
  }

  // Fill the path.
  [fillColor set];
  [roundedRectPath fill];

  // Compute the icon's location and draw it there.
  CGFloat iconInset =
      (kMDButtonBounds.size.width - kMDButtonIconSize.width) / 2;
  NSRect iconDestRect = NSInsetRect(kMDButtonBounds, iconInset, iconInset);
  [imageRep.icon drawInRect:iconDestRect
                   fromRect:NSZeroRect
                  operation:NSCompositeSourceOver
                   fraction:1];
}

@end

@interface ToolbarButton ()
// Returns an image that draws the browser tools button icon using vector
// commands.
- (NSImage*)browserToolsIconForFillColor:(SkColor)fillColor;
// Returns an button image by combining |iconImage| with the specified button
// background.
- (NSImage*)imageForIcon:(NSImage*)iconImage
     withBackgroundStyle:(ToolbarButtonImageBackgroundStyle)style;
// Creates and assigns images for the button's various states (pressed, hover,
// etc.).
- (void)setImagesFromIconId:(gfx::VectorIconId)iconId;
// Implemented to set the button's icon when added to the browser window. We
// can't set the image before this because its appearance depends upon the
// browser window's theme.
- (void)viewDidMoveToWindow;

@end


@implementation ToolbarButton

@synthesize handleMiddleClick = handleMiddleClick_;

- (void)otherMouseDown:(NSEvent*)theEvent {
  if (![self shouldHandleEvent:theEvent]) {
    [super otherMouseDown:theEvent];
    return;
  }

  NSEvent* nextEvent = theEvent;
  BOOL isInside;

  // Loop until middle button is released. Also, the mouse cursor is outside of
  // the button, the button should not be highlighted.
  do {
    NSPoint mouseLoc = [self convertPoint:[nextEvent locationInWindow]
                                 fromView:nil];
    isInside = [self mouse:mouseLoc inRect:[self bounds]];
    [self highlight:isInside];
    [self setState:isInside ? NSOnState : NSOffState];

    NSUInteger mask = NSOtherMouseDraggedMask | NSOtherMouseUpMask;
    nextEvent = [[self window] nextEventMatchingMask:mask];
  } while (!([nextEvent buttonNumber] == 2 &&
             [nextEvent type] == NSOtherMouseUp));

  // Discard the events before the middle button up event.
  // If we don't discard it, the events will be re-processed later.
  [[self window] discardEventsMatchingMask:NSAnyEventMask
                               beforeEvent:nextEvent];

  [self highlight:NO];
  [self setState:NSOffState];
  if (isInside)
    [self sendAction:[self action] to:[self target]];
}

- (BOOL)shouldHandleEvent:(NSEvent*)theEvent {
  // |buttonNumber| is the mouse button whose action triggered theEvent.
  // 2 corresponds to the middle mouse button.
  return handleMiddleClick_ && [theEvent buttonNumber] == 2;
}

- (void)drawFocusRingMask {
  // Match the hover image's bezel.
  [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect([self bounds], 2, 2)
                                   xRadius:2
                                   yRadius:2] fill];
}

- (SkColor)iconColor:(BOOL)themeIsDark {
  return themeIsDark ? SK_ColorWHITE : SkColorSetRGB(0x5A, 0x5A, 0x5A);
}

- (NSImage*)browserToolsIconForFillColor:(SkColor)fillColor {
  // Create a |BrowserToolsImageRep| to draw the browser tools icon using
  // the provided fill color.
  base::scoped_nsobject<BrowserToolsImageRep> imageRep =
  [[BrowserToolsImageRep alloc]
      initWithDrawSelector:@selector(drawBrowserToolsIcon:)
                  delegate:[BrowserToolsImageRep class]];
  [imageRep setFillColor:skia::SkColorToCalibratedNSColor(fillColor)];

  // Create the image from the image rep.
  NSImage* browserToolsIcon =
      [[[NSImage alloc] initWithSize:kMDButtonIconSize] autorelease];
  [browserToolsIcon setCacheMode:NSImageCacheAlways];
  [browserToolsIcon addRepresentation:imageRep];

  return browserToolsIcon;
}

- (NSImage*)imageForIcon:(NSImage*)iconImage
     withBackgroundStyle:(ToolbarButtonImageBackgroundStyle)style {
  // Create a |ToolbarButtonImageRep| to draw the button image using
  // the provided icon and background style.
  base::scoped_nsobject<ToolbarButtonImageRep> imageRep =
      [[ToolbarButtonImageRep alloc]
          initWithDrawSelector:@selector(drawImage:)
                      delegate:[ToolbarButtonImageRep class]];
  [imageRep setIcon:iconImage];
  [imageRep setStyle:style];

  // Create the image from the image rep.
  NSImage* image =
      [[[NSImage alloc] initWithSize:kMDButtonBounds.size] autorelease];
  [image setCacheMode:NSImageCacheAlways];
  [image addRepresentation:imageRep];

  return image;
}

- (void)setImagesFromIconId:(gfx::VectorIconId)iconId {
  // Compute the normal and disabled icon colors.
  BOOL isDarkTheme = [[self window] hasDarkTheme];
  const SkColor iconColor = [self iconColor:isDarkTheme];
  CGFloat normalAlpha = isDarkTheme ? 0xCC : 0xFF;
  const SkColor normalColor = SkColorSetA(iconColor, normalAlpha);
  const SkColor disabledColor = SkColorSetA(iconColor, 0x33);

  // Create the normal and disabled state icons. These icons are
  // always the same shape, but use a different color.
  NSImage* normalIcon = nil;
  NSImage* disabledIcon = nil;
  if (iconId == gfx::VectorIconId::BROWSER_TOOLS) {
    normalIcon = [self browserToolsIconForFillColor:normalColor];
    disabledIcon = [self browserToolsIconForFillColor:disabledColor];
  } else {
    normalIcon = NSImageFromImageSkia(
        gfx::CreateVectorIcon(iconId, kMDButtonIconSize.width, normalColor));
    disabledIcon = NSImageFromImageSkia(
        gfx::CreateVectorIcon(iconId, kMDButtonIconSize.width, disabledColor));
  }

  ImageButtonCell* theCell = base::mac::ObjCCast<ImageButtonCell>([self cell]);
  // Set the image for the default state, which is just the icon.
  [theCell setImage:normalIcon forButtonState:image_button_cell::kDefaultState];

  // Determine the appropriate image background style for the hover and pressed
  // states.
  ToolbarButtonImageBackgroundStyle hoverStyle;
  ToolbarButtonImageBackgroundStyle pressedStyle;
  const ui::ThemeProvider* themeProvider = [[self window] themeProvider];
  bool isNonIncognitoSystemTheme = themeProvider &&
      !themeProvider->InIncognitoMode() && themeProvider->UsingSystemTheme();
  // Use the regular style only when using the system (i.e. non-custom) theme
  // and not in Incognito mode.
  if (isNonIncognitoSystemTheme) {
    hoverStyle = ToolbarButtonImageBackgroundStyle::HOVER;
    pressedStyle = ToolbarButtonImageBackgroundStyle::PRESSED;
  } else {
    hoverStyle = ToolbarButtonImageBackgroundStyle::HOVER_THEMED;
    pressedStyle = ToolbarButtonImageBackgroundStyle::PRESSED_THEMED;
  }

  // Create and set the image for the hover state.
  NSImage* hoverImage =
      [self imageForIcon:normalIcon withBackgroundStyle:hoverStyle];
  [theCell setImage:hoverImage
     forButtonState:image_button_cell::kHoverState];

  // Create and set the image for the pressed state.
  NSImage* pressedImage =
      [self imageForIcon:normalIcon withBackgroundStyle:pressedStyle];
  [theCell setImage:pressedImage
     forButtonState:image_button_cell::kPressedState];

  // Set the image for the disabled state, which is just the disabled icon,
  // except if this is the home button.
  if (iconId == gfx::VectorIconId::NAVIGATE_RELOAD) {
    [theCell setImage:nil forButtonState:image_button_cell::kDisabledState];
  } else {
    [theCell setImage:disabledIcon
       forButtonState:image_button_cell::kDisabledState];
  }
  [self setNeedsDisplay:YES];
}

- (void)resetIcons {
  // Compute the button's icon.
  gfx::VectorIconId icon_id = gfx::VectorIconId::VECTOR_ICON_NONE;
  switch ([self viewID]) {
    case VIEW_ID_BACK_BUTTON:
      icon_id = gfx::VectorIconId::NAVIGATE_BACK;
      break;
    case VIEW_ID_FORWARD_BUTTON:
      icon_id = gfx::VectorIconId::NAVIGATE_FORWARD;
      break;
    case VIEW_ID_HOME_BUTTON:
      icon_id = gfx::VectorIconId::NAVIGATE_HOME;
      break;
    case VIEW_ID_APP_MENU:
      icon_id = gfx::VectorIconId::BROWSER_TOOLS;
      break;
    default:
      break;
  }

  // Set it.
  if (icon_id != gfx::VectorIconId::VECTOR_ICON_NONE) {
    [self setImagesFromIconId:icon_id];
  }
}

- (void)viewDidMoveToWindow {
  // In Material Design we want to catch when the button is attached to its
  // window so that we can configure its appearance based on the window's
  // theme.
  if ([self window] && ui::MaterialDesignController::IsModeMaterial()) {
    [self resetIcons];
  }
}

// ThemedWindowDrawing implementation.

- (void)windowDidChangeTheme {
  [self resetIcons];
}

- (void)windowDidChangeActive {
}

@end
