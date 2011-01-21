// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_controller.h"

#include <Carbon/Carbon.h>

#include "app/l10n_util.h"
#include "app/l10n_util_mac.h"
#include "app/mac/scoped_nsdisable_screen_updates.h"
#include "app/mac/nsimage_cache.h"
#include "base/mac/mac_util.h"
#import "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"  // IDC_*
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_ui_util_mac.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view_mac.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/background_gradient_view.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_editor_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#import "chrome/browser/ui/cocoa/browser_window_controller_private.h"
#import "chrome/browser/ui/cocoa/dev_tools_controller.h"
#import "chrome/browser/ui/cocoa/download/download_shelf_controller.h"
#import "chrome/browser/ui/cocoa/event_utils.h"
#import "chrome/browser/ui/cocoa/fast_resize_view.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/focus_tracker.h"
#import "chrome/browser/ui/cocoa/fullscreen_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen_window.h"
#import "chrome/browser/ui/cocoa/image_utils.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field_editor.h"
#import "chrome/browser/ui/cocoa/previewable_contents_controller.h"
#import "chrome/browser/ui/cocoa/sad_tab_controller.h"
#import "chrome/browser/ui/cocoa/sidebar_controller.h"
#import "chrome/browser/ui/cocoa/status_bubble_mac.h"
#import "chrome/browser/ui/cocoa/tab_contents_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_view.h"
#import "chrome/browser/ui/cocoa/tabs/tab_view.h"
#import "chrome/browser/ui/cocoa/tabpose_window.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/dock_info.h"
#include "chrome/browser/ui/toolbar/encoding_menu_controller.h"
#include "chrome/browser/ui/window_sizer.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

// ORGANIZATION: This is a big file. It is (in principle) organized as follows
// (in order):
// 1. Interfaces. Very short, one-time-use classes may include an implementation
//    immediately after their interface.
// 2. The general implementation section, ordered as follows:
//      i. Public methods and overrides.
//     ii. Overrides/implementations of undocumented methods.
//    iii. Delegate methods for various protocols, formal and informal, to which
//        |BrowserWindowController| conforms.
// 3. (temporary) Implementation sections for various categories.
//
// Private methods are defined and implemented separately in
// browser_window_controller_private.{h,mm}.
//
// Not all of the above guidelines are followed and more (re-)organization is
// needed. BUT PLEASE TRY TO KEEP THIS FILE ORGANIZED. I'd rather re-organize as
// little as possible, since doing so messes up the file's history.
//
// TODO(viettrungluu): [crbug.com/35543] on-going re-organization, splitting
// things into multiple files -- the plan is as follows:
// - in general, everything stays in browser_window_controller.h, but is split
//   off into categories (see below)
// - core stuff stays in browser_window_controller.mm
// - ... overrides also stay (without going into a category, in particular)
// - private stuff which everyone needs goes into
//   browser_window_controller_private.{h,mm}; if no one else needs them, they
//   can go in individual files (see below)
// - area/task-specific stuff go in browser_window_controller_<area>.mm
// - ... in categories called "(<Area>)" or "(<PrivateArea>)"
// Plan of action:
// - first re-organize into categories
// - then split into files

// Notes on self-inflicted (not user-inflicted) window resizing and moving:
//
// When the bookmark bar goes from hidden to shown (on a non-NTP) page, or when
// the download shelf goes from hidden to shown, we grow the window downwards in
// order to maintain a constant content area size. When either goes from shown
// to hidden, we consequently shrink the window from the bottom, also to keep
// the content area size constant. To keep things simple, if the window is not
// entirely on-screen, we don't grow/shrink the window.
//
// The complications come in when there isn't enough room (on screen) below the
// window to accomodate the growth. In this case, we grow the window first
// downwards, and then upwards. So, when it comes to shrinking, we do the
// opposite: shrink from the top by the amount by which we grew at the top, and
// then from the bottom -- unless the user moved/resized/zoomed the window, in
// which case we "reset state" and just shrink from the bottom.
//
// A further complication arises due to the way in which "zoom" ("maximize")
// works on Mac OS X. Basically, for our purposes, a window is "zoomed" whenever
// it occupies the full available vertical space. (Note that the green zoom
// button does not track zoom/unzoomed state per se, but basically relies on
// this heuristic.) We don't, in general, want to shrink the window if the
// window is zoomed (scenario: window is zoomed, download shelf opens -- which
// doesn't cause window growth, download shelf closes -- shouldn't cause the
// window to become unzoomed!). However, if we grew the window
// (upwards/downwards) to become zoomed in the first place, we *should* shrink
// the window by the amounts by which we grew (scenario: window occupies *most*
// of vertical space, download shelf opens causing growth so that window
// occupies all of vertical space -- i.e., window is effectively zoomed,
// download shelf closes -- should return the window to its previous state).
//
// A major complication is caused by the way grows/shrinks are handled and
// animated. Basically, the BWC doesn't see the global picture, but it sees
// grows and shrinks in small increments (as dictated by the animation). Thus
// window growth/shrinkage (at the top/bottom) have to be tracked incrementally.
// Allowing shrinking from the zoomed state also requires tracking: We check on
// any shrink whether we're both zoomed and have previously grown -- if so, we
// set a flag, and constrain any resize by the allowed amounts. On further
// shrinks, we check the flag (since the size/position of the window will no
// longer indicate that the window is shrinking from an apparent zoomed state)
// and if it's set we continue to constrain the resize.


@interface NSWindow(NSPrivateApis)
// Note: These functions are private, use -[NSObject respondsToSelector:]
// before calling them.

- (void)setBottomCornerRounded:(BOOL)rounded;

- (NSRect)_growBoxRect;

@end


// IncognitoImageView subclasses NSView to allow mouse events to pass through it
// so you can drag the window by dragging on the spy guy.
@interface IncognitoImageView : NSView {
 @private
  scoped_nsobject<NSImage> image_;
}

- (void)setImage:(NSImage*)image;

@end

@implementation IncognitoImageView

- (BOOL)mouseDownCanMoveWindow {
  return YES;
}

- (void)drawRect:(NSRect)rect {
  [NSGraphicsContext saveGraphicsState];

  scoped_nsobject<NSShadow> shadow([[NSShadow alloc] init]);
  [shadow.get() setShadowColor:[NSColor colorWithCalibratedWhite:0.0
                                                           alpha:0.75]];
  [shadow.get() setShadowOffset:NSMakeSize(0, 0)];
  [shadow.get() setShadowBlurRadius:3.0];
  [shadow.get() set];

  [image_.get() drawInRect:[self bounds]
                  fromRect:NSZeroRect
                 operation:NSCompositeSourceOver
                  fraction:1.0
              neverFlipped:YES];
  [NSGraphicsContext restoreGraphicsState];
}

- (void)setImage:(NSImage*)image {
  image_.reset([image retain]);
}

@end


@implementation BrowserWindowController

+ (BrowserWindowController*)browserWindowControllerForWindow:(NSWindow*)window {
  while (window) {
    id controller = [window windowController];
    if ([controller isKindOfClass:[BrowserWindowController class]])
      return (BrowserWindowController*)controller;
    window = [window parentWindow];
  }
  return nil;
}

+ (BrowserWindowController*)browserWindowControllerForView:(NSView*)view {
  NSWindow* window = [view window];
  return [BrowserWindowController browserWindowControllerForWindow:window];
}

// Load the browser window nib and do any Cocoa-specific initialization.
// Takes ownership of |browser|. Note that the nib also sets this controller
// up as the window's delegate.
- (id)initWithBrowser:(Browser*)browser {
  return [self initWithBrowser:browser takeOwnership:YES];
}

// Private(TestingAPI) init routine with testing options.
- (id)initWithBrowser:(Browser*)browser takeOwnership:(BOOL)ownIt {
  // Use initWithWindowNibPath:: instead of initWithWindowNibName: so we
  // can override it in a unit test.
  NSString* nibpath = [base::mac::MainAppBundle()
                        pathForResource:@"BrowserWindow"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    DCHECK(browser);
    initializing_ = YES;
    browser_.reset(browser);
    ownsBrowser_ = ownIt;
    NSWindow* window = [self window];
    windowShim_.reset(new BrowserWindowCocoa(browser, self, window));

    // Create the bar visibility lock set; 10 is arbitrary, but should hopefully
    // be big enough to hold all locks that'll ever be needed.
    barVisibilityLocks_.reset([[NSMutableSet setWithCapacity:10] retain]);

    // Sets the window to not have rounded corners, which prevents
    // the resize control from being inset slightly and looking ugly.
    if ([window respondsToSelector:@selector(setBottomCornerRounded:)])
      [window setBottomCornerRounded:NO];

    // Get the most appropriate size for the window, then enforce the
    // minimum width and height. The window shim will handle flipping
    // the coordinates for us so we can use it to save some code.
    // Note that this may leave a significant portion of the window
    // offscreen, but there will always be enough window onscreen to
    // drag the whole window back into view.
    NSSize minSize = [[self window] minSize];
    gfx::Rect desiredContentRect = browser_->GetSavedWindowBounds();
    gfx::Rect windowRect = desiredContentRect;
    if (windowRect.width() < minSize.width)
      windowRect.set_width(minSize.width);
    if (windowRect.height() < minSize.height)
      windowRect.set_height(minSize.height);

    // When we are given x/y coordinates of 0 on a created popup window, assume
    // none were given by the window.open() command.
    if (browser_->type() & Browser::TYPE_POPUP &&
        windowRect.x() == 0 && windowRect.y() == 0) {
      gfx::Size size = windowRect.size();
      windowRect.set_origin(WindowSizer::GetDefaultPopupOrigin(size));
    }

    // Size and position the window.  Note that it is not yet onscreen.  Popup
    // windows may get resized later on in this function, once the actual size
    // of the toolbar/tabstrip is known.
    windowShim_->SetBounds(windowRect);

    // Puts the incognito badge on the window frame, if necessary.
    [self installIncognitoBadge];

    // Create a sub-controller for the docked devTools and add its view to the
    // hierarchy.  This must happen before the sidebar controller is
    // instantiated.
    devToolsController_.reset(
        [[DevToolsController alloc] initWithDelegate:self]);
    [[devToolsController_ view] setFrame:[[self tabContentArea] bounds]];
    [[self tabContentArea] addSubview:[devToolsController_ view]];

    // Create a sub-controller for the docked sidebar and add its view to the
    // hierarchy.  This must happen before the previewable contents controller
    // is instantiated.
    sidebarController_.reset([[SidebarController alloc] initWithDelegate:self]);
    [[sidebarController_ view] setFrame:[[devToolsController_ view] bounds]];
    [[devToolsController_ view] addSubview:[sidebarController_ view]];

    // Create the previewable contents controller.  This provides the switch
    // view that TabStripController needs.
    previewableContentsController_.reset(
        [[PreviewableContentsController alloc] init]);
    [[previewableContentsController_ view]
        setFrame:[[sidebarController_ view] bounds]];
    [[sidebarController_ view]
        addSubview:[previewableContentsController_ view]];

    // Create a controller for the tab strip, giving it the model object for
    // this window's Browser and the tab strip view. The controller will handle
    // registering for the appropriate tab notifications from the back-end and
    // managing the creation of new tabs.
    [self createTabStripController];

    // Create the infobar container view, so we can pass it to the
    // ToolbarController.
    infoBarContainerController_.reset(
        [[InfoBarContainerController alloc] initWithResizeDelegate:self]);
    [[[self window] contentView] addSubview:[infoBarContainerController_ view]];

    // Create a controller for the toolbar, giving it the toolbar model object
    // and the toolbar view from the nib. The controller will handle
    // registering for the appropriate command state changes from the back-end.
    // Adds the toolbar to the content area.
    toolbarController_.reset([[ToolbarController alloc]
                               initWithModel:browser->toolbar_model()
                                    commands:browser->command_updater()
                                     profile:browser->profile()
                                     browser:browser
                              resizeDelegate:self]);
    [toolbarController_ setHasToolbar:[self hasToolbar]
                       hasLocationBar:[self hasLocationBar]];
    [[[self window] contentView] addSubview:[toolbarController_ view]];

    // Create a sub-controller for the bookmark bar.
    bookmarkBarController_.reset(
        [[BookmarkBarController alloc]
            initWithBrowser:browser_.get()
               initialWidth:NSWidth([[[self window] contentView] frame])
                   delegate:self
             resizeDelegate:self]);

    // Add bookmark bar to the view hierarchy, which also triggers the nib load.
    // The bookmark bar is defined (in the nib) to be bottom-aligned to its
    // parent view (among other things), so position and resize properties don't
    // need to be set.
    [[[self window] contentView] addSubview:[bookmarkBarController_ view]
                                 positioned:NSWindowBelow
                                 relativeTo:[toolbarController_ view]];
    [bookmarkBarController_ setBookmarkBarEnabled:[self supportsBookmarkBar]];

    // We don't want to try and show the bar before it gets placed in its parent
    // view, so this step shoudn't be inside the bookmark bar controller's
    // |-awakeFromNib|.
    [self updateBookmarkBarVisibilityWithAnimation:NO];

    // Allow bar visibility to be changed.
    [self enableBarVisibilityUpdates];

    // Force a relayout of all the various bars.
    [self layoutSubviews];

    // For a popup window, |desiredContentRect| contains the desired height of
    // the content, not of the whole window.  Now that all the views are laid
    // out, measure the current content area size and grow if needed.  The
    // window has not been placed onscreen yet, so this extra resize will not
    // cause visible jank.
    if (browser_->type() & Browser::TYPE_POPUP) {
      CGFloat deltaH = desiredContentRect.height() -
                       NSHeight([[self tabContentArea] frame]);
      // Do not shrink the window, as that may break minimum size invariants.
      if (deltaH > 0) {
        // Convert from tabContentArea coordinates to window coordinates.
        NSSize convertedSize =
            [[self tabContentArea] convertSize:NSMakeSize(0, deltaH)
                                        toView:nil];
        NSRect frame = [[self window] frame];
        frame.size.height += convertedSize.height;
        frame.origin.y -= convertedSize.height;
        [[self window] setFrame:frame display:NO];
      }
    }

    // Create the bridge for the status bubble.
    statusBubble_ = new StatusBubbleMac([self window], self);

    // Register for application hide/unhide notifications.
    [[NSNotificationCenter defaultCenter]
         addObserver:self
            selector:@selector(applicationDidHide:)
                name:NSApplicationDidHideNotification
              object:nil];
    [[NSNotificationCenter defaultCenter]
         addObserver:self
            selector:@selector(applicationDidUnhide:)
                name:NSApplicationDidUnhideNotification
              object:nil];

    // This must be done after the view is added to the window since it relies
    // on the window bounds to determine whether to show buttons or not.
    if ([self hasToolbar])  // Do not create the buttons in popups.
      [toolbarController_ createBrowserActionButtons];

    // We are done initializing now.
    initializing_ = NO;
  }
  return self;
}

- (void)dealloc {
  browser_->CloseAllTabs();
  [downloadShelfController_ exiting];

  // Explicitly release |fullscreenController_| here, as it may call back to
  // this BWC in |-dealloc|.  We are required to call |-exitFullscreen| before
  // releasing the controller.
  [fullscreenController_ exitFullscreen];
  fullscreenController_.reset();

  // Under certain testing configurations we may not actually own the browser.
  if (ownsBrowser_ == NO)
    ignore_result(browser_.release());

  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [super dealloc];
}

- (BrowserWindow*)browserWindow {
  return windowShim_.get();
}

- (ToolbarController*)toolbarController {
  return toolbarController_.get();
}

- (TabStripController*)tabStripController {
  return tabStripController_.get();
}

- (StatusBubbleMac*)statusBubble {
  return statusBubble_;
}

- (LocationBarViewMac*)locationBarBridge {
  return [toolbarController_ locationBarBridge];
}

- (void)destroyBrowser {
  [NSApp removeWindowsItem:[self window]];

  // We need the window to go away now.
  // We can't actually use |-autorelease| here because there's an embedded
  // run loop in the |-performClose:| which contains its own autorelease pool.
  // Instead call it after a zero-length delay, which gets us back to the main
  // event loop.
  [self performSelector:@selector(autorelease)
             withObject:nil
             afterDelay:0];
}

// Called when the window meets the criteria to be closed (ie,
// |-windowShouldClose:| returns YES). We must be careful to preserve the
// semantics of BrowserWindow::Close() and not call the Browser's dtor directly
// from this method.
- (void)windowWillClose:(NSNotification*)notification {
  DCHECK_EQ([notification object], [self window]);
  DCHECK(browser_->tabstrip_model()->empty());
  [savedRegularWindow_ close];
  // We delete statusBubble here because we need to kill off the dependency
  // that its window has on our window before our window goes away.
  delete statusBubble_;
  statusBubble_ = NULL;
  // We can't actually use |-autorelease| here because there's an embedded
  // run loop in the |-performClose:| which contains its own autorelease pool.
  // Instead call it after a zero-length delay, which gets us back to the main
  // event loop.
  [self performSelector:@selector(autorelease)
             withObject:nil
             afterDelay:0];
}

- (void)attachConstrainedWindow:(ConstrainedWindowMac*)window {
  [tabStripController_ attachConstrainedWindow:window];
}

- (void)removeConstrainedWindow:(ConstrainedWindowMac*)window {
  [tabStripController_ removeConstrainedWindow:window];
}

- (BOOL)canAttachConstrainedWindow {
  return ![previewableContentsController_ isShowingPreview];
}

- (void)updateDevToolsForContents:(TabContents*)contents {
  [devToolsController_ updateDevToolsForTabContents:contents
                                        withProfile:browser_->profile()];
  [devToolsController_ ensureContentsVisible];
}

- (void)updateSidebarForContents:(TabContents*)contents {
  [sidebarController_ updateSidebarForTabContents:contents];
  [sidebarController_ ensureContentsVisible];
}

// Called when the user wants to close a window or from the shutdown process.
// The Browser object is in control of whether or not we're allowed to close. It
// may defer closing due to several states, such as onUnload handlers needing to
// be fired. If closing is deferred, the Browser will handle the processing
// required to get us to the closing state and (by watching for all the tabs
// going away) will again call to close the window when it's finally ready.
- (BOOL)windowShouldClose:(id)sender {
  // Disable updates while closing all tabs to avoid flickering.
  app::mac::ScopedNSDisableScreenUpdates disabler;
  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!browser_->ShouldCloseWindow())
    return NO;

  // saveWindowPositionIfNeeded: only works if we are the last active
  // window, but orderOut: ends up activating another window, so we
  // have to save the window position before we call orderOut:.
  [self saveWindowPositionIfNeeded];

  if (!browser_->tabstrip_model()->empty()) {
    // Tab strip isn't empty.  Hide the frame (so it appears to have closed
    // immediately) and close all the tabs, allowing the renderers to shut
    // down. When the tab strip is empty we'll be called back again.
    [[self window] orderOut:self];
    browser_->OnWindowClosing();
    return NO;
  }

  // the tab strip is empty, it's ok to close the window
  return YES;
}

// Called right after our window became the main window.
- (void)windowDidBecomeMain:(NSNotification*)notification {
  BrowserList::SetLastActive(browser_.get());
  [self saveWindowPositionIfNeeded];

  // TODO(dmaclach): Instead of redrawing the whole window, views that care
  // about the active window state should be registering for notifications.
  [[self window] setViewsNeedDisplay:YES];

  // TODO(viettrungluu): For some reason, the above doesn't suffice.
  if ([self isFullscreen])
    [floatingBarBackingView_ setNeedsDisplay:YES];  // Okay even if nil.
}

- (void)windowDidResignMain:(NSNotification*)notification {
  // TODO(dmaclach): Instead of redrawing the whole window, views that care
  // about the active window state should be registering for notifications.
  [[self window] setViewsNeedDisplay:YES];

  // TODO(viettrungluu): For some reason, the above doesn't suffice.
  if ([self isFullscreen])
    [floatingBarBackingView_ setNeedsDisplay:YES];  // Okay even if nil.
}

// Called when we are activated (when we gain focus).
- (void)windowDidBecomeKey:(NSNotification*)notification {
  // We need to activate the controls (in the "WebView"). To do this, get the
  // selected TabContents's RenderWidgetHostViewMac and tell it to activate.
  if (TabContents* contents = browser_->GetSelectedTabContents()) {
    if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
      rwhv->SetActive(true);
  }
}

// Called when we are deactivated (when we lose focus).
- (void)windowDidResignKey:(NSNotification*)notification {
  // If our app is still active and we're still the key window, ignore this
  // message, since it just means that a menu extra (on the "system status bar")
  // was activated; we'll get another |-windowDidResignKey| if we ever really
  // lose key window status.
  if ([NSApp isActive] && ([NSApp keyWindow] == [self window]))
    return;

  // We need to deactivate the controls (in the "WebView"). To do this, get the
  // selected TabContents's RenderWidgetHostView and tell it to deactivate.
  if (TabContents* contents = browser_->GetSelectedTabContents()) {
    if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
      rwhv->SetActive(false);
  }
}

// Called when we have been minimized.
- (void)windowDidMiniaturize:(NSNotification *)notification {
  // Let the selected RenderWidgetHostView know, so that it can tell plugins.
  if (TabContents* contents = browser_->GetSelectedTabContents()) {
    if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
      rwhv->SetWindowVisibility(false);
  }
}

// Called when we have been unminimized.
- (void)windowDidDeminiaturize:(NSNotification *)notification {
  // Let the selected RenderWidgetHostView know, so that it can tell plugins.
  if (TabContents* contents = browser_->GetSelectedTabContents()) {
    if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
      rwhv->SetWindowVisibility(true);
  }
}

// Called when the application has been hidden.
- (void)applicationDidHide:(NSNotification *)notification {
  // Let the selected RenderWidgetHostView know, so that it can tell plugins
  // (unless we are minimized, in which case nothing has really changed).
  if (![[self window] isMiniaturized]) {
    if (TabContents* contents = browser_->GetSelectedTabContents()) {
      if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
        rwhv->SetWindowVisibility(false);
    }
  }
}

// Called when the application has been unhidden.
- (void)applicationDidUnhide:(NSNotification *)notification {
  // Let the selected RenderWidgetHostView know, so that it can tell plugins
  // (unless we are minimized, in which case nothing has really changed).
  if (![[self window] isMiniaturized]) {
    if (TabContents* contents = browser_->GetSelectedTabContents()) {
      if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
        rwhv->SetWindowVisibility(true);
    }
  }
}

// Called when the user clicks the zoom button (or selects it from the Window
// menu) to determine the "standard size" of the window, based on the content
// and other factors. If the current size/location differs nontrivally from the
// standard size, Cocoa resizes the window to the standard size, and saves the
// current size as the "user size". If the current size/location is the same (up
// to a fudge factor) as the standard size, Cocoa resizes the window to the
// saved user size. (It is possible for the two to coincide.) In this way, the
// zoom button acts as a toggle. We determine the standard size based on the
// content, but enforce a minimum width (calculated using the dimensions of the
// screen) to ensure websites with small intrinsic width (such as google.com)
// don't end up with a wee window. Moreover, we always declare the standard
// width to be at least as big as the current width, i.e., we never want zooming
// to the standard width to shrink the window. This is consistent with other
// browsers' behaviour, and is desirable in multi-tab situations. Note, however,
// that the "toggle" behaviour means that the window can still be "unzoomed" to
// the user size.
- (NSRect)windowWillUseStandardFrame:(NSWindow*)window
                        defaultFrame:(NSRect)frame {
  // Forget that we grew the window up (if we in fact did).
  [self resetWindowGrowthState];

  // |frame| already fills the current screen. Never touch y and height since we
  // always want to fill vertically.

  // If the shift key is down, maximize. Hopefully this should make the
  // "switchers" happy.
  if ([[NSApp currentEvent] modifierFlags] & NSShiftKeyMask) {
    return frame;
  }

  // To prevent strange results on portrait displays, the basic minimum zoomed
  // width is the larger of: 60% of available width, 60% of available height
  // (bounded by available width).
  const CGFloat kProportion = 0.6;
  CGFloat zoomedWidth =
      std::max(kProportion * frame.size.width,
               std::min(kProportion * frame.size.height, frame.size.width));

  TabContents* contents = browser_->GetSelectedTabContents();
  if (contents) {
    // If the intrinsic width is bigger, then make it the zoomed width.
    const int kScrollbarWidth = 16;  // TODO(viettrungluu): ugh.
    TabContentsViewMac* tab_contents_view =
        static_cast<TabContentsViewMac*>(contents->view());
    CGFloat intrinsicWidth = static_cast<CGFloat>(
        tab_contents_view->preferred_width() + kScrollbarWidth);
    zoomedWidth = std::max(zoomedWidth,
                           std::min(intrinsicWidth, frame.size.width));
  }

  // Never shrink from the current size on zoom (see above).
  NSRect currentFrame = [[self window] frame];
  zoomedWidth = std::max(zoomedWidth, currentFrame.size.width);

  // |frame| determines our maximum extents. We need to set the origin of the
  // frame -- and only move it left if necessary.
  if (currentFrame.origin.x + zoomedWidth > frame.origin.x + frame.size.width)
    frame.origin.x = frame.origin.x + frame.size.width - zoomedWidth;
  else
    frame.origin.x = currentFrame.origin.x;

  // Set the width. Don't touch y or height.
  frame.size.width = zoomedWidth;

  return frame;
}

- (void)activate {
  [[self window] makeKeyAndOrderFront:self];
}

// Determine whether we should let a window zoom/unzoom to the given |newFrame|.
// We avoid letting unzoom move windows between screens, because it's really
// strange and unintuitive.
- (BOOL)windowShouldZoom:(NSWindow*)window toFrame:(NSRect)newFrame {
  // Figure out which screen |newFrame| is on.
  NSScreen* newScreen = nil;
  CGFloat newScreenOverlapArea = 0.0;
  for (NSScreen* screen in [NSScreen screens]) {
    NSRect overlap = NSIntersectionRect(newFrame, [screen frame]);
    CGFloat overlapArea = overlap.size.width * overlap.size.height;
    if (overlapArea > newScreenOverlapArea) {
      newScreen = screen;
      newScreenOverlapArea = overlapArea;
    }
  }
  // If we're somehow not on any screen, allow the zoom.
  if (!newScreen)
    return YES;

  // If the new screen is the current screen, we can return a definitive YES.
  // Note: This check is not strictly necessary, but just short-circuits in the
  // "no-brainer" case. To test the complicated logic below, comment this out!
  NSScreen* curScreen = [window screen];
  if (newScreen == curScreen)
    return YES;

  // Worry a little: What happens when a window is on two (or more) screens?
  // E.g., what happens in a 50-50 scenario? Cocoa may reasonably elect to zoom
  // to the other screen rather than staying on the officially current one. So
  // we compare overlaps with the current window frame, and see if Cocoa's
  // choice was reasonable (allowing a small rounding error). This should
  // hopefully avoid us ever erroneously denying a zoom when a window is on
  // multiple screens.
  NSRect curFrame = [window frame];
  NSRect newScrIntersectCurFr = NSIntersectionRect([newScreen frame], curFrame);
  NSRect curScrIntersectCurFr = NSIntersectionRect([curScreen frame], curFrame);
  if (newScrIntersectCurFr.size.width*newScrIntersectCurFr.size.height >=
      (curScrIntersectCurFr.size.width*curScrIntersectCurFr.size.height - 1.0))
    return YES;

  // If it wasn't reasonable, return NO.
  return NO;
}

// Adjusts the window height by the given amount.
- (void)adjustWindowHeightBy:(CGFloat)deltaH {
  // By not adjusting the window height when initializing, we can ensure that
  // the window opens with the same size that was saved on close.
  if (initializing_ || [self isFullscreen] || deltaH == 0)
    return;

  NSWindow* window = [self window];
  NSRect windowFrame = [window frame];
  NSRect workarea = [[window screen] visibleFrame];

  // If the window is not already fully in the workarea, do not adjust its frame
  // at all.
  if (!NSContainsRect(workarea, windowFrame))
    return;

  // Record the position of the top/bottom of the window, so we can easily check
  // whether we grew the window upwards/downwards.
  CGFloat oldWindowMaxY = NSMaxY(windowFrame);
  CGFloat oldWindowMinY = NSMinY(windowFrame);

  // We are "zoomed" if we occupy the full vertical space.
  bool isZoomed = (windowFrame.origin.y == workarea.origin.y &&
                   windowFrame.size.height == workarea.size.height);

  // If we're shrinking the window....
  if (deltaH < 0) {
    bool didChange = false;

    // Don't reset if not currently zoomed since shrinking can take several
    // steps!
    if (isZoomed)
      isShrinkingFromZoomed_ = YES;

    // If we previously grew at the top, shrink as much as allowed at the top
    // first.
    if (windowTopGrowth_ > 0) {
      CGFloat shrinkAtTopBy = MIN(-deltaH, windowTopGrowth_);
      windowFrame.size.height -= shrinkAtTopBy;  // Shrink the window.
      deltaH += shrinkAtTopBy;            // Update the amount left to shrink.
      windowTopGrowth_ -= shrinkAtTopBy;  // Update the growth state.
      didChange = true;
    }

    // Similarly for the bottom (not an "else if" since we may have to
    // simultaneously shrink at both the top and at the bottom). Note that
    // |deltaH| may no longer be nonzero due to the above.
    if (deltaH < 0 && windowBottomGrowth_ > 0) {
      CGFloat shrinkAtBottomBy = MIN(-deltaH, windowBottomGrowth_);
      windowFrame.origin.y += shrinkAtBottomBy;     // Move the window up.
      windowFrame.size.height -= shrinkAtBottomBy;  // Shrink the window.
      deltaH += shrinkAtBottomBy;               // Update the amount left....
      windowBottomGrowth_ -= shrinkAtBottomBy;  // Update the growth state.
      didChange = true;
    }

    // If we're shrinking from zoomed but we didn't change the top or bottom
    // (since we've reached the limits imposed by |window...Growth_|), then stop
    // here. Don't reset |isShrinkingFromZoomed_| since we might get called
    // again for the same shrink.
    if (isShrinkingFromZoomed_ && !didChange)
      return;
  } else {
    isShrinkingFromZoomed_ = NO;

    // Don't bother with anything else.
    if (isZoomed)
      return;
  }

  // Shrinking from zoomed is handled above (and is constrained by
  // |window...Growth_|).
  if (!isShrinkingFromZoomed_) {
    // Resize the window down until it hits the bottom of the workarea, then if
    // needed continue resizing upwards.  Do not resize the window to be taller
    // than the current workarea.
    // Resize the window as requested, keeping the top left corner fixed.
    windowFrame.origin.y -= deltaH;
    windowFrame.size.height += deltaH;

    // If the bottom left corner is now outside the visible frame, move the
    // window up to make it fit, but make sure not to move the top left corner
    // out of the visible frame.
    if (windowFrame.origin.y < workarea.origin.y) {
      windowFrame.origin.y = workarea.origin.y;
      windowFrame.size.height =
          std::min(windowFrame.size.height, workarea.size.height);
    }

    // Record (if applicable) how much we grew the window in either direction.
    // (N.B.: These only record growth, not shrinkage.)
    if (NSMaxY(windowFrame) > oldWindowMaxY)
      windowTopGrowth_ += NSMaxY(windowFrame) - oldWindowMaxY;
    if (NSMinY(windowFrame) < oldWindowMinY)
      windowBottomGrowth_ += oldWindowMinY - NSMinY(windowFrame);
  }

  // Disable subview resizing while resizing the window, or else we will get
  // unwanted renderer resizes.  The calling code must call layoutSubviews to
  // make things right again.
  NSView* contentView = [window contentView];
  [contentView setAutoresizesSubviews:NO];
  [window setFrame:windowFrame display:NO];
  [contentView setAutoresizesSubviews:YES];
}

// Main method to resize browser window subviews.  This method should be called
// when resizing any child of the content view, rather than resizing the views
// directly.  If the view is already the correct height, does not force a
// relayout.
- (void)resizeView:(NSView*)view newHeight:(CGFloat)height {
  // We should only ever be called for one of the following four views.
  // |downloadShelfController_| may be nil. If we are asked to size the bookmark
  // bar directly, its superview must be this controller's content view.
  DCHECK(view);
  DCHECK(view == [toolbarController_ view] ||
         view == [infoBarContainerController_ view] ||
         view == [downloadShelfController_ view] ||
         view == [bookmarkBarController_ view]);

  // Change the height of the view and call |-layoutSubViews|. We set the height
  // here without regard to where the view is on the screen or whether it needs
  // to "grow up" or "grow down."  The below call to |-layoutSubviews| will
  // position each view correctly.
  NSRect frame = [view frame];
  if (NSHeight(frame) == height)
    return;

  // Grow or shrink the window by the amount of the height change.  We adjust
  // the window height only in two cases:
  // 1) We are adjusting the height of the bookmark bar and it is currently
  // animating either open or closed.
  // 2) We are adjusting the height of the download shelf.
  //
  // We do not adjust the window height for bookmark bar changes on the NTP.
  BOOL shouldAdjustBookmarkHeight =
      [bookmarkBarController_ isAnimatingBetweenState:bookmarks::kHiddenState
                                             andState:bookmarks::kShowingState];
  if ((shouldAdjustBookmarkHeight && view == [bookmarkBarController_ view]) ||
      view == [downloadShelfController_ view]) {
    [[self window] disableScreenUpdatesUntilFlush];
    CGFloat deltaH = height - frame.size.height;
    [self adjustWindowHeightBy:deltaH];
  }

  frame.size.height = height;
  // TODO(rohitrao): Determine if calling setFrame: twice is bad.
  [view setFrame:frame];
  [self layoutSubviews];
}

- (void)setAnimationInProgress:(BOOL)inProgress {
  [[self tabContentArea] setFastResizeMode:inProgress];
}

// Update a toggle state for an NSMenuItem if modified.
// Take care to ensure |item| looks like a NSMenuItem.
// Called by validateUserInterfaceItem:.
- (void)updateToggleStateWithTag:(NSInteger)tag forItem:(id)item {
  if (![item respondsToSelector:@selector(state)] ||
      ![item respondsToSelector:@selector(setState:)])
    return;

  // On Windows this logic happens in bookmark_bar_view.cc.  On the
  // Mac we're a lot more MVC happy so we've moved it into a
  // controller.  To be clear, this simply updates the menu item; it
  // does not display the bookmark bar itself.
  if (tag == IDC_SHOW_BOOKMARK_BAR) {
    bool toggled = windowShim_->IsBookmarkBarVisible();
    NSInteger oldState = [item state];
    NSInteger newState = toggled ? NSOnState : NSOffState;
    if (oldState != newState)
      [item setState:newState];
  }

  // Update the checked/Unchecked state of items in the encoding menu.
  // On Windows, this logic is part of |EncodingMenuModel| in
  // browser/ui/views/toolbar_view.h.
  EncodingMenuController encoding_controller;
  if (encoding_controller.DoesCommandBelongToEncodingMenu(tag)) {
    DCHECK(browser_.get());
    Profile* profile = browser_->profile();
    DCHECK(profile);
    TabContents* current_tab = browser_->GetSelectedTabContents();
    if (!current_tab) {
      return;
    }
    const std::string encoding = current_tab->encoding();

    bool toggled = encoding_controller.IsItemChecked(profile, encoding, tag);
    NSInteger oldState = [item state];
    NSInteger newState = toggled ? NSOnState : NSOffState;
    if (oldState != newState)
      [item setState:newState];
  }
}

- (BOOL)supportsFullscreen {
  // TODO(avi, thakis): GTMWindowSheetController has no api to move
  // tabsheets between windows. Until then, we have to prevent having to
  // move a tabsheet between windows, e.g. no fullscreen toggling
  NSArray* a = [[tabStripController_ sheetController] viewsWithAttachedSheets];
  return [a count] == 0;
}

// Called to validate menu and toolbar items when this window is key. All the
// items we care about have been set with the |-commandDispatch:| or
// |-commandDispatchUsingKeyModifiers:| actions and a target of FirstResponder
// in IB. If it's not one of those, let it continue up the responder chain to be
// handled elsewhere. We pull out the tag as the cross-platform constant to
// differentiate and dispatch the various commands.
// NOTE: we might have to handle state for app-wide menu items,
// although we could cheat and directly ask the app controller if our
// command_updater doesn't support the command. This may or may not be an issue,
// too early to tell.
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];
  BOOL enable = NO;
  if (action == @selector(commandDispatch:) ||
      action == @selector(commandDispatchUsingKeyModifiers:)) {
    NSInteger tag = [item tag];
    if (browser_->command_updater()->SupportsCommand(tag)) {
      // Generate return value (enabled state)
      enable = browser_->command_updater()->IsCommandEnabled(tag);
      switch (tag) {
        case IDC_CLOSE_TAB:
          // Disable "close tab" if we're not the key window or if there's only
          // one tab.
          enable &= [self numberOfTabs] > 1 && [[self window] isKeyWindow];
          break;
        case IDC_FULLSCREEN: {
          enable &= [self supportsFullscreen];
          if ([static_cast<NSObject*>(item) isKindOfClass:[NSMenuItem class]]) {
            NSString* menuTitle = l10n_util::GetNSString(
                [self isFullscreen] ? IDS_EXIT_FULLSCREEN_MAC :
                                      IDS_ENTER_FULLSCREEN_MAC);
            [static_cast<NSMenuItem*>(item) setTitle:menuTitle];
          }
          break;
        }
        case IDC_SYNC_BOOKMARKS:
          enable &= browser_->profile()->IsSyncAccessible();
          sync_ui_util::UpdateSyncItem(item, enable, browser_->profile());
          break;
        default:
          // Special handling for the contents of the Text Encoding submenu. On
          // Mac OS, instead of enabling/disabling the top-level menu item, we
          // enable/disable the submenu's contents (per Apple's HIG).
          EncodingMenuController encoding_controller;
          if (encoding_controller.DoesCommandBelongToEncodingMenu(tag)) {
            enable &= browser_->command_updater()->IsCommandEnabled(
                IDC_ENCODING_MENU) ? YES : NO;
          }
      }

      // If the item is toggleable, find its toggle state and
      // try to update it.  This is a little awkward, but the alternative is
      // to check after a commandDispatch, which seems worse.
      [self updateToggleStateWithTag:tag forItem:item];
    }
  }
  return enable;
}

// Called when the user picks a menu or toolbar item when this window is key.
// Calls through to the browser object to execute the command. This assumes that
// the command is supported and doesn't check, otherwise it would have been
// disabled in the UI in validateUserInterfaceItem:.
- (void)commandDispatch:(id)sender {
  DCHECK(sender);
  // Identify the actual BWC to which the command should be dispatched. It might
  // belong to a background window, yet this controller gets it because it is
  // the foreground window's controller and thus in the responder chain. Some
  // senders don't have this problem (for example, menus only operate on the
  // foreground window), so this is only an issue for senders that are part of
  // windows.
  BrowserWindowController* targetController = self;
  if ([sender respondsToSelector:@selector(window)])
    targetController = [[sender window] windowController];
  DCHECK([targetController isKindOfClass:[BrowserWindowController class]]);
  DCHECK(targetController->browser_.get());
  targetController->browser_->ExecuteCommand([sender tag]);
}

// Same as |-commandDispatch:|, but executes commands using a disposition
// determined by the key flags. If the window is in the background and the
// command key is down, ignore the command key, but process any other modifiers.
- (void)commandDispatchUsingKeyModifiers:(id)sender {
  DCHECK(sender);
  // See comment above for why we do this.
  BrowserWindowController* targetController = self;
  if ([sender respondsToSelector:@selector(window)])
    targetController = [[sender window] windowController];
  DCHECK([targetController isKindOfClass:[BrowserWindowController class]]);
  NSInteger command = [sender tag];
  NSUInteger modifierFlags = [[NSApp currentEvent] modifierFlags];
  if ((command == IDC_RELOAD) &&
      (modifierFlags & (NSShiftKeyMask | NSControlKeyMask))) {
    command = IDC_RELOAD_IGNORING_CACHE;
    // Mask off Shift and Control so they don't affect the disposition below.
    modifierFlags &= ~(NSShiftKeyMask | NSControlKeyMask);
  }
  if (![[sender window] isMainWindow]) {
    // Remove the command key from the flags, it means "keep the window in
    // the background" in this case.
    modifierFlags &= ~NSCommandKeyMask;
  }
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEventWithFlags(
          [NSApp currentEvent], modifierFlags);
  switch (command) {
    case IDC_BACK:
    case IDC_FORWARD:
    case IDC_RELOAD:
    case IDC_RELOAD_IGNORING_CACHE:
      if (disposition == CURRENT_TAB) {
        // Forcibly reset the location bar, since otherwise it won't discard any
        // ongoing user edits, since it doesn't realize this is a user-initiated
        // action.
        [targetController locationBarBridge]->Revert();
      }
  }
  DCHECK(targetController->browser_.get());
  targetController->browser_->ExecuteCommandWithDisposition(command,
                                                            disposition);
}

// Called when another part of the internal codebase needs to execute a
// command.
- (void)executeCommand:(int)command {
  browser_->ExecuteCommandIfEnabled(command);
}

// StatusBubble delegate method: tell the status bubble the frame it should
// position itself in.
- (NSRect)statusBubbleBaseFrame {
  NSView* view = [previewableContentsController_ view];
  return [view convertRect:[view bounds] toView:nil];
}

- (GTMWindowSheetController*)sheetController {
  return [tabStripController_ sheetController];
}

- (void)updateToolbarWithContents:(TabContents*)tab
               shouldRestoreState:(BOOL)shouldRestore {
  [toolbarController_ updateToolbarWithContents:tab
                             shouldRestoreState:shouldRestore];
}

- (void)setStarredState:(BOOL)isStarred {
  [toolbarController_ setStarredState:isStarred];
}

// Accept tabs from a BrowserWindowController with the same Profile.
- (BOOL)canReceiveFrom:(TabWindowController*)source {
  if (![source isKindOfClass:[BrowserWindowController class]]) {
    return NO;
  }

  BrowserWindowController* realSource =
      static_cast<BrowserWindowController*>(source);
  if (browser_->profile() != realSource->browser_->profile()) {
    return NO;
  }

  // Can't drag a tab from a normal browser to a pop-up
  if (browser_->type() != realSource->browser_->type()) {
    return NO;
  }

  return YES;
}

// Move a given tab view to the location of the current placeholder. If there is
// no placeholder, it will go at the end. |controller| is the window controller
// of a tab being dropped from a different window. It will be nil if the drag is
// within the window, otherwise the tab is removed from that window before being
// placed into this one. The implementation will call |-removePlaceholder| since
// the drag is now complete.  This also calls |-layoutTabs| internally so
// clients do not need to call it again.
- (void)moveTabView:(NSView*)view
     fromController:(TabWindowController*)dragController {
  if (dragController) {
    // Moving between windows. Figure out the TabContents to drop into our tab
    // model from the source window's model.
    BOOL isBrowser =
        [dragController isKindOfClass:[BrowserWindowController class]];
    DCHECK(isBrowser);
    if (!isBrowser) return;
    BrowserWindowController* dragBWC = (BrowserWindowController*)dragController;
    int index = [dragBWC->tabStripController_ modelIndexForTabView:view];
    TabContentsWrapper* contents =
        dragBWC->browser_->GetTabContentsWrapperAt(index);
    // The tab contents may have gone away if given a window.close() while it
    // is being dragged. If so, bail, we've got nothing to drop.
    if (!contents)
      return;

    // Convert |view|'s frame (which starts in the source tab strip's coordinate
    // system) to the coordinate system of the destination tab strip. This needs
    // to be done before being detached so the window transforms can be
    // performed.
    NSRect destinationFrame = [view frame];
    NSPoint tabOrigin = destinationFrame.origin;
    tabOrigin = [[dragController tabStripView] convertPoint:tabOrigin
                                                     toView:nil];
    tabOrigin = [[view window] convertBaseToScreen:tabOrigin];
    tabOrigin = [[self window] convertScreenToBase:tabOrigin];
    tabOrigin = [[self tabStripView] convertPoint:tabOrigin fromView:nil];
    destinationFrame.origin = tabOrigin;

    // Before the tab is detached from its originating tab strip, store the
    // pinned state so that it can be maintained between the windows.
    bool isPinned = dragBWC->browser_->tabstrip_model()->IsTabPinned(index);

    // Now that we have enough information about the tab, we can remove it from
    // the dragging window. We need to do this *before* we add it to the new
    // window as this will remove the TabContents' delegate.
    [dragController detachTabView:view];

    // Deposit it into our model at the appropriate location (it already knows
    // where it should go from tracking the drag). Doing this sets the tab's
    // delegate to be the Browser.
    [tabStripController_ dropTabContents:contents
                               withFrame:destinationFrame
                             asPinnedTab:isPinned];
  } else {
    // Moving within a window.
    int index = [tabStripController_ modelIndexForTabView:view];
    [tabStripController_ moveTabFromIndex:index];
  }

  // Remove the placeholder since the drag is now complete.
  [self removePlaceholder];
}

// Tells the tab strip to forget about this tab in preparation for it being
// put into a different tab strip, such as during a drop on another window.
- (void)detachTabView:(NSView*)view {
  int index = [tabStripController_ modelIndexForTabView:view];
  browser_->tabstrip_model()->DetachTabContentsAt(index);
}

- (NSView*)selectedTabView {
  return [tabStripController_ selectedTabView];
}

- (void)setIsLoading:(BOOL)isLoading force:(BOOL)force {
  [toolbarController_ setIsLoading:isLoading force:force];
}

// Make the location bar the first responder, if possible.
- (void)focusLocationBar:(BOOL)selectAll {
  [toolbarController_ focusLocationBar:selectAll];
}

- (void)focusTabContents {
  [[self window] makeFirstResponder:[tabStripController_ selectedTabView]];
}

- (void)layoutTabs {
  [tabStripController_ layoutTabs];
}

- (TabWindowController*)detachTabToNewWindow:(TabView*)tabView {
  // Disable screen updates so that this appears as a single visual change.
  app::mac::ScopedNSDisableScreenUpdates disabler;

  // Fetch the tab contents for the tab being dragged.
  int index = [tabStripController_ modelIndexForTabView:tabView];
  TabContentsWrapper* contents = browser_->GetTabContentsWrapperAt(index);

  // Set the window size. Need to do this before we detach the tab so it's
  // still in the window. We have to flip the coordinates as that's what
  // is expected by the Browser code.
  NSWindow* sourceWindow = [tabView window];
  NSRect windowRect = [sourceWindow frame];
  NSScreen* screen = [sourceWindow screen];
  windowRect.origin.y =
      [screen frame].size.height - windowRect.size.height -
          windowRect.origin.y;
  gfx::Rect browserRect(windowRect.origin.x, windowRect.origin.y,
                        windowRect.size.width, windowRect.size.height);

  NSRect sourceTabRect = [tabView frame];
  NSView* tabStrip = [self tabStripView];

  // Pushes tabView's frame back inside the tabstrip.
  NSSize tabOverflow =
      [self overflowFrom:[tabStrip convertRectToBase:sourceTabRect]
                      to:[tabStrip frame]];
  NSRect tabRect = NSOffsetRect(sourceTabRect,
                                -tabOverflow.width, -tabOverflow.height);

  // Before detaching the tab, store the pinned state.
  bool isPinned = browser_->tabstrip_model()->IsTabPinned(index);

  // Detach it from the source window, which just updates the model without
  // deleting the tab contents. This needs to come before creating the new
  // Browser because it clears the TabContents' delegate, which gets hooked
  // up during creation of the new window.
  browser_->tabstrip_model()->DetachTabContentsAt(index);

  // Create the new window with a single tab in its model, the one being
  // dragged.
  DockInfo dockInfo;
  Browser* newBrowser = browser_->tabstrip_model()->delegate()->
      CreateNewStripWithContents(contents, browserRect, dockInfo, false);

  // Propagate the tab pinned state of the new tab (which is the only tab in
  // this new window).
  newBrowser->tabstrip_model()->SetTabPinned(0, isPinned);

  // Get the new controller by asking the new window for its delegate.
  BrowserWindowController* controller =
      reinterpret_cast<BrowserWindowController*>(
          [newBrowser->window()->GetNativeHandle() delegate]);
  DCHECK(controller && [controller isKindOfClass:[TabWindowController class]]);

  // Force the added tab to the right size (remove stretching.)
  tabRect.size.height = [TabStripController defaultTabHeight];

  // And make sure we use the correct frame in the new view.
  [[controller tabStripController] setFrameOfSelectedTab:tabRect];
  return controller;
}

- (void)insertPlaceholderForTab:(TabView*)tab
                          frame:(NSRect)frame
                      yStretchiness:(CGFloat)yStretchiness {
  [super insertPlaceholderForTab:tab frame:frame yStretchiness:yStretchiness];
  [tabStripController_ insertPlaceholderForTab:tab
                                         frame:frame
                                 yStretchiness:yStretchiness];
}

- (void)removePlaceholder {
  [super removePlaceholder];
  [tabStripController_ insertPlaceholderForTab:nil
                                         frame:NSZeroRect
                                 yStretchiness:0];
}

- (BOOL)isDragSessionActive {
  // The tab can be dragged within the existing tab strip or detached
  // into its own window (then the overlay window will be present).
  return [[self tabStripController] isDragSessionActive] ||
         [self overlayWindow] != nil;
}

- (BOOL)tabDraggingAllowed {
  return [tabStripController_ tabDraggingAllowed];
}

- (BOOL)tabTearingAllowed {
  return ![self isFullscreen];
}

- (BOOL)windowMovementAllowed {
  return ![self isFullscreen];
}

- (BOOL)isTabFullyVisible:(TabView*)tab {
  return [tabStripController_ isTabFullyVisible:tab];
}

- (void)showNewTabButton:(BOOL)show {
  [tabStripController_ showNewTabButton:show];
}

- (BOOL)isBookmarkBarVisible {
  return [bookmarkBarController_ isVisible];
}

- (BOOL)isBookmarkBarAnimating {
  return [bookmarkBarController_ isAnimationRunning];
}

- (void)updateBookmarkBarVisibilityWithAnimation:(BOOL)animate {
  [bookmarkBarController_
      updateAndShowNormalBar:[self shouldShowBookmarkBar]
             showDetachedBar:[self shouldShowDetachedBookmarkBar]
               withAnimation:animate];
}

- (BOOL)isDownloadShelfVisible {
  return downloadShelfController_ != nil &&
      [downloadShelfController_ isVisible];
}

- (DownloadShelfController*)downloadShelf {
  if (!downloadShelfController_.get()) {
    downloadShelfController_.reset([[DownloadShelfController alloc]
        initWithBrowser:browser_.get() resizeDelegate:self]);
    [[[self window] contentView] addSubview:[downloadShelfController_ view]];
    [downloadShelfController_ show:nil];
  }
  return downloadShelfController_;
}

- (void)addFindBar:(FindBarCocoaController*)findBarCocoaController {
  // Shouldn't call addFindBar twice.
  DCHECK(!findBarCocoaController_.get());

  // Create a controller for the findbar.
  findBarCocoaController_.reset([findBarCocoaController retain]);
  NSView *contentView = [[self window] contentView];
  [contentView addSubview:[findBarCocoaController_ view]
               positioned:NSWindowAbove
               relativeTo:[toolbarController_ view]];

  // Place the find bar immediately below the toolbar/attached bookmark bar. In
  // fullscreen mode, it hangs off the top of the screen when the bar is hidden.
  CGFloat maxY = [self placeBookmarkBarBelowInfoBar] ?
      NSMinY([[toolbarController_ view] frame]) :
      NSMinY([[bookmarkBarController_ view] frame]);
  CGFloat maxWidth = NSWidth([contentView frame]);
  [findBarCocoaController_ positionFindBarViewAtMaxY:maxY maxWidth:maxWidth];
}

- (NSWindow*)createFullscreenWindow {
  return [[[FullscreenWindow alloc] initForScreen:[[self window] screen]]
           autorelease];
}

- (NSInteger)numberOfTabs {
  // count() includes pinned tabs.
  return browser_->tabstrip_model()->count();
}

- (BOOL)hasLiveTabs {
  return !browser_->tabstrip_model()->empty();
}

- (NSString*)selectedTabTitle {
  TabContents* contents = browser_->GetSelectedTabContents();
  return base::SysUTF16ToNSString(contents->GetTitle());
}

- (NSRect)regularWindowFrame {
  return [self isFullscreen] ? [savedRegularWindow_ frame] :
                               [[self window] frame];
}

// (Override of |TabWindowController| method.)
- (BOOL)hasTabStrip {
  return [self supportsWindowFeature:Browser::FEATURE_TABSTRIP];
}

// TabContentsControllerDelegate protocol.
- (void)tabContentsViewFrameWillChange:(TabContentsController*)source
                             frameRect:(NSRect)frameRect {
  TabContents* contents = [source tabContents];
  RenderWidgetHostView* render_widget_host_view = contents ?
      contents->GetRenderWidgetHostView() : NULL;
  if (!render_widget_host_view)
    return;

  gfx::Rect reserved_rect;

  NSWindow* window = [self window];
  if ([window respondsToSelector:@selector(_growBoxRect)]) {
    NSView* view = [source view];
    if (view && [view superview]) {
      NSRect windowGrowBoxRect = [window _growBoxRect];
      NSRect viewRect = [[view superview] convertRect:frameRect toView:nil];
      NSRect growBoxRect = NSIntersectionRect(windowGrowBoxRect, viewRect);
      if (!NSIsEmptyRect(growBoxRect)) {
        // Before we return a rect, we need to convert it from window
        // coordinates to content area coordinates and flip the coordinate
        // system.
        // Superview is used here because, first, it's a frame rect, so it is
        // specified in the parent's coordinates and, second, view is not
        // positioned yet.
        growBoxRect = [[view superview] convertRect:growBoxRect fromView:nil];
        growBoxRect.origin.y =
            NSHeight(frameRect) - NSHeight(growBoxRect);
        growBoxRect =
            NSOffsetRect(growBoxRect, -frameRect.origin.x, -frameRect.origin.y);

        reserved_rect =
            gfx::Rect(growBoxRect.origin.x, growBoxRect.origin.y,
                      growBoxRect.size.width, growBoxRect.size.height);
      }
    }
  }

  render_widget_host_view->set_reserved_contents_rect(reserved_rect);
}

// TabStripControllerDelegate protocol.
- (void)onSelectTabWithContents:(TabContents*)contents {
  // Update various elements that are interested in knowing the current
  // TabContents.

  // Update all the UI bits.
  windowShim_->UpdateTitleBar();

  [sidebarController_ updateSidebarForTabContents:contents];
  [devToolsController_ updateDevToolsForTabContents:contents
                                        withProfile:browser_->profile()];

  // Update the bookmark bar.
  // Must do it after sidebar and devtools update, otherwise bookmark bar might
  // call resizeView -> layoutSubviews and cause unnecessary relayout.
  // TODO(viettrungluu): perhaps update to not terminate running animations (if
  // applicable)?
  [self updateBookmarkBarVisibilityWithAnimation:NO];

  [infoBarContainerController_ changeTabContents:contents];

  // Update devTools and sidebar contents after size for all views is set.
  [sidebarController_ ensureContentsVisible];
  [devToolsController_ ensureContentsVisible];
}

- (void)onReplaceTabWithContents:(TabContents*)contents {
  // This is only called when instant results are committed.  Simply remove the
  // preview view; the tab strip controller will reinstall the view as the
  // active view.
  [previewableContentsController_ hidePreview];
  [self updateBookmarkBarVisibilityWithAnimation:NO];
}

- (void)onSelectedTabChange:(TabStripModelObserver::TabChangeType)change {
  // Update titles if this is the currently selected tab and if it isn't just
  // the loading state which changed.
  if (change != TabStripModelObserver::LOADING_ONLY)
    windowShim_->UpdateTitleBar();

  // Update the bookmark bar if this is the currently selected tab and if it
  // isn't just the title which changed. This for transitions between the NTP
  // (showing its floating bookmark bar) and normal web pages (showing no
  // bookmark bar).
  // TODO(viettrungluu): perhaps update to not terminate running animations?
  if (change != TabStripModelObserver::TITLE_NOT_LOADING)
    [self updateBookmarkBarVisibilityWithAnimation:NO];
}

- (void)onTabDetachedWithContents:(TabContents*)contents {
  [infoBarContainerController_ tabDetachedWithContents:contents];
}

- (void)userChangedTheme {
  // TODO(dmaclach): Instead of redrawing the whole window, views that care
  // about the active window state should be registering for notifications.
  [[self window] setViewsNeedDisplay:YES];
}

- (ui::ThemeProvider*)themeProvider {
  return browser_->profile()->GetThemeProvider();
}

- (ThemedWindowStyle)themedWindowStyle {
  ThemedWindowStyle style = 0;
  if (browser_->profile()->IsOffTheRecord())
    style |= THEMED_INCOGNITO;

  Browser::Type type = browser_->type();
  if (type == Browser::TYPE_POPUP)
    style |= THEMED_POPUP;
  else if (type == Browser::TYPE_DEVTOOLS)
    style |= THEMED_DEVTOOLS;

  return style;
}

- (NSPoint)themePatternPhase {
  // Our patterns want to be drawn from the upper left hand corner of the view.
  // Cocoa wants to do it from the lower left of the window.
  //
  // Rephase our pattern to fit this view. Some other views (Tabs, Toolbar etc.)
  // will phase their patterns relative to this so all the views look right.
  //
  // To line up the background pattern with the pattern in the browser window
  // the background pattern for the tabs needs to be moved left by 5 pixels.
  const CGFloat kPatternHorizontalOffset = -5;
  NSView* tabStripView = [self tabStripView];
  NSRect tabStripViewWindowBounds = [tabStripView bounds];
  NSView* windowChromeView = [[[self window] contentView] superview];
  tabStripViewWindowBounds =
      [tabStripView convertRect:tabStripViewWindowBounds
                         toView:windowChromeView];
  NSPoint phase = NSMakePoint(NSMinX(tabStripViewWindowBounds)
                                  + kPatternHorizontalOffset,
                              NSMinY(tabStripViewWindowBounds)
                                  + [TabStripController defaultTabHeight]);
  return phase;
}

- (NSPoint)bookmarkBubblePoint {
  return [toolbarController_ bookmarkBubblePoint];
}

// Show the bookmark bubble (e.g. user just clicked on the STAR).
- (void)showBookmarkBubbleForURL:(const GURL&)url
               alreadyBookmarked:(BOOL)alreadyMarked {
  if (!bookmarkBubbleController_) {
    BookmarkModel* model = browser_->profile()->GetBookmarkModel();
    const BookmarkNode* node = model->GetMostRecentlyAddedNodeForURL(url);
    bookmarkBubbleController_ =
        [[BookmarkBubbleController alloc] initWithParentWindow:[self window]
                                                         model:model
                                                          node:node
                                             alreadyBookmarked:alreadyMarked];
    [bookmarkBubbleController_ showWindow:self];
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(bubbleWindowWillClose:)
                   name:NSWindowWillCloseNotification
                 object:[bookmarkBubbleController_ window]];
  }
}

// Nil out the weak bookmark bubble controller reference.
- (void)bubbleWindowWillClose:(NSNotification*)notification {
  DCHECK([notification object] == [bookmarkBubbleController_ window]);
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center removeObserver:self
                 name:NSWindowWillCloseNotification
               object:[bookmarkBubbleController_ window]];
  bookmarkBubbleController_ = nil;
}

// Handle the editBookmarkNode: action sent from bookmark bubble controllers.
- (void)editBookmarkNode:(id)sender {
  BOOL responds = [sender respondsToSelector:@selector(node)];
  DCHECK(responds);
  if (responds) {
    const BookmarkNode* node = [sender node];
    if (node) {
      // A BookmarkEditorController is a sheet that owns itself, and
      // deallocates itself when closed.
      [[[BookmarkEditorController alloc]
         initWithParentWindow:[self window]
                      profile:browser_->profile()
                       parent:node->GetParent()
                         node:node
                configuration:BookmarkEditor::SHOW_TREE]
        runAsModalSheet];
    }
  }
}

// If the browser is in incognito mode, install the image view to decorate
// the window at the upper right. Use the same base y coordinate as the
// tab strip.
- (void)installIncognitoBadge {
  // Only install if this browser window is OTR and has a tab strip.
  if (!browser_->profile()->IsOffTheRecord() || ![self hasTabStrip])
    return;

  // Install the image into the badge view and size the view appropriately.
  // Hide it for now; positioning and showing will be done by the layout code.
  NSImage* image = app::mac::GetCachedImageWithName(@"otr_icon.pdf");
  incognitoBadge_.reset([[IncognitoImageView alloc] init]);
  [incognitoBadge_ setImage:image];
  [incognitoBadge_ setFrameSize:[image size]];
  [incognitoBadge_ setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [incognitoBadge_ setHidden:YES];

  // Install the view.
  [[[[self window] contentView] superview] addSubview:incognitoBadge_];
}

// Documented in 10.6+, but present starting in 10.5. Called when we get a
// three-finger swipe.
- (void)swipeWithEvent:(NSEvent*)event {
  // Map forwards and backwards to history; left is positive, right is negative.
  unsigned int command = 0;
  if ([event deltaX] > 0.5) {
    command = IDC_BACK;
  } else if ([event deltaX] < -0.5) {
    command = IDC_FORWARD;
  } else if ([event deltaY] > 0.5) {
    // TODO(pinkerton): figure out page-up, http://crbug.com/16305
  } else if ([event deltaY] < -0.5) {
    // TODO(pinkerton): figure out page-down, http://crbug.com/16305
    browser_->ExecuteCommand(IDC_TABPOSE);
  }

  // Ensure the command is valid first (ExecuteCommand() won't do that) and
  // then make it so.
  if (browser_->command_updater()->IsCommandEnabled(command))
    browser_->ExecuteCommandWithDisposition(command,
        event_utils::WindowOpenDispositionFromNSEvent(event));
}

// Documented in 10.6+, but present starting in 10.5. Called repeatedly during
// a pinch gesture, with incremental change values.
- (void)magnifyWithEvent:(NSEvent*)event {
  // The deltaZ difference necessary to trigger a zoom action. Derived from
  // experimentation to find a value that feels reasonable.
  const float kZoomStepValue = 150;

  // Find the (absolute) thresholds on either side of the current zoom factor,
  // then convert those to actual numbers to trigger a zoom in or out.
  // This logic deliberately makes the range around the starting zoom value for
  // the gesture twice as large as the other ranges (i.e., the notches are at
  // ..., -3*step, -2*step, -step, step, 2*step, 3*step, ... but not at 0)
  // so that it's easier to get back to your starting point than it is to
  // overshoot.
  float nextStep = (abs(currentZoomStepDelta_) + 1) * kZoomStepValue;
  float backStep = abs(currentZoomStepDelta_) * kZoomStepValue;
  float zoomInThreshold = (currentZoomStepDelta_ >= 0) ? nextStep : -backStep;
  float zoomOutThreshold = (currentZoomStepDelta_ <= 0) ? -nextStep : backStep;

  unsigned int command = 0;
  totalMagnifyGestureAmount_ += [event deltaZ];
  if (totalMagnifyGestureAmount_ > zoomInThreshold) {
    command = IDC_ZOOM_PLUS;
  } else if (totalMagnifyGestureAmount_ < zoomOutThreshold) {
    command = IDC_ZOOM_MINUS;
  }

  if (command && browser_->command_updater()->IsCommandEnabled(command)) {
    currentZoomStepDelta_ += (command == IDC_ZOOM_PLUS) ? 1 : -1;
    browser_->ExecuteCommandWithDisposition(command,
        event_utils::WindowOpenDispositionFromNSEvent(event));
  }
}

// Documented in 10.6+, but present starting in 10.5. Called at the beginning
// of a gesture.
- (void)beginGestureWithEvent:(NSEvent*)event {
  totalMagnifyGestureAmount_ = 0;
  currentZoomStepDelta_ = 0;
}

// Delegate method called when window is resized.
- (void)windowDidResize:(NSNotification*)notification {
  // Resize (and possibly move) the status bubble. Note that we may get called
  // when the status bubble does not exist.
  if (statusBubble_) {
    statusBubble_->UpdateSizeAndPosition();
  }

  // Let the selected RenderWidgetHostView know, so that it can tell plugins.
  if (TabContents* contents = browser_->GetSelectedTabContents()) {
    if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
      rwhv->WindowFrameChanged();
  }
}

// Handle the openLearnMoreAboutCrashLink: action from SadTabController when
// "Learn more" link in "Aw snap" page (i.e. crash page or sad tab) is
// clicked. Decoupling the action from its target makes unitestting possible.
- (void)openLearnMoreAboutCrashLink:(id)sender {
  if ([sender isKindOfClass:[SadTabController class]]) {
    SadTabController* sad_tab = static_cast<SadTabController*>(sender);
    TabContents* tab_contents = [sad_tab tabContents];
    if (tab_contents) {
      GURL helpUrl =
          google_util::AppendGoogleLocaleParam(GURL(chrome::kCrashReasonURL));
      tab_contents->OpenURL(helpUrl, GURL(), CURRENT_TAB, PageTransition::LINK);
    }
  }
}

// Delegate method called when window did move. (See below for why we don't use
// |-windowWillMove:|, which is called less frequently than |-windowDidMove|
// instead.)
- (void)windowDidMove:(NSNotification*)notification {
  NSWindow* window = [self window];
  NSRect windowFrame = [window frame];
  NSRect workarea = [[window screen] visibleFrame];

  // We reset the window growth state whenever the window is moved out of the
  // work area or away (up or down) from the bottom or top of the work area.
  // Unfortunately, Cocoa sends |-windowWillMove:| too frequently (including
  // when clicking on the title bar to activate), and of course
  // |-windowWillMove| is called too early for us to apply our heuristic. (The
  // heuristic we use for detecting window movement is that if |windowTopGrowth_
  // > 0|, then we should be at the bottom of the work area -- if we're not,
  // we've moved. Similarly for the other side.)
  if (!NSContainsRect(workarea, windowFrame) ||
      (windowTopGrowth_ > 0 && NSMinY(windowFrame) != NSMinY(workarea)) ||
      (windowBottomGrowth_ > 0 && NSMaxY(windowFrame) != NSMaxY(workarea)))
    [self resetWindowGrowthState];

  // Let the selected RenderWidgetHostView know, so that it can tell plugins.
  if (TabContents* contents = browser_->GetSelectedTabContents()) {
    if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
      rwhv->WindowFrameChanged();
  }
}

// Delegate method called when window will be resized; not called for
// |-setFrame:display:|.
- (NSSize)windowWillResize:(NSWindow*)sender toSize:(NSSize)frameSize {
  [self resetWindowGrowthState];
  return frameSize;
}

// Delegate method: see |NSWindowDelegate| protocol.
- (id)windowWillReturnFieldEditor:(NSWindow*)sender toObject:(id)obj {
  // Ask the toolbar controller if it wants to return a custom field editor
  // for the specific object.
  return [toolbarController_ customFieldEditorForObject:obj];
}

// (Needed for |BookmarkBarControllerDelegate| protocol.)
- (void)bookmarkBar:(BookmarkBarController*)controller
 didChangeFromState:(bookmarks::VisualState)oldState
            toState:(bookmarks::VisualState)newState {
  [toolbarController_
      setDividerOpacity:[bookmarkBarController_ toolbarDividerOpacity]];
  [self adjustToolbarAndBookmarkBarForCompression:
          [controller getDesiredToolbarHeightCompression]];
}

// (Needed for |BookmarkBarControllerDelegate| protocol.)
- (void)bookmarkBar:(BookmarkBarController*)controller
willAnimateFromState:(bookmarks::VisualState)oldState
            toState:(bookmarks::VisualState)newState {
  [toolbarController_
      setDividerOpacity:[bookmarkBarController_ toolbarDividerOpacity]];
  [self adjustToolbarAndBookmarkBarForCompression:
          [controller getDesiredToolbarHeightCompression]];
}

// (Private/TestingAPI)
- (void)resetWindowGrowthState {
  windowTopGrowth_ = 0;
  windowBottomGrowth_ = 0;
  isShrinkingFromZoomed_ = NO;
}

- (NSSize)overflowFrom:(NSRect)source
                    to:(NSRect)target {
  // If |source|'s boundary is outside of |target|'s, set its distance
  // to |x|.  Note that |source| can overflow to both side, but we
  // have nothing to do for such case.
  CGFloat x = 0;
  if (NSMaxX(target) < NSMaxX(source)) // |source| overflows to right
    x = NSMaxX(source) - NSMaxX(target);
  else if (NSMinX(source) < NSMinX(target)) // |source| overflows to left
    x = NSMinX(source) - NSMinX(target);

  // Same as |x| above.
  CGFloat y = 0;
  if (NSMaxY(target) < NSMaxY(source))
    y = NSMaxY(source) - NSMaxY(target);
  else if (NSMinY(source) < NSMinY(target))
    y = NSMinY(source) - NSMinY(target);

  return NSMakeSize(x, y);
}

// Override to swap in the correct tab strip controller based on the new
// tab strip mode.
- (void)toggleTabStripDisplayMode {
  [super toggleTabStripDisplayMode];
  [self createTabStripController];
}

- (BOOL)useVerticalTabs {
  return browser_->tabstrip_model()->delegate()->UseVerticalTabs();
}

- (void)showInstant:(TabContents*)previewContents {
  [previewableContentsController_ showPreview:previewContents];
  [self updateBookmarkBarVisibilityWithAnimation:NO];
}

- (void)hideInstant {
  // TODO(rohitrao): Revisit whether or not this method should be called when
  // instant isn't showing.
  if (![previewableContentsController_ isShowingPreview])
    return;

  [previewableContentsController_ hidePreview];
  [self updateBookmarkBarVisibilityWithAnimation:NO];
}

- (NSRect)instantFrame {
  // The view's bounds are in its own coordinate system.  Convert that to the
  // window base coordinate system, then translate it into the screen's
  // coordinate system.
  NSView* view = [previewableContentsController_ view];
  if (!view)
    return NSZeroRect;

  NSRect frame = [view convertRect:[view bounds] toView:nil];
  NSPoint originInScreenCoords =
      [[view window] convertBaseToScreen:frame.origin];
  frame.origin = originInScreenCoords;
  return frame;
}

- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(NSInteger)code
            context:(void*)context {
  [sheet orderOut:self];
}

@end  // @implementation BrowserWindowController


@implementation BrowserWindowController(Fullscreen)

- (void)setFullscreen:(BOOL)fullscreen {
  // The logic in this function is a bit complicated and very carefully
  // arranged.  See the below comments for more details.

  if (fullscreen == [self isFullscreen])
    return;

  if (![self supportsFullscreen])
    return;

  // Fade to black.
  const CGDisplayReservationInterval kFadeDurationSeconds = 0.6;
  Boolean didFadeOut = NO;
  CGDisplayFadeReservationToken token;
  if (CGAcquireDisplayFadeReservation(kFadeDurationSeconds, &token)
      == kCGErrorSuccess) {
    didFadeOut = YES;
    CGDisplayFade(token, kFadeDurationSeconds / 2, kCGDisplayBlendNormal,
        kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, /*synchronous=*/true);
  }

  // Close the bookmark bubble, if it's open.  We use |-ok:| instead of
  // |-cancel:| or |-close| because that matches the behavior when the bubble
  // loses key status.
  [bookmarkBubbleController_ ok:self];

  // Save the current first responder so we can restore after views are moved.
  NSWindow* window = [self window];
  scoped_nsobject<FocusTracker> focusTracker(
      [[FocusTracker alloc] initWithWindow:window]);
  BOOL showDropdown = [self floatingBarHasFocus];

  // While we move views (and focus) around, disable any bar visibility changes.
  [self disableBarVisibilityUpdates];

  // If we're entering fullscreen, create the fullscreen controller.  If we're
  // exiting fullscreen, kill the controller.
  if (fullscreen) {
    fullscreenController_.reset([[FullscreenController alloc]
                                  initWithBrowserController:self]);
  } else {
    [fullscreenController_ exitFullscreen];
    fullscreenController_.reset();
  }

  // Destroy the tab strip's sheet controller.  We will recreate it in the new
  // window when needed.
  [tabStripController_ destroySheetController];

  // Retain the tab strip view while we remove it from its superview.
  scoped_nsobject<NSView> tabStripView;
  if ([self hasTabStrip] && ![self useVerticalTabs]) {
    tabStripView.reset([[self tabStripView] retain]);
    [tabStripView removeFromSuperview];
  }

  // Ditto for the content view.
  scoped_nsobject<NSView> contentView([[window contentView] retain]);
  // Disable autoresizing of subviews while we move views around. This prevents
  // spurious renderer resizes.
  [contentView setAutoresizesSubviews:NO];
  [contentView removeFromSuperview];

  NSWindow* destWindow = nil;
  if (fullscreen) {
    DCHECK(!savedRegularWindow_);
    savedRegularWindow_ = [window retain];
    destWindow = [self createFullscreenWindow];
  } else {
    DCHECK(savedRegularWindow_);
    destWindow = [savedRegularWindow_ autorelease];
    savedRegularWindow_ = nil;
  }
  DCHECK(destWindow);

  // Have to do this here, otherwise later calls can crash because the window
  // has no delegate.
  [window setDelegate:nil];
  [destWindow setDelegate:self];

  // With this call, valgrind complains that a "Conditional jump or move depends
  // on uninitialised value(s)".  The error happens in -[NSThemeFrame
  // drawOverlayRect:].  I'm pretty convinced this is an Apple bug, but there is
  // no visual impact.  I have been unable to tickle it away with other window
  // or view manipulation Cocoa calls.  Stack added to suppressions_mac.txt.
  [contentView setAutoresizesSubviews:YES];
  [destWindow setContentView:contentView];

  // Move the incognito badge if present.
  if (incognitoBadge_.get()) {
    [incognitoBadge_ removeFromSuperview];
    [incognitoBadge_ setHidden:YES];  // Will be shown in layout.
    [[[destWindow contentView] superview] addSubview:incognitoBadge_];
  }

  // Add the tab strip after setting the content view and moving the incognito
  // badge (if any), so that the tab strip will be on top (in the z-order).
  if ([self hasTabStrip] && ![self useVerticalTabs])
    [[[destWindow contentView] superview] addSubview:tabStripView];

  [window setWindowController:nil];
  [self setWindow:destWindow];
  [destWindow setWindowController:self];
  [self adjustUIForFullscreen:fullscreen];

  // When entering fullscreen mode, the controller forces a layout for us.  When
  // exiting, we need to call layoutSubviews manually.
  if (fullscreen) {
    [fullscreenController_ enterFullscreenForContentView:contentView
                                            showDropdown:showDropdown];
  } else {
    [self layoutSubviews];
  }

  // Move the status bubble over, if we have one.
  if (statusBubble_)
    statusBubble_->SwitchParentWindow(destWindow);

  // Move the title over.
  [destWindow setTitle:[window title]];

  // The window needs to be onscreen before we can set its first responder.
  // Ordering the window to the front can change the active Space (either to
  // the window's old Space or to the application's assigned Space). To prevent
  // this by temporarily change the collectionBehavior.
  NSWindowCollectionBehavior behavior = [window collectionBehavior];
  [destWindow setCollectionBehavior:
      NSWindowCollectionBehaviorMoveToActiveSpace];
  [destWindow makeKeyAndOrderFront:self];
  [destWindow setCollectionBehavior:behavior];

  [focusTracker restoreFocusInWindow:destWindow];
  [window orderOut:self];

  // We're done moving focus, so re-enable bar visibility changes.
  [self enableBarVisibilityUpdates];

  // Fade back in.
  if (didFadeOut) {
    CGDisplayFade(token, kFadeDurationSeconds / 2, kCGDisplayBlendSolidColor,
        kCGDisplayBlendNormal, 0.0, 0.0, 0.0, /*synchronous=*/false);
    CGReleaseDisplayFadeReservation(token);
  }
}

- (BOOL)isFullscreen {
  return fullscreenController_.get() && [fullscreenController_ isFullscreen];
}

- (void)resizeFullscreenWindow {
  DCHECK([self isFullscreen]);
  if (![self isFullscreen])
    return;

  NSWindow* window = [self window];
  [window setFrame:[[window screen] frame] display:YES];
  [self layoutSubviews];
}

- (CGFloat)floatingBarShownFraction {
  return floatingBarShownFraction_;
}

- (void)setFloatingBarShownFraction:(CGFloat)fraction {
  floatingBarShownFraction_ = fraction;
  [self layoutSubviews];
}

- (BOOL)isBarVisibilityLockedForOwner:(id)owner {
  DCHECK(owner);
  DCHECK(barVisibilityLocks_);
  return [barVisibilityLocks_ containsObject:owner];
}

- (void)lockBarVisibilityForOwner:(id)owner
                    withAnimation:(BOOL)animate
                            delay:(BOOL)delay {
  if (![self isBarVisibilityLockedForOwner:owner]) {
    [barVisibilityLocks_ addObject:owner];

    // If enabled, show the overlay if necessary (and if in fullscreen mode).
    if (barVisibilityUpdatesEnabled_) {
      [fullscreenController_ ensureOverlayShownWithAnimation:animate
                                                       delay:delay];
    }
  }
}

- (void)releaseBarVisibilityForOwner:(id)owner
                       withAnimation:(BOOL)animate
                               delay:(BOOL)delay {
  if ([self isBarVisibilityLockedForOwner:owner]) {
    [barVisibilityLocks_ removeObject:owner];

    // If enabled, hide the overlay if necessary (and if in fullscreen mode).
    if (barVisibilityUpdatesEnabled_ &&
        ![barVisibilityLocks_ count]) {
      [fullscreenController_ ensureOverlayHiddenWithAnimation:animate
                                                        delay:delay];
    }
  }
}

- (BOOL)floatingBarHasFocus {
  NSResponder* focused = [[self window] firstResponder];
  return [focused isKindOfClass:[AutocompleteTextFieldEditor class]];
}

- (void)openTabpose {
  NSUInteger modifierFlags = [[NSApp currentEvent] modifierFlags];
  BOOL slomo = (modifierFlags & NSShiftKeyMask) != 0;

  // Cover info bars, inspector window, and detached bookmark bar on NTP.
  // Do not cover download shelf.
  NSRect activeArea = [[self tabContentArea] frame];
  activeArea.size.height +=
      NSHeight([[infoBarContainerController_ view] frame]);
  if ([self isBookmarkBarVisible] && [self placeBookmarkBarBelowInfoBar]) {
    NSView* bookmarkBarView = [bookmarkBarController_ view];
    activeArea.size.height += NSHeight([bookmarkBarView frame]);
  }

  [TabposeWindow openTabposeFor:[self window]
                           rect:activeArea
                          slomo:slomo
                  tabStripModel:browser_->tabstrip_model()];
}

@end  // @implementation BrowserWindowController(Fullscreen)


@implementation BrowserWindowController(WindowType)

- (BOOL)supportsWindowFeature:(int)feature {
  return browser_->SupportsWindowFeature(
      static_cast<Browser::WindowFeature>(feature));
}

- (BOOL)hasTitleBar {
  return [self supportsWindowFeature:Browser::FEATURE_TITLEBAR];
}

- (BOOL)hasToolbar {
  return [self supportsWindowFeature:Browser::FEATURE_TOOLBAR];
}

- (BOOL)hasLocationBar {
  return [self supportsWindowFeature:Browser::FEATURE_LOCATIONBAR];
}

- (BOOL)supportsBookmarkBar {
  return [self supportsWindowFeature:Browser::FEATURE_BOOKMARKBAR];
}

- (BOOL)isNormalWindow {
  return browser_->type() == Browser::TYPE_NORMAL;
}

@end  // @implementation BrowserWindowController(WindowType)
