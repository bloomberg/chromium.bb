// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_controller_private.h"

#include <cmath>

#include "base/command_line.h"
#import "base/memory/scoped_nsobject.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#import "chrome/browser/ui/cocoa/browser/avatar_button_controller.h"
#import "chrome/browser/ui/cocoa/fast_resize_view.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/floating_bar_backing_view.h"
#import "chrome/browser/ui/cocoa/framed_browser_window.h"
#import "chrome/browser/ui/cocoa/fullscreen_window.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"
#import "chrome/browser/ui/cocoa/nsview_additions.h"
#import "chrome/browser/ui/cocoa/presentation_mode_controller.h"
#import "chrome/browser/ui/cocoa/status_bubble_mac.h"
#import "chrome/browser/ui/cocoa/tab_contents/previewable_contents_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_view.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#import "ui/base/cocoa/focus_tracker.h"
#include "ui/base/ui_base_types.h"

using content::RenderWidgetHostView;
using content::WebContents;

namespace {

// Space between the incognito badge and the right edge of the window.
const CGFloat kAvatarRightOffset = 4;

// The amount by which to shrink the tab strip (on the right) when the
// incognito badge is present.
const CGFloat kAvatarTabStripShrink = 18;

// Insets for the location bar, used when the full toolbar is hidden.
// TODO(viettrungluu): We can argue about the "correct" insetting; I like the
// following best, though arguably 0 inset is better/more correct.
const CGFloat kLocBarLeftRightInset = 1;
const CGFloat kLocBarTopInset = 0;
const CGFloat kLocBarBottomInset = 1;

}  // namespace

@implementation BrowserWindowController(Private)

// Create the tab strip controller.
- (void)createTabStripController {
  DCHECK([previewableContentsController_ activeContainer]);
  DCHECK([[previewableContentsController_ activeContainer] window]);
  tabStripController_.reset([[TabStripController alloc]
      initWithView:[self tabStripView]
        switchView:[previewableContentsController_ activeContainer]
           browser:browser_.get()
          delegate:self]);
}

- (void)saveWindowPositionIfNeeded {
  if (!chrome::ShouldSaveWindowPlacement(browser_.get()))
    return;

  // If we're in fullscreen mode, save the position of the regular window
  // instead.
  NSWindow* window = [self isFullscreen] ? savedRegularWindow_ : [self window];

  // Window positions are stored relative to the origin of the primary monitor.
  NSRect monitorFrame = [[[NSScreen screens] objectAtIndex:0] frame];
  NSScreen* windowScreen = [window screen];

  // Start with the window's frame, which is in virtual coordinates.
  // Do some y twiddling to flip the coordinate system.
  gfx::Rect bounds(NSRectToCGRect([window frame]));
  bounds.set_y(monitorFrame.size.height - bounds.y() - bounds.height());

  // Browser::SaveWindowPlacement saves information for session restore.
  ui::WindowShowState show_state = ui::SHOW_STATE_NORMAL;
  if ([window isMiniaturized])
    show_state = ui::SHOW_STATE_MINIMIZED;
  else if ([self isFullscreen])
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
  if (!prefs || browser_ != chrome::GetLastActiveBrowser())
    return;

  // Save the current work area, in flipped coordinates.
  gfx::Rect workArea(NSRectToCGRect([windowScreen visibleFrame]));
  workArea.set_y(monitorFrame.size.height - workArea.y() - workArea.height());

  DictionaryPrefUpdate update(
      prefs,
      chrome::GetWindowPlacementKey(browser_.get()).c_str());
  DictionaryValue* windowPreferences = update.Get();
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
       usingRect:(NSRect)defaultSheetRect {
  // Position the sheet as follows:
  //  - If the bookmark bar is hidden or shown as a bubble (on the NTP when the
  //    bookmark bar is disabled), position the sheet immediately below the
  //    normal toolbar.
  //  - If the bookmark bar is shown (attached to the normal toolbar), position
  //    the sheet below the bookmark bar.
  //  - If the bookmark bar is currently animating, position the sheet according
  //    to where the bar will be when the animation ends.
  switch ([bookmarkBarController_ state]) {
    case BookmarkBar::SHOW: {
      NSRect bookmarkBarFrame = [[bookmarkBarController_ view] frame];
      defaultSheetRect.origin.y = bookmarkBarFrame.origin.y;
      break;
    }
    case BookmarkBar::HIDDEN:
    case BookmarkBar::DETACHED: {
      NSRect toolbarFrame = [[toolbarController_ view] frame];
      defaultSheetRect.origin.y = toolbarFrame.origin.y;
      break;
    }
  }
  return defaultSheetRect;
}

- (void)layoutSubviews {
  // With the exception of the top tab strip, the subviews which we lay out are
  // subviews of the content view, so we mainly work in the content view's
  // coordinate system. Note, however, that the content view's coordinate system
  // and the window's base coordinate system should coincide.
  NSWindow* window = [self window];
  NSView* contentView = [window contentView];
  NSRect contentBounds = [contentView bounds];
  CGFloat minX = NSMinX(contentBounds);
  CGFloat minY = NSMinY(contentBounds);
  CGFloat width = NSWidth(contentBounds);

  // Suppress title drawing if necessary.
  if ([window respondsToSelector:@selector(setShouldHideTitle:)])
    [(id)window setShouldHideTitle:![self hasTitleBar]];

  // Update z-order. The code below depends on this.
  [self updateSubviewZOrder];

  BOOL inPresentationMode = [self inPresentationMode];
  CGFloat floatingBarHeight = [self floatingBarHeight];
  // In presentation mode, |yOffset| accounts for the sliding position of the
  // floating bar and the extra offset needed to dodge the menu bar.
  CGFloat yOffset = inPresentationMode ?
      (std::floor((1 - floatingBarShownFraction_) * floatingBarHeight) -
          [presentationModeController_ floatingBarVerticalOffset]) : 0;
  CGFloat maxY = NSMaxY(contentBounds) + yOffset;

  if ([self hasTabStrip]) {
    // If we need to lay out the top tab strip, replace |maxY| with a higher
    // value, and then lay out the tab strip.
    NSRect windowFrame = [contentView convertRect:[window frame] fromView:nil];
    maxY = NSHeight(windowFrame) + yOffset;
    maxY = [self layoutTabStripAtMaxY:maxY
                                width:width
                           fullscreen:[self isFullscreen]];
  }

  // Sanity-check |maxY|.
  DCHECK_GE(maxY, minY);
  DCHECK_LE(maxY, NSMaxY(contentBounds) + yOffset);

  // Place the toolbar at the top of the reserved area.
  maxY = [self layoutToolbarAtMinX:minX maxY:maxY width:width];
  CGFloat toolbarBottomY = maxY;

  // If we're not displaying the bookmark bar below the infobar, then it goes
  // immediately below the toolbar.
  BOOL placeBookmarkBarBelowInfoBar = [self placeBookmarkBarBelowInfoBar];
  if (!placeBookmarkBarBelowInfoBar)
    maxY = [self layoutTopBookmarkBarAtMinX:minX maxY:maxY width:width];

  // The floating bar backing view doesn't actually add any height.
  NSRect floatingBarBackingRect =
      NSMakeRect(minX, maxY, width, floatingBarHeight);
  [self layoutFloatingBarBackingView:floatingBarBackingRect
                    presentationMode:inPresentationMode];

  // Place the find bar immediately below the toolbar/attached bookmark bar. In
  // presentation mode, it hangs off the top of the screen when the bar is
  // hidden.  The find bar is unaffected by the side tab positioning.
  [findBarCocoaController_ positionFindBarViewAtMaxY:maxY maxWidth:width];
  [fullscreenExitBubbleController_ positionInWindowAtTop:maxY width:width];

  // If in presentation mode, reset |maxY| to top of screen, so that the
  // floating bar slides over the things which appear to be in the content area.
  if (inPresentationMode)
    maxY = NSMaxY(contentBounds);

  // Also place the infobar container immediate below the toolbar, except in
  // presentation mode in which case it's at the top of the visual content area.
  maxY = [self layoutInfoBarAtMinX:minX maxY:maxY width:width];

  // Place the download shelf, if any, at the bottom of the view.
  minY = [self layoutDownloadShelfAtMinX:minX minY:minY width:width];

  // Place the bookmark bar.
  if (placeBookmarkBarBelowInfoBar) {
    if ([bookmarkBarController_ shouldShowAtBottomWhenDetached]) {
      [self layoutBottomBookmarkBarInContentFrame:
          NSMakeRect(minX, minY, width, maxY - minY)];
    } else {
      maxY = [self layoutTopBookmarkBarAtMinX:minX maxY:maxY width:width];
    }
  }

  // In presentation mode the content area takes up all the remaining space
  // (from the bottom of the infobar down). In normal mode the content area
  // takes up the space between the bottom of the toolbar down.
  CGFloat contentAreaTop = 0;
  if (inPresentationMode) {
    toolbarToWebContentsOffset_ = 0;
    contentAreaTop = maxY;
  } else {
    toolbarToWebContentsOffset_ = toolbarBottomY - maxY;
    contentAreaTop = toolbarBottomY;
  }
  [self updateContentOffsets];

  NSRect contentAreaRect = NSMakeRect(minX, minY, width, contentAreaTop - minY);
  [self layoutTabContentArea:contentAreaRect];

  // Normally, we don't need to tell the toolbar whether or not to show the
  // divider, but things break down during animation.
  [toolbarController_ setDividerOpacity:[self toolbarDividerOpacity]];
}

- (CGFloat)floatingBarHeight {
  if (![self inPresentationMode])
    return 0;

  CGFloat totalHeight = [presentationModeController_ floatingBarVerticalOffset];

  if ([self hasTabStrip])
    totalHeight += NSHeight([[self tabStripView] frame]);

  if ([self hasToolbar]) {
    totalHeight += NSHeight([[toolbarController_ view] frame]);
  } else if ([self hasLocationBar]) {
    totalHeight += NSHeight([[toolbarController_ view] frame]) +
                   kLocBarTopInset + kLocBarBottomInset;
  }

  if (![self placeBookmarkBarBelowInfoBar])
    totalHeight += NSHeight([[bookmarkBarController_ view] frame]);

  return totalHeight;
}

- (CGFloat)layoutTabStripAtMaxY:(CGFloat)maxY
                          width:(CGFloat)width
                     fullscreen:(BOOL)fullscreen {
  // Nothing to do if no tab strip.
  if (![self hasTabStrip])
    return maxY;

  NSView* tabStripView = [self tabStripView];
  CGFloat tabStripHeight = NSHeight([tabStripView frame]);
  maxY -= tabStripHeight;
  [tabStripView setFrame:NSMakeRect(0, maxY, width, tabStripHeight)];

  // Set left indentation based on fullscreen mode status.
  [tabStripController_ setLeftIndentForControls:(fullscreen ? 0 :
      [[tabStripController_ class] defaultLeftIndentForControls])];

  // Lay out the icognito/avatar badge because calculating the indentation on
  // the right depends on it.
  if ([self shouldShowAvatar]) {
    NSView* avatarButton = [avatarButtonController_ view];
    CGFloat buttonHeight = std::min(
        static_cast<CGFloat>(profiles::kAvatarIconHeight), tabStripHeight);
    [avatarButton setFrameSize:NSMakeSize(profiles::kAvatarIconWidth,
                                          buttonHeight)];

    // Actually place the badge *above* |maxY|, by +2 to miss the divider.
    CGFloat badgeXOffset = -kAvatarRightOffset;
    CGFloat badgeYOffset = 2 * [[avatarButton superview] cr_lineWidth];
    NSPoint origin =
        NSMakePoint(width - NSWidth([avatarButton frame]) + badgeXOffset,
                    maxY + badgeYOffset);
    [avatarButton setFrameOrigin:origin];
    [avatarButton setHidden:NO];  // Make sure it's shown.
  }

  // Calculate the right indentation.  The default indentation built into the
  // tabstrip leaves enough room for the fullscreen button or presentation mode
  // toggle button on Lion.  On non-Lion systems, the right indent needs to be
  // adjusted to make room for the new tab button when an avatar is present.
  CGFloat rightIndent = 0;
  if (base::mac::IsOSLionOrLater()) {
    FramedBrowserWindow* window =
        static_cast<FramedBrowserWindow*>([self window]);
    DCHECK([window isKindOfClass:[FramedBrowserWindow class]]);
    rightIndent += -[window fullScreenButtonOriginAdjustment].x;
  } else if ([self shouldShowAvatar]) {
    rightIndent += kAvatarTabStripShrink;
  }
  [tabStripController_ setRightIndentForControls:rightIndent];

  // Go ahead and layout the tabs.
  [tabStripController_ layoutTabsWithoutAnimation];

  return maxY;
}

- (CGFloat)layoutToolbarAtMinX:(CGFloat)minX
                          maxY:(CGFloat)maxY
                         width:(CGFloat)width {
  NSView* toolbarView = [toolbarController_ view];
  NSRect toolbarFrame = [toolbarView frame];
  if ([self hasToolbar]) {
    // The toolbar is present in the window, so we make room for it.
    DCHECK(![toolbarView isHidden]);
    toolbarFrame.origin.x = minX;
    toolbarFrame.origin.y = maxY - NSHeight(toolbarFrame);
    toolbarFrame.size.width = width;
    maxY -= NSHeight(toolbarFrame);
  } else {
    if ([self hasLocationBar]) {
      // Location bar is present with no toolbar. Put a border of
      // |kLocBar...Inset| pixels around the location bar.
      // TODO(viettrungluu): This is moderately ridiculous. The toolbar should
      // really be aware of what its height should be (the way the toolbar
      // compression stuff is currently set up messes things up).
      DCHECK(![toolbarView isHidden]);
      toolbarFrame.origin.x = kLocBarLeftRightInset;
      toolbarFrame.origin.y = maxY - NSHeight(toolbarFrame) - kLocBarTopInset;
      toolbarFrame.size.width = width - 2 * kLocBarLeftRightInset;
      maxY -= kLocBarTopInset + NSHeight(toolbarFrame) + kLocBarBottomInset;
    } else {
      DCHECK([toolbarView isHidden]);
    }
  }
  [toolbarView setFrame:toolbarFrame];
  return maxY;
}

- (BOOL)placeBookmarkBarBelowInfoBar {
  // If we are currently displaying the NTP detached bookmark bar or animating
  // to/from it (from/to anything else), we display the bookmark bar below the
  // infobar.
  return [bookmarkBarController_ isInState:BookmarkBar::DETACHED] ||
         [bookmarkBarController_ isAnimatingToState:BookmarkBar::DETACHED] ||
         [bookmarkBarController_ isAnimatingFromState:BookmarkBar::DETACHED];
}

- (CGFloat)layoutTopBookmarkBarAtMinX:(CGFloat)minX
                                 maxY:(CGFloat)maxY
                               width:(CGFloat)width {
  [bookmarkBarController_ updateHiddenState];

  NSView* bookmarkBarView = [bookmarkBarController_ view];
  NSRect frame = [bookmarkBarView frame];
  frame.origin.x = minX;
  frame.origin.y = maxY - NSHeight(frame);
  frame.size.width = width;
  [bookmarkBarView setFrame:frame];
  maxY -= NSHeight(frame);

  // Pin the bookmark bar to the top of the window and make the width flexible.
  [bookmarkBarView setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];

  // TODO(viettrungluu): Does this really belong here? Calling it shouldn't be
  // necessary in the non-NTP case.
  [bookmarkBarController_ layoutSubviews];

  return maxY;
}

- (void)layoutBottomBookmarkBarInContentFrame:(NSRect)contentFrame {
  [bookmarkBarController_ updateHiddenState];

  NSView* bookmarkBarView = [bookmarkBarController_ view];
  NSRect frame;
  frame.size.width = NSWidth(contentFrame) -
      chrome::search::kHorizontalPaddingForBottomBookmarkBar * 2;
  frame.size.width = std::min(frame.size.width,
      static_cast<CGFloat>(chrome::search::kMaxWidthForBottomBookmarkBar));
  frame.size.height = NSHeight([bookmarkBarView frame]);
  frame.origin.x = NSMinX(contentFrame) +
      roundf((NSWidth(contentFrame)- frame.size.width) / 2.0);
  frame.origin.y = NSMinY(contentFrame);
  [bookmarkBarView setFrame:frame];

  // Disable auto-resizing.
  [bookmarkBarView setAutoresizingMask:0];

  // TODO(viettrungluu): Does this really belong here? Calling it shouldn't be
  // necessary in the non-NTP case.
  [bookmarkBarController_ layoutSubviews];
}

- (void)layoutFloatingBarBackingView:(NSRect)frame
                    presentationMode:(BOOL)presentationMode {
  // Only display when in presentation mode.
  if (presentationMode) {
    // For certain window types such as app windows (e.g., the dev tools
    // window), there's no actual overlay. (Displaying one would result in an
    // overly sliding in only under the menu, which gives an ugly effect.)
    if (floatingBarBackingView_.get()) {
      // Set its frame.
      [floatingBarBackingView_ setFrame:frame];
    }

    // But we want the logic to work as usual (for show/hide/etc. purposes).
    [presentationModeController_ overlayFrameChanged:frame];
  } else {
    // Okay to call even if |floatingBarBackingView_| is nil.
    if ([floatingBarBackingView_ superview])
      [floatingBarBackingView_ removeFromSuperview];
  }
}

- (CGFloat)layoutInfoBarAtMinX:(CGFloat)minX
                          maxY:(CGFloat)maxY
                         width:(CGFloat)width {
  NSView* containerView = [infoBarContainerController_ view];
  NSRect containerFrame = [containerView frame];
  maxY -= NSHeight(containerFrame);
  maxY += [infoBarContainerController_ overlappingTipHeight];
  containerFrame.origin.x = minX;
  containerFrame.origin.y = maxY;
  containerFrame.size.width = width;
  [containerView setFrame:containerFrame];
  return maxY;
}

- (CGFloat)layoutDownloadShelfAtMinX:(CGFloat)minX
                                minY:(CGFloat)minY
                               width:(CGFloat)width {
  if (downloadShelfController_.get()) {
    NSView* downloadView = [downloadShelfController_ view];
    NSRect downloadFrame = [downloadView frame];
    downloadFrame.origin.x = minX;
    downloadFrame.origin.y = minY;
    downloadFrame.size.width = width;
    [downloadView setFrame:downloadFrame];
    minY += NSHeight(downloadFrame);
  }
  return minY;
}

- (void)layoutTabContentArea:(NSRect)newFrame {
  NSView* tabContentView = [self tabContentArea];
  NSRect tabContentFrame = [tabContentView frame];

  bool contentShifted =
      NSMaxY(tabContentFrame) != NSMaxY(newFrame) ||
      NSMinX(tabContentFrame) != NSMinX(newFrame);

  tabContentFrame = newFrame;
  [tabContentView setFrame:tabContentFrame];

  // If the relayout shifts the content area up or down, let the renderer know.
  if (contentShifted) {
    if (WebContents* contents = chrome::GetActiveWebContents(browser_.get())) {
      if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
        rwhv->WindowFrameChanged();
    }
  }
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

- (BOOL)shouldShowPresentationModeToggle {
  return base::mac::IsOSLionOrLater() && [self isFullscreen];
}

- (void)moveViewsForFullscreenForSnowLeopard:(BOOL)fullscreen
    regularWindow:(NSWindow*)regularWindow
    fullscreenWindow:(NSWindow*)fullscreenWindow {
  // This method is only for Snow Leopard.
  DCHECK(base::mac::IsOSSnowLeopard());

  NSWindow* sourceWindow = fullscreen ? regularWindow : fullscreenWindow;
  NSWindow* destWindow = fullscreen ? fullscreenWindow : regularWindow;

  // Close the bookmark bubble, if it's open.  Use |-ok:| instead of |-cancel:|
  // or |-close| because that matches the behavior when the bubble loses key
  // status.
  [bookmarkBubbleController_ ok:self];

  // Save the current first responder so we can restore after views are moved.
  scoped_nsobject<FocusTracker> focusTracker(
      [[FocusTracker alloc] initWithWindow:sourceWindow]);

  // While we move views (and focus) around, disable any bar visibility changes.
  [self disableBarVisibilityUpdates];

  // Retain the tab strip view while we remove it from its superview.
  scoped_nsobject<NSView> tabStripView;
  if ([self hasTabStrip]) {
    tabStripView.reset([[self tabStripView] retain]);
    [tabStripView removeFromSuperview];
  }

  // Ditto for the content view.
  scoped_nsobject<NSView> contentView([[sourceWindow contentView] retain]);
  // Disable autoresizing of subviews while we move views around. This prevents
  // spurious renderer resizes.
  [contentView setAutoresizesSubviews:NO];
  [contentView removeFromSuperview];

  // Have to do this here, otherwise later calls can crash because the window
  // has no delegate.
  [sourceWindow setDelegate:nil];
  [destWindow setDelegate:self];

  // With this call, valgrind complains that a "Conditional jump or move depends
  // on uninitialised value(s)".  The error happens in -[NSThemeFrame
  // drawOverlayRect:].  I'm pretty convinced this is an Apple bug, but there is
  // no visual impact.  I have been unable to tickle it away with other window
  // or view manipulation Cocoa calls.  Stack added to suppressions_mac.txt.
  [contentView setAutoresizesSubviews:YES];
  [destWindow setContentView:contentView];

  // Move the incognito badge if present.
  if ([self shouldShowAvatar]) {
    [[avatarButtonController_ view] removeFromSuperview];
    [[avatarButtonController_ view] setHidden:YES];  // Will be shown in layout.
    [[[destWindow contentView] superview] addSubview:
        [avatarButtonController_ view]];
  }

  // Add the tab strip after setting the content view and moving the incognito
  // badge (if any), so that the tab strip will be on top (in the z-order).
  if ([self hasTabStrip])
    [[[destWindow contentView] superview] addSubview:tabStripView];

  [sourceWindow setWindowController:nil];
  [self setWindow:destWindow];
  [destWindow setWindowController:self];

  // Move the status bubble over, if we have one.
  if (statusBubble_)
    statusBubble_->SwitchParentWindow(destWindow);

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

  [focusTracker restoreFocusInWindow:destWindow];
  [sourceWindow orderOut:self];

  // We're done moving focus, so re-enable bar visibility changes.
  [self enableBarVisibilityUpdates];
}

- (void)setPresentationModeInternal:(BOOL)presentationMode
                      forceDropdown:(BOOL)forceDropdown {
  if (presentationMode == [self inPresentationMode])
    return;

  if (presentationMode) {
    BOOL fullscreen_for_tab =
        browser_->fullscreen_controller()->IsFullscreenForTabOrPending();
    BOOL kiosk_mode =
        CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode);
    BOOL showDropdown = !fullscreen_for_tab &&
        !kiosk_mode &&
        (forceDropdown || [self floatingBarHasFocus]);
    NSView* contentView = [[self window] contentView];
    presentationModeController_.reset(
        [[PresentationModeController alloc] initWithBrowserController:self]);
    [presentationModeController_ enterPresentationModeForContentView:contentView
                                 showDropdown:showDropdown];
  } else {
    [presentationModeController_ exitPresentationMode];
    presentationModeController_.reset();
  }

  [self adjustUIForPresentationMode:presentationMode];
  [self layoutSubviews];
}

- (void)enterFullscreenForSnowLeopard {
  DCHECK(base::mac::IsOSSnowLeopard());

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

  // Create the fullscreen window.  After this line, isFullscreen will return
  // YES.
  fullscreenWindow_.reset([[self createFullscreenWindow] retain]);
  savedRegularWindow_ = [[self window] retain];
  savedRegularWindowFrame_ = [savedRegularWindow_ frame];

  [self moveViewsForFullscreenForSnowLeopard:YES
                               regularWindow:[self window]
                            fullscreenWindow:fullscreenWindow_.get()];
  [self adjustUIForPresentationMode:YES];
  [self setPresentationModeInternal:YES forceDropdown:NO];
  [self layoutSubviews];

  [self windowDidEnterFullScreen:nil];

  // Fade back in.
  if (didFadeOut) {
    CGDisplayFade(token, kFadeDurationSeconds / 2, kCGDisplayBlendSolidColor,
        kCGDisplayBlendNormal, 0.0, 0.0, 0.0, /*synchronous=*/false);
    CGReleaseDisplayFadeReservation(token);
  }
}

- (void)exitFullscreenForSnowLeopard {
  DCHECK(base::mac::IsOSSnowLeopard());

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

  [self moveViewsForFullscreenForSnowLeopard:NO
                                        regularWindow:savedRegularWindow_
                                     fullscreenWindow:fullscreenWindow_.get()];

  // When exiting fullscreen mode, we need to call layoutSubviews manually.
  [savedRegularWindow_ autorelease];
  savedRegularWindow_ = nil;
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

// TODO(rohitrao): This function has shrunk into uselessness, and
// |-setFullscreen:| has grown rather large.  Find a good way to break up
// |-setFullscreen:| into smaller pieces.  http://crbug.com/36449
- (void)adjustUIForPresentationMode:(BOOL)fullscreen {
  // Create the floating bar backing view if necessary.
  if (fullscreen && !floatingBarBackingView_.get() &&
      ([self hasTabStrip] || [self hasToolbar] || [self hasLocationBar])) {
    floatingBarBackingView_.reset(
        [[FloatingBarBackingView alloc] initWithFrame:NSZeroRect]);
    [floatingBarBackingView_ setAutoresizingMask:(NSViewWidthSizable |
                                                  NSViewMinYMargin)];
  }

  // Force the bookmark bar z-order to update.
  [[bookmarkBarController_ view] removeFromSuperview];
  [self updateSubviewZOrder];
}

- (void)showFullscreenExitBubbleIfNecessary {
  // This method is called in response to
  // |-updateFullscreenExitBubbleURL:bubbleType:|. If on Lion the system is
  // transitioning, do not show the bubble because it will cause visual jank
  // <http://crbug.com/130649>. This will be called again as part of
  // |-windowDidEnterFullScreen:|, so arrange to do that work then instead.
  if (enteringFullscreen_)
    return;

  [presentationModeController_ ensureOverlayHiddenWithAnimation:NO delay:NO];

  if (fullscreenBubbleType_ == FEB_TYPE_NONE ||
      fullscreenBubbleType_ == FEB_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION) {
    // Show no exit instruction bubble on Mac when in Browser Fullscreen.
    [self destroyFullscreenExitBubbleIfNecessary];
  } else {
    [fullscreenExitBubbleController_ closeImmediately];
    fullscreenExitBubbleController_.reset(
        [[FullscreenExitBubbleController alloc]
            initWithOwner:self
                  browser:browser_.get()
                      url:fullscreenUrl_
               bubbleType:fullscreenBubbleType_]);
    [fullscreenExitBubbleController_ showWindow];
  }
}

- (void)destroyFullscreenExitBubbleIfNecessary {
  [fullscreenExitBubbleController_ closeImmediately];
  fullscreenExitBubbleController_.reset();
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
  [self registerForContentViewResizeNotifications];

  NSWindow* window = [self window];
  savedRegularWindowFrame_ = [window frame];
  BOOL mode = enteringPresentationMode_ ||
       browser_->fullscreen_controller()->IsFullscreenForTabOrPending();
  enteringFullscreen_ = YES;
  [self setPresentationModeInternal:mode forceDropdown:NO];
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification {
  if (base::mac::IsOSLionOrLater())
    [self deregisterForContentViewResizeNotifications];
  enteringFullscreen_ = NO;
  enteringPresentationMode_ = NO;
  [self showFullscreenExitBubbleIfNecessary];
  browser_->WindowFullscreenStateChanged();
}

- (void)windowWillExitFullScreen:(NSNotification*)notification {
  if (base::mac::IsOSLionOrLater())
    [self registerForContentViewResizeNotifications];
  [self destroyFullscreenExitBubbleIfNecessary];
  [self setPresentationModeInternal:NO forceDropdown:NO];
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
  if (base::mac::IsOSLionOrLater())
    [self deregisterForContentViewResizeNotifications];
  browser_->WindowFullscreenStateChanged();
}

- (void)windowDidFailToEnterFullScreen:(NSWindow*)window {
  [self deregisterForContentViewResizeNotifications];
  enteringFullscreen_ = NO;
  [self setPresentationModeInternal:NO forceDropdown:NO];

  // Force a relayout to try and get the window back into a reasonable state.
  [self layoutSubviews];
}

- (void)windowDidFailToExitFullScreen:(NSWindow*)window {
  [self deregisterForContentViewResizeNotifications];

  // Force a relayout to try and get the window back into a reasonable state.
  [self layoutSubviews];
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

- (CGFloat)toolbarDividerOpacity {
  if ([self isShowingInstantResults])
    return 1;
  return [bookmarkBarController_ toolbarDividerOpacity];
}

- (BOOL)isShowingInstantResults {
  if (!browser_->search_model()->mode().is_search_suggestions())
    return NO;

  // If the search suggestions are already being displayed in the preview
  // contents then return YES.
  if ([previewableContentsController_ isShowingPreview])
    return YES;

  // Search suggestions might be shown directly in the web contents in some
  // cases.
  return browser_->search_model()->mode().origin ==
      chrome::search::Mode::ORIGIN_SEARCH;
}

- (void)updateContentOffsets {
  // Normally the preview contents is used to show instant results which must
  // obscure the bookmark bar. This is achieved by setting the offset to 0
  // so that it overlaps the bookmark bar. The only exception is when the
  // preview contents is showing the NTP which must sit below the bookmark bar.
  CGFloat previewOffset = 0;
  if (browser_->search_model()->mode().is_ntp())
    previewOffset = toolbarToWebContentsOffset_;
  [previewableContentsController_ setPreviewOffset:previewOffset];

  // Normally the tab contents sits below the bookmark bar. This is achieved by
  // setting the offset to the height of the bookmark bar. The only exception
  // is on the search results page where the instant results are shown inside
  // the page and not in the preview contents as usual.
  CGFloat tabContentsOffset = toolbarToWebContentsOffset_;
  if (browser_->search_model()->mode().is_search_suggestions() &&
      browser_->search_model()->mode().origin ==
          chrome::search::Mode::ORIGIN_SEARCH) {
    tabContentsOffset = 0;
  }
  [previewableContentsController_ setActiveContainerOffset:tabContentsOffset];

  // Prevent the fast resize view from drawing white over the bookmark bar.
  [[self tabContentArea] setContentOffset:toolbarToWebContentsOffset_];
}

- (void)updateSubviewZOrder {
  NSView* contentView = [[self window] contentView];
  NSView* toolbarView = [toolbarController_ view];

  if ([self inPresentationMode]) {
    // Toolbar is above tab contents so that it can slide down from top of
    // screen.
    [contentView cr_ensureSubview:toolbarView
                     isPositioned:NSWindowAbove
                       relativeTo:[self tabContentArea]];
  } else {
    // Toolbar is below tab contents so that the infobar arrow can appear above
    // it.  Unlike other views the toolbar never overlaps the actual web
    // content.
    [contentView cr_ensureSubview:toolbarView
                     isPositioned:NSWindowBelow
                       relativeTo:[self tabContentArea]];
  }

  // The bookmark bar is always below the toolbar. In normal mode this means
  // that it is below tab contents. This allows instant results to be above
  // the bookmark bar.
  [contentView cr_ensureSubview:[bookmarkBarController_ view]
                   isPositioned:NSWindowBelow
                     relativeTo:toolbarView];

  if ([self inPresentationMode]) {
    // In presentation mode the infobar is below all other views.
    [contentView cr_ensureSubview:[infoBarContainerController_ view]
                     isPositioned:NSWindowBelow
                       relativeTo:[self tabContentArea]];
  } else {
    // Above the toolbar but still below tab contents. Similar to the bookmark
    // bar, this allows instant results to be above the info bar.
    [contentView cr_ensureSubview:[infoBarContainerController_ view]
                     isPositioned:NSWindowAbove
                       relativeTo:toolbarView];
  }

  // The find bar is above everything except instant search results.
  if (findBarCocoaController_) {
    NSView* relativeView = nil;
    if ([self inPresentationMode])
      relativeView =  toolbarView;
    else if ([self isShowingInstantResults])
      relativeView = [infoBarContainerController_ view];
    else
      relativeView = [self tabContentArea];
    [contentView cr_ensureSubview:[findBarCocoaController_ view]
                     isPositioned:NSWindowAbove
                       relativeTo:relativeView];
  }

  if (floatingBarBackingView_) {
    if ([self placeBookmarkBarBelowInfoBar]) {
      [contentView cr_ensureSubview:floatingBarBackingView_
                       isPositioned:NSWindowAbove
                         relativeTo:[bookmarkBarController_ view]];
    } else {
      [contentView cr_ensureSubview:floatingBarBackingView_
                       isPositioned:NSWindowBelow
                         relativeTo:[bookmarkBarController_ view]];
    }
  }
}

@end  // @implementation BrowserWindowController(Private)
