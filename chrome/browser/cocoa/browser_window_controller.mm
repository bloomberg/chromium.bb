// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Carbon/Carbon.h>

#include "base/mac_util.h"
#include "base/scoped_nsdisable_screen_updates.h"
#import "base/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"  // IDC_*
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/encoding_menu_controller.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_editor_controller.h"
#import "chrome/browser/cocoa/browser_window_cocoa.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/chrome_browser_window.h"
#import "chrome/browser/cocoa/download_shelf_controller.h"
#import "chrome/browser/cocoa/event_utils.h"
#import "chrome/browser/cocoa/extension_shelf_controller.h"
#import "chrome/browser/cocoa/find_bar_cocoa_controller.h"
#include "chrome/browser/cocoa/find_bar_bridge.h"
#import "chrome/browser/cocoa/fullscreen_window.h"
#import "chrome/browser/cocoa/infobar_container_controller.h"
#import "chrome/browser/cocoa/status_bubble_mac.h"
#import "chrome/browser/cocoa/tab_strip_model_observer_bridge.h"
#import "chrome/browser/cocoa/tab_strip_view.h"
#import "chrome/browser/cocoa/tab_strip_controller.h"
#import "chrome/browser/cocoa/tab_view.h"
#import "chrome/browser/cocoa/toolbar_controller.h"
#import "chrome/browser/browser_theme_provider.h"
#include "chrome/common/pref_service.h"
#import "chrome/browser/cocoa/background_gradient_view.h"
#include "grit/theme_resources.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

namespace {

// Size of the gradient. Empirically determined so that the gradient looks
// like what the heuristic does when there are just a few tabs.
const int kWindowGradientHeight = 24;

}

@interface GTMTheme (BrowserThemeProviderInitialization)
+ (GTMTheme*)themeWithBrowserThemeProvider:(BrowserThemeProvider*)provider
                            isOffTheRecord:(BOOL)offTheRecord;
@end

@interface NSWindow (NSPrivateApis)
// Note: These functions are private, use -[NSObject respondsToSelector:]
// before calling them.

- (void)setAutorecalculatesContentBorderThickness:(BOOL)b
                                          forEdge:(NSRectEdge)e;
- (void)setContentBorderThickness:(CGFloat)b forEdge:(NSRectEdge)e;

- (void)setBottomCornerRounded:(BOOL)rounded;

- (NSRect)_growBoxRect;

@end


@interface BrowserWindowController(Private)

// Saves the window's position in the local state preferences.
- (void)saveWindowPositionIfNeeded;

// Saves the window's position to the given pref service.
- (void)saveWindowPositionToPrefs:(PrefService*)prefs;

// We need to adjust where sheets come out of the window, as by default they
// erupt from the omnibox, which is rather weird.
- (NSRect)window:(NSWindow*)window
willPositionSheet:(NSWindow*)sheet
       usingRect:(NSRect)defaultSheetRect;

// Assign a theme to the window.
- (void)setTheme;

// Repositions the windows subviews.
- (void)layoutSubviews;

@end


@implementation BrowserWindowController

// Load the browser window nib and do any Cocoa-specific initialization.
// Takes ownership of |browser|. Note that the nib also sets this controller
// up as the window's delegate.
- (id)initWithBrowser:(Browser*)browser {
  return [self initWithBrowser:browser takeOwnership:YES];
}

// Private (TestingAPI) init routine with testing options.
- (id)initWithBrowser:(Browser*)browser takeOwnership:(BOOL)ownIt {
  // Use initWithWindowNibPath:: instead of initWithWindowNibName: so we
  // can override it in a unit test.
  NSString* nibpath = [mac_util::MainAppBundle()
                        pathForResource:@"BrowserWindow"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    DCHECK(browser);
    browser_.reset(browser);
    ownsBrowser_ = ownIt;
    tabObserver_.reset(
        new TabStripModelObserverBridge(browser->tabstrip_model(), self));
    windowShim_.reset(new BrowserWindowCocoa(browser, self, [self window]));

    // The window is now fully realized and |-windowDidLoad:| has been
    // called. We shouldn't do much in wDL because |windowShim_| won't yet
    // be initialized (as it's called in response to |[self window]| above).
    // Retain it per the comment in the header.
    window_.reset([[self window] retain]);

    // Sets the window to not have rounded corners, which prevents
    // the resize control from being inset slightly and looking ugly.
    if ([window_ respondsToSelector:@selector(setBottomCornerRounded:)])
      [window_ setBottomCornerRounded:NO];

    [self setTheme];

    // Get the most appropriate size for the window, then enforce the
    // minimum width and height. The window shim will handle flipping
    // the coordinates for us so we can use it to save some code.
    // Note that this may leave a significant portion of the window
    // offscreen, but there will always be enough window onscreen to
    // drag the whole window back into view.
    NSSize minSize = [[self window] minSize];
    gfx::Rect windowRect = browser_->GetSavedWindowBounds();
    if (windowRect.width() < minSize.width)
      windowRect.set_width(minSize.width);
    if (windowRect.height() < minSize.height)
      windowRect.set_height(minSize.height);
    windowShim_->SetBounds(windowRect);

    // Puts the incognito badge on the window frame, if necessary. Do this
    // before creating the tab strip to avoid redundant tab layout.
    [self installIncognitoBadge];

    // Create a controller for the tab strip, giving it the model object for
    // this window's Browser and the tab strip view. The controller will handle
    // registering for the appropriate tab notifications from the back-end and
    // managing the creation of new tabs.
    tabStripController_.reset([[TabStripController alloc]
                                initWithView:[self tabStripView]
                                  switchView:[self tabContentArea]
                                     browser:browser_.get()]);

    // Create the infobar container view, so we can pass it to the
    // ToolbarController.
    infoBarContainerController_.reset(
        [[InfoBarContainerController alloc]
          initWithTabStripModel:(browser_->tabstrip_model())
                 resizeDelegate:self]);
    [[[self window] contentView] addSubview:[infoBarContainerController_ view]];

    // Create a controller for the toolbar, giving it the toolbar model object
    // and the toolbar view from the nib. The controller will handle
    // registering for the appropriate command state changes from the back-end.
    toolbarController_.reset([[ToolbarController alloc]
                               initWithModel:browser->toolbar_model()
                                    commands:browser->command_updater()
                                     profile:browser->profile()
                                     browser:browser
                              resizeDelegate:self]);
    // If we are a pop-up, we have a titlebar and no toolbar.
    if (!browser_->SupportsWindowFeature(Browser::FEATURE_TOOLBAR) &&
        browser_->SupportsWindowFeature(Browser::FEATURE_TITLEBAR)) {
      [toolbarController_ setHasToolbar:NO];
    }
    [[[self window] contentView] addSubview:[toolbarController_ view]];

    // Create a sub-controller for the bookmark bar.
    bookmarkBarController_.reset(
        [[BookmarkBarController alloc]
            initWithBrowser:browser_.get()
               initialWidth:NSWidth([[[self window] contentView] frame])
           compressDelegate:toolbarController_.get()
             resizeDelegate:self
                urlDelegate:self]);

    // Add bookmark bar to the view hierarchy.  This also triggers the
    // nib load.  The bookmark bar is defined (in the nib) to be
    // bottom-aligned to it's parent view (among other things), so
    // position and resize properties don't need to be set.
    [[[self window] contentView] addSubview:[bookmarkBarController_ view]
                                 positioned:NSWindowBelow
                                 relativeTo:[toolbarController_ view]];

    // Disable the bookmark bar if this window doesn't support them.
    if (!browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR)) {
      [bookmarkBarController_ setBookmarkBarEnabled:NO];
    }

    // We don't want to try and show the bar before it gets placed in
    // it's parent view, so this step shoudn't be inside the bookmark
    // bar controller's awakeFromNib.
    [bookmarkBarController_ showIfNeeded];

    if (browser_->SupportsWindowFeature(Browser::FEATURE_EXTENSIONSHELF)) {
      // Create the extension shelf.
      extensionShelfController_.reset([[ExtensionShelfController alloc]
                                        initWithBrowser:browser_.get()
                                         resizeDelegate:self]);
      [[[self window] contentView] addSubview:[extensionShelfController_ view]];
      [extensionShelfController_ wasInsertedIntoWindow];
    }

    // Force a relayout of all the various bars.
    [self layoutSubviews];

    // Create the bridge for the status bubble.
    statusBubble_.reset(new StatusBubbleMac([self window], self));
  }
  return self;
}

- (void)dealloc {
  browser_->CloseAllTabs();
  [downloadShelfController_ exiting];

  // Under certain testing configurations we may not actually own the browser.
  if (ownsBrowser_ == NO)
    browser_.release();
  // Since |window_| outlives our obj-c shutdown sequence, clear out the
  // delegate so nothing tries to call us back in the meantime as part of
  // window destruction.
  [window_ setDelegate:nil];

  [super dealloc];
}

// Access the C++ bridge between the NSWindow and the rest of Chromium
- (BrowserWindow*)browserWindow {
  return windowShim_.get();
}

- (void)destroyBrowser {
  [NSApp removeWindowsItem:[self window]];

  // We need the window to go away now.
  [self autorelease];
}

// Called when the window meets the criteria to be closed (ie,
// |-windowShouldClose:| returns YES). We must be careful to preserve the
// semantics of BrowserWindow::Close() and not call the Browser's dtor directly
// from this method.
- (void)windowWillClose:(NSNotification*)notification {
  // Don't update the window any longer.
  // TODO(shess,dmaclach): This is a work-around for some cases where
  // the window's views were living longer than this controller, and
  // were then being re-displayed.  Better would be to have it live
  // the appropriate amount of time, at which point we can remove
  // this.  [And perhaps the funky -autorelease below can be fixed.]
  [window_ setAutodisplay:NO];

  DCHECK(!browser_->tabstrip_model()->count());

  // We can't actually use |-autorelease| here because there's an embedded
  // run loop in the |-performClose:| which contains its own autorelease pool.
  // Instead we call it after a zero-length delay, which gets us back
  // to the main event loop.
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

// Called when the user wants to close a window or from the shutdown process.
// The Browser object is in control of whether or not we're allowed to close. It
// may defer closing due to several states, such as onUnload handlers needing to
// be fired. If closing is deferred, the Browser will handle the processing
// required to get us to the closing state and (by watching for all the tabs
// going away) will again call to close the window when it's finally ready.
- (BOOL)windowShouldClose:(id)sender {
  // Disable updates while closing all tabs to avoid flickering.
  base::ScopedNSDisableScreenUpdates disabler;
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
}

- (void)windowDidResignMain:(NSNotification*)notification {
  // TODO(dmaclach): Instead of redrawing the whole window, views that care
  // about the active window state should be registering for notifications.
  [[self window] setViewsNeedDisplay:YES];
}

// Called when we are activated (when we gain focus).
- (void)windowDidBecomeKey:(NSNotification*)notification {
  // We need to activate the controls (in the "WebView"). To do this, get the
  // selected TabContents's RenderWidgetHostViewMac and tell it to activate.
  if (TabContents* contents = browser_->GetSelectedTabContents()) {
    if (RenderWidgetHostView* rwhv = contents->render_widget_host_view())
      rwhv->SetActive(true);
  }
}

// Called when we are deactivated (when we lose focus).
- (void)windowDidResignKey:(NSNotification*)notification {
  // If our app is still active and we're still the key window, ignore this
  // message, since it just means that a menu extra (on the "system status bar")
  // was activated; we'll get another |-windowDidResignKey| if we ever really
  // lose key window status.
  if ([NSApp isActive] &&
      ([NSApp keyWindow] == static_cast<NSWindow*>(window_)))
    return;

  // We need to deactivate the controls (in the "WebView"). To do this, get the
  // selected TabContents's RenderWidgetHostView and tell it to deactivate.
  if (TabContents* contents = browser_->GetSelectedTabContents()) {
    if (RenderWidgetHostView* rwhv = contents->render_widget_host_view())
      rwhv->SetActive(false);
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

  TabContents* contents = browser_->tabstrip_model()->GetSelectedTabContents();
  if (contents) {
    // If the intrinsic width is bigger, then make it the zoomed width.
    const int kScrollbarWidth = 16;  // FIXME(viettrungluu@gmail.com): ugh.
    CGFloat intrinsicWidth = static_cast<CGFloat>(
        contents->view()->preferred_width() + kScrollbarWidth);
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

// Main method to resize browser window subviews.  This method should be called
// when resizing any child of the content view, rather than resizing the views
// directly.  If the view is already the correct height, does not force a
// relayout.
- (void)resizeView:(NSView*)view newHeight:(float)height {
  // We should only ever be called for one of the following four views.
  // |downloadShelfController_| may be nil. If we are asked to size the bookmark
  // bar directly, its superview must be this controller's content view.
  DCHECK(view);
  DCHECK(view == [toolbarController_ view] ||
         view == [infoBarContainerController_ view] ||
         view == [extensionShelfController_ view] ||
         view == [downloadShelfController_ view] ||
         view == [bookmarkBarController_ view]);

  // Change the height of the view and call layoutViews.  We set the height here
  // without regard to where the view is on the screen or whether it needs to
  // "grow up" or "grow down."  The below call to layoutSubviews will position
  // each view correctly.
  NSRect frame = [view frame];
  if (frame.size.height == height)
    return;

  frame.size.height = height;
  // TODO(rohitrao): Determine if calling setFrame: twice is bad.
  [view setFrame:frame];
  [self layoutSubviews];
}

// Update a toggle state for an NSMenuItem if modified.
// Take care to insure |item| looks like a NSMenuItem.
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
  // browser/views/toolbar_view.h.
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
      enable = browser_->command_updater()->IsCommandEnabled(tag) ? YES : NO;
      switch (tag) {
        case IDC_CLOSE_TAB:
          // Disable "close tab" if we're not the key window or if there's only
          // one tab.
          enable &= [self numberOfTabs] > 1 && [[self window] isKeyWindow];
          break;
        case IDC_RESTORE_TAB:
          // We have to ask the Browser manually if we can restore. The
          // command updater doesn't know.
          enable &= browser_->CanRestoreTab();
          break;
        case IDC_FULLSCREEN:
          enable &= [self supportsFullscreen];
          break;
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
  NSInteger tag = [sender tag];
  switch (tag) {
    case IDC_RELOAD:
      if ([sender isKindOfClass:[NSButton class]]) {
        // We revert the bar when the reload button is pressed, but don't when
        // Command+r is pressed (Issue 15464). Unlike the event handler function
        // for Windows (ToolbarView::ButtonPressed()), this function handles
        // both reload button press event and Command+r press event. Thus the
        // 'isKindofClass' check is necessary.
        [self locationBar]->Revert();
      }
      break;
  }
  browser_->ExecuteCommand(tag);
}

// Same as |-commandDispatch:|, but executes commands using a disposition
// determined by the key flags.
- (void)commandDispatchUsingKeyModifiers:(id)sender {
  NSInteger tag = [sender tag];
  browser_->ExecuteCommandWithDisposition(tag,
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]));
}

// Called when another part of the internal codebase needs to execute a
// command.
- (void)executeCommand:(int)command {
  if (browser_->command_updater()->IsCommandEnabled(command))
    browser_->ExecuteCommand(command);
}

// StatusBubble delegate method: tell the status bubble how far above the bottom
// of the window it should position itself.
- (float)verticalOffsetForStatusBubble {
  return verticalOffsetForStatusBubble_;
}

- (GTMWindowSheetController*)sheetController {
  return [tabStripController_ sheetController];
}

- (LocationBar*)locationBar {
  return [toolbarController_ locationBar];
}

- (StatusBubbleMac*)statusBubble {
  return statusBubble_.get();
}

- (void)updateToolbarWithContents:(TabContents*)tab
               shouldRestoreState:(BOOL)shouldRestore {
  [toolbarController_ updateToolbarWithContents:tab
                             shouldRestoreState:shouldRestore];
}

- (void)setStarredState:(BOOL)isStarred {
  [toolbarController_ setStarredState:isStarred];
}

// Return the rect, in WebKit coordinates (flipped), of the window's grow box
// in the coordinate system of the content area of the currently selected tab.
// |windowGrowBox| needs to be in the window's coordinate system.
- (NSRect)selectedTabGrowBoxRect {
  if (![window_ respondsToSelector:@selector(_growBoxRect)])
    return NSZeroRect;

  // Before we return a rect, we need to convert it from window coordinates
  // to tab content area coordinates and flip the coordinate system.
  NSRect growBoxRect =
      [[self tabContentArea] convertRect:[window_ _growBoxRect] fromView:nil];
  growBoxRect.origin.y =
      [[self tabContentArea] frame].size.height - growBoxRect.size.height -
      growBoxRect.origin.y;
  return growBoxRect;
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
    int index = [dragBWC->tabStripController_ indexForTabView:view];
    TabContents* contents =
        dragBWC->browser_->tabstrip_model()->GetTabContentsAt(index);
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

    // Now that we have enough information about the tab, we can remove it from
    // the dragging window. We need to do this *before* we add it to the new
    // window as this will remove the TabContents' delegate.
    [dragController detachTabView:view];

    // Deposit it into our model at the appropriate location (it already knows
    // where it should go from tracking the drag). Doing this sets the tab's
    // delegate to be the Browser.
    [tabStripController_ dropTabContents:contents withFrame:destinationFrame];
  } else {
    // Moving within a window.
    int index = [tabStripController_ indexForTabView:view];
    [tabStripController_ moveTabFromIndex:index];
  }

  // Remove the placeholder since the drag is now complete.
  [self removePlaceholder];
}

// Tells the tab strip to forget about this tab in preparation for it being
// put into a different tab strip, such as during a drop on another window.
- (void)detachTabView:(NSView*)view {
  int index = [tabStripController_ indexForTabView:view];
  browser_->tabstrip_model()->DetachTabContentsAt(index);
}

- (NSView*)selectedTabView {
  return [tabStripController_ selectedTabView];
}

- (TabStripController*)tabStripController {
  return tabStripController_;
}

- (void)setIsLoading:(BOOL)isLoading {
  [toolbarController_ setIsLoading:isLoading];
}

- (void)activate {
  // TODO(rohitrao): Figure out the proper activation behavior for fullscreen
  // windows.  When there is only one window open, this code makes sense, but
  // what should we do if we need to activate a non-fullscreen background window
  // while a fullscreen window has focus?  http://crbug.com/24893
  if (fullscreen_)
    [fullscreen_window_ makeKeyAndOrderFront:self];
  else
    [window_ makeKeyAndOrderFront:self];
}

// Make the location bar the first responder, if possible.
- (void)focusLocationBar {
  [toolbarController_ focusLocationBar];
}

- (void)layoutTabs {
  [tabStripController_ layoutTabs];
}

- (TabWindowController*)detachTabToNewWindow:(TabView*)tabView {
  // Disable screen updates so that this appears as a single visual change.
  base::ScopedNSDisableScreenUpdates disabler;

  // Fetch the tab contents for the tab being dragged
  int index = [tabStripController_ indexForTabView:tabView];
  TabContents* contents = browser_->tabstrip_model()->GetTabContentsAt(index);

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

  NSRect tabRect = [tabView frame];

  // Detach it from the source window, which just updates the model without
  // deleting the tab contents. This needs to come before creating the new
  // Browser because it clears the TabContents' delegate, which gets hooked
  // up during creation of the new window.
  browser_->tabstrip_model()->DetachTabContentsAt(index);

  // Create the new window with a single tab in its model, the one being
  // dragged.
  DockInfo dockInfo;
  Browser* newBrowser =
      browser_->tabstrip_model()->TearOffTabContents(contents,
                                                     browserRect,
                                                     dockInfo);

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

- (BOOL)isTabFullyVisible:(TabView*)tab {
  return [tabStripController_ isTabFullyVisible:tab];
}

- (void)showNewTabButton:(BOOL)show {
  [tabStripController_ showNewTabButton:show];
}

- (BOOL)isBookmarkBarVisible {
  return [bookmarkBarController_ isVisible];
}

- (void)updateBookmarkBarVisibility {
  [bookmarkBarController_ updateVisibility];
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
  [[[self window] contentView] addSubview:[findBarCocoaController_ view]
                               positioned:NSWindowAbove
                               relativeTo:[toolbarController_ view]];
  [findBarCocoaController_
    positionFindBarView:[infoBarContainerController_ view]];
}

// Adjust the UI for fullscreen mode.  E.g. when going fullscreen,
// remove the toolbar.  When stopping fullscreen, add it back in.
- (void)adjustUIForFullscreen:(BOOL)fullscreen {
  if (fullscreen) {
    // Disable showing of the bookmark bar.  This does not toggle the
    // preference.
    [bookmarkBarController_ setBookmarkBarEnabled:NO];
    // Make room for more content area.
    [[toolbarController_ view] removeFromSuperview];
    // Hide the menubar, and allow it to un-hide when moving the mouse
    // to the top of the screen.  Does this eliminate the need for an
    // info bubble describing how to exit fullscreen mode?
    mac_util::RequestFullScreen();
  } else {
    mac_util::ReleaseFullScreen();
    [[[self window] contentView] addSubview:[toolbarController_ view]];
    if (browser_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR)) {
      [bookmarkBarController_ setBookmarkBarEnabled:YES];
    }
  }

  // Force a relayout.
  [self layoutSubviews];
}

- (NSWindow*)fullscreenWindow {
  return [[[FullscreenWindow alloc] initForScreen:[[self window] screen]]
           autorelease];
}

- (void)setFullscreen:(BOOL)fullscreen {
  if (![self supportsFullscreen])
      return;

  fullscreen_ = fullscreen;
  if (fullscreen) {
    // Move content to a new fullscreen window
    NSView* content = [[self window] contentView];
    // Disable autoresizing of subviews while we move views around.  This
    // prevents spurious renderer resizes.
    [content setAutoresizesSubviews:NO];
    fullscreen_window_.reset([[self fullscreenWindow] retain]);
    [content removeFromSuperview];
    [fullscreen_window_ setContentView:content];
    [self setWindow:fullscreen_window_.get()];
    [window_ setWindowController:nil];
    [window_ setDelegate:nil];
    // Required for proper event dispatch.
    [fullscreen_window_ setWindowController:self];
    [fullscreen_window_ setDelegate:self];

    // Minimize our UI.  This call triggers a relayout, so it needs to come
    // after we move the contentview to the new window.
    [self adjustUIForFullscreen:fullscreen];
    // Show one window, hide the other.
    [fullscreen_window_ makeKeyAndOrderFront:self];
    [content setAutoresizesSubviews:YES];
    [content setNeedsDisplay:YES];
    [window_ orderOut:self];
  } else {
    NSView* content = [fullscreen_window_ contentView];
    // Disable autoresizing of subviews while we move views around.  This
    // prevents spurious renderer resizes.
    [content setAutoresizesSubviews:NO];
    [content removeFromSuperview];
    [window_ setContentView:content];
    [fullscreen_window_ setDelegate:nil];
    [fullscreen_window_ setWindowController:nil];
    [window_ setWindowController:self];
    [window_ setDelegate:self];
    [self setWindow:window_.get()];
    // This call triggers a relayout, so it needs to come after we move the
    // contentview to the new window.
    [self adjustUIForFullscreen:fullscreen];
    [content setAutoresizesSubviews:YES];
    [content setNeedsDisplay:YES];

    // With this call, valgrind yells at me about "Conditional jump or
    // move depends on uninitialised value(s)".  The error happens in
    // -[NSThemeFrame drawOverlayRect:].  I'm pretty convinced this is
    // an Apple bug, but there is no visual impact.  I have been
    // unable to tickle it away with other window or view manipulation
    // Cocoa calls.  Stack added to suppressions_mac.txt.
    [window_ makeKeyAndOrderFront:self];

    [fullscreen_window_ close];
    fullscreen_window_.reset(nil);
  }
}

- (BOOL)isFullscreen {
  return fullscreen_;
}

// Called by the bookmark bar to open a URL.
- (void)openBookmarkURL:(const GURL&)url
            disposition:(WindowOpenDisposition)disposition {
  browser_->OpenURL(url, GURL(), disposition, PageTransition::AUTO_BOOKMARK);
}

- (NSInteger)numberOfTabs {
  return browser_->tabstrip_model()->count();
}

- (NSString*)selectedTabTitle {
  TabContents* contents = browser_->tabstrip_model()->GetSelectedTabContents();
  return base::SysUTF16ToNSString(contents->GetTitle());
}

// TYPE_POPUP is not normal (e.g. no tab strip)
- (BOOL)isNormalWindow {
  if (browser_->type() == Browser::TYPE_NORMAL)
    return YES;
  return NO;
}

- (void)selectTabWithContents:(TabContents*)newContents
             previousContents:(TabContents*)oldContents
                      atIndex:(NSInteger)index
                  userGesture:(bool)wasUserGesture {
  DCHECK(oldContents != newContents);

  // Update various elements that are interested in knowing the current
  // TabContents.
#if 0
// TODO(pinkerton):Update as more things become window-specific
  contents_container_->SetTabContents(newContents);
#endif

  // Update all the UI bits.
  windowShim_->UpdateTitleBar();

#if 0
// TODO(pinkerton):Update as more things become window-specific
  toolbar_->SetProfile(newContents->profile());
  UpdateToolbar(newContents, true);
  UpdateUIForContents(newContents);
#endif
}

- (void)tabChangedWithContents:(TabContents*)contents
                       atIndex:(NSInteger)index
                   loadingOnly:(BOOL)loading {
  if (index == browser_->tabstrip_model()->selected_index()) {
    // Update titles if this is the currently selected tab.
    windowShim_->UpdateTitleBar();
  }
}

- (void)userChangedTheme {
  [self setTheme];
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter postNotificationName:kGTMThemeDidChangeNotification
                               object:theme_];
  // TODO(dmaclach): Instead of redrawing the whole window, views that care
  // about the active window state should be registering for notifications.
  [[self window] setViewsNeedDisplay:YES];
}

- (GTMTheme*)gtm_themeForWindow:(NSWindow*)window {
  return theme_ ? theme_ : [GTMTheme defaultTheme];
}

- (NSPoint)gtm_themePatternPhaseForWindow:(NSWindow*)window {
  // Our patterns want to be drawn from the upper left hand corner of the view.
  // Cocoa wants to do it from the lower left of the window.
  // Rephase our pattern to fit this view. Some other views (Tabs, Toolbar etc.)
  // will phase their patterns relative to this so all the views look right.
  NSView* tabStripView = [self tabStripView];
  NSRect tabStripViewWindowBounds = [tabStripView bounds];
  NSView* windowChromeView = [[window contentView] superview];
  tabStripViewWindowBounds =
      [tabStripView convertRect:tabStripViewWindowBounds
                         toView:windowChromeView];
  NSPoint phase = NSMakePoint(NSMinX(tabStripViewWindowBounds),
                              NSMinY(tabStripViewWindowBounds)
                              + [TabStripController defaultTabHeight]);
  return phase;
}

- (NSPoint)topLeftForBubble {
  NSRect rect = [toolbarController_ starButtonInWindowCoordinates];
  NSPoint p = NSMakePoint(NSMinX(rect), NSMinY(rect));  // bottom left

  // Adjust top-left based on our knowledge of how the view looks.
  p.x -= 2;
  p.y += 7;

  return p;
}

// Show the bookmark bubble (e.g. user just clicked on the STAR).
- (void)showBookmarkBubbleForURL:(const GURL&)url
               alreadyBookmarked:(BOOL)alreadyBookmarked {
  BookmarkModel* model = browser_->profile()->GetBookmarkModel();
  const BookmarkNode* node = model->GetMostRecentlyAddedNodeForURL(url);

  // Bring up the bubble.  But clicking on STAR while the bubble is
  // open should make it go away.
  if (bookmarkBubbleController_.get()) {
    [self doneWithBubbleController:bookmarkBubbleController_.get()];
  } else {
    bookmarkBubbleController_.reset([[BookmarkBubbleController alloc]
                                      initWithDelegate:self
                                          parentWindow:[self window]
                                      topLeftForBubble:[self topLeftForBubble]
                                                 model:model
                                                  node:node
                                     alreadyBookmarked:alreadyBookmarked]);
    [bookmarkBubbleController_ showWindow];
  }
}

// Implement BookmarkBubbleControllerDelegate
- (void)editBookmarkNode:(const BookmarkNode*)node {
  // A BookmarkEditorController is a sheet that owns itself, and
  // deallocates itself when closed.
  [[[BookmarkEditorController alloc]
     initWithParentWindow:[self window]
                  profile:browser_->profile()
                   parent:node->GetParent()
                     node:node
            configuration:BookmarkEditor::SHOW_TREE
                  handler:NULL]
    runAsModalSheet];
}

// Implement BookmarkBubbleControllerDelegate
- (void)doneWithBubbleController:(BookmarkBubbleController*)controller {
  bookmarkBubbleController_.reset(nil);
}

// Delegate method called when window is resized.
- (void)windowDidResize:(NSNotification*)notification {
  // Resize (and possibly move) the status bubble. Note that we may get called
  // when the status bubble does not exist.
  if (statusBubble_.get())
    statusBubble_->UpdateSizeAndPosition();
}

@end

@implementation BrowserWindowController (Private)

// If the browser is in incognito mode, install the image view to decorate
// the window at the upper right. Use the same base y coordinate as the
// tab strip.
- (void)installIncognitoBadge {
  if (!browser_->profile()->IsOffTheRecord())
    return;
  // Don't install if we're not a normal browser (ie, a popup).
  if (![self isNormalWindow])
    return;

  static const float kOffset = 4;
  NSString* incognitoPath = [mac_util::MainAppBundle()
                                pathForResource:@"otr_icon"
                                         ofType:@"pdf"];
  scoped_nsobject<NSImage> incognitoImage(
      [[NSImage alloc] initWithContentsOfFile:incognitoPath]);
  const NSSize imageSize = [incognitoImage size];
  NSRect tabFrame = [[self tabStripView] frame];
  NSRect incognitoFrame = tabFrame;
  incognitoFrame.origin.x = NSMaxX(incognitoFrame) - imageSize.width -
                              kOffset;
  incognitoFrame.size = imageSize;
  scoped_nsobject<NSImageView> incognitoView(
      [[NSImageView alloc] initWithFrame:incognitoFrame]);
  [incognitoView setImage:incognitoImage.get()];
  [incognitoView setWantsLayer:YES];
  [incognitoView setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  scoped_nsobject<NSShadow> shadow([[NSShadow alloc] init]);
  [shadow.get() setShadowColor:[NSColor colorWithCalibratedWhite:0.0
                                                           alpha:0.5]];
  [shadow.get() setShadowOffset:NSMakeSize(0, -1)];
  [shadow setShadowBlurRadius:2.0];
  [incognitoView setShadow:shadow];

  // Shrink the tab strip's width so there's no overlap and install the
  // view.
  tabFrame.size.width -= incognitoFrame.size.width + kOffset;
  [[self tabStripView] setFrame:tabFrame];
  [[[[self window] contentView] superview] addSubview:incognitoView.get()];
}

- (void)saveWindowPositionIfNeeded {
  if (browser_ != BrowserList::GetLastActive())
    return;

  if (!g_browser_process || !g_browser_process->local_state() ||
      !browser_->ShouldSaveWindowPlacement())
    return;

  [self saveWindowPositionToPrefs:g_browser_process->local_state()];
}

- (void)saveWindowPositionToPrefs:(PrefService*)prefs {
  // Window positions are stored relative to the origin of the primary monitor.
  NSRect monitorFrame = [[[NSScreen screens] objectAtIndex:0] frame];

  // Start with the window's frame, which is in virtual coordinates.
  // Do some y twiddling to flip the coordinate system.
  gfx::Rect bounds(NSRectToCGRect([[self window] frame]));
  bounds.set_y(monitorFrame.size.height - bounds.y() - bounds.height());

  // We also need to save the current work area, in flipped coordinates.
  gfx::Rect workArea(NSRectToCGRect([[[self window] screen] visibleFrame]));
  workArea.set_y(monitorFrame.size.height - workArea.y() - workArea.height());

  DictionaryValue* windowPreferences = prefs->GetMutableDictionary(
      browser_->GetWindowPlacementKey().c_str());
  windowPreferences->SetInteger(L"left", bounds.x());
  windowPreferences->SetInteger(L"top", bounds.y());
  windowPreferences->SetInteger(L"right", bounds.right());
  windowPreferences->SetInteger(L"bottom", bounds.bottom());
  windowPreferences->SetBoolean(L"maximized", false);
  windowPreferences->SetBoolean(L"always_on_top", false);
  windowPreferences->SetInteger(L"work_area_left", workArea.x());
  windowPreferences->SetInteger(L"work_area_top", workArea.y());
  windowPreferences->SetInteger(L"work_area_right", workArea.right());
  windowPreferences->SetInteger(L"work_area_bottom", workArea.bottom());
}

- (NSRect)window:(NSWindow*)window
willPositionSheet:(NSWindow*)sheet
       usingRect:(NSRect)defaultSheetRect {
  // Any sheet should come from right above the apparent content area, or
  // equivalently right below the apparent toolbar.
  NSRect toolbarFrame = [[toolbarController_ view] frame];
  defaultSheetRect.origin.y = toolbarFrame.origin.y;

  // Position as follows:
  //  - On a normal (non-NTP) page, position the sheet under the bookmark bar if
  //    it's visible. Else put it immediately below the normal toolbar.
  //  - On the NTP, if the bookmark bar is enabled ("always visible"), then it
  //    looks like a normal bookmark bar, so position the sheet under it; if it
  //    isn't enabled, the bookmark bar will look as if it's a part of the
  //    content area, so just put it immediately below the toolbar.
  if ([bookmarkBarController_ isVisible] &&
      (![bookmarkBarController_ isNewTabPage] ||
        [bookmarkBarController_ isAlwaysVisible])) {
    NSRect bookmarkBarFrame = [[bookmarkBarController_ view] frame];
    defaultSheetRect.origin.y -= bookmarkBarFrame.size.height;
  }

  return defaultSheetRect;
}

// Undocumented method for multi-touch gestures in 10.5. Future OS's will
// likely add a public API, but the worst that will happen is that this will
// turn into dead code and just won't get called.
- (void)swipeWithEvent:(NSEvent*)event {
  // Map forwards and backwards to history; left is positive, right is negative.
  unsigned int command = 0;
  if ([event deltaX] > 0.5)
    command = IDC_BACK;
  else if ([event deltaX] < -0.5)
    command = IDC_FORWARD;
  else if ([event deltaY] > 0.5)
    ;  // TODO(pinkerton): figure out page-up
  else if ([event deltaY] < -0.5)
    ;  // TODO(pinkerton): figure out page-down

  // Ensure the command is valid first (ExecuteCommand() won't do that) and
  // then make it so.
  if (browser_->command_updater()->IsCommandEnabled(command))
    browser_->ExecuteCommand(command);
}

- (id)windowWillReturnFieldEditor:(NSWindow*)sender toObject:(id)obj {
  // Ask the toolbar controller if it wants to return a custom field editor
  // for the specific object.
  return [toolbarController_ customFieldEditorForObject:obj];
}

- (void)setTheme {
  ThemeProvider* theme_provider = browser_->profile()->GetThemeProvider();
  BrowserThemeProvider* browser_theme_provider =
     static_cast<BrowserThemeProvider*>(theme_provider);
  if (browser_theme_provider) {
    bool offtheRecord = browser_->profile()->IsOffTheRecord();
    GTMTheme* theme =
        [GTMTheme themeWithBrowserThemeProvider:browser_theme_provider
                                 isOffTheRecord:offtheRecord];
    theme_.reset([theme retain]);
  }
}

// Private method to layout browser window subviews.  Positions the toolbar and
// the infobar above the tab content area.  Positions the download shelf below
// the tab content area.  If the toolbar is not a child of the contentview, this
// method will not leave room for it.  If we are currently running in fullscreen
// mode, or if the tabstrip is not a descendant of the window, this method fills
// the entire content area.  Otherwise, this method places the topmost view
// directly beneath the tabstrip.
- (void)layoutSubviews {
  NSView* contentView = fullscreen_ ? [fullscreen_window_ contentView]
                                    : [[self window] contentView];
  NSRect contentFrame = [contentView frame];
  int maxY = NSMaxY(contentFrame);
  int minY = NSMinY(contentFrame);
  if (!fullscreen_ && [self isNormalWindow]) {
    maxY = NSMinY([[self tabStripView] frame]);
  }
  DCHECK_GE(maxY, minY);

  // Suppress title drawing for normal windows (popups use normal
  // window title bars).
  [window_ setShouldHideTitle:[self isNormalWindow]];

  // Place the toolbar at the top of the reserved area, but only if we're not in
  // fullscreen mode.
  NSView* toolbarView = [toolbarController_ view];
  NSRect toolbarFrame = [toolbarView frame];
  if (!fullscreen_) {
    // The toolbar is present in the window, so we make room for it.
    toolbarFrame.origin.x = 0;
    toolbarFrame.origin.y = maxY - NSHeight(toolbarFrame);
    toolbarFrame.size.width = NSWidth(contentFrame);
    maxY -= NSHeight(toolbarFrame);
  }
  [toolbarView setFrame:toolbarFrame];

  if ([bookmarkBarController_ isAlwaysVisible]) {
    NSView* bookmarkBarView = [bookmarkBarController_ view];
    [bookmarkBarView setHidden:NO];
    NSRect bookmarkBarFrame = [bookmarkBarView frame];
    bookmarkBarFrame.origin.y = maxY - NSHeight(bookmarkBarFrame);
    bookmarkBarFrame.size.width = NSWidth(contentFrame);
    [bookmarkBarView setFrame:bookmarkBarFrame];
    maxY -= NSHeight(bookmarkBarFrame);
  }

  // Place the infobar container in place below the toolbar.
  NSView* infoBarView = [infoBarContainerController_ view];
  NSRect infoBarFrame = [infoBarView frame];
  infoBarFrame.origin.y = maxY - NSHeight(infoBarFrame);
  infoBarFrame.size.width = NSWidth(contentFrame);
  [infoBarView setFrame:infoBarFrame];
  maxY -= NSHeight(infoBarFrame);

  if (![bookmarkBarController_ isAlwaysVisible] &&
      [bookmarkBarController_ isVisible]) {
    NSView* bookmarkBarView = [bookmarkBarController_ view];
    [bookmarkBarView setHidden:NO];
    NSRect bookmarkBarFrame = [bookmarkBarView frame];
    bookmarkBarFrame.origin.y = maxY - NSHeight(bookmarkBarFrame);
    bookmarkBarFrame.size.width = NSWidth(contentFrame);
    [bookmarkBarView setFrame:bookmarkBarFrame];
    maxY -= NSHeight(bookmarkBarFrame);
  }

  if (![bookmarkBarController_ isVisible]) {
    // If the bookmark bar is not visible in either mode, we need to hide it
    // otherwise it'll render over other elements.
    [[bookmarkBarController_ view] setHidden:YES];
  }

  // Place the extension shelf at the bottom of the view, if it exists.
  if (extensionShelfController_.get()) {
    NSView* extensionView = [extensionShelfController_ view];
    NSRect extensionFrame = [extensionView frame];
    extensionFrame.origin.y = minY;
    extensionFrame.size.width = NSWidth(contentFrame);
    [extensionView setFrame:extensionFrame];
    minY += NSHeight(extensionFrame);
  }

  // Place the download shelf above the extension shelf, if it exists.
  if (downloadShelfController_.get()) {
    NSView* downloadView = [downloadShelfController_ view];
    NSRect downloadFrame = [downloadView frame];
    downloadFrame.origin.y = minY;
    downloadFrame.size.width = NSWidth(contentFrame);
    [downloadView setFrame:downloadFrame];
    minY += NSHeight(downloadFrame);
  }

  // Finally, the tabContentArea takes up all of the remaining space.
  NSView* tabContentView = [self tabContentArea];
  NSRect tabContentFrame = [tabContentView frame];
  tabContentFrame.origin.y = minY;
  tabContentFrame.size.height = maxY - minY;
  tabContentFrame.size.width = NSWidth(contentFrame);
  [tabContentView setFrame:tabContentFrame];

  // Position the find bar relative to the infobar container.
  [findBarCocoaController_
    positionFindBarView:[infoBarContainerController_ view]];

  verticalOffsetForStatusBubble_ = minY;

  // The bottom of the visible toolbar stack is the one that shows the
  // divider stroke. If the bookmark bar is visible and not in new tab page
  // mode, it is the bottom visible toolbar and so it must, otherwise the
  // main toolbar is.
  BOOL bookmarkToolbarShowsDivider = [bookmarkBarController_ isAlwaysVisible];
  [[toolbarController_ backgroundGradientView]
      setShowsDivider:!bookmarkToolbarShowsDivider];
  [[bookmarkBarController_ backgroundGradientView]
      setShowsDivider:bookmarkToolbarShowsDivider];
}

@end

@implementation GTMTheme (BrowserThemeProviderInitialization)
+ (GTMTheme*)themeWithBrowserThemeProvider:(BrowserThemeProvider*)provider
                            isOffTheRecord:(BOOL)isOffTheRecord {
  // First check if it's in the cache.
  // TODO(pinkerton): This might be a good candidate for a singleton.
  typedef std::pair<std::string, BOOL> ThemeKey;
  static std::map<ThemeKey, GTMTheme*> cache;
  ThemeKey key(provider->GetThemeID(), isOffTheRecord);
  GTMTheme* theme = cache[key];
  if (theme)
    return theme;

  theme = [[GTMTheme alloc] init];  // "Leak" it in the cache.
  cache[key] = theme;

  // TODO(pinkerton): Need to be able to theme the entire incognito window
  // http://crbug.com/18568 The hardcoding of the colors here will need to be
  // removed when that bug is addressed, but are here in order for things to be
  // usable in the meantime.
  if (isOffTheRecord) {
    NSColor* incognitoColor = [NSColor colorWithCalibratedRed:83/255.0
                                                        green:108.0/255.0
                                                         blue:140/255.0
                                                        alpha:1.0];
    [theme setBackgroundColor:incognitoColor];
    [theme setValue:[NSColor blackColor]
      forAttribute:@"textColor"
             style:GTMThemeStyleTabBarSelected
             state:GTMThemeStateActiveWindow];
    [theme setValue:[NSColor blackColor]
      forAttribute:@"textColor"
             style:GTMThemeStyleTabBarDeselected
             state:GTMThemeStateActiveWindow];
    [theme setValue:[NSColor blackColor]
      forAttribute:@"textColor"
             style:GTMThemeStyleBookmarksBarButton
             state:GTMThemeStateActiveWindow];
    return theme;
  }

  NSImage* frameImage = provider->GetNSImageNamed(IDR_THEME_FRAME);
  NSImage* frameInactiveImage =
      provider->GetNSImageNamed(IDR_THEME_FRAME_INACTIVE);

  [theme setValue:frameImage
     forAttribute:@"backgroundImage"
            style:GTMThemeStyleWindow
            state:GTMThemeStateActiveWindow];

  NSColor* tabTextColor =
      provider->GetNSColor(BrowserThemeProvider::COLOR_TAB_TEXT);
  [theme setValue:tabTextColor
     forAttribute:@"textColor"
            style:GTMThemeStyleTabBarSelected
            state:GTMThemeStateActiveWindow];

  NSColor* tabInactiveTextColor =
      provider->GetNSColor(BrowserThemeProvider::COLOR_BACKGROUND_TAB_TEXT);
  [theme setValue:tabInactiveTextColor
     forAttribute:@"textColor"
            style:GTMThemeStyleTabBarDeselected
            state:GTMThemeStateActiveWindow];

  NSColor* bookmarkBarTextColor =
      provider->GetNSColor(BrowserThemeProvider::COLOR_BOOKMARK_TEXT);
  [theme setValue:bookmarkBarTextColor
     forAttribute:@"textColor"
            style:GTMThemeStyleBookmarksBarButton
            state:GTMThemeStateActiveWindow];

  [theme setValue:frameInactiveImage
     forAttribute:@"backgroundImage"
            style:GTMThemeStyleWindow
            state:0];

  NSImage* toolbarImage = provider->GetNSImageNamed(IDR_THEME_TOOLBAR);
  [theme setValue:toolbarImage
     forAttribute:@"backgroundImage"
            style:GTMThemeStyleToolBar
            state:GTMThemeStateActiveWindow];
  NSImage* toolbarBackgroundImage =
      provider->GetNSImageNamed(IDR_THEME_TAB_BACKGROUND);
  [theme setValue:toolbarBackgroundImage
     forAttribute:@"backgroundImage"
            style:GTMThemeStyleTabBarDeselected
            state:GTMThemeStateActiveWindow];

  NSImage* toolbarButtonImage =
      provider->GetNSImageNamed(IDR_THEME_BUTTON_BACKGROUND);
  if (toolbarButtonImage) {
    [theme setValue:toolbarButtonImage
     forAttribute:@"backgroundImage"
            style:GTMThemeStyleToolBarButton
            state:GTMThemeStateActiveWindow];
  } else {
    NSColor* startColor = [NSColor colorWithCalibratedWhite:1.0 alpha:0.0];
    NSColor* endColor = [NSColor colorWithCalibratedWhite:1.0 alpha:0.3];
    scoped_nsobject<NSGradient> gradient([[NSGradient alloc]
                                          initWithStartingColor:startColor
                                                    endingColor:endColor]);

    [theme setValue:gradient
       forAttribute:@"gradient"
              style:GTMThemeStyleToolBarButton
              state:GTMThemeStateActiveWindow];

    [theme setValue:gradient
       forAttribute:@"gradient"
              style:GTMThemeStyleToolBarButton
              state:GTMThemeStateActiveWindow];
  }

  NSColor* toolbarButtonIconColor =
      provider->GetNSColorTint(BrowserThemeProvider::TINT_BUTTONS);
  [theme setValue:toolbarButtonIconColor
     forAttribute:@"iconColor"
            style:GTMThemeStyleToolBarButton
            state:GTMThemeStateActiveWindow];

  NSColor* toolbarButtonBorderColor = toolbarButtonIconColor;
  [theme setValue:toolbarButtonBorderColor
     forAttribute:@"borderColor"
            style:GTMThemeStyleToolBar
            state:GTMThemeStateActiveWindow];

  NSColor* toolbarBackgroundColor =
      provider->GetNSColor(BrowserThemeProvider::COLOR_TOOLBAR);
  [theme setValue:toolbarBackgroundColor
     forAttribute:@"backgroundColor"
            style:GTMThemeStyleToolBar
            state:GTMThemeStateActiveWindow];

  NSImage* frameOverlayImage =
      provider->GetNSImageNamed(IDR_THEME_FRAME_OVERLAY);
  if (frameOverlayImage) {
    [theme setValue:frameOverlayImage
       forAttribute:@"overlay"
              style:GTMThemeStyleWindow
              state:GTMThemeStateActiveWindow];
  }

  NSImage* frameOverlayInactiveImage =
      provider->GetNSImageNamed(IDR_THEME_FRAME_OVERLAY_INACTIVE);
  if (frameOverlayInactiveImage) {
    [theme setValue:frameOverlayInactiveImage
       forAttribute:@"overlay"
              style:GTMThemeStyleWindow
              state:GTMThemeStateInactiveWindow];
  }

  return theme;
}
@end
