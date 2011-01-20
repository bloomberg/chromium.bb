// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_frame_view.h"

#import <objc/runtime.h>
#import <Carbon/Carbon.h>

#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#import "chrome/browser/themes/browser_theme_provider.h"
#import "chrome/browser/ui/cocoa/framed_browser_window.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "grit/theme_resources.h"

static const CGFloat kBrowserFrameViewPaintHeight = 60.0;
static const NSPoint kBrowserFrameViewPatternPhaseOffset = { -5, 3 };

static BOOL gCanDrawTitle = NO;
static BOOL gCanGetCornerRadius = NO;

@interface NSView (Swizzles)
- (void)drawRectOriginal:(NSRect)rect;
- (BOOL)_mouseInGroup:(NSButton*)widget;
- (void)updateTrackingAreas;
- (NSUInteger)_shadowFlagsOriginal;
@end

// Undocumented APIs. They are really on NSGrayFrame rather than
// BrowserFrameView, but we call them from methods swizzled onto NSGrayFrame.
@interface BrowserFrameView (UndocumentedAPI)

- (float)roundedCornerRadius;
- (CGRect)_titlebarTitleRect;
- (void)_drawTitleStringIn:(struct CGRect)arg1 withColor:(id)color;
- (NSUInteger)_shadowFlags;

@end

@implementation BrowserFrameView

+ (void)load {
  // This is where we swizzle drawRect, and add in two methods that we
  // need. If any of these fail it shouldn't affect the functionality of the
  // others. If they all fail, we will lose window frame theming and
  // roll overs for our close widgets, but things should still function
  // correctly.
  base::mac::ScopedNSAutoreleasePool pool;
  Class grayFrameClass = NSClassFromString(@"NSGrayFrame");
  DCHECK(grayFrameClass);
  if (!grayFrameClass) return;

  // Exchange draw rect.
  Method m0 = class_getInstanceMethod([self class], @selector(drawRect:));
  DCHECK(m0);
  if (m0) {
    BOOL didAdd = class_addMethod(grayFrameClass,
                                  @selector(drawRectOriginal:),
                                  method_getImplementation(m0),
                                  method_getTypeEncoding(m0));
    DCHECK(didAdd);
    if (didAdd) {
      Method m1 = class_getInstanceMethod(grayFrameClass, @selector(drawRect:));
      Method m2 = class_getInstanceMethod(grayFrameClass,
                                          @selector(drawRectOriginal:));
      DCHECK(m1 && m2);
      if (m1 && m2) {
        method_exchangeImplementations(m1, m2);
      }
    }
  }

  // Add _mouseInGroup.
  m0 = class_getInstanceMethod([self class], @selector(_mouseInGroup:));
  DCHECK(m0);
  if (m0) {
    BOOL didAdd = class_addMethod(grayFrameClass,
                                  @selector(_mouseInGroup:),
                                  method_getImplementation(m0),
                                  method_getTypeEncoding(m0));
    DCHECK(didAdd);
  }
  // Add updateTrackingArea.
  m0 = class_getInstanceMethod([self class], @selector(updateTrackingAreas));
  DCHECK(m0);
  if (m0) {
    BOOL didAdd = class_addMethod(grayFrameClass,
                                  @selector(updateTrackingAreas),
                                  method_getImplementation(m0),
                                  method_getTypeEncoding(m0));
    DCHECK(didAdd);
  }

  gCanDrawTitle =
      [grayFrameClass
        instancesRespondToSelector:@selector(_titlebarTitleRect)] &&
      [grayFrameClass
        instancesRespondToSelector:@selector(_drawTitleStringIn:withColor:)];
  gCanGetCornerRadius =
      [grayFrameClass
        instancesRespondToSelector:@selector(roundedCornerRadius)];

  // Add _shadowFlags. This is a method on NSThemeFrame, not on NSGrayFrame.
  // NSThemeFrame is NSGrayFrame's superclass.
  Class themeFrameClass = NSClassFromString(@"NSThemeFrame");
  DCHECK(themeFrameClass);
  if (!themeFrameClass) return;
  m0 = class_getInstanceMethod([self class], @selector(_shadowFlags));
  DCHECK(m0);
  if (m0) {
    BOOL didAdd = class_addMethod(themeFrameClass,
                                  @selector(_shadowFlagsOriginal),
                                  method_getImplementation(m0),
                                  method_getTypeEncoding(m0));
    DCHECK(didAdd);
    if (didAdd) {
      Method m1 = class_getInstanceMethod(themeFrameClass,
                                          @selector(_shadowFlags));
      Method m2 = class_getInstanceMethod(themeFrameClass,
                                          @selector(_shadowFlagsOriginal));
      DCHECK(m1 && m2);
      if (m1 && m2) {
        method_exchangeImplementations(m1, m2);
      }
    }
  }
}

- (id)initWithFrame:(NSRect)frame {
  // This class is not for instantiating.
  [self doesNotRecognizeSelector:_cmd];
  return nil;
}

- (id)initWithCoder:(NSCoder*)coder {
  // This class is not for instantiating.
  [self doesNotRecognizeSelector:_cmd];
  return nil;
}

// Here is our custom drawing for our frame.
- (void)drawRect:(NSRect)rect {
  // If this isn't the window class we expect, then pass it on to the
  // original implementation.
  if (![[self window] isKindOfClass:[FramedBrowserWindow class]]) {
    [self drawRectOriginal:rect];
    return;
  }

  // WARNING: There is an obvious optimization opportunity here that you DO NOT
  // want to take. To save painting cycles, you might think it would be a good
  // idea to call out to -drawRectOriginal: only if no theme were drawn. In
  // reality, however, if you fail to call -drawRectOriginal:, or if you call it
  // after a clipping path is set, the rounded corners at the top of the window
  // will not draw properly. Do not try to be smart here.

  // Only paint the top of the window.
  NSWindow* window = [self window];
  NSRect windowRect = [self convertRect:[window frame] fromView:nil];
  windowRect.origin = NSMakePoint(0, 0);

  NSRect paintRect = windowRect;
  paintRect.origin.y = NSMaxY(paintRect) - kBrowserFrameViewPaintHeight;
  paintRect.size.height = kBrowserFrameViewPaintHeight;
  rect = NSIntersectionRect(paintRect, rect);
  [self drawRectOriginal:rect];

  // Set up our clip.
  float cornerRadius = 4.0;
  if (gCanGetCornerRadius)
    cornerRadius = [self roundedCornerRadius];
  [[NSBezierPath bezierPathWithRoundedRect:windowRect
                                   xRadius:cornerRadius
                                   yRadius:cornerRadius] addClip];
  [[NSBezierPath bezierPathWithRect:rect] addClip];

  // Do the theming.
  BOOL themed = [BrowserFrameView drawWindowThemeInDirtyRect:rect
                                                     forView:self
                                                      bounds:windowRect
                                                      offset:NSZeroPoint
                                        forceBlackBackground:NO];

  // If the window needs a title and we painted over the title as drawn by the
  // default window paint, paint it ourselves.
  if (themed && gCanDrawTitle && ![[self window] _isTitleHidden]) {
    [self _drawTitleStringIn:[self _titlebarTitleRect]
                   withColor:[BrowserFrameView titleColorForThemeView:self]];
  }

  // Pinstripe the top.
  if (themed) {
    NSSize windowPixel = [self convertSizeFromBase:NSMakeSize(1, 1)];

    windowRect = [self convertRect:[window frame] fromView:nil];
    windowRect.origin = NSMakePoint(0, 0);
    windowRect.origin.y -= 0.5 * windowPixel.height;
    windowRect.origin.x -= 0.5 * windowPixel.width;
    windowRect.size.width += windowPixel.width;
    [[NSColor colorWithCalibratedWhite:1.0 alpha:0.5] set];
    NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:windowRect
                                                         xRadius:cornerRadius
                                                         yRadius:cornerRadius];
    [path setLineWidth:windowPixel.width];
    [path stroke];
  }
}

+ (BOOL)drawWindowThemeInDirtyRect:(NSRect)dirtyRect
                           forView:(NSView*)view
                            bounds:(NSRect)bounds
                            offset:(NSPoint)offset
              forceBlackBackground:(BOOL)forceBlackBackground {
  ui::ThemeProvider* themeProvider = [[view window] themeProvider];
  if (!themeProvider)
    return NO;

  ThemedWindowStyle windowStyle = [[view window] themedWindowStyle];

  // Devtools windows don't get themed.
  if (windowStyle & THEMED_DEVTOOLS)
    return NO;

  BOOL active = [[view window] isMainWindow];
  BOOL incognito = windowStyle & THEMED_INCOGNITO;
  BOOL popup = windowStyle & THEMED_POPUP;

  // Find a theme image.
  NSColor* themeImageColor = nil;
  int themeImageID;
  if (popup && active)
    themeImageID = IDR_THEME_TOOLBAR;
  else if (popup && !active)
    themeImageID = IDR_THEME_TAB_BACKGROUND;
  else if (!popup && active && incognito)
    themeImageID = IDR_THEME_FRAME_INCOGNITO;
  else if (!popup && active && !incognito)
    themeImageID = IDR_THEME_FRAME;
  else if (!popup && !active && incognito)
    themeImageID = IDR_THEME_FRAME_INCOGNITO_INACTIVE;
  else
    themeImageID = IDR_THEME_FRAME_INACTIVE;
  if (themeProvider->HasCustomImage(IDR_THEME_FRAME))
    themeImageColor = themeProvider->GetNSImageColorNamed(themeImageID, true);

  // If no theme image, use a gradient if incognito.
  NSGradient* gradient = nil;
  if (!themeImageColor && incognito)
    gradient = themeProvider->GetNSGradient(
        active ? BrowserThemeProvider::GRADIENT_FRAME_INCOGNITO :
                 BrowserThemeProvider::GRADIENT_FRAME_INCOGNITO_INACTIVE);

  BOOL themed = NO;
  if (themeImageColor) {
    // The titlebar/tabstrip header on the mac is slightly smaller than on
    // Windows.  To keep the window background lined up with the tab and toolbar
    // patterns, we have to shift the pattern slightly, rather than simply
    // drawing it from the top left corner.  The offset below was empirically
    // determined in order to line these patterns up.
    //
    // This will make the themes look slightly different than in Windows/Linux
    // because of the differing heights between window top and tab top, but this
    // has been approved by UI.
    NSView* frameView = [[[view window] contentView] superview];
    NSPoint topLeft = NSMakePoint(NSMinX(bounds), NSMaxY(bounds));
    NSPoint topLeftInFrameCoordinates =
        [view convertPoint:topLeft toView:frameView];

    NSPoint phase = kBrowserFrameViewPatternPhaseOffset;
    phase.x += (offset.x + topLeftInFrameCoordinates.x);
    phase.y += (offset.y + topLeftInFrameCoordinates.y);

    // Align the phase to physical pixels so resizing the window under HiDPI
    // doesn't cause wiggling of the theme.
    phase = [frameView convertPointToBase:phase];
    phase.x = floor(phase.x);
    phase.y = floor(phase.y);
    phase = [frameView convertPointFromBase:phase];

    // Default to replacing any existing pixels with the theme image, but if
    // asked paint black first and blend the theme with black.
    NSCompositingOperation operation = NSCompositeCopy;
    if (forceBlackBackground) {
      [[NSColor blackColor] set];
      NSRectFill(dirtyRect);
      operation = NSCompositeSourceOver;
    }

    [[NSGraphicsContext currentContext] setPatternPhase:phase];
    [themeImageColor set];
    NSRectFillUsingOperation(dirtyRect, operation);
    themed = YES;
  } else if (gradient) {
    NSPoint startPoint = NSMakePoint(NSMinX(bounds), NSMaxY(bounds));
    NSPoint endPoint = startPoint;
    endPoint.y -= kBrowserFrameViewPaintHeight;
    [gradient drawFromPoint:startPoint toPoint:endPoint options:0];
    themed = YES;
  }

  // Check to see if we have an overlay image.
  NSImage* overlayImage = nil;
  if (themeProvider->HasCustomImage(IDR_THEME_FRAME_OVERLAY)) {
    overlayImage = themeProvider->
        GetNSImageNamed(active ? IDR_THEME_FRAME_OVERLAY :
                                 IDR_THEME_FRAME_OVERLAY_INACTIVE,
                        true);
  }

  if (overlayImage) {
    // Anchor to top-left and don't scale.
    NSSize overlaySize = [overlayImage size];
    NSRect imageFrame = NSMakeRect(0, 0, overlaySize.width, overlaySize.height);
    [overlayImage drawAtPoint:NSMakePoint(offset.x,
                                          NSHeight(bounds) + offset.y -
                                               overlaySize.height)
                     fromRect:imageFrame
                    operation:NSCompositeSourceOver
                     fraction:1.0];
  }

  return themed;
}

+ (NSColor*)titleColorForThemeView:(NSView*)view {
  ui::ThemeProvider* themeProvider = [[view window] themeProvider];
  if (!themeProvider)
    return [NSColor windowFrameTextColor];

  ThemedWindowStyle windowStyle = [[view window] themedWindowStyle];
  BOOL active = [[view window] isMainWindow];
  BOOL incognito = windowStyle & THEMED_INCOGNITO;
  BOOL popup = windowStyle & THEMED_POPUP;

  NSColor* titleColor = nil;
  if (popup && active) {
    titleColor = themeProvider->GetNSColor(
        BrowserThemeProvider::COLOR_TAB_TEXT, false);
  } else if (popup && !active) {
    titleColor = themeProvider->GetNSColor(
        BrowserThemeProvider::COLOR_BACKGROUND_TAB_TEXT, false);
  }

  if (titleColor)
    return titleColor;

  if (incognito)
    return [NSColor whiteColor];
  else
    return [NSColor windowFrameTextColor];
}

// Check to see if the mouse is currently in one of our window widgets.
- (BOOL)_mouseInGroup:(NSButton*)widget {
  BOOL mouseInGroup = NO;
  if ([[self window] isKindOfClass:[FramedBrowserWindow class]]) {
    FramedBrowserWindow* window =
        static_cast<FramedBrowserWindow*>([self window]);
    mouseInGroup = [window mouseInGroup:widget];
  } else if ([super respondsToSelector:@selector(_mouseInGroup:)]) {
    mouseInGroup = [super _mouseInGroup:widget];
  }
  return mouseInGroup;
}

// Let our window handle updating the window widget tracking area.
- (void)updateTrackingAreas {
  [super updateTrackingAreas];
  if ([[self window] isKindOfClass:[FramedBrowserWindow class]]) {
    FramedBrowserWindow* window =
        static_cast<FramedBrowserWindow*>([self window]);
    [window updateTrackingAreas];
  }
}

// When the compositor is active, the whole content area is transparent (with
// an OpenGL surface behind it), so Cocoa draws the shadow only around the
// toolbar area.
// Tell the window server that we want a shadow as if none of the content
// area is transparent.
- (NSUInteger)_shadowFlags {
  // A slightly less intrusive hack would be to call
  // _setContentHasShadow:NO on the window. That seems to be what Terminal.app
  // is doing. However, it leads to this function returning 'code | 64', which
  // doesn't do what we want. For some reason, it does the right thing in
  // Terminal.app.
  // TODO(thakis): Figure out why -_setContentHasShadow: works in Terminal.app
  // and use that technique instead. http://crbug.com/53382

  // If this isn't the window class we expect, then pass it on to the
  // original implementation.
  if (![[self window] isKindOfClass:[FramedBrowserWindow class]])
    return [self _shadowFlagsOriginal];

  return [self _shadowFlagsOriginal] | 128;
}

@end
