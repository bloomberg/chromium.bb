// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/WebTestThemeEngineMac.h"

#import <AppKit/NSAffineTransform.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSScroller.h>
#import <AppKit/NSWindow.h>
#include <Carbon/Carbon.h>
#include "skia/ext/skia_utils_mac.h"
#include "third_party/WebKit/public/platform/WebCanvas.h"
#include "third_party/WebKit/public/platform/WebRect.h"

using blink::WebCanvas;
using blink::WebRect;
using blink::WebThemeEngine;

// We can't directly tell the NSScroller to draw itself as active or inactive,
// instead we have to make it a child of an (in)active window. This class lets
// us fake that parent window.
@interface FakeActiveWindow : NSWindow {
@private
    BOOL hasActiveControls;
}
+ (NSWindow*)alwaysActiveWindow;
+ (NSWindow*)alwaysInactiveWindow;
- (id)initWithActiveControls:(BOOL)_hasActiveControls;
- (BOOL)_hasActiveControls;
@end

@implementation FakeActiveWindow

static NSWindow* alwaysActiveWindow = nil;
static NSWindow* alwaysInactiveWindow = nil;

+ (NSWindow*)alwaysActiveWindow
{
    if (alwaysActiveWindow == nil)
        alwaysActiveWindow = [[self alloc] initWithActiveControls:YES];
    return alwaysActiveWindow;
}

+ (NSWindow*)alwaysInactiveWindow
{
    if (alwaysInactiveWindow == nil)
        alwaysInactiveWindow = [[self alloc] initWithActiveControls:NO];
    return alwaysInactiveWindow;
}

- (id)initWithActiveControls:(BOOL)_hasActiveControls
{
    if ((self = [super initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                 styleMask:0
                                   backing:NSBackingStoreBuffered
                                     defer:YES])) {
        hasActiveControls = _hasActiveControls;
    }
    return self;
}

- (BOOL)_hasActiveControls
{
    return hasActiveControls;
}

@end

namespace content {

namespace {

ThemeTrackEnableState stateToHIEnableState(WebThemeEngine::State state)
{
    switch (state) {
    case WebThemeEngine::StateDisabled:
        return kThemeTrackDisabled;
    case WebThemeEngine::StateInactive:
        return kThemeTrackInactive;
    default:
        return kThemeTrackActive;
    }
}

}  // namespace

void WebTestThemeEngineMac::paintScrollbarThumb(
    WebCanvas* canvas,
    WebThemeEngine::State state,
    WebThemeEngine::Size size,
    const WebRect& rect,
    const WebThemeEngine::ScrollbarInfo& scrollbarInfo)
{
    // To match the Mac port, we still use HITheme for inner scrollbars.
    if (scrollbarInfo.parent == WebThemeEngine::ScrollbarParentRenderLayer)
        paintHIThemeScrollbarThumb(canvas, state, size, rect, scrollbarInfo);
    else
        paintNSScrollerScrollbarThumb(canvas, state, size, rect, scrollbarInfo);
}

// Duplicated from webkit/glue/webthemeengine_impl_mac.cc in the downstream
// Chromium WebThemeEngine implementation.
void WebTestThemeEngineMac::paintHIThemeScrollbarThumb(
    WebCanvas* canvas,
    WebThemeEngine::State state,
    WebThemeEngine::Size size,
    const WebRect& rect,
    const WebThemeEngine::ScrollbarInfo& scrollbarInfo)
{
    HIThemeTrackDrawInfo trackInfo;
    trackInfo.version = 0;
    trackInfo.kind = size == WebThemeEngine::SizeRegular ? kThemeMediumScrollBar : kThemeSmallScrollBar;
    trackInfo.bounds = CGRectMake(rect.x, rect.y, rect.width, rect.height);
    trackInfo.min = 0;
    trackInfo.max = scrollbarInfo.maxValue;
    trackInfo.value = scrollbarInfo.currentValue;
    trackInfo.trackInfo.scrollbar.viewsize = scrollbarInfo.visibleSize;
    trackInfo.attributes = 0;
    if (scrollbarInfo.orientation == WebThemeEngine::ScrollbarOrientationHorizontal)
        trackInfo.attributes |= kThemeTrackHorizontal;

    trackInfo.enableState = stateToHIEnableState(state);

    trackInfo.trackInfo.scrollbar.pressState =
        state == WebThemeEngine::StatePressed ? kThemeThumbPressed : 0;
    trackInfo.attributes |= (kThemeTrackShowThumb | kThemeTrackHideTrack);
    gfx::SkiaBitLocker bitLocker(canvas);
    CGContextRef cgContext = bitLocker.cgContext();
    HIThemeDrawTrack(&trackInfo, 0, cgContext, kHIThemeOrientationNormal);
}

void WebTestThemeEngineMac::paintNSScrollerScrollbarThumb(
    WebCanvas* canvas,
    WebThemeEngine::State state,
    WebThemeEngine::Size size,
    const WebRect& rect,
    const WebThemeEngine::ScrollbarInfo& scrollbarInfo)
{
    [NSGraphicsContext saveGraphicsState];
    NSScroller* scroller = [[NSScroller alloc] initWithFrame:NSMakeRect(rect.x, rect.y, rect.width, rect.height)];
    [scroller setEnabled:state != WebThemeEngine::StateDisabled];
    if (state == WebThemeEngine::StateInactive)
        [[[FakeActiveWindow alwaysInactiveWindow] contentView] addSubview:scroller];
    else
        [[[FakeActiveWindow alwaysActiveWindow] contentView] addSubview:scroller];

    [scroller setControlSize:size == WebThemeEngine::SizeRegular ? NSRegularControlSize : NSSmallControlSize];

    double value = double(scrollbarInfo.currentValue) / double(scrollbarInfo.maxValue);
    [scroller setDoubleValue: value];

    float knobProportion = float(scrollbarInfo.visibleSize) / float(scrollbarInfo.totalSize);
    [scroller setKnobProportion: knobProportion];

    gfx::SkiaBitLocker bitLocker(canvas);
    CGContextRef cgContext = bitLocker.cgContext();
    NSGraphicsContext* nsGraphicsContext = [NSGraphicsContext graphicsContextWithGraphicsPort:cgContext flipped:YES];
    [NSGraphicsContext setCurrentContext:nsGraphicsContext];

    // Despite passing in frameRect() to the scroller, it always draws at (0, 0).
    // Force it to draw in the right location by translating the whole graphics
    // context.
    CGContextSaveGState(cgContext);
    NSAffineTransform *transform = [NSAffineTransform transform];
    [transform translateXBy:rect.x yBy:rect.y];
    [transform concat];

    [scroller drawKnob];
    CGContextRestoreGState(cgContext);

    [scroller release];

    [NSGraphicsContext restoreGraphicsState];
}

}  // namespace content
