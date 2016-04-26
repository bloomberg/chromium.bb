// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_controller_private.h"

#include <cmath>

#import "base/auto_reset.h"
#include "base/command_line.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/fullscreen.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window_state.h"
#import "chrome/browser/ui/cocoa/browser_window_fullscreen_transition.h"
#import "chrome/browser/ui/cocoa/browser_window_layout.h"
#import "chrome/browser/ui/cocoa/browser/exclusive_access_controller_views.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"
#import "chrome/browser/ui/cocoa/custom_frame_view.h"
#import "chrome/browser/ui/cocoa/dev_tools_controller.h"
#import "chrome/browser/ui/cocoa/fast_resize_view.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/floating_bar_backing_view.h"
#import "chrome/browser/ui/cocoa/framed_browser_window.h"
#import "chrome/browser/ui/cocoa/fullscreen_window.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#import "chrome/browser/ui/cocoa/presentation_mode_controller.h"
#import "chrome/browser/ui/cocoa/profiles/avatar_button_controller.h"
#import "chrome/browser/ui/cocoa/profiles/avatar_icon_controller.h"
#import "chrome/browser/ui/cocoa/status_bubble_mac.h"
#import "chrome/browser/ui/cocoa/tab_contents/overlayable_contents_controller.h"
#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_view.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_cocoa.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/cocoa/focus_tracker.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/ui_base_types.h"

using content::RenderWidgetHostView;
using content::WebContents;

namespace {

// The screen on which the window was fullscreened, and whether the device had
// multiple screens available.
enum WindowLocation {
  PRIMARY_SINGLE_SCREEN = 0,
  PRIMARY_MULTIPLE_SCREEN = 1,
  SECONDARY_MULTIPLE_SCREEN = 2,
  WINDOW_LOCATION_COUNT = 3
};

// There are 2 mechanisms for invoking fullscreen: AppKit and Immersive.
// There are 2 types of AppKit Fullscreen: Presentation Mode and Canonical
// Fullscreen.
enum FullscreenStyle {
  IMMERSIVE_FULLSCREEN = 0,
  PRESENTATION_MODE = 1,
  CANONICAL_FULLSCREEN = 2,
  FULLSCREEN_STYLE_COUNT = 3
};

// Emits a histogram entry indicating the Fullscreen window location.
void RecordFullscreenWindowLocation(NSWindow* window) {
  NSArray* screens = [NSScreen screens];
  bool primary_screen = ([[window screen] isEqual:[screens objectAtIndex:0]]);
  bool multiple_screens = [screens count] > 1;

  WindowLocation location = PRIMARY_SINGLE_SCREEN;
  if (multiple_screens) {
    location =
        primary_screen ? PRIMARY_MULTIPLE_SCREEN : SECONDARY_MULTIPLE_SCREEN;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "OSX.Fullscreen.Enter.WindowLocation", location, WINDOW_LOCATION_COUNT);
}

// Emits a histogram entry indicating the Fullscreen style.
void RecordFullscreenStyle(FullscreenStyle style) {
  UMA_HISTOGRAM_ENUMERATION(
      "OSX.Fullscreen.Enter.Style", style, FULLSCREEN_STYLE_COUNT);
}

}  // namespace

@interface NSWindow (NSPrivateApis)
// Note: These functions are private, use -[NSObject respondsToSelector:]
// before calling them.
- (NSWindow*)_windowForToolbar;
@end

@implementation BrowserWindowController(Private)

// Create the tab strip controller.
- (void)createTabStripController {
  DCHECK([overlayableContentsController_ activeContainer]);
  DCHECK([[overlayableContentsController_ activeContainer] window]);
  tabStripController_.reset([[TabStripController alloc]
      initWithView:[self tabStripView]
        switchView:[overlayableContentsController_ activeContainer]
           browser:browser_.get()
          delegate:self]);
}

- (void)updateFullscreenCollectionBehavior {
  // Set the window to participate in Lion Fullscreen mode.  Setting this flag
  // has no effect on Snow Leopard or earlier.  Panels can share a fullscreen
  // space with a tabbed window, but they can not be primary fullscreen
  // windows.
  // This ensures the fullscreen button is appropriately positioned. It must
  // be done before calling layoutSubviews because the new avatar button's
  // position depends on the fullscreen button's position, as well as
  // TabStripController's rightIndentForControls.
  // The fullscreen button's position may depend on the old avatar button's
  // width, but that does not require calling layoutSubviews first.
  NSWindow* window = [self window];
  NSUInteger collectionBehavior = [window collectionBehavior];
  collectionBehavior &= ~NSWindowCollectionBehaviorFullScreenAuxiliary;
  collectionBehavior &= ~NSWindowCollectionBehaviorFullScreenPrimary;
  collectionBehavior |= browser_->type() == Browser::TYPE_TABBED ||
                                browser_->type() == Browser::TYPE_POPUP
                            ? NSWindowCollectionBehaviorFullScreenPrimary
                            : NSWindowCollectionBehaviorFullScreenAuxiliary;
  [window setCollectionBehavior:collectionBehavior];
}

- (void)saveWindowPositionIfNeeded {
  if (!chrome::ShouldSaveWindowPlacement(browser_.get()))
    return;

  // If we're in fullscreen mode, save the position of the regular window
  // instead.
  NSWindow* window =
      [self isInAnyFullscreenMode] ? savedRegularWindow_ : [self window];

  // Window positions are stored relative to the origin of the primary monitor.
  NSRect monitorFrame = [[[NSScreen screens] firstObject] frame];
  NSScreen* windowScreen = [window screen];

  // Start with the window's frame, which is in virtual coordinates.
  // Do some y twiddling to flip the coordinate system.
  gfx::Rect bounds(NSRectToCGRect([window frame]));
  bounds.set_y(monitorFrame.size.height - bounds.y() - bounds.height());

  // Browser::SaveWindowPlacement saves information for session restore.
  ui::WindowShowState show_state = ui::SHOW_STATE_NORMAL;
  if ([window isMiniaturized])
    show_state = ui::SHOW_STATE_MINIMIZED;
  else if ([self isInAnyFullscreenMode])
    show_state = ui::SHOW_STATE_FULLSCREEN;
  chrome::SaveWindowPlacement(browser_.get(), bounds, show_state);

  // |windowScreen| can be nil (for example, if the monitor arrangement was
  // changed while in fullscreen mode).  If we see a nil screen, return without
  // saving.
  // TODO(rohitrao): We should just not save anything for fullscreen windows.
  // http://crbug.com/36479.
  if (!windowScreen)
    return;

  // Only save main window information to preferences.
  PrefService* prefs = browser_->profile()->GetPrefs();
  if (!prefs || browser_.get() != chrome::GetLastActiveBrowser())
    return;

  // Save the current work area, in flipped coordinates.
  gfx::Rect workArea(NSRectToCGRect([windowScreen visibleFrame]));
  workArea.set_y(monitorFrame.size.height - workArea.y() - workArea.height());

  std::unique_ptr<DictionaryPrefUpdate> update =
      chrome::GetWindowPlacementDictionaryReadWrite(
          chrome::GetWindowName(browser_.get()),
          browser_->profile()->GetPrefs());
  base::DictionaryValue* windowPreferences = update->Get();
  windowPreferences->SetInteger("left", bounds.x());
  windowPreferences->SetInteger("top", bounds.y());
  windowPreferences->SetInteger("right", bounds.right());
  windowPreferences->SetInteger("bottom", bounds.bottom());
  windowPreferences->SetBoolean("maximized", false);
  windowPreferences->SetBoolean("always_on_top", false);
  windowPreferences->SetInteger("work_area_left", workArea.x());
  windowPreferences->SetInteger("work_area_top", workArea.y());
  windowPreferences->SetInteger("work_area_right", workArea.right());
  windowPreferences->SetInteger("work_area_bottom", workArea.bottom());
}

- (NSRect)window:(NSWindow*)window
willPositionSheet:(NSWindow*)sheet
       usingRect:(NSRect)defaultSheetLocation {
  // Position the sheet as follows:
  //  - If the bookmark bar is shown (attached to the normal toolbar), position
  //    the sheet below the bookmark bar.
  //  - If the bookmark bar is hidden or shown as a bubble (on the NTP when the
  //    bookmark bar is disabled), position the sheet immediately below the
  //    normal toolbar.
  //  - If the bookmark bar is currently animating, position the sheet according
  //    to where the bar will be when the animation ends.
  CGFloat defaultSheetY = defaultSheetLocation.origin.y;
  if ([self supportsBookmarkBar] &&
      [bookmarkBarController_ currentState] == BookmarkBar::SHOW) {
    defaultSheetY = NSMinY([[bookmarkBarController_ view] frame]);
  } else if ([self hasToolbar]) {
    defaultSheetY = NSMinY([[toolbarController_ view] frame]);
  } else {
    // The toolbar is not shown in popup and application modes. The sheet
    // should be located at the top of the window, under the title of the
    // window.
    defaultSheetY = NSMaxY([[window contentView] frame]);
  }

  // AppKit may shift the window up to fit the sheet on screen, but it will
  // never adjust the height of the sheet, or the origin of the sheet relative
  // to the window. Adjust the origin to prevent sheets from extending past the
  // bottom of the screen.

  // Don't allow the sheet to extend past the bottom of the window. This logic
  // intentionally ignores the size of the screens, since the window might span
  // multiple screens, and AppKit may reposition the window.
  CGFloat sheetHeight = NSHeight([sheet frame]);
  defaultSheetY = std::max(defaultSheetY, sheetHeight);

  // It doesn't make sense to provide a Y higher than the height of the window.
  CGFloat windowHeight = NSHeight([window frame]);
  defaultSheetY = std::min(defaultSheetY, windowHeight);

  defaultSheetLocation.origin.y = defaultSheetY;
  return defaultSheetLocation;
}

- (void)layoutSubviews {
  // TODO(spqchan): Change blockLayoutSubviews so that it only blocks the web
  // content from resizing.
  if (blockLayoutSubviews_)
    return;

  // Suppress title drawing if necessary.
  if ([self.window respondsToSelector:@selector(setShouldHideTitle:)])
    [(id)self.window setShouldHideTitle:![self hasTitleBar]];

  [bookmarkBarController_ updateHiddenState];
  [self updateSubviewZOrder];

  base::scoped_nsobject<BrowserWindowLayout> layout(
      [[BrowserWindowLayout alloc] init]);
  [self updateLayoutParameters:layout];
  [self applyLayout:layout];

  [toolbarController_ setDividerOpacity:[self toolbarDividerOpacity]];

  // Will update the location of the permission bubble when showing/hiding the
  // top level toolbar in fullscreen.
  PermissionBubbleManager* manager = [self permissionBubbleManager];
  if (manager)
    manager->UpdateAnchorPosition();

  browser_->GetBubbleManager()->UpdateAllBubbleAnchors();
}

- (void)applyTabStripLayout:(const chrome::TabStripLayout&)layout {
  // Update the presence of the window controls.
  if (layout.addCustomWindowControls)
    [tabStripController_ addCustomWindowControls];
  else
    [tabStripController_ removeCustomWindowControls];

  // Update the layout of the avatar.
  if (!NSIsEmptyRect(layout.avatarFrame)) {
    NSView* avatarButton = [avatarButtonController_ view];
    [avatarButton setFrame:layout.avatarFrame];
    [avatarButton setHidden:NO];
  }

  // Check if the tab strip's frame has changed.
  BOOL requiresRelayout =
      !NSEqualRects([[self tabStripView] frame], layout.frame);

  // Check if the left indent has changed.
  if (layout.leftIndent != [tabStripController_ leftIndentForControls]) {
    [tabStripController_ setLeftIndentForControls:layout.leftIndent];
    requiresRelayout = YES;
  }

  // Check if the right indent has changed.
  if (layout.rightIndent != [tabStripController_ rightIndentForControls]) {
    [tabStripController_ setRightIndentForControls:layout.rightIndent];
    requiresRelayout = YES;
  }

  // It is undesirable to force tabs relayout when the tap strip's frame did
  // not change, because it will interrupt tab animations in progress.
  // In addition, there appears to be an AppKit bug on <10.9 where interrupting
  // a tab animation resulted in the tab frame being the animator's target
  // frame instead of the interrupting setFrame. (See http://crbug.com/415093)
  if (requiresRelayout) {
    [[self tabStripView] setFrame:layout.frame];
    [tabStripController_ layoutTabsWithoutAnimation];
  }
}

- (BOOL)placeBookmarkBarBelowInfoBar {
  // If we are currently displaying the NTP detached bookmark bar or animating
  // to/from it (from/to anything else), we display the bookmark bar below the
  // info bar.
  return [bookmarkBarController_ isInState:BookmarkBar::DETACHED] ||
         [bookmarkBarController_ isAnimatingToState:BookmarkBar::DETACHED] ||
         [bookmarkBarController_ isAnimatingFromState:BookmarkBar::DETACHED];
}

- (void)layoutTabContentArea:(NSRect)newFrame {
  NSView* tabContentView = [self tabContentArea];
  NSRect tabContentFrame = [tabContentView frame];

  tabContentFrame = newFrame;
  [tabContentView setFrame:tabContentFrame];
}

- (void)adjustToolbarAndBookmarkBarForCompression:(CGFloat)compression {
  CGFloat newHeight =
      [toolbarController_ desiredHeightForCompression:compression];
  NSRect toolbarFrame = [[toolbarController_ view] frame];
  CGFloat deltaH = newHeight - toolbarFrame.size.height;

  if (deltaH == 0)
    return;

  toolbarFrame.size.height = newHeight;
  NSRect bookmarkFrame = [[bookmarkBarController_ view] frame];
  bookmarkFrame.size.height = bookmarkFrame.size.height - deltaH;
  [[toolbarController_ view] setFrame:toolbarFrame];
  [[bookmarkBarController_ view] setFrame:bookmarkFrame];
  [self layoutSubviews];
}

// Fullscreen and presentation mode methods

- (void)moveViewsForImmersiveFullscreen:(BOOL)fullscreen
                          regularWindow:(NSWindow*)regularWindow
                       fullscreenWindow:(NSWindow*)fullscreenWindow {
  NSWindow* sourceWindow = fullscreen ? regularWindow : fullscreenWindow;
  NSWindow* destWindow = fullscreen ? fullscreenWindow : regularWindow;

  // Close the bookmark bubble, if it's open.  Use |-ok:| instead of |-cancel:|
  // or |-close| because that matches the behavior when the bubble loses key
  // status.
  [bookmarkBubbleController_ ok:self];

  // Save the current first responder so we can restore after views are moved.
  base::scoped_nsobject<FocusTracker> focusTracker(
      [[FocusTracker alloc] initWithWindow:sourceWindow]);

  // While we move views (and focus) around, disable any bar visibility changes.
  [self disableBarVisibilityUpdates];

  // Retain the tab strip view while we remove it from its superview.
  base::scoped_nsobject<NSView> tabStripView;
  if ([self hasTabStrip]) {
    tabStripView.reset([[self tabStripView] retain]);
    [tabStripView removeFromSuperview];
  }

  // Disable autoresizing of subviews while we move views around. This prevents
  // spurious renderer resizes.
  [self.chromeContentView setAutoresizesSubviews:NO];
  [self.chromeContentView removeFromSuperview];

  // Have to do this here, otherwise later calls can crash because the window
  // has no delegate.
  [sourceWindow setDelegate:nil];
  [destWindow setDelegate:self];

  // With this call, valgrind complains that a "Conditional jump or move depends
  // on uninitialised value(s)".  The error happens in -[NSThemeFrame
  // drawOverlayRect:].  I'm pretty convinced this is an Apple bug, but there is
  // no visual impact.  I have been unable to tickle it away with other window
  // or view manipulation Cocoa calls.  Stack added to suppressions_mac.txt.
  [self.chromeContentView setAutoresizesSubviews:YES];
  [[destWindow contentView] addSubview:self.chromeContentView
                            positioned:NSWindowBelow
                            relativeTo:nil];
  [self.chromeContentView setFrame:[[destWindow contentView] bounds]];

  // Move the incognito badge if present.
  if ([self shouldShowAvatar]) {
    NSView* avatarButtonView = [avatarButtonController_ view];

    [avatarButtonView removeFromSuperview];
    [avatarButtonView setHidden:YES];  // Will be shown in layout.
    [[destWindow contentView] addSubview:avatarButtonView];
  }

  // Add the tab strip after setting the content view and moving the incognito
  // badge (if any), so that the tab strip will be on top (in the z-order).
  if ([self hasTabStrip])
    [[destWindow contentView] addSubview:tabStripView];

  [sourceWindow setWindowController:nil];
  [self setWindow:destWindow];
  [destWindow setWindowController:self];

  // Move the status bubble over, if we have one.
  if (statusBubble_)
    statusBubble_->SwitchParentWindow(destWindow);

  // Updates the bubble position.
  PermissionBubbleManager* manager = [self permissionBubbleManager];
  if (manager)
    manager->UpdateAnchorPosition();

  // Move the title over.
  [destWindow setTitle:[sourceWindow title]];

  // The window needs to be onscreen before we can set its first responder.
  // Ordering the window to the front can change the active Space (either to
  // the window's old Space or to the application's assigned Space). To prevent
  // this by temporarily change the collectionBehavior.
  NSWindowCollectionBehavior behavior = [sourceWindow collectionBehavior];
  [destWindow setCollectionBehavior:
      NSWindowCollectionBehaviorMoveToActiveSpace];
  [destWindow makeKeyAndOrderFront:self];
  [destWindow setCollectionBehavior:behavior];

  if (![focusTracker restoreFocusInWindow:destWindow]) {
    // During certain types of fullscreen transitions, the view that had focus
    // may have gone away (e.g., the one for a Flash FS widget).  In this case,
    // FocusTracker will fail to restore focus to anything, so we set the focus
    // to the tab contents as a reasonable fall-back.
    [self focusTabContents];
  }
  [sourceWindow orderOut:self];

  // We're done moving focus, so re-enable bar visibility changes.
  [self enableBarVisibilityUpdates];
}

- (void)permissionBubbleWindowWillClose:(NSNotification*)notification {
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center removeObserver:self
                    name:NSWindowWillCloseNotification
                  object:[notification object]];
  [self releaseBarVisibilityForOwner:[notification object]
                       withAnimation:YES
                               delay:YES];
}

- (void)configurePresentationModeController {
  BOOL fullscreenForTab = [self isFullscreenForTabContentOrExtension];
  BOOL kioskMode =
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode);
  BOOL showDropdown =
      !fullscreenForTab && !kioskMode && ([self floatingBarHasFocus]);

  PermissionBubbleManager* manager = [self permissionBubbleManager];
  if (manager && manager->IsBubbleVisible()) {
    NSWindow* bubbleWindow = manager->GetBubbleWindow();
    DCHECK(bubbleWindow);
    // A visible permission bubble will force the dropdown to remain
    // visible.
    [self lockBarVisibilityForOwner:bubbleWindow withAnimation:NO delay:NO];
    showDropdown = YES;
    // Register to be notified when the permission bubble is closed, to
    // allow fullscreen to hide the dropdown.
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(permissionBubbleWindowWillClose:)
                   name:NSWindowWillCloseNotification
                 object:bubbleWindow];
  }

  NSView* contentView = [[self window] contentView];
  [presentationModeController_
      enterPresentationModeForContentView:contentView
                             showDropdown:showDropdown];
}

- (void)adjustUIForExitingFullscreenAndStopOmniboxSliding {
  [presentationModeController_ exitPresentationMode];
  presentationModeController_.reset();

  // Force the bookmark bar z-order to update.
  [[bookmarkBarController_ view] removeFromSuperview];
  [self layoutSubviews];
}

- (void)adjustUIForSlidingFullscreenStyle:(fullscreen_mac::SlidingStyle)style {
  if (!presentationModeController_) {
    presentationModeController_.reset(
        [self newPresentationModeControllerWithStyle:style]);
    [self configurePresentationModeController];
  } else {
    presentationModeController_.get().slidingStyle = style;
  }

  if (!floatingBarBackingView_.get() &&
      ([self hasTabStrip] || [self hasToolbar] || [self hasLocationBar])) {
    floatingBarBackingView_.reset(
        [[FloatingBarBackingView alloc] initWithFrame:NSZeroRect]);
    [floatingBarBackingView_
        setAutoresizingMask:(NSViewWidthSizable | NSViewMinYMargin)];
  }

  // Force the bookmark bar z-order to update.
  [[bookmarkBarController_ view] removeFromSuperview];
  [self layoutSubviews];
}

- (PresentationModeController*)newPresentationModeControllerWithStyle:
    (fullscreen_mac::SlidingStyle)style {
  return [[PresentationModeController alloc] initWithBrowserController:self
                                                                 style:style];
}

- (void)enterImmersiveFullscreen {
  RecordFullscreenWindowLocation([self window]);
  RecordFullscreenStyle(IMMERSIVE_FULLSCREEN);

  // Set to NO by |-windowDidEnterFullScreen:|.
  enteringImmersiveFullscreen_ = YES;

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

  // Create the fullscreen window.
  fullscreenWindow_.reset([[self createFullscreenWindow] retain]);
  savedRegularWindow_ = [[self window] retain];
  savedRegularWindowFrame_ = [savedRegularWindow_ frame];

  [self moveViewsForImmersiveFullscreen:YES
                          regularWindow:[self window]
                       fullscreenWindow:fullscreenWindow_.get()];

  fullscreen_mac::SlidingStyle style = fullscreen_mac::OMNIBOX_TABS_HIDDEN;
  [self adjustUIForSlidingFullscreenStyle:style];

  [fullscreenWindow_ display];

  // AppKit is helpful and prevents NSWindows from having the same height as
  // the screen while the menu bar is showing. This only applies to windows on
  // a secondary screen, in a separate space. Calling [NSWindow
  // setFrame:display:] with the screen's height will always reduce the
  // height by the height of the MenuBar. Calling the method with any other
  // height works fine. The relevant method in the 10.10 AppKit SDK is called:
  // _canAdjustSizeForScreensHaveSeparateSpacesIfFillingSecondaryScreen
  //
  // TODO(erikchen): Refactor the logic to allow the window to be shown after
  // the menubar has been hidden. This would remove the need for this hack.
  // http://crbug.com/403203
  NSRect frame = [[[self window] screen] frame];
  if (!NSEqualRects(frame, [fullscreenWindow_ frame]))
    [fullscreenWindow_ setFrame:[[[self window] screen] frame] display:YES];

  [self layoutSubviews];

  [self windowDidEnterFullScreen:nil];

  // Fade back in.
  if (didFadeOut) {
    CGDisplayFade(token, kFadeDurationSeconds / 2, kCGDisplayBlendSolidColor,
        kCGDisplayBlendNormal, 0.0, 0.0, 0.0, /*synchronous=*/false);
    CGReleaseDisplayFadeReservation(token);
  }
}

- (void)exitImmersiveFullscreen {
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

  [self windowWillExitFullScreen:nil];

  [self moveViewsForImmersiveFullscreen:NO
                          regularWindow:savedRegularWindow_
                       fullscreenWindow:fullscreenWindow_.get()];

  // When exiting fullscreen mode, we need to call layoutSubviews manually.
  [savedRegularWindow_ autorelease];
  savedRegularWindow_ = nil;

  // No close event is thrown when a window is dealloc'd after orderOut.
  // Explicitly close the window to notify bubbles.
  [fullscreenWindow_.get() close];
  fullscreenWindow_.reset();
  [self layoutSubviews];

  [self windowDidExitFullScreen:nil];

  // Fade back in.
  if (didFadeOut) {
    CGDisplayFade(token, kFadeDurationSeconds / 2, kCGDisplayBlendSolidColor,
        kCGDisplayBlendNormal, 0.0, 0.0, 0.0, /*synchronous=*/false);
    CGReleaseDisplayFadeReservation(token);
  }
}

- (void)showFullscreenExitBubbleIfNecessary {
  // This method is called in response to
  // |-updateFullscreenExitBubbleURL:bubbleType:|. If we're in the middle of the
  // transition into fullscreen (i.e., using the AppKit Fullscreen API), do not
  // show the bubble because it will cause visual jank
  // (http://crbug.com/130649). This will be called again as part of
  // |-windowDidEnterFullScreen:|, so arrange to do that work then instead.
  if (enteringAppKitFullscreen_)
    return;

  [self hideOverlayIfPossibleWithAnimation:NO delay:NO];

  switch (exclusiveAccessController_->bubble_type()) {
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE:
    case EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION:
      // Show no exit instruction bubble on Mac when in Browser Fullscreen.
      exclusiveAccessController_->Destroy();
      break;

    default:
      exclusiveAccessController_->Show();
  }
}

- (void)contentViewDidResize:(NSNotification*)notification {
  [self layoutSubviews];
}

- (void)registerForContentViewResizeNotifications {
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(contentViewDidResize:)
             name:NSViewFrameDidChangeNotification
           object:[[self window] contentView]];
}

- (void)deregisterForContentViewResizeNotifications {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSViewFrameDidChangeNotification
              object:[[self window] contentView]];
}

- (NSSize)window:(NSWindow*)window
    willUseFullScreenContentSize:(NSSize)proposedSize {
  return proposedSize;
}

- (NSApplicationPresentationOptions)window:(NSWindow*)window
    willUseFullScreenPresentationOptions:(NSApplicationPresentationOptions)opt {
  return (opt |
          NSApplicationPresentationAutoHideDock |
          NSApplicationPresentationAutoHideMenuBar);
}

- (void)windowWillEnterFullScreen:(NSNotification*)notification {
  RecordFullscreenWindowLocation([self window]);
  RecordFullscreenStyle(enteringPresentationMode_ ? PRESENTATION_MODE
                                                  : CANONICAL_FULLSCREEN);

  if (notification)  // For System Fullscreen when non-nil.
    [self registerForContentViewResizeNotifications];

  [[tabStripController_ activeTabContentsController]
      setBlockFullscreenResize:YES];

  NSWindow* window = [self window];
  savedRegularWindowFrame_ = [window frame];

  enteringAppKitFullscreen_ = YES;
  enteringAppKitFullscreenOnPrimaryScreen_ =
      [[[self window] screen] isEqual:[[NSScreen screens] firstObject]];

  [self setSheetHiddenForFullscreenTransition:YES];
  [self adjustUIForEnteringFullscreen];
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification {
  // In Yosemite, some combination of the titlebar and toolbar always show in
  // full-screen mode. We do not want either to show. Search for the window that
  // contains the views, and hide it. There is no need to ever unhide the view.
  // http://crbug.com/380235
  if (base::mac::IsOSYosemiteOrLater()) {
    for (NSWindow* window in [[NSApplication sharedApplication] windows]) {
      if ([window
              isKindOfClass:NSClassFromString(@"NSToolbarFullScreenWindow")]) {
        // Hide the toolbar if it is for a FramedBrowserWindow.
        if ([window respondsToSelector:@selector(_windowForToolbar)]) {
          if ([[window _windowForToolbar]
                  isKindOfClass:[FramedBrowserWindow class]])
            [[window contentView] setHidden:YES];
        }
      }
    }
  }

  if ([self shouldUseMavericksAppKitFullscreenHack]) {
    // Apply a hack to fix the size of the window. This is the last run of the
    // MessageLoop where the hack will not work, so dispatch the hack to the
    // top of the MessageLoop.
    base::Callback<void(void)> callback = base::BindBlock(^{
        if (![self isInAppKitFullscreen])
          return;

        // The window's frame should be exactly 22 points too short.
        CGFloat kExpectedHeightDifference = 22;
        NSRect currentFrame = [[self window] frame];
        NSRect expectedFrame = [[[self window] screen] frame];
        if (!NSEqualPoints(currentFrame.origin, expectedFrame.origin))
          return;
        if (currentFrame.size.width != expectedFrame.size.width)
          return;
        CGFloat heightDelta =
            expectedFrame.size.height - currentFrame.size.height;
        if (fabs(heightDelta - kExpectedHeightDifference) > 0.01)
          return;

        [[self window] setFrame:expectedFrame display:YES];
    });
    base::MessageLoop::current()->PostTask(FROM_HERE, callback);
  }

  [self setSheetHiddenForFullscreenTransition:NO];

  if (notification)  // For System Fullscreen when non-nil.
    [self deregisterForContentViewResizeNotifications];

  enteringImmersiveFullscreen_ = NO;
  enteringPresentationMode_ = NO;

  [self resetCustomAppKitFullscreenVariables];
  [[tabStripController_ activeTabContentsController]
      updateFullscreenWidgetFrame];

  [self showFullscreenExitBubbleIfNecessary];
  browser_->WindowFullscreenStateChanged();
}

- (void)windowWillExitFullScreen:(NSNotification*)notification {
  if (notification)  // For System Fullscreen when non-nil.
    [self registerForContentViewResizeNotifications];
  exitingAppKitFullscreen_ = YES;

  // Like windowWillEnterFullScreen, if we use custom animations,
  // adjustUIForExitingFullscreen should be called after the layout resizes in
  // startCustomAnimationToExitFullScreenWithDuration.
  if (isUsingCustomAnimation_) {
    blockLayoutSubviews_ = YES;
    [self.chromeContentView setAutoresizesSubviews:NO];
    [self setSheetHiddenForFullscreenTransition:YES];
  } else {
    [self adjustUIForExitingFullscreen];
  }
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
  DCHECK(exitingAppKitFullscreen_);

  // If the custom transition isn't complete, then just set the flag and
  // return. Once the transition is completed, windowDidExitFullscreen will
  // be called again.
  if (isUsingCustomAnimation_ &&
      ![fullscreenTransition_ isTransitionCompleted]) {
    appKitDidExitFullscreen_ = YES;
    return;
  }

  if (notification)  // For System Fullscreen when non-nil.
    [self deregisterForContentViewResizeNotifications];

  browser_->WindowFullscreenStateChanged();
  [self.chromeContentView setAutoresizesSubviews:YES];

  [self resetCustomAppKitFullscreenVariables];

  // Ensures that the permission bubble shows up properly at the front.
  PermissionBubbleManager* manager = [self permissionBubbleManager];
  if (manager && manager->IsBubbleVisible()) {
    NSWindow* bubbleWindow = manager->GetBubbleWindow();
    DCHECK(bubbleWindow);
    [bubbleWindow orderFront:nil];
  }

  // Ensure that the window is layout properly.
  [self layoutSubviews];
}

- (void)windowDidFailToEnterFullScreen:(NSWindow*)window {
  [self deregisterForContentViewResizeNotifications];
  [self resetCustomAppKitFullscreenVariables];
  [self adjustUIForExitingFullscreenAndStopOmniboxSliding];
}

- (void)windowDidFailToExitFullScreen:(NSWindow*)window {
  [self deregisterForContentViewResizeNotifications];
  [self resetCustomAppKitFullscreenVariables];

  // Force a relayout to try and get the window back into a reasonable state.
  [self layoutSubviews];
}

- (void)setSheetHiddenForFullscreenTransition:(BOOL)shoudHide {
  if (!isUsingCustomAnimation_)
    return;

  ConstrainedWindowSheetController* sheetController =
      [ConstrainedWindowSheetController
          controllerForParentWindow:[self window]];
  if (shoudHide)
    [sheetController hideSheetForFullscreenTransition];
  else
    [sheetController unhideSheetForFullscreenTransition];
}

- (void)adjustUIForExitingFullscreen {
  exclusiveAccessController_->Destroy();
  [self adjustUIForExitingFullscreenAndStopOmniboxSliding];
}

- (void)adjustUIForEnteringFullscreen {
  fullscreen_mac::SlidingStyle style;
  if ([self isFullscreenForTabContentOrExtension]) {
    style = fullscreen_mac::OMNIBOX_TABS_NONE;
  } else if (enteringPresentationMode_ || ![self shouldShowFullscreenToolbar]) {
    style = fullscreen_mac::OMNIBOX_TABS_HIDDEN;
  } else {
    style = fullscreen_mac::OMNIBOX_TABS_PRESENT;
  }

  [self adjustUIForSlidingFullscreenStyle:style];
}

- (void)enableBarVisibilityUpdates {
  // Early escape if there's nothing to do.
  if (barVisibilityUpdatesEnabled_)
    return;

  barVisibilityUpdatesEnabled_ = YES;

  if ([barVisibilityLocks_ count])
    [presentationModeController_ ensureOverlayShownWithAnimation:NO delay:NO];
  else
    [presentationModeController_ ensureOverlayHiddenWithAnimation:NO delay:NO];
}

- (void)disableBarVisibilityUpdates {
  // Early escape if there's nothing to do.
  if (!barVisibilityUpdatesEnabled_)
    return;

  barVisibilityUpdatesEnabled_ = NO;
  [presentationModeController_ cancelAnimationAndTimers];
}

- (void)hideOverlayIfPossibleWithAnimation:(BOOL)animation delay:(BOOL)delay {
  if (!barVisibilityUpdatesEnabled_ || [barVisibilityLocks_ count])
    return;
  [presentationModeController_ ensureOverlayHiddenWithAnimation:animation
                                                          delay:delay];
}

- (CGFloat)toolbarDividerOpacity {
  return [bookmarkBarController_ toolbarDividerOpacity];
}

- (void)updateInfoBarTipVisibility {
  // If there's no toolbar then hide the infobar tip.
  [infoBarContainerController_
      setShouldSuppressTopInfoBarTip:![self hasToolbar]];
}

- (NSInteger)pageInfoBubblePointY {
  LocationBarViewMac* locationBarView = [self locationBarBridge];

  // The point, in window coordinates.
  NSPoint iconBottom = locationBarView->GetPageInfoBubblePoint();

  // The toolbar, in window coordinates.
  NSView* toolbar = [toolbarController_ view];
  CGFloat toolbarY = NSMinY([toolbar convertRect:[toolbar bounds] toView:nil]);

  return iconBottom.y - toolbarY;
}

- (void)enterAppKitFullscreen {
  if (FramedBrowserWindow* framedBrowserWindow =
          base::mac::ObjCCast<FramedBrowserWindow>([self window])) {
    [framedBrowserWindow toggleSystemFullScreen];
  }
}

- (void)exitAppKitFullscreen {
  if (FramedBrowserWindow* framedBrowserWindow =
          base::mac::ObjCCast<FramedBrowserWindow>([self window])) {
    [framedBrowserWindow toggleSystemFullScreen];
  }
}

- (NSRect)fullscreenButtonFrame {
  NSButton* fullscreenButton =
      [[self window] standardWindowButton:NSWindowFullScreenButton];
  if (!fullscreenButton)
    return NSZeroRect;

  NSRect buttonFrame = [fullscreenButton frame];

  // When called from -windowWillExitFullScreen:, the button's frame may not
  // be updated yet to match the new window size.
  // We need to return where the button should be positioned.
  NSView* rootView = [[[self window] contentView] superview];
  if ([rootView respondsToSelector:@selector(_fullScreenButtonOrigin)])
    buttonFrame.origin = [rootView _fullScreenButtonOrigin];

  return buttonFrame;
}

- (void)updateLayoutParameters:(BrowserWindowLayout*)layout {
  [layout setContentViewSize:[[[self window] contentView] bounds].size];

  NSSize windowSize = (fullscreenTransition_.get())
                          ? [fullscreenTransition_ desiredWindowLayoutSize]
                          : [[self window] frame].size;

  [layout setWindowSize:windowSize];

  [layout setInAnyFullscreen:[self isInAnyFullscreenMode]];
  [layout setFullscreenSlidingStyle:
      presentationModeController_.get().slidingStyle];
  [layout setFullscreenMenubarOffset:
      [presentationModeController_ menubarOffset]];
  [layout setFullscreenToolbarFraction:
      [presentationModeController_ toolbarFraction]];

  [layout setHasTabStrip:[self hasTabStrip]];
  [layout setFullscreenButtonFrame:[self fullscreenButtonFrame]];

  if ([self shouldShowAvatar]) {
    NSView* avatar = [avatarButtonController_ view];
    [layout setShouldShowAvatar:YES];
    [layout setShouldUseNewAvatar:[self shouldUseNewAvatarButton]];
    [layout setAvatarSize:[avatar frame].size];
    [layout setAvatarLineWidth:[[avatar superview] cr_lineWidth]];
  }

  [layout setHasToolbar:[self hasToolbar]];
  [layout setToolbarHeight:NSHeight([[toolbarController_ view] bounds])];

  [layout setHasLocationBar:[self hasLocationBar]];

  [layout setPlaceBookmarkBarBelowInfoBar:[self placeBookmarkBarBelowInfoBar]];
  [layout setBookmarkBarHidden:[bookmarkBarController_ view].isHidden];
  [layout setBookmarkBarHeight:
      NSHeight([[bookmarkBarController_ view] bounds])];

  [layout setInfoBarHeight:[infoBarContainerController_ heightOfInfoBars]];
  [layout setPageInfoBubblePointY:[self pageInfoBubblePointY]];

  [layout setHasDownloadShelf:(downloadShelfController_.get() != nil)];
  [layout setDownloadShelfHeight:
      NSHeight([[downloadShelfController_ view] bounds])];
}

- (void)applyLayout:(BrowserWindowLayout*)layout {
  chrome::LayoutOutput output = [layout computeLayout];

  if (!NSIsEmptyRect(output.tabStripLayout.frame))
    [self applyTabStripLayout:output.tabStripLayout];

  if (!NSIsEmptyRect(output.toolbarFrame))
    [[toolbarController_ view] setFrame:output.toolbarFrame];

  if (!NSIsEmptyRect(output.bookmarkFrame)) {
    NSView* bookmarkBarView = [bookmarkBarController_ view];
    [bookmarkBarView setFrame:output.bookmarkFrame];

    // Pin the bookmark bar to the top of the window and make the width
    // flexible.
    [bookmarkBarView setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];

    [bookmarkBarController_ layoutSubviews];
  }

  // The info bar is never hidden. Sometimes it has zero effective height.
  [[infoBarContainerController_ view] setFrame:output.infoBarFrame];
  [infoBarContainerController_
      setMaxTopArrowHeight:output.infoBarMaxTopArrowHeight];
  [infoBarContainerController_
      setInfobarArrowX:[self locationBarBridge]->GetPageInfoBubblePoint().x];

  if (!NSIsEmptyRect(output.downloadShelfFrame))
    [[downloadShelfController_ view] setFrame:output.downloadShelfFrame];

  [self layoutTabContentArea:output.contentAreaFrame];

  if (!NSIsEmptyRect(output.fullscreenBackingBarFrame)) {
    [floatingBarBackingView_ setFrame:output.fullscreenBackingBarFrame];
    [presentationModeController_
        overlayFrameChanged:output.fullscreenBackingBarFrame];
  }

  [findBarCocoaController_
      positionFindBarViewAtMaxY:output.findBarMaxY
                       maxWidth:NSWidth(output.contentAreaFrame)];

  exclusiveAccessController_->Layout(output.fullscreenExitButtonMaxY);
}

- (void)updateSubviewZOrder {
  if ([self isInAnyFullscreenMode])
    [self updateSubviewZOrderFullscreen];
  else
    [self updateSubviewZOrderNormal];
}

- (void)updateSubviewZOrderNormal {
  base::scoped_nsobject<NSMutableArray> subviews([[NSMutableArray alloc] init]);
  if ([downloadShelfController_ view])
    [subviews addObject:[downloadShelfController_ view]];
  if ([bookmarkBarController_ view])
    [subviews addObject:[bookmarkBarController_ view]];
  if ([toolbarController_ view])
    [subviews addObject:[toolbarController_ view]];
  if ([infoBarContainerController_ view])
    [subviews addObject:[infoBarContainerController_ view]];
  if ([self tabContentArea])
    [subviews addObject:[self tabContentArea]];
  if ([findBarCocoaController_ view])
    [subviews addObject:[findBarCocoaController_ view]];

  [self setContentViewSubviews:subviews];
}

- (void)updateSubviewZOrderFullscreen {
  base::scoped_nsobject<NSMutableArray> subviews([[NSMutableArray alloc] init]);
  if ([downloadShelfController_ view])
    [subviews addObject:[downloadShelfController_ view]];
  if ([self tabContentArea])
    [subviews addObject:[self tabContentArea]];
  if ([self placeBookmarkBarBelowInfoBar]) {
    if ([bookmarkBarController_ view])
      [subviews addObject:[bookmarkBarController_ view]];
    if (floatingBarBackingView_)
      [subviews addObject:floatingBarBackingView_];
  } else {
    if (floatingBarBackingView_)
      [subviews addObject:floatingBarBackingView_];
    if ([bookmarkBarController_ view])
      [subviews addObject:[bookmarkBarController_ view]];
  }
  if ([toolbarController_ view])
    [subviews addObject:[toolbarController_ view]];
  if ([infoBarContainerController_ view])
    [subviews addObject:[infoBarContainerController_ view]];
  if ([findBarCocoaController_ view])
    [subviews addObject:[findBarCocoaController_ view]];

  [self setContentViewSubviews:subviews];
}

- (void)setContentViewSubviews:(NSArray*)subviews {
  // Subviews already match.
  if ([[self.chromeContentView subviews] isEqual:subviews])
    return;

  // The tabContentArea isn't a subview, so just set all the subviews.
  NSView* tabContentArea = [self tabContentArea];
  if (![[self.chromeContentView subviews] containsObject:tabContentArea]) {
    [self.chromeContentView setSubviews:subviews];
    return;
  }

  // Remove all subviews that aren't the tabContentArea.
  for (NSView* view in [[[self.chromeContentView subviews] copy] autorelease]) {
    if (view != tabContentArea)
      [view removeFromSuperview];
  }

  // Add in the subviews below the tabContentArea.
  NSInteger index = [subviews indexOfObject:tabContentArea];
  for (int i = index - 1; i >= 0; --i) {
    NSView* view = [subviews objectAtIndex:i];
    [self.chromeContentView addSubview:view
                            positioned:NSWindowBelow
                            relativeTo:nil];
  }

  // Add in the subviews above the tabContentArea.
  for (NSUInteger i = index + 1; i < [subviews count]; ++i) {
    NSView* view = [subviews objectAtIndex:i];
    [self.chromeContentView addSubview:view
                            positioned:NSWindowAbove
                            relativeTo:nil];
  }
}

+ (BOOL)systemSettingsRequireMavericksAppKitFullscreenHack {
  if (!base::mac::IsOSMavericks())
    return NO;
  return [NSScreen respondsToSelector:@selector(screensHaveSeparateSpaces)] &&
         [NSScreen screensHaveSeparateSpaces];
}

- (BOOL)shouldUseMavericksAppKitFullscreenHack {
  if (![[self class] systemSettingsRequireMavericksAppKitFullscreenHack])
    return NO;
  if (!enteringAppKitFullscreen_)
    return NO;
  if (enteringAppKitFullscreenOnPrimaryScreen_)
    return NO;

  return YES;
}

- (BOOL)shouldUseCustomAppKitFullscreenTransition:(BOOL)enterFullScreen {
  // Custom fullscreen transitions should only be available in OSX 10.9+.
  if (base::mac::IsOSMountainLionOrEarlier())
    return NO;

  // Disable the custom exit animation in OSX 10.9: http://crbug.com/526327#c3.
  if (base::mac::IsOSMavericks() && !enterFullScreen)
    return NO;

  NSView* root = [[self.window contentView] superview];
  if (!root.layer)
    return NO;

  // AppKit on OSX 10.9 has a bug for applications linked against OSX 10.8 SDK
  // and earlier. Under specific circumstances, it prevents the custom AppKit
  // transition from working well. See http://crbug.com/396980 for more
  // details.
  if ([[self class] systemSettingsRequireMavericksAppKitFullscreenHack] &&
      ![[[self window] screen] isEqual:[[NSScreen screens] firstObject]]) {
    return NO;
  }

  return YES;
}

- (void)resetCustomAppKitFullscreenVariables {
  [self setSheetHiddenForFullscreenTransition:NO];
  [self.chromeContentView setAutoresizesSubviews:YES];

  fullscreenTransition_.reset();
  [[tabStripController_ activeTabContentsController]
      setBlockFullscreenResize:NO];
  blockLayoutSubviews_ = NO;

  enteringAppKitFullscreen_ = NO;
  exitingAppKitFullscreen_ = NO;
  isUsingCustomAnimation_ = NO;
}

- (NSArray*)customWindowsToEnterFullScreenForWindow:(NSWindow*)window {
  DCHECK([window isEqual:self.window]);

  if (![self shouldUseCustomAppKitFullscreenTransition:YES])
    return nil;

  fullscreenTransition_.reset(
      [[BrowserWindowFullscreenTransition alloc] initEnterWithController:self]);

  NSArray* customWindows =
      [fullscreenTransition_ customWindowsForFullScreenTransition];
  isUsingCustomAnimation_ = customWindows != nil;
  return customWindows;
}

- (NSArray*)customWindowsToExitFullScreenForWindow:(NSWindow*)window {
  DCHECK([window isEqual:self.window]);

  if (![self shouldUseCustomAppKitFullscreenTransition:NO])
    return nil;

  fullscreenTransition_.reset(
      [[BrowserWindowFullscreenTransition alloc] initExitWithController:self]);

  NSArray* customWindows =
      [fullscreenTransition_ customWindowsForFullScreenTransition];
  isUsingCustomAnimation_ = customWindows != nil;
  return customWindows;
}

- (void)window:(NSWindow*)window
    startCustomAnimationToEnterFullScreenWithDuration:(NSTimeInterval)duration {
  DCHECK([window isEqual:self.window]);
  [fullscreenTransition_ startCustomFullScreenAnimationWithDuration:duration];
}

- (void)window:(NSWindow*)window
    startCustomAnimationToExitFullScreenWithDuration:(NSTimeInterval)duration {
  DCHECK([window isEqual:self.window]);

  [fullscreenTransition_ startCustomFullScreenAnimationWithDuration:duration];

  base::AutoReset<BOOL> autoReset(&blockLayoutSubviews_, NO);
  [self adjustUIForExitingFullscreen];
}

- (BOOL)shouldConstrainFrameRect {
  if ([fullscreenTransition_ shouldWindowBeUnconstrained])
    return NO;

  return [super shouldConstrainFrameRect];
}

- (WebContents*)webContents {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

- (PermissionBubbleManager*)permissionBubbleManager {
  if (WebContents* contents = [self webContents])
    return PermissionBubbleManager::FromWebContents(contents);
  return nil;
}

- (BOOL)isFullscreenForTabContentOrExtension {
  FullscreenController* controller =
      browser_->exclusive_access_manager()->fullscreen_controller();
  return controller->IsWindowFullscreenForTabOrPending() ||
         controller->IsExtensionFullscreenOrPending();
}

@end  // @implementation BrowserWindowController(Private)
