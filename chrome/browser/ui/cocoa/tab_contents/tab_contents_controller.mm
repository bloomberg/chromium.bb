// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"

#include <utility>

#include "base/command_line.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/devtools/devtools_window.h"
#import "chrome/browser/themes/theme_properties.h"
#import "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/rect.h"

using content::WebContents;
using content::WebContentsObserver;

// FullscreenObserver is used by TabContentsController to monitor for the
// showing/destruction of fullscreen render widgets.  When notified,
// TabContentsController will alter its child view hierarchy to either embed a
// fullscreen render widget view or restore the normal WebContentsView render
// view.  The embedded fullscreen render widget will fill the user's screen in
// the case where TabContentsController's NSView is a subview of a browser
// window that has been toggled into fullscreen mode (e.g., via
// FullscreenController).
class FullscreenObserver : public WebContentsObserver {
 public:
  explicit FullscreenObserver(TabContentsController* controller)
      : controller_(controller) {}

  void Observe(content::WebContents* new_web_contents) {
    WebContentsObserver::Observe(new_web_contents);
  }

  WebContents* web_contents() const {
    return WebContentsObserver::web_contents();
  }

  virtual void DidShowFullscreenWidget(int routing_id) OVERRIDE {
    [controller_ toggleFullscreenWidget:YES];
  }

  virtual void DidDestroyFullscreenWidget(int routing_id) OVERRIDE {
    [controller_ toggleFullscreenWidget:NO];
  }

  virtual void DidToggleFullscreenModeForTab(bool entered_fullscreen) OVERRIDE {
    [controller_ toggleFullscreenWidget:YES];
  }

 private:
  TabContentsController* const controller_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenObserver);
};

@interface TabContentsController (TabContentsContainerViewDelegate)
- (BOOL)contentsInFullscreenCaptureMode;
// Computes and returns the frame to use for the contents view within the
// container view.
- (NSRect)frameForContentsView;
@end

// An NSView with special-case handling for when the contents view does not
// expand to fill the entire tab contents area. See 'AutoEmbedFullscreen mode'
// in header file comments.
@interface TabContentsContainerView : NSView {
 @private
  TabContentsController* delegate_;  // weak
}

- (NSColor*)computeBackgroundColor;
@end

@implementation TabContentsContainerView

- (id)initWithDelegate:(TabContentsController*)delegate {
  if ((self = [super initWithFrame:NSZeroRect])) {
    delegate_ = delegate;
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableCoreAnimation)) {
      // TODO(ccameron): Remove the -drawRect: method once the
      // kDisableCoreAnimation switch is removed.
      ScopedCAActionDisabler disabler;
      base::scoped_nsobject<CALayer> layer([[CALayer alloc] init]);
      [layer setBackgroundColor:CGColorGetConstantColor(kCGColorWhite)];
      [self setLayer:layer];
      [self setWantsLayer:YES];
    }
  }
  return self;
}

// Called by the delegate during dealloc to invalidate the pointer held by this
// view.
- (void)delegateDestroyed {
  delegate_ = nil;
}

- (NSColor*)computeBackgroundColor {
  // This view is sometimes flashed into visibility (e.g, when closing
  // windows), so ensure that the flash be white in those cases.
  if (![delegate_ contentsInFullscreenCaptureMode])
    return [NSColor whiteColor];

  // Fill with a dark tint of the new tab page's background color.  This is
  // only seen when the subview is sized specially for fullscreen tab capture.
  NSColor* bgColor = nil;
  ThemeService* const theme =
      static_cast<ThemeService*>([[self window] themeProvider]);
  if (theme)
    bgColor = theme->GetNSColor(ThemeProperties::COLOR_NTP_BACKGROUND);
  if (!bgColor)
    bgColor = [NSColor whiteColor];
  const float kDarknessFraction = 0.80f;
  return [bgColor blendedColorWithFraction:kDarknessFraction
                                   ofColor:[NSColor blackColor]];
}

// Override -drawRect to fill the view with a solid color outside of the
// subview's frame.
//
// Note: This method is never called when CoreAnimation is enabled.
- (void)drawRect:(NSRect)dirtyRect {
  NSView* const contentsView =
      [[self subviews] count] > 0 ? [[self subviews] objectAtIndex:0] : nil;
  if (!contentsView || !NSContainsRect([contentsView frame], dirtyRect)) {
    [[self computeBackgroundColor] setFill];
    NSRectFill(dirtyRect);
  }
  [super drawRect:dirtyRect];
}

// Override auto-resizing logic to query the delegate for the exact frame to
// use for the contents view.
- (void)resizeSubviewsWithOldSize:(NSSize)oldBoundsSize {
  NSView* const contentsView =
      [[self subviews] count] > 0 ? [[self subviews] objectAtIndex:0] : nil;
  if (!contentsView || [contentsView autoresizingMask] == NSViewNotSizable ||
      !delegate_) {
    return;
  }

  ScopedCAActionDisabler disabler;
  [contentsView setFrame:[delegate_ frameForContentsView]];
}

// Update the background layer's color whenever the view needs to repaint.
- (void)setNeedsDisplayInRect:(NSRect)rect {
  [super setNeedsDisplayInRect:rect];

  // Convert from an NSColor to a CGColorRef.
  NSColor* nsBackgroundColor = [self computeBackgroundColor];
  NSColorSpace* nsColorSpace = [nsBackgroundColor colorSpace];
  CGColorSpaceRef cgColorSpace = [nsColorSpace CGColorSpace];
  const NSInteger numberOfComponents = [nsBackgroundColor numberOfComponents];
  CGFloat components[numberOfComponents];
  [nsBackgroundColor getComponents:components];
  base::ScopedCFTypeRef<CGColorRef> cgBackgroundColor(
      CGColorCreate(cgColorSpace, components));

  ScopedCAActionDisabler disabler;
  [[self layer] setBackgroundColor:cgBackgroundColor];
}

@end  // @implementation TabContentsContainerView

@implementation TabContentsController
@synthesize webContents = contents_;

- (id)initWithContents:(WebContents*)contents {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    fullscreenObserver_.reset(new FullscreenObserver(self));
    [self changeWebContents:contents];
  }
  return self;
}

- (void)dealloc {
  [static_cast<TabContentsContainerView*>([self view]) delegateDestroyed];
  // Make sure the contents view has been removed from the container view to
  // allow objects to be released.
  [[self view] removeFromSuperview];
  [super dealloc];
}

- (void)loadView {
  base::scoped_nsobject<NSView> view(
      [[TabContentsContainerView alloc] initWithDelegate:self]);
  [view setAutoresizingMask:NSViewHeightSizable|NSViewWidthSizable];
  [self setView:view];
}

- (void)ensureContentsSizeDoesNotChange {
  NSView* contentsContainer = [self view];
  NSArray* subviews = [contentsContainer subviews];
  if ([subviews count] > 0) {
    NSView* currentSubview = [subviews objectAtIndex:0];
    [currentSubview setAutoresizingMask:NSViewNotSizable];
  }
}

- (void)ensureContentsVisible {
  if (!contents_)
    return;
  ScopedCAActionDisabler disabler;
  NSView* contentsContainer = [self view];
  NSArray* subviews = [contentsContainer subviews];
  NSView* contentsNativeView;
  content::RenderWidgetHostView* const fullscreenView =
      isEmbeddingFullscreenWidget_ ?
      contents_->GetFullscreenRenderWidgetHostView() : NULL;
  if (fullscreenView) {
    contentsNativeView = fullscreenView->GetNativeView();
  } else {
    isEmbeddingFullscreenWidget_ = NO;
    contentsNativeView = contents_->GetNativeView();
  }
  [contentsNativeView setFrame:[self frameForContentsView]];
  if ([subviews count] == 0) {
    [contentsContainer addSubview:contentsNativeView];
  } else if ([subviews objectAtIndex:0] != contentsNativeView) {
    [contentsContainer replaceSubview:[subviews objectAtIndex:0]
                                 with:contentsNativeView];
  }
  [contentsNativeView setAutoresizingMask:NSViewWidthSizable|
                                          NSViewHeightSizable];

  [contentsContainer setNeedsDisplay:YES];

  // The rendering path with overlapping views disabled causes bugs when
  // transitioning between composited and non-composited mode.
  // http://crbug.com/279472
  if (!fullscreenView)
    contents_->SetAllowOverlappingViews(true);
}

- (void)changeWebContents:(WebContents*)newContents {
  contents_ = newContents;
  fullscreenObserver_->Observe(contents_);
  isEmbeddingFullscreenWidget_ =
      contents_ && contents_->GetFullscreenRenderWidgetHostView();
}

// Returns YES if the tab represented by this controller is the front-most.
- (BOOL)isCurrentTab {
  // We're the current tab if we're in the view hierarchy, otherwise some other
  // tab is.
  return [[self view] superview] ? YES : NO;
}

- (void)willBecomeUnselectedTab {
  // The RWHV is ripped out of the view hierarchy on tab switches, so it never
  // formally resigns first responder status.  Handle this by explicitly sending
  // a Blur() message to the renderer, but only if the RWHV currently has focus.
  content::RenderViewHost* rvh = [self webContents]->GetRenderViewHost();
  if (rvh) {
    if (rvh->GetView() && rvh->GetView()->HasFocus()) {
      rvh->Blur();
      return;
    }
    WebContents* devtools = DevToolsWindow::GetInTabWebContents(
        [self webContents], NULL);
    if (devtools) {
      content::RenderViewHost* devtoolsView = devtools->GetRenderViewHost();
      if (devtoolsView && devtoolsView->GetView() &&
          devtoolsView->GetView()->HasFocus()) {
        devtoolsView->Blur();
      }
    }
  }
}

- (void)willBecomeSelectedTab {
  // Do not explicitly call Focus() here, as the RWHV may not actually have
  // focus (for example, if the omnibox has focus instead).  The WebContents
  // logic will restore focus to the appropriate view.
}

- (void)tabDidChange:(WebContents*)updatedContents {
  // Calling setContentView: here removes any first responder status
  // the view may have, so avoid changing the view hierarchy unless
  // the view is different.
  if ([self webContents] != updatedContents) {
    [self changeWebContents:updatedContents];
    [self ensureContentsVisible];
  }
}

- (void)toggleFullscreenWidget:(BOOL)enterFullscreen {
  isEmbeddingFullscreenWidget_ = enterFullscreen &&
      contents_ && contents_->GetFullscreenRenderWidgetHostView();
  [self ensureContentsVisible];
}

- (BOOL)contentsInFullscreenCaptureMode {
  // Note: Grab a known-valid WebContents pointer from |fullscreenObserver_|.
  content::WebContents* const wc = fullscreenObserver_->web_contents();
  if (!wc ||
      wc->GetCapturerCount() == 0 ||
      wc->GetPreferredSize().IsEmpty() ||
      !(isEmbeddingFullscreenWidget_ ||
        (wc->GetDelegate() &&
         wc->GetDelegate()->IsFullscreenForTabOrPending(wc)))) {
    return NO;
  }
  return YES;
}

- (NSRect)frameForContentsView {
  const NSSize containerSize = [[self view] frame].size;
  gfx::Rect rect;
  rect.set_width(containerSize.width);
  rect.set_height(containerSize.height);

  // In most cases, the contents view is simply sized to fill the container
  // view's bounds. Only WebContentses that are in fullscreen mode and being
  // screen-captured will engage the special layout/sizing behavior.
  if (![self contentsInFullscreenCaptureMode])
    return NSRectFromCGRect(rect.ToCGRect());

  // Size the contents view to the capture video resolution and center it. If
  // the container view is not large enough to fit it at the preferred size,
  // scale down to fit (preserving aspect ratio).
  content::WebContents* const wc = fullscreenObserver_->web_contents();
  const gfx::Size captureSize = wc->GetPreferredSize();
  if (captureSize.width() <= rect.width() &&
      captureSize.height() <= rect.height()) {
    // No scaling, just centering.
    rect.ClampToCenteredSize(captureSize);
  } else {
    // Scale down, preserving aspect ratio, and center.
    // TODO(miu): This is basically media::ComputeLetterboxRegion(), and it
    // looks like others have written this code elsewhere.  Let's consolidate
    // into a shared function ui/gfx/geometry or around there.
    const int64 x = static_cast<int64>(captureSize.width()) * rect.height();
    const int64 y = static_cast<int64>(captureSize.height()) * rect.width();
    if (y < x) {
      rect.ClampToCenteredSize(gfx::Size(
          rect.width(), static_cast<int>(y / captureSize.width())));
    } else {
      rect.ClampToCenteredSize(gfx::Size(
          static_cast<int>(x / captureSize.height()), rect.height()));
    }
  }

  return NSRectFromCGRect(rect.ToCGRect());
}

@end
