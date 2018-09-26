// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"

#include <stdint.h>

#include <utility>

#include "base/feature_list.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "chrome/browser/devtools/devtools_window.h"
#import "chrome/browser/themes/theme_properties.h"
#import "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_within_tab_helper.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/cocoa/animation_utils.h"
#import "ui/base/cocoa/touch_bar_forward_declarations.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scrollbar_size.h"

using content::WebContents;
using content::WebContentsObserver;

@interface TabContentsController (TabContentsContainerViewDelegate)
// Computes and returns the frame to use for the contents view using the size of
// |container| as the target size.
- (NSRect)frameForContentsViewIn:(NSView*)container;

// Returns YES if the content view should be resized.
- (BOOL)shouldResizeContentView;

// Returns YES if the content view is inside a popup.
- (BOOL)isPopup;

@end

// An NSView with special-case handling for when the contents view does not
// expand to fill the entire tab contents area. See 'AutoEmbedFullscreen mode'
// in header file comments.
@interface TabContentsContainerView : NSView {
 @private
  TabContentsController* delegate_;  // weak
}

- (void)updateBackgroundColorFromWindowTheme:(NSWindow*)window;
@end

@implementation TabContentsContainerView

- (id)initWithDelegate:(TabContentsController*)delegate {
  if ((self = [super initWithFrame:NSZeroRect])) {
    delegate_ = delegate;
    ScopedCAActionDisabler disabler;
    base::scoped_nsobject<CALayer> layer([[CALayer alloc] init]);
    [self setLayer:layer];
    [self setWantsLayer:YES];
  }
  return self;
}

// Called by the delegate during dealloc to invalidate the pointer held by this
// view.
- (void)delegateDestroyed {
  delegate_ = nil;
}

// Override auto-resizing logic to query the delegate for the exact frame to
// use for the contents view.
// TODO(spqchan): The popup check is a temporary solution to fix the regression
// issue described in crbug.com/604288. This method doesn't really affect
// fullscreen if the content is inside a normal browser window, but would
// cause a flash fullscreen widget to blow up if it's inside a popup.
- (void)resizeSubviewsWithOldSize:(NSSize)oldBoundsSize {
  NSView* const contentsView =
      [[self subviews] count] > 0 ? [[self subviews] objectAtIndex:0] : nil;
  if (!contentsView || [contentsView autoresizingMask] == NSViewNotSizable ||
      !delegate_ ||
      (![delegate_ shouldResizeContentView] && [delegate_ isPopup])) {
    return;
  }

  ScopedCAActionDisabler disabler;
  [contentsView setFrame:[delegate_ frameForContentsViewIn:self]];
}

// Update the background layer's color whenever the view needs to repaint.
- (void)setNeedsDisplayInRect:(NSRect)rect {
  [super setNeedsDisplayInRect:rect];
  [self updateBackgroundColorFromWindowTheme:[self window]];
}

- (void)updateBackgroundColorFromWindowTheme:(NSWindow*)window {
  // This view is sometimes flashed into visibility (e.g, when closing
  // windows or opening new tabs), so ensure that the flash be the theme
  // background color in those cases.
  const ThemeProvider* theme = [window themeProvider];
  if (!theme)
    return;

  SkColor skBackgroundColor =
      theme->GetColor(ThemeProperties::COLOR_NTP_BACKGROUND);

  ScopedCAActionDisabler disabler;
  base::ScopedCFTypeRef<CGColorRef> cgBackgroundColor(
      skia::CGColorCreateFromSkColor(skBackgroundColor));
  [[self layer] setBackgroundColor:cgBackgroundColor];
}

- (void)viewWillMoveToWindow:(NSWindow*)newWindow {
  [self updateBackgroundColorFromWindowTheme:newWindow];
}

- (ViewID)viewID {
  return VIEW_ID_TAB_CONTAINER;
}

- (BOOL)acceptsFirstResponder {
  return [[self subviews] count] > 0 &&
      [[[self subviews] objectAtIndex:0] acceptsFirstResponder];
}

// When receiving a click-to-focus in the solid color area surrounding the
// WebContents' native view, immediately transfer focus to WebContents' native
// view.
- (BOOL)becomeFirstResponder {
  if (![self acceptsFirstResponder])
    return NO;
  return [[self window] makeFirstResponder:[[self subviews] objectAtIndex:0]];
}

- (BOOL)canBecomeKeyView {
  return NO;  // Tab/Shift-Tab should focus the subview, not this view.
}

@end  // @implementation TabContentsContainerView

@interface TabContentsController (
    SeparateFullscreenWindowDelegate)<NSWindowDelegate>

- (NSView*)createScreenshotView;

- (NSWindow*)createSeparateWindowForTab:(content::WebContents*)separatedTab;

@end

@implementation TabContentsController
@synthesize webContents = contents_;
@synthesize blockFullscreenResize = blockFullscreenResize_;

- (id)initWithContents:(WebContents*)contents isPopup:(BOOL)popup {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    [self changeWebContents:contents];
    isPopup_ = popup;
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

- (void)ensureContentsVisibleInSuperview:(NSView*)superview {
  if (!contents_)
    return;

  ScopedCAActionDisabler disabler;
  NSView* contentsContainer = [self view];
  NSArray* subviews = [contentsContainer subviews];
  NSView* contentsNativeView = nil;

  if ([self shouldResizeContentView])
    [contentsNativeView setFrame:[self frameForContentsViewIn:superview]];

  if ([subviews count] == 0) {
    [contentsContainer addSubview:contentsNativeView];
  } else if ([subviews objectAtIndex:0] != contentsNativeView) {
    [contentsContainer replaceSubview:[subviews objectAtIndex:0]
                                 with:contentsNativeView];
  }

  [contentsNativeView setAutoresizingMask:NSViewNotSizable];
  [contentsContainer setFrame:[superview bounds]];
  [superview addSubview:contentsContainer];
  [contentsNativeView setAutoresizingMask:NSViewWidthSizable|
                                          NSViewHeightSizable];

  [contentsContainer setNeedsDisplay:YES];
}

- (void)updateFullscreenWidgetFrame {
}

- (void)changeWebContents:(WebContents*)newContents {
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
    if (rvh->GetWidget()->GetView() &&
        rvh->GetWidget()->GetView()->HasFocus()) {
      rvh->GetWidget()->Blur();
      return;
    }
    WebContents* devtools = DevToolsWindow::GetInTabWebContents(
        [self webContents], NULL);
    if (devtools) {
      content::RenderViewHost* devtoolsView = devtools->GetRenderViewHost();
      if (devtoolsView && devtoolsView->GetWidget()->GetView() &&
          devtoolsView->GetWidget()->GetView()->HasFocus()) {
        devtoolsView->GetWidget()->Blur();
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
    [self ensureContentsVisibleInSuperview:[[self view] superview]];
  }
}

- (void)toggleFullscreenWidget:(BOOL)enterFullscreen {
}

- (BOOL)contentsInFullscreenCaptureMode {
  return NO;
}

- (NSRect)frameForContentsViewIn:(NSView*)container {
  gfx::Rect rect([container bounds]);

  // In most cases, the contents view is simply sized to fill the container
  // view's bounds. Only WebContentses that are in fullscreen mode and being
  // screen-captured will engage the special layout/sizing behavior.
  return NSRectFromCGRect(rect.ToCGRect());
}

- (BOOL)shouldResizeContentView {
  return YES;
}

- (BOOL)isPopup {
  return isPopup_;
}

@end

@implementation TabContentsController (SeparateFullscreenWindowDelegate)

- (NSView*)createScreenshotView {
  return nil;
}

// Creates a new window with the tab without detaching it from its source
// window.
- (NSWindow*)createSeparateWindowForTab:(WebContents*)separatedTab {
  return nil;
}
@end
