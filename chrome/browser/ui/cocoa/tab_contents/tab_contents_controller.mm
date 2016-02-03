// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"

#include <stdint.h>

#include <utility>

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "chrome/browser/devtools/devtools_window.h"
#import "chrome/browser/themes/theme_properties.h"
#import "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "chrome/browser/ui/view_ids.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/cocoa/animation_utils.h"
#import "ui/base/cocoa/nscolor_additions.h"
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

  void DidShowFullscreenWidget(int routing_id) override {
    [controller_ toggleFullscreenWidget:YES];
  }

  void DidDestroyFullscreenWidget(int routing_id) override {
    [controller_ toggleFullscreenWidget:NO];
  }

  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override {
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
- (void)updateBackgroundColor;
@end

@implementation TabContentsContainerView

- (id)initWithDelegate:(TabContentsController*)delegate {
  if ((self = [super initWithFrame:NSZeroRect])) {
    delegate_ = delegate;
    ScopedCAActionDisabler disabler;
    base::scoped_nsobject<CALayer> layer([[CALayer alloc] init]);
    [self setLayer:layer];
    [self setWantsLayer:YES];
    [self updateBackgroundColor];
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
  // windows or opening new tabs), so ensure that the flash be the theme
  // background color in those cases.
  NSColor* backgroundColor = nil;
  const ui::ThemeProvider* theme = [[self window] themeProvider];
  if (theme)
    backgroundColor = theme->GetNSColor(ThemeProperties::COLOR_NTP_BACKGROUND);
  if (!backgroundColor)
    backgroundColor = [NSColor whiteColor];

  // If the page is in fullscreen tab capture mode, change the background color
  // to be a dark tint of the new tab page's background color.
  if ([delegate_ contentsInFullscreenCaptureMode]) {
    const float kDarknessFraction = 0.80f;
    return [backgroundColor blendedColorWithFraction:kDarknessFraction
                                             ofColor:[NSColor blackColor]];
  } else {
    return backgroundColor;
  }
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
  [self updateBackgroundColor];
}

- (void)updateBackgroundColor {
  ScopedCAActionDisabler disabler;
  [[self layer] setBackgroundColor:[[self computeBackgroundColor] cr_CGColor]];
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

@implementation TabContentsController
@synthesize webContents = contents_;
@synthesize blockFullscreenResize = blockFullscreenResize_;

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
  if (!isEmbeddingFullscreenWidget_ || !blockFullscreenResize_)
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
}

- (void)updateFullscreenWidgetFrame {
  // This should only apply if a fullscreen widget is embedded.
  if (!isEmbeddingFullscreenWidget_ || blockFullscreenResize_)
    return;

  content::RenderWidgetHostView* const fullscreenView =
      contents_->GetFullscreenRenderWidgetHostView();
  if (fullscreenView)
    [fullscreenView->GetNativeView() setFrame:[self frameForContentsView]];
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
    const int64_t x = static_cast<int64_t>(captureSize.width()) * rect.height();
    const int64_t y = static_cast<int64_t>(captureSize.height()) * rect.width();
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
