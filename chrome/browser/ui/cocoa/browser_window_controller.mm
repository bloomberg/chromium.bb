// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_controller.h"

#include <cmath>
#include <numeric>

#include "base/command_line.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#import "base/memory/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"  // IDC_*
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/profiles/avatar_menu_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_ui_util_mac.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window_state.h"
#import "chrome/browser/ui/cocoa/background_gradient_view.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_editor_controller.h"
#import "chrome/browser/ui/cocoa/browser/avatar_button_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#import "chrome/browser/ui/cocoa/browser_window_controller_private.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/chrome_to_mobile_bubble_controller.h"
#import "chrome/browser/ui/cocoa/dev_tools_controller.h"
#import "chrome/browser/ui/cocoa/download/download_shelf_controller.h"
#import "chrome/browser/ui/cocoa/event_utils.h"
#include "chrome/browser/ui/cocoa/extensions/extension_keybinding_registry_cocoa.h"
#import "chrome/browser/ui/cocoa/fast_resize_view.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/framed_browser_window.h"
#import "chrome/browser/ui/cocoa/fullscreen_window.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field_editor.h"
#import "chrome/browser/ui/cocoa/presentation_mode_controller.h"
#import "chrome/browser/ui/cocoa/status_bubble_mac.h"
#import "chrome/browser/ui/cocoa/tab_contents/previewable_contents_controller.h"
#import "chrome/browser/ui/cocoa/tab_contents/sad_tab_controller.h"
#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"
#import "chrome/browser/ui/cocoa/tabpose_window.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_view.h"
#import "chrome/browser/ui/cocoa/tabs/tab_view.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/tabs/dock_info.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/toolbar/encoding_menu_controller.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/mac/scoped_ns_disable_screen_updates.h"

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

using content::OpenURLParams;
using content::Referrer;
using content::RenderWidgetHostView;
using content::WebContents;

@interface NSWindow (NSPrivateApis)
// Note: These functions are private, use -[NSObject respondsToSelector:]
// before calling them.

- (void)setBottomCornerRounded:(BOOL)rounded;

- (NSRect)_growBoxRect;

@end

// Replicate specific 10.7 SDK declarations for building with prior SDKs.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

enum {
  NSWindowAnimationBehaviorDefault = 0,
  NSWindowAnimationBehaviorNone = 2,
  NSWindowAnimationBehaviorDocumentWindow = 3,
  NSWindowAnimationBehaviorUtilityWindow = 4,
  NSWindowAnimationBehaviorAlertPanel = 5
};
typedef NSInteger NSWindowAnimationBehavior;

enum {
  NSWindowCollectionBehaviorFullScreenPrimary = 1 << 7,
  NSWindowCollectionBehaviorFullScreenAuxiliary = 1 << 8
};

enum {
  NSFullScreenWindowMask = 1 << 14
};

@interface NSWindow (LionSDKDeclarations)
- (void)setRestorable:(BOOL)flag;
- (void)setAnimationBehavior:(NSWindowAnimationBehavior)newAnimationBehavior;
@end

#endif  // MAC_OS_X_VERSION_10_7

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
  bool hasTabStrip = browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP);
  if ((self = [super initTabWindowControllerWithTabStrip:hasTabStrip])) {
    DCHECK(browser);
    initializing_ = YES;
    browser_.reset(browser);
    ownsBrowser_ = ownIt;
    NSWindow* window = [self window];
    windowShim_.reset(new BrowserWindowCocoa(browser, self));

    // Set different minimum sizes on tabbed windows vs non-tabbed, e.g. popups.
    // This has to happen before -enforceMinWindowSize: is called further down.
    NSSize minSize = [self isTabbedWindow] ?
      NSMakeSize(400, 272) : NSMakeSize(100, 122);
    [[self window] setMinSize:minSize];

    // Create the bar visibility lock set; 10 is arbitrary, but should hopefully
    // be big enough to hold all locks that'll ever be needed.
    barVisibilityLocks_.reset([[NSMutableSet setWithCapacity:10] retain]);

    // Set the window to not have rounded corners, which prevents the resize
    // control from being inset slightly and looking ugly. Only bother to do
    // this on Snow Leopard; on Lion and later all windows have rounded bottom
    // corners, and this won't work anyway.
    if (base::mac::IsOSSnowLeopard() &&
        [window respondsToSelector:@selector(setBottomCornerRounded:)])
      [window setBottomCornerRounded:NO];

    // Lion will attempt to automagically save and restore the UI. This
    // functionality appears to be leaky (or at least interacts badly with our
    // architecture) and thus BrowserWindowController never gets released. This
    // prevents the browser from being able to quit <http://crbug.com/79113>.
    if ([window respondsToSelector:@selector(setRestorable:)])
      [window setRestorable:NO];

    // Get the windows to swish in on Lion.
    if ([window respondsToSelector:@selector(setAnimationBehavior:)])
      [window setAnimationBehavior:NSWindowAnimationBehaviorDocumentWindow];

    // Get the most appropriate size for the window, then enforce the
    // minimum width and height. The window shim will handle flipping
    // the coordinates for us so we can use it to save some code.
    // Note that this may leave a significant portion of the window
    // offscreen, but there will always be enough window onscreen to
    // drag the whole window back into view.
    ui::WindowShowState show_state = ui::SHOW_STATE_DEFAULT;
    gfx::Rect desiredContentRect;
    chrome::GetSavedWindowBoundsAndShowState(browser_.get(),
                                             &desiredContentRect,
                                             &show_state);
    gfx::Rect windowRect = desiredContentRect;
    windowRect = [self enforceMinWindowSize:windowRect];

    // When we are given x/y coordinates of 0 on a created popup window, assume
    // none were given by the window.open() command.
    if ((browser_->is_type_popup() || browser_->is_type_panel()) &&
         windowRect.x() == 0 && windowRect.y() == 0) {
      gfx::Size size = windowRect.size();
      windowRect.set_origin(
          WindowSizer::GetDefaultPopupOrigin(size,
                                             browser_->host_desktop_type()));
    }

    // Size and position the window.  Note that it is not yet onscreen.  Popup
    // windows may get resized later on in this function, once the actual size
    // of the toolbar/tabstrip is known.
    windowShim_->SetBounds(windowRect);

    // Puts the incognito badge on the window frame, if necessary.
    [self installAvatar];

    // Create a sub-controller for the docked devTools and add its view to the
    // hierarchy.
    devToolsController_.reset([[DevToolsController alloc] init]);
    [[devToolsController_ view] setFrame:[[self tabContentArea] bounds]];
    [[self tabContentArea] addSubview:[devToolsController_ view]];

    // Create the previewable contents controller.  This provides the switch
    // view that TabStripController needs.
    previewableContentsController_.reset(
        [[PreviewableContentsController alloc] initWithBrowser:browser
                                              windowController:self]);
    [[previewableContentsController_ view]
        setFrame:[[devToolsController_ view] bounds]];
    [[devToolsController_ view]
        addSubview:[previewableContentsController_ view]];

    // Create a controller for the tab strip, giving it the model object for
    // this window's Browser and the tab strip view. The controller will handle
    // registering for the appropriate tab notifications from the back-end and
    // managing the creation of new tabs.
    [self createTabStripController];

    // Create a controller for the toolbar, giving it the toolbar model object
    // and the toolbar view from the nib. The controller will handle
    // registering for the appropriate command state changes from the back-end.
    // Adds the toolbar to the content area.
    toolbarController_.reset([[ToolbarController alloc]
              initWithModel:browser->toolbar_model()
                   commands:browser->command_controller()->command_updater()
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

    // Create the infobar container view, so we can pass it to the
    // ToolbarController.
    infoBarContainerController_.reset(
        [[InfoBarContainerController alloc] initWithResizeDelegate:self]);
    [[[self window] contentView] addSubview:[infoBarContainerController_ view]];

    // We don't want to try and show the bar before it gets placed in its parent
    // view, so this step shoudn't be inside the bookmark bar controller's
    // |-awakeFromNib|.
    windowShim_->BookmarkBarStateChanged(
        BookmarkBar::DONT_ANIMATE_STATE_CHANGE);

    // Allow bar visibility to be changed.
    [self enableBarVisibilityUpdates];

    // Force a relayout of all the various bars.
    [self layoutSubviews];

    // Set the window to participate in Lion Fullscreen mode.  Setting this flag
    // has no effect on Snow Leopard or earlier.  Panels can share a fullscreen
    // space with a tabbed window, but they can not be primary fullscreen
    // windows.  Do this after |-layoutSubviews| so that the fullscreen button
    // can be adjusted in FramedBrowserWindow.
    NSUInteger collectionBehavior = [window collectionBehavior];
    collectionBehavior |=
       browser_->type() == Browser::TYPE_TABBED ||
           browser_->type() == Browser::TYPE_POPUP ?
               NSWindowCollectionBehaviorFullScreenPrimary :
               NSWindowCollectionBehaviorFullScreenAuxiliary;
    [window setCollectionBehavior:collectionBehavior];

    // For a popup window, |desiredContentRect| contains the desired height of
    // the content, not of the whole window.  Now that all the views are laid
    // out, measure the current content area size and grow if needed.  The
    // window has not been placed onscreen yet, so this extra resize will not
    // cause visible jank.
    if (browser_->is_type_popup() || browser_->is_type_panel()) {
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

    extension_keybinding_registry_.reset(
        new ExtensionKeybindingRegistryCocoa(browser_->profile(),
            [self window],
            extensions::ExtensionKeybindingRegistry::ALL_EXTENSIONS,
            windowShim_.get()));

    // We are done initializing now.
    initializing_ = NO;
  }
  return self;
}

- (void)dealloc {
  browser_->tab_strip_model()->CloseAllTabs();
  [downloadShelfController_ exiting];

  // Explicitly release |presentationModeController_| here, as it may call back
  // to this BWC in |-dealloc|.  We are required to call |-exitPresentationMode|
  // before releasing the controller.
  [presentationModeController_ exitPresentationMode];
  presentationModeController_.reset();

  // Under certain testing configurations we may not actually own the browser.
  if (ownsBrowser_ == NO)
    ignore_result(browser_.release());

  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [super dealloc];
}

- (gfx::Rect)enforceMinWindowSize:(gfx::Rect)bounds {
  gfx::Rect checkedBounds = bounds;

  NSSize minSize = [[self window] minSize];
  if (bounds.width() < minSize.width)
      checkedBounds.set_width(minSize.width);
  if (bounds.height() < minSize.height)
      checkedBounds.set_height(minSize.height);

  return checkedBounds;
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

- (InfoBarContainerController*)infoBarContainerController {
  return infoBarContainerController_.get();
}

- (StatusBubbleMac*)statusBubble {
  return statusBubble_;
}

- (LocationBarViewMac*)locationBarBridge {
  return [toolbarController_ locationBarBridge];
}

- (Profile*)profile {
  return browser_->profile();
}

- (AvatarButtonController*)avatarButtonController {
  return avatarButtonController_.get();
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
  DCHECK(browser_->tab_strip_model()->empty());
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

- (void)updateDevToolsForContents:(WebContents*)contents {
  [devToolsController_ updateDevToolsForWebContents:contents
                                        withProfile:browser_->profile()];
}

// Called when the user wants to close a window or from the shutdown process.
// The Browser object is in control of whether or not we're allowed to close. It
// may defer closing due to several states, such as onUnload handlers needing to
// be fired. If closing is deferred, the Browser will handle the processing
// required to get us to the closing state and (by watching for all the tabs
// going away) will again call to close the window when it's finally ready.
- (BOOL)windowShouldClose:(id)sender {
  // Disable updates while closing all tabs to avoid flickering.
  gfx::ScopedNSDisableScreenUpdates disabler;
  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!browser_->ShouldCloseWindow())
    return NO;

  // saveWindowPositionIfNeeded: only works if we are the last active
  // window, but orderOut: ends up activating another window, so we
  // have to save the window position before we call orderOut:.
  [self saveWindowPositionIfNeeded];

  if (!browser_->tab_strip_model()->empty()) {
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
  // selected WebContents's RenderWidgetHostView and tell it to activate.
  if (WebContents* contents = chrome::GetActiveWebContents(browser_.get())) {
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
  // selected WebContents's RenderWidgetHostView and tell it to deactivate.
  if (WebContents* contents = chrome::GetActiveWebContents(browser_.get())) {
    if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
      rwhv->SetActive(false);
  }
}

// Called when we have been minimized.
- (void)windowDidMiniaturize:(NSNotification *)notification {
  [self saveWindowPositionIfNeeded];

  // Let the selected RenderWidgetHostView know, so that it can tell plugins.
  if (WebContents* contents = chrome::GetActiveWebContents(browser_.get())) {
    if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
      rwhv->SetWindowVisibility(false);
  }
}

// Called when we have been unminimized.
- (void)windowDidDeminiaturize:(NSNotification *)notification {
  // Let the selected RenderWidgetHostView know, so that it can tell plugins.
  if (WebContents* contents = chrome::GetActiveWebContents(browser_.get())) {
    if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
      rwhv->SetWindowVisibility(true);
  }
}

// Called when the application has been hidden.
- (void)applicationDidHide:(NSNotification *)notification {
  // Let the selected RenderWidgetHostView know, so that it can tell plugins
  // (unless we are minimized, in which case nothing has really changed).
  if (![[self window] isMiniaturized]) {
    if (WebContents* contents = chrome::GetActiveWebContents(browser_.get())) {
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
    if (WebContents* contents = chrome::GetActiveWebContents(browser_.get())) {
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
      std::max(kProportion * NSWidth(frame),
               std::min(kProportion * NSHeight(frame), NSWidth(frame)));

  WebContents* contents = chrome::GetActiveWebContents(browser_.get());
  if (contents) {
    // If the intrinsic width is bigger, then make it the zoomed width.
    const int kScrollbarWidth = 16;  // TODO(viettrungluu): ugh.
    CGFloat intrinsicWidth = static_cast<CGFloat>(
        contents->GetPreferredSize().width() + kScrollbarWidth);
    zoomedWidth = std::max(zoomedWidth,
                           std::min(intrinsicWidth, NSWidth(frame)));
  }

  // Never shrink from the current size on zoom (see above).
  NSRect currentFrame = [[self window] frame];
  zoomedWidth = std::max(zoomedWidth, NSWidth(currentFrame));

  // |frame| determines our maximum extents. We need to set the origin of the
  // frame -- and only move it left if necessary.
  if (currentFrame.origin.x + zoomedWidth > NSMaxX(frame))
    frame.origin.x = NSMaxX(frame) - zoomedWidth;
  else
    frame.origin.x = currentFrame.origin.x;

  // Set the width. Don't touch y or height.
  frame.size.width = zoomedWidth;

  return frame;
}

- (void)activate {
  [BrowserWindowUtils activateWindowForController:self];
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
    CGFloat overlapArea = NSWidth(overlap) * NSHeight(overlap);
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
  if (NSWidth(newScrIntersectCurFr) * NSHeight(newScrIntersectCurFr) >=
      (NSWidth(curScrIntersectCurFr) * NSHeight(curScrIntersectCurFr) - 1.0)) {
    return YES;
  }

  // If it wasn't reasonable, return NO.
  return NO;
}

// Adjusts the window height by the given amount.
- (BOOL)adjustWindowHeightBy:(CGFloat)deltaH {
  // By not adjusting the window height when initializing, we can ensure that
  // the window opens with the same size that was saved on close.
  if (initializing_ || [self isFullscreen] || deltaH == 0)
    return NO;

  NSWindow* window = [self window];
  NSRect windowFrame = [window frame];
  NSRect workarea = [[window screen] visibleFrame];

  // If the window is not already fully in the workarea, do not adjust its frame
  // at all.
  if (!NSContainsRect(workarea, windowFrame))
    return NO;

  // Record the position of the top/bottom of the window, so we can easily check
  // whether we grew the window upwards/downwards.
  CGFloat oldWindowMaxY = NSMaxY(windowFrame);
  CGFloat oldWindowMinY = NSMinY(windowFrame);

  // We are "zoomed" if we occupy the full vertical space.
  bool isZoomed = (windowFrame.origin.y == workarea.origin.y &&
                   NSHeight(windowFrame) == NSHeight(workarea));

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
      return NO;
  } else {
    isShrinkingFromZoomed_ = NO;

    // Don't bother with anything else.
    if (isZoomed)
      return NO;
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
          std::min(NSHeight(windowFrame), NSHeight(workarea));
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
  return YES;
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

  // Disable screen updates to prevent flickering.
  if (view == [bookmarkBarController_ view] ||
      view == [downloadShelfController_ view]) {
    [[self window] disableScreenUpdatesUntilFlush];
  }

  // Grow or shrink the window by the amount of the height change.  We adjust
  // the window height only in two cases:
  // 1) We are adjusting the height of the bookmark bar and it is currently
  // animating either open or closed.
  // 2) We are adjusting the height of the download shelf.
  //
  // We do not adjust the window height for bookmark bar changes on the NTP.
  BOOL shouldAdjustBookmarkHeight =
      [bookmarkBarController_ isAnimatingBetweenState:BookmarkBar::HIDDEN
                                             andState:BookmarkBar::SHOW];

  BOOL resizeRectDirty = NO;
  if ((shouldAdjustBookmarkHeight && view == [bookmarkBarController_ view]) ||
      view == [downloadShelfController_ view]) {
    CGFloat deltaH = height - NSHeight(frame);
    if ([self adjustWindowHeightBy:deltaH] &&
        view == [downloadShelfController_ view]) {
      // If the window height didn't change, the download shelf will change the
      // size of the contents. If the contents size doesn't change, send it
      // an explicit grow box invalidation (else, the resize message does that.)
      resizeRectDirty = YES;
    }
  }

  frame.size.height = height;
  // TODO(rohitrao): Determine if calling setFrame: twice is bad.
  [view setFrame:frame];
  [self layoutSubviews];

  if (resizeRectDirty) {
    // Send new resize rect to foreground tab.
    if (content::WebContents* contents =
            chrome::GetActiveWebContents(browser_.get())) {
      if (content::RenderViewHost* rvh = contents->GetRenderViewHost()) {
        rvh->ResizeRectChanged(windowShim_->GetRootWindowResizerRect());
      }
    }
  }
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
    WebContents* current_tab = chrome::GetActiveWebContents(browser_.get());
    if (!current_tab) {
      return;
    }
    const std::string encoding = current_tab->GetEncoding();

    bool toggled = encoding_controller.IsItemChecked(profile, encoding, tag);
    NSInteger oldState = [item state];
    NSInteger newState = toggled ? NSOnState : NSOffState;
    if (oldState != newState)
      [item setState:newState];
  }
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
    if (chrome::SupportsCommand(browser_.get(), tag)) {
      // Generate return value (enabled state)
      enable = chrome::IsCommandEnabled(browser_.get(), tag);
      switch (tag) {
        case IDC_CLOSE_TAB:
          // Disable "close tab" if the receiving window is not tabbed.
          // We simply check whether the item has a keyboard shortcut set here;
          // app_controller_mac.mm actually determines whether the item should
          // be enabled.
          if ([static_cast<NSObject*>(item) isKindOfClass:[NSMenuItem class]])
            enable &= !![[static_cast<NSMenuItem*>(item) keyEquivalent] length];
          break;
        case IDC_FULLSCREEN: {
          if ([static_cast<NSObject*>(item) isKindOfClass:[NSMenuItem class]]) {
            NSString* menuTitle = l10n_util::GetNSString(
                [self isFullscreen] && ![self inPresentationMode] ?
                    IDS_EXIT_FULLSCREEN_MAC :
                    IDS_ENTER_FULLSCREEN_MAC);
            [static_cast<NSMenuItem*>(item) setTitle:menuTitle];

            if (base::mac::IsOSSnowLeopard())
              [static_cast<NSMenuItem*>(item) setHidden:YES];
          }
          break;
        }
        case IDC_PRESENTATION_MODE: {
          if ([static_cast<NSObject*>(item) isKindOfClass:[NSMenuItem class]]) {
            NSString* menuTitle = l10n_util::GetNSString(
                [self inPresentationMode] ? IDS_EXIT_PRESENTATION_MAC :
                                            IDS_ENTER_PRESENTATION_MAC);
            [static_cast<NSMenuItem*>(item) setTitle:menuTitle];
          }
          break;
        }
        case IDC_SHOW_SYNC_SETUP: {
          Profile* original_profile =
              browser_->profile()->GetOriginalProfile();
          enable &= original_profile->IsSyncAccessible();
          sync_ui_util::UpdateSyncItem(item, enable, original_profile);
          break;
        }
        default:
          // Special handling for the contents of the Text Encoding submenu. On
          // Mac OS, instead of enabling/disabling the top-level menu item, we
          // enable/disable the submenu's contents (per Apple's HIG).
          EncodingMenuController encoding_controller;
          if (encoding_controller.DoesCommandBelongToEncodingMenu(tag)) {
            enable &= chrome::IsCommandEnabled(browser_.get(),
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
  chrome::ExecuteCommand(targetController->browser_.get(), [sender tag]);
}

// Same as |-commandDispatch:|, but executes commands using a disposition
// determined by the key flags. If the window is in the background and the
// command key is down, ignore the command key, but process any other modifiers.
- (void)commandDispatchUsingKeyModifiers:(id)sender {
  DCHECK(sender);

  if (![sender isEnabled]) {
    // This code is reachable e.g. if the user mashes the back button, queuing
    // up a bunch of events before the button's enabled state is updated:
    // http://crbug.com/63254
    return;
  }

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
  chrome::ExecuteCommandWithDisposition(targetController->browser_.get(),
                                        command, disposition);
}

// Called when another part of the internal codebase needs to execute a
// command.
- (void)executeCommand:(int)command {
  chrome::ExecuteCommand(browser_.get(), command);
}

- (BOOL)handledByExtensionCommand:(NSEvent*)event {
  return extension_keybinding_registry_->ProcessKeyEvent(
      content::NativeWebKeyboardEvent(event));
}

// StatusBubble delegate method: tell the status bubble the frame it should
// position itself in.
- (NSRect)statusBubbleBaseFrame {
  NSView* view = [previewableContentsController_ view];
  return [view convertRect:[view bounds] toView:nil];
}

- (void)updateToolbarWithContents:(WebContents*)tab
               shouldRestoreState:(BOOL)shouldRestore {
  [toolbarController_ updateToolbarWithContents:tab
                             shouldRestoreState:shouldRestore];
}

- (void)setStarredState:(BOOL)isStarred {
  [toolbarController_ setStarredState:isStarred];
}

- (void)zoomChangedForActiveTab:(BOOL)canShowBubble {
  [toolbarController_ zoomChangedForActiveTab:canShowBubble];
}

// Return the rect, in WebKit coordinates (flipped), of the window's grow box
// in the coordinate system of the content area of the currently selected tab.
// |windowGrowBox| needs to be in the window's coordinate system.
- (NSRect)selectedTabGrowBoxRect {
  NSWindow* window = [self window];
  if (![window respondsToSelector:@selector(_growBoxRect)])
    return NSZeroRect;

  // Before we return a rect, we need to convert it from window coordinates
  // to tab content area coordinates and flip the coordinate system.
  NSRect growBoxRect =
      [[self tabContentArea] convertRect:[window _growBoxRect] fromView:nil];
  growBoxRect.origin.y =
      NSHeight([[self tabContentArea] frame]) - NSMaxY(growBoxRect);
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
    // Moving between windows. Figure out the WebContents to drop into our tab
    // model from the source window's model.
    BOOL isBrowser =
        [dragController isKindOfClass:[BrowserWindowController class]];
    DCHECK(isBrowser);
    if (!isBrowser) return;
    BrowserWindowController* dragBWC = (BrowserWindowController*)dragController;
    int index = [dragBWC->tabStripController_ modelIndexForTabView:view];
    WebContents* contents =
        dragBWC->browser_->tab_strip_model()->GetWebContentsAt(index);
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
    bool isPinned = dragBWC->browser_->tab_strip_model()->IsTabPinned(index);

    // Now that we have enough information about the tab, we can remove it from
    // the dragging window. We need to do this *before* we add it to the new
    // window as this will remove the WebContents' delegate.
    [dragController detachTabView:view];

    // Deposit it into our model at the appropriate location (it already knows
    // where it should go from tracking the drag). Doing this sets the tab's
    // delegate to be the Browser.
    [tabStripController_ dropWebContents:contents
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
  browser_->tab_strip_model()->DetachWebContentsAt(index);
}

- (NSView*)activeTabView {
  return [tabStripController_ activeTabView];
}

- (void)setIsLoading:(BOOL)isLoading force:(BOOL)force {
  [toolbarController_ setIsLoading:isLoading force:force];
}

// Make the location bar the first responder, if possible.
- (void)focusLocationBar:(BOOL)selectAll {
  [toolbarController_ focusLocationBar:selectAll];
}

- (void)focusTabContents {
  [[self window] makeFirstResponder:[tabStripController_ activeTabView]];
}

- (void)layoutTabs {
  [tabStripController_ layoutTabs];
}

- (TabWindowController*)detachTabToNewWindow:(TabView*)tabView {
  // Disable screen updates so that this appears as a single visual change.
  gfx::ScopedNSDisableScreenUpdates disabler;

  // Fetch the tab contents for the tab being dragged.
  int index = [tabStripController_ modelIndexForTabView:tabView];
  WebContents* contents = browser_->tab_strip_model()->GetWebContentsAt(index);

  // Set the window size. Need to do this before we detach the tab so it's
  // still in the window. We have to flip the coordinates as that's what
  // is expected by the Browser code.
  NSWindow* sourceWindow = [tabView window];
  NSRect windowRect = [sourceWindow frame];
  NSScreen* screen = [sourceWindow screen];
  windowRect.origin.y = NSHeight([screen frame]) - NSMaxY(windowRect);
  gfx::Rect browserRect(windowRect.origin.x, windowRect.origin.y,
                        NSWidth(windowRect), NSHeight(windowRect));

  NSRect sourceTabRect = [tabView frame];
  NSView* tabStrip = [self tabStripView];

  // Pushes tabView's frame back inside the tabstrip.
  NSSize tabOverflow =
      [self overflowFrom:[tabStrip convertRect:sourceTabRect toView:nil]
                      to:[tabStrip frame]];
  NSRect tabRect = NSOffsetRect(sourceTabRect,
                                -tabOverflow.width, -tabOverflow.height);

  // Before detaching the tab, store the pinned state.
  bool isPinned = browser_->tab_strip_model()->IsTabPinned(index);

  // Detach it from the source window, which just updates the model without
  // deleting the tab contents. This needs to come before creating the new
  // Browser because it clears the WebContents' delegate, which gets hooked
  // up during creation of the new window.
  browser_->tab_strip_model()->DetachWebContentsAt(index);

  // Create the new window with a single tab in its model, the one being
  // dragged.
  DockInfo dockInfo;
  TabStripModelDelegate::NewStripContents item;
  item.web_contents = contents;
  item.add_types = TabStripModel::ADD_ACTIVE |
                   (isPinned ? TabStripModel::ADD_PINNED
                             : TabStripModel::ADD_NONE);
  std::vector<TabStripModelDelegate::NewStripContents> contentses;
  contentses.push_back(item);
  Browser* newBrowser = browser_->tab_strip_model()->delegate()->
      CreateNewStripWithContents(contentses, browserRect, dockInfo, false);

  // Get the new controller by asking the new window for its delegate.
  BrowserWindowController* controller =
      reinterpret_cast<BrowserWindowController*>(
          [newBrowser->window()->GetNativeWindow() delegate]);
  DCHECK(controller && [controller isKindOfClass:[TabWindowController class]]);

  // Force the added tab to the right size (remove stretching.)
  tabRect.size.height = [TabStripController defaultTabHeight];

  // And make sure we use the correct frame in the new view.
  [[controller tabStripController] setFrameOfActiveTab:tabRect];
  return controller;
}

- (void)insertPlaceholderForTab:(TabView*)tab
                          frame:(NSRect)frame {
  [super insertPlaceholderForTab:tab frame:frame];
  [tabStripController_ insertPlaceholderForTab:tab frame:frame];
}

- (void)removePlaceholder {
  [super removePlaceholder];
  [tabStripController_ insertPlaceholderForTab:nil frame:NSZeroRect];
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

- (BOOL)shouldShowAvatar {
  if (![self hasTabStrip])
    return NO;
  if (browser_->profile()->IsOffTheRecord() ||
      ManagedMode::IsInManagedMode()) {
    return YES;
  }

  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  if (cache.GetIndexOfProfileWithPath(browser_->profile()->GetPath()) ==
      std::string::npos) {
    return NO;
  }

  return AvatarMenuModel::ShouldShowAvatarMenu();
}

- (BOOL)isBookmarkBarVisible {
  return [bookmarkBarController_ isVisible];
}

- (BOOL)isBookmarkBarAnimating {
  return [bookmarkBarController_ isAnimationRunning];
}

- (BookmarkBarController*)bookmarkBarController {
  return bookmarkBarController_;
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
  }
  return downloadShelfController_;
}

- (void)addFindBar:(FindBarCocoaController*)findBarCocoaController {
  // Shouldn't call addFindBar twice.
  DCHECK(!findBarCocoaController_.get());

  // Create a controller for the findbar.
  findBarCocoaController_.reset([findBarCocoaController retain]);
  NSView* contentView = [[self window] contentView];
  NSView* relativeView =
      [self inPresentationMode] ? [toolbarController_ view] :
                                  [infoBarContainerController_ view];
  [contentView addSubview:[findBarCocoaController_ view]
               positioned:NSWindowAbove
               relativeTo:relativeView];

  // Place the find bar immediately below the toolbar/attached bookmark bar. In
  // presentation mode, it hangs off the top of the screen when the bar is
  // hidden.
  CGFloat maxY = [self placeBookmarkBarBelowInfoBar] ?
      NSMinY([[toolbarController_ view] frame]) :
      NSMinY([[bookmarkBarController_ view] frame]);
  CGFloat maxWidth = NSWidth([contentView frame]);
  [findBarCocoaController_ positionFindBarViewAtMaxY:maxY maxWidth:maxWidth];

  // This allows the FindBarCocoaController to call |layoutSubviews| and get
  // its position adjusted.
  [findBarCocoaController_ setBrowserWindowController:self];
}

- (NSWindow*)createFullscreenWindow {
  return [[[FullscreenWindow alloc] initForScreen:[[self window] screen]]
           autorelease];
}

- (NSInteger)numberOfTabs {
  // count() includes pinned tabs.
  return browser_->tab_strip_model()->count();
}

- (BOOL)hasLiveTabs {
  return !browser_->tab_strip_model()->empty();
}

- (NSString*)activeTabTitle {
  WebContents* contents = chrome::GetActiveWebContents(browser_.get());
  return base::SysUTF16ToNSString(contents->GetTitle());
}

- (NSRect)regularWindowFrame {
  return [self isFullscreen] ? savedRegularWindowFrame_ :
                               [[self window] frame];
}

// (Override of |TabWindowController| method.)
- (BOOL)hasTabStrip {
  return [self supportsWindowFeature:Browser::FEATURE_TABSTRIP];
}

- (BOOL)isTabDraggable:(NSView*)tabView {
  // TODO(avi, thakis): ConstrainedWindowSheetController has no api to move
  // tabsheets between windows. Until then, we have to prevent having to move a
  // tabsheet between windows, e.g. no tearing off of tabs.
  int index = [tabStripController_ modelIndexForTabView:tabView];
  WebContents* contents = chrome::GetWebContentsAt(browser_.get(), index);
  if (!contents)
    return NO;
  return !WebContentsModalDialogManager::FromWebContents(contents)->
      IsShowingDialog();
}

// TabStripControllerDelegate protocol.
- (void)onActivateTabWithContents:(WebContents*)contents {
  // Update various elements that are interested in knowing the current
  // WebContents.

  // Update all the UI bits.
  windowShim_->UpdateTitleBar();

  [devToolsController_ updateDevToolsForWebContents:contents
                                        withProfile:browser_->profile()];

  // Update the bookmark bar.
  // Must do it after devtools updates, otherwise bookmark bar might
  // call resizeView -> layoutSubviews and cause unnecessary relayout.
  // TODO(viettrungluu): perhaps update to not terminate running animations (if
  // applicable)?
  windowShim_->BookmarkBarStateChanged(
      BookmarkBar::DONT_ANIMATE_STATE_CHANGE);

  [infoBarContainerController_ changeWebContents:contents];

  [previewableContentsController_ onActivateTabWithContents:contents];
}

- (void)onTabChanged:(TabStripModelObserver::TabChangeType)change
        withContents:(WebContents*)contents {
  // Update titles if this is the currently selected tab and if it isn't just
  // the loading state which changed.
  if (change != TabStripModelObserver::LOADING_ONLY)
    windowShim_->UpdateTitleBar();

  // Update the bookmark bar if this is the currently selected tab and if it
  // isn't just the title which changed. This for transitions between the NTP
  // (showing its floating bookmark bar) and normal web pages (showing no
  // bookmark bar).
  // TODO(viettrungluu): perhaps update to not terminate running animations?
  if (change != TabStripModelObserver::TITLE_NOT_LOADING) {
    windowShim_->BookmarkBarStateChanged(
        BookmarkBar::DONT_ANIMATE_STATE_CHANGE);
  }
}

- (void)onTabDetachedWithContents:(WebContents*)contents {
  [infoBarContainerController_ tabDetachedWithContents:contents];
}

- (void)userChangedTheme {
  // TODO(dmaclach): Instead of redrawing the whole window, views that care
  // about the active window state should be registering for notifications.
  [[self window] setViewsNeedDisplay:YES];
}

- (ui::ThemeProvider*)themeProvider {
  return ThemeServiceFactory::GetForProfile(browser_->profile());
}

- (ThemedWindowStyle)themedWindowStyle {
  ThemedWindowStyle style = 0;
  if (browser_->profile()->IsOffTheRecord())
    style |= THEMED_INCOGNITO;

  if (browser_->is_devtools())
    style |= THEMED_DEVTOOLS;
  if (browser_->is_type_popup())
    style |= THEMED_POPUP;

  return style;
}

- (NSPoint)themePatternPhase {
  NSView* windowChromeView = [[[self window] contentView] superview];
  return [BrowserWindowUtils themePatternPhaseFor:windowChromeView
                                     withTabStrip:[self tabStripView]];
}

- (NSPoint)bookmarkBubblePoint {
  return [toolbarController_ bookmarkBubblePoint];
}

// Show the bookmark bubble (e.g. user just clicked on the STAR).
- (void)showBookmarkBubbleForURL:(const GURL&)url
               alreadyBookmarked:(BOOL)alreadyMarked {
  if (!bookmarkBubbleController_) {
    BookmarkModel* model =
        BookmarkModelFactory::GetForProfile(browser_->profile());
    const BookmarkNode* node = model->GetMostRecentlyAddedNodeForURL(url);
    bookmarkBubbleController_ =
        [[BookmarkBubbleController alloc] initWithParentWindow:[self window]
                                                         model:model
                                                          node:node
                                             alreadyBookmarked:alreadyMarked];
    [bookmarkBubbleController_ showWindow:self];
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(bookmarkBubbleWindowWillClose:)
                   name:NSWindowWillCloseNotification
                 object:[bookmarkBubbleController_ window]];
  }
}

// Nil out the weak bookmark bubble controller reference.
- (void)bookmarkBubbleWindowWillClose:(NSNotification*)notification {
  DCHECK_EQ([notification object], [bookmarkBubbleController_ window]);

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center removeObserver:self
                    name:NSWindowWillCloseNotification
                  object:[bookmarkBubbleController_ window]];
  bookmarkBubbleController_ = nil;
}

// Show the Chrome To Mobile bubble (e.g. user just clicked on the icon).
- (void)showChromeToMobileBubble {
  // Do nothing if the bubble is already showing.
  if (chromeToMobileBubbleController_)
    return;

  chromeToMobileBubbleController_ =
      [[ChromeToMobileBubbleController alloc]
          initWithParentWindow:[self window]
                       browser:browser_.get()];
  [chromeToMobileBubbleController_ showWindow:self];
}

// Nil out the weak Chrome To Mobile bubble controller reference.
- (void)chromeToMobileBubbleWindowWillClose {
  chromeToMobileBubbleController_ = nil;
}

// Handle the editBookmarkNode: action sent from bookmark bubble controllers.
- (void)editBookmarkNode:(id)sender {
  BOOL responds = [sender respondsToSelector:@selector(node)];
  DCHECK(responds);
  if (responds) {
    const BookmarkNode* node = [sender node];
    if (node)
      BookmarkEditor::Show([self window], browser_->profile(),
          BookmarkEditor::EditDetails::EditNode(node),
          BookmarkEditor::SHOW_TREE);
  }
}

// If the browser is in incognito mode or has multi-profiles, install the image
// view to decorate the window at the upper right. Use the same base y
// coordinate as the tab strip.
- (void)installAvatar {
  // Install the image into the badge view. Hide it for now; positioning and
  // sizing will be done by the layout code. The AvatarButton will choose which
  // image to display based on the browser.
  avatarButtonController_.reset(
      [[AvatarButtonController alloc] initWithBrowser:browser_.get()]);
  NSView* view = [avatarButtonController_ view];
  [view setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
  [view setHidden:![self shouldShowAvatar]];

  // Install the view.
  [[[[self window] contentView] superview] addSubview:view];
}

// Called when we get a three-finger swipe.
- (void)swipeWithEvent:(NSEvent*)event {
  CGFloat deltaX = [event deltaX];
  CGFloat deltaY = [event deltaY];

  // Map forwards and backwards to history; left is positive, right is negative.
  unsigned int command = 0;
  if (deltaX > 0.5) {
    command = IDC_BACK;
  } else if (deltaX < -0.5) {
    command = IDC_FORWARD;
  } else if (deltaY > 0.5) {
    // TODO(pinkerton): figure out page-up, http://crbug.com/16305
  } else if (deltaY < -0.5) {
    // TODO(pinkerton): figure out page-down, http://crbug.com/16305
    chrome::ExecuteCommand(browser_.get(), IDC_TABPOSE);
  }

  // Ensure the command is valid first (ExecuteCommand() won't do that) and
  // then make it so.
  if (chrome::IsCommandEnabled(browser_.get(), command)) {
    chrome::ExecuteCommandWithDisposition(
        browser_.get(),
        command,
        event_utils::WindowOpenDispositionFromNSEvent(event));
  }
}

// Called repeatedly during a pinch gesture, with incremental change values.
- (void)magnifyWithEvent:(NSEvent*)event {
  // The deltaZ difference necessary to trigger a zoom action. Derived from
  // experimentation to find a value that feels reasonable.
  const float kZoomStepValue = 0.6;

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
  totalMagnifyGestureAmount_ += [event magnification];
  if (totalMagnifyGestureAmount_ > zoomInThreshold) {
    command = IDC_ZOOM_PLUS;
  } else if (totalMagnifyGestureAmount_ < zoomOutThreshold) {
    command = IDC_ZOOM_MINUS;
  }

  if (command && chrome::IsCommandEnabled(browser_.get(), command)) {
    currentZoomStepDelta_ += (command == IDC_ZOOM_PLUS) ? 1 : -1;
    chrome::ExecuteCommandWithDisposition(
        browser_.get(),
        command,
        event_utils::WindowOpenDispositionFromNSEvent(event));
  }
}

// Delegate method called when window is resized.
- (void)windowDidResize:(NSNotification*)notification {
  [self saveWindowPositionIfNeeded];

  // Resize (and possibly move) the status bubble. Note that we may get called
  // when the status bubble does not exist.
  if (statusBubble_) {
    statusBubble_->UpdateSizeAndPosition();
  }

  // Let the selected RenderWidgetHostView know, so that it can tell plugins.
  if (WebContents* contents = chrome::GetActiveWebContents(browser_.get())) {
    if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
      rwhv->WindowFrameChanged();
  }

  // The FindBar needs to know its own position to properly detect overlaps
  // with find results. The position changes whenever the window is resized,
  // and |layoutSubviews| computes the FindBar's position.
  // TODO: calling |layoutSubviews| here is a waste, find a better way to
  // do this.
  if ([findBarCocoaController_ isFindBarVisible])
    [self layoutSubviews];

  if ([self placeBookmarkBarBelowInfoBar] &&
      [bookmarkBarController_ shouldShowAtBottomWhenDetached]) {
    [self layoutBottomBookmarkBarInContentFrame:[[self tabContentArea] frame]];
  }
}

// Handle the openLearnMoreAboutCrashLink: action from SadTabController when
// "Learn more" link in "Aw snap" page (i.e. crash page or sad tab) is
// clicked. Decoupling the action from its target makes unit testing possible.
- (void)openLearnMoreAboutCrashLink:(id)sender {
  if ([sender isKindOfClass:[SadTabController class]]) {
    SadTabController* sad_tab = static_cast<SadTabController*>(sender);
    WebContents* web_contents = [sad_tab webContents];
    if (web_contents) {
      OpenURLParams params(
          GURL(chrome::kCrashReasonURL), Referrer(), CURRENT_TAB,
          content::PAGE_TRANSITION_LINK, false);
      web_contents->OpenURL(params);
    }
  }
}

// Delegate method called when window did move. (See below for why we don't use
// |-windowWillMove:|, which is called less frequently than |-windowDidMove|
// instead.)
- (void)windowDidMove:(NSNotification*)notification {
  [self saveWindowPositionIfNeeded];

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
  if (WebContents* contents = chrome::GetActiveWebContents(browser_.get())) {
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
 didChangeFromState:(BookmarkBar::State)oldState
            toState:(BookmarkBar::State)newState {
  [toolbarController_
      setDividerOpacity:[bookmarkBarController_ toolbarDividerOpacity]];
  [self adjustToolbarAndBookmarkBarForCompression:
          [controller getDesiredToolbarHeightCompression]];
}

// (Needed for |BookmarkBarControllerDelegate| protocol.)
- (void)bookmarkBar:(BookmarkBarController*)controller
willAnimateFromState:(BookmarkBar::State)oldState
            toState:(BookmarkBar::State)newState {
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

// (Private/TestingAPI)
- (FullscreenExitBubbleController*)fullscreenExitBubbleController {
  return fullscreenExitBubbleController_.get();
}

- (void)commitInstant {
  if (chrome::BrowserInstantController* controller =
          browser_->instant_controller())
    controller->instant()->CommitIfPossible(INSTANT_COMMIT_FOCUS_LOST);
}

- (BOOL)isInstantTabShowing {
  return previewableContentsController_ &&
      [previewableContentsController_ isShowingPreview];
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

  // Account for the bookmark bar height if it is currently in the detached
  // state on the new tab page.
  if ([bookmarkBarController_ isInState:BookmarkBar::DETACHED])
    frame.size.height += NSHeight([[bookmarkBarController_ view] bounds]);

  return frame;
}

- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(NSInteger)code
            context:(void*)context {
  [sheet orderOut:self];
}

@end  // @implementation BrowserWindowController


@implementation BrowserWindowController(Fullscreen)

- (void)handleLionToggleFullscreen {
  DCHECK(base::mac::IsOSLionOrLater());
  chrome::ExecuteCommand(browser_.get(), IDC_FULLSCREEN);
}

// On Lion, this method is called by either the Lion fullscreen button or the
// "Enter Full Screen" menu item.  On Snow Leopard, this function is never
// called by the UI directly, but it provides the implementation for
// |-setPresentationMode:|.
- (void)setFullscreen:(BOOL)fullscreen
                  url:(const GURL&)url
           bubbleType:(FullscreenExitBubbleType)bubbleType {
  if (fullscreen == [self isFullscreen])
    return;

  if (!chrome::IsCommandEnabled(browser_.get(), IDC_FULLSCREEN))
    return;

  if (base::mac::IsOSLionOrLater()) {
    enteredPresentationModeFromFullscreen_ = YES;
    if ([[self window] isKindOfClass:[FramedBrowserWindow class]])
      [static_cast<FramedBrowserWindow*>([self window]) toggleSystemFullScreen];
  } else {
    if (fullscreen)
      [self enterFullscreenForSnowLeopard];
    else
      [self exitFullscreenForSnowLeopard];
  }
}

- (void)enterFullscreenForURL:(const GURL&)url
                   bubbleType:(FullscreenExitBubbleType)bubbleType {
  [self setFullscreen:YES url:url bubbleType:bubbleType];
}

- (void)exitFullscreen {
  // url: and bubbleType: are ignored when leaving fullscreen.
  [self setFullscreen:NO url:GURL() bubbleType:FEB_TYPE_NONE];
}

- (void)updateFullscreenExitBubbleURL:(const GURL&)url
                           bubbleType:(FullscreenExitBubbleType)bubbleType {
  fullscreenUrl_ = url;
  fullscreenBubbleType_ = bubbleType;
  [self showFullscreenExitBubbleIfNecessary];
}

- (BOOL)isFullscreen {
  return (fullscreenWindow_.get() != nil) ||
         ([[self window] styleMask] & NSFullScreenWindowMask) ||
         enteringFullscreen_;
}

// On Lion, this function is called by either the presentation mode toggle
// button or the "Enter Presentation Mode" menu item.  In the latter case, this
// function also triggers the Lion machinery to enter fullscreen mode as well as
// set presentation mode.  On Snow Leopard, this function is called by the
// "Enter Presentation Mode" menu item, and triggering presentation mode always
// moves the user into fullscreen mode.
- (void)setPresentationMode:(BOOL)presentationMode
                        url:(const GURL&)url
                 bubbleType:(FullscreenExitBubbleType)bubbleType {
  fullscreenUrl_ = url;
  fullscreenBubbleType_ = bubbleType;

  // Presentation mode on Snow Leopard maps directly to fullscreen mode.
  if (base::mac::IsOSSnowLeopard()) {
    [self setFullscreen:presentationMode url:url bubbleType:bubbleType];
    return;
  }

  if (presentationMode) {
    BOOL fullscreen = [self isFullscreen];
    enteredPresentationModeFromFullscreen_ = fullscreen;
    enteringPresentationMode_ = YES;

    if (fullscreen) {
      // If already in fullscreen mode, just toggle the presentation mode
      // setting.  Go through an elaborate dance to force the overlay to show,
      // then animate out once the mouse moves away.  This helps draw attention
      // to the fact that the UI is in an overlay.  Focus the tab contents
      // because the omnibox is the most likely source of bar visibility locks,
      // and taking focus away from the omnibox releases its lock.
      [self lockBarVisibilityForOwner:self withAnimation:NO delay:NO];
      [self focusTabContents];
      [self setPresentationModeInternal:YES forceDropdown:YES];
      [self releaseBarVisibilityForOwner:self withAnimation:YES delay:YES];
      // Since -windowDidEnterFullScreen: won't be called in the
      // fullscreen --> presentation mode case, manually show the exit bubble
      // and notify the change happened with WindowFullscreenStateChanged().
      [self showFullscreenExitBubbleIfNecessary];
      browser_->WindowFullscreenStateChanged();
    } else {
      // If not in fullscreen mode, trigger the Lion fullscreen mode machinery.
      // Presentation mode will automatically be enabled in
      // |-windowWillEnterFullScreen:|.
      NSWindow* window = [self window];
      if ([window isKindOfClass:[FramedBrowserWindow class]])
        [static_cast<FramedBrowserWindow*>(window) toggleSystemFullScreen];
    }
  } else {
    // Exiting presentation mode does not exit system fullscreen; it merely
    // switches from presentation mode to normal fullscreen.
    [self setPresentationModeInternal:NO forceDropdown:NO];
  }
}

- (void)enterPresentationModeForURL:(const GURL&)url
                         bubbleType:(FullscreenExitBubbleType)bubbleType {
  [self setPresentationMode:YES url:url bubbleType:bubbleType];
}

- (void)exitPresentationMode {
  // url: and bubbleType: are ignored when leaving presentation mode.
  [self setPresentationMode:NO url:GURL() bubbleType:FEB_TYPE_NONE];
}

- (BOOL)inPresentationMode {
  return presentationModeController_.get() &&
      [presentationModeController_ inPresentationMode];
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

    // If enabled, show the overlay if necessary (and if in presentation mode).
    if (barVisibilityUpdatesEnabled_) {
      [presentationModeController_ ensureOverlayShownWithAnimation:animate
                                                             delay:delay];
    }
  }
}

- (void)releaseBarVisibilityForOwner:(id)owner
                       withAnimation:(BOOL)animate
                               delay:(BOOL)delay {
  if ([self isBarVisibilityLockedForOwner:owner]) {
    [barVisibilityLocks_ removeObject:owner];

    // If enabled, hide the overlay if necessary (and if in presentation mode).
    if (barVisibilityUpdatesEnabled_ &&
        ![barVisibilityLocks_ count]) {
      [presentationModeController_ ensureOverlayHiddenWithAnimation:animate
                                                              delay:delay];
    }
  }
}

- (BOOL)floatingBarHasFocus {
  NSResponder* focused = [[self window] firstResponder];
  return [focused isKindOfClass:[AutocompleteTextFieldEditor class]];
}

- (void)tabposeWillClose:(NSNotification*)notif {
  // Re-show the container after Tabpose closes.
  [[infoBarContainerController_ view] setHidden:NO];
}

- (void)openTabpose {
  NSUInteger modifierFlags = [[NSApp currentEvent] modifierFlags];
  BOOL slomo = (modifierFlags & NSShiftKeyMask) != 0;

  // Cover info bars, inspector window, and detached bookmark bar on NTP.
  // Do not cover download shelf.
  NSRect activeArea = [[self tabContentArea] frame];
  // Take out the anti-spoof height so that Tabpose doesn't draw on top of the
  // browser chrome.
  activeArea.size.height +=
      NSHeight([[infoBarContainerController_ view] frame]) -
          [infoBarContainerController_ overlappingTipHeight];
  if ([self isBookmarkBarVisible] && [self placeBookmarkBarBelowInfoBar]) {
    NSView* bookmarkBarView = [bookmarkBarController_ view];
    activeArea.size.height += NSHeight([bookmarkBarView frame]);
  }

  // Hide the infobar container so that the anti-spoof bulge doesn't show when
  // Tabpose is open.
  [[infoBarContainerController_ view] setHidden:YES];

  TabposeWindow* window =
      [TabposeWindow openTabposeFor:[self window]
                               rect:activeArea
                              slomo:slomo
                      tabStripModel:browser_->tab_strip_model()];

  // When the Tabpose window closes, the infobar container needs to be made
  // visible again.
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(tabposeWillClose:)
                 name:NSWindowWillCloseNotification
               object:window];
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

- (BOOL)isTabbedWindow {
  return browser_->is_type_tabbed();
}

@end  // @implementation BrowserWindowController(WindowType)
