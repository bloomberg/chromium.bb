// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_controller_private.h"

#include <cmath>

#include "base/command_line.h"
#include "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/fullscreen.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window_state.h"
#import "chrome/browser/ui/cocoa/dev_tools_controller.h"
#import "chrome/browser/ui/cocoa/fast_resize_view.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/floating_bar_backing_view.h"
#import "chrome/browser/ui/cocoa/framed_browser_window.h"
#import "chrome/browser/ui/cocoa/fullscreen_mode_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen_window.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#import "chrome/browser/ui/cocoa/nsview_additions.h"
#import "chrome/browser/ui/cocoa/presentation_mode_controller.h"
#import "chrome/browser/ui/cocoa/profiles/avatar_button_controller.h"
#import "chrome/browser/ui/cocoa/profiles/avatar_icon_controller.h"
#import "chrome/browser/ui/cocoa/status_bubble_mac.h"
#import "chrome/browser/ui/cocoa/tab_contents/overlayable_contents_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_view.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/cocoa/version_independent_window.h"
#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_cocoa.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
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

// Width of the full screen icon. Used to position the AvatarButton to the
// left of the icon.
const CGFloat kFullscreenIconWidth = 32;

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
  DCHECK([overlayableContentsController_ activeContainer]);
  DCHECK([[overlayableContentsController_ activeContainer] window]);
  tabStripController_.reset([[TabStripController alloc]
      initWithView:[self tabStripView]
        switchView:[overlayableContentsController_ activeContainer]
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
  base::DictionaryValue* windowPreferences = update.Get();
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
  switch ([bookmarkBarController_ currentState]) {
    case BookmarkBar::SHOW: {
      NSRect bookmarkBarFrame = [[bookmarkBarController_ view] frame];
      defaultSheetRect.origin.y = bookmarkBarFrame.origin.y;
      break;
    }
    case BookmarkBar::HIDDEN:
    case BookmarkBar::DETACHED: {
      if ([self hasToolbar]) {
        NSRect toolbarFrame = [[toolbarController_ view] frame];
        defaultSheetRect.origin.y = toolbarFrame.origin.y;
      } else {
        // The toolbar is not shown in application mode. The sheet should be
        // located at the top of the window, under the title of the window.
        defaultSheetRect.origin.y = NSHeight([[window contentView] frame]) -
                                    defaultSheetRect.size.height;
      }
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

  BOOL useSimplifiedFullscreen = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableSimplifiedFullscreen);

  // Suppress title drawing if necessary.
  if ([window respondsToSelector:@selector(setShouldHideTitle:)])
    [(id)window setShouldHideTitle:![self hasTitleBar]];

  // Update z-order. The code below depends on this.
  [self updateSubviewZOrder:[self inPresentationMode]];

  BOOL inPresentationMode = [self inPresentationMode];
  CGFloat floatingBarHeight = [self floatingBarHeight];
  // In presentation mode, |yOffset| accounts for the sliding position of the
  // floating bar and the extra offset needed to dodge the menu bar.
  CGFloat yOffset = inPresentationMode && !useSimplifiedFullscreen ?
      (std::floor((1 - floatingBarShownFraction_) * floatingBarHeight) -
          [presentationModeController_ floatingBarVerticalOffset]) : 0;
  CGFloat maxY = NSMaxY(contentBounds) + yOffset;

  if ([self hasTabStrip]) {
    // If we need to lay out the top tab strip, replace |maxY| with a higher
    // value, and then lay out the tab strip.
    NSRect windowFrame = [contentView convertRect:[window frame] fromView:nil];
    maxY = NSHeight(windowFrame) + yOffset;
    if (useSimplifiedFullscreen && [self isFullscreen]) {
      CGFloat tabStripHeight = NSHeight([[self tabStripView] frame]);
      CGFloat revealAmount = (1 - floatingBarShownFraction_) * tabStripHeight;
      // In simplified fullscreen, only the toolbar is visible by default, and
      // the tabstrip and menu bar come down (each separately) when the user
      // mouses near the top of the window. Push the maxY of the toolbar up by
      // the amount of the tabstrip that is revealed, while removing the amount
      // of space needed by the menu bar.
      maxY += std::floor(
          revealAmount - [fullscreenModeController_ menuBarHeight]);
    }
    maxY = [self layoutTabStripAtMaxY:maxY
                                width:width
                           fullscreen:[self isFullscreen]];
  }

  // Sanity-check |maxY|.
  DCHECK_GE(maxY, minY);
  DCHECK_LE(maxY, NSMaxY(contentBounds) + yOffset);

  // Place the toolbar at the top of the reserved area.
  maxY = [self layoutToolbarAtMinX:minX maxY:maxY width:width];

  // If we're not displaying the bookmark bar below the info bar, then it goes
  // immediately below the toolbar.
  BOOL placeBookmarkBarBelowInfoBar = [self placeBookmarkBarBelowInfoBar];
  if (!placeBookmarkBarBelowInfoBar)
    maxY = [self layoutBookmarkBarAtMinX:minX maxY:maxY width:width];

  // The floating bar backing view doesn't actually add any height.
  NSRect floatingBarBackingRect =
      NSMakeRect(minX, maxY, width, floatingBarHeight);
  [self layoutFloatingBarBackingView:floatingBarBackingRect
                    presentationMode:inPresentationMode];

  // Place the find bar immediately below the toolbar/attached bookmark bar. In
  // presentation mode, it hangs off the top of the screen when the bar is
  // hidden.
  [findBarCocoaController_ positionFindBarViewAtMaxY:maxY maxWidth:width];
  [fullscreenExitBubbleController_ positionInWindowAtTop:maxY width:width];

  // If in presentation mode, reset |maxY| to top of screen, so that the
  // floating bar slides over the things which appear to be in the content area.
  if (inPresentationMode ||
      (useSimplifiedFullscreen && !fullscreenUrl_.is_empty())) {
    maxY = NSMaxY(contentBounds);
  }

  // Also place the info bar container immediate below the toolbar, except in
  // presentation mode in which case it's at the top of the visual content area.
  maxY = [self layoutInfoBarAtMinX:minX maxY:maxY width:width];

  // If the bookmark bar is detached, place it next in the visual content area.
  if (placeBookmarkBarBelowInfoBar)
    maxY = [self layoutBookmarkBarAtMinX:minX maxY:maxY width:width];

  // Place the download shelf, if any, at the bottom of the view.
  minY = [self layoutDownloadShelfAtMinX:minX minY:minY width:width];

  // Finally, the content area takes up all of the remaining space.
  NSRect contentAreaRect = NSMakeRect(minX, minY, width, maxY - minY);
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
  NSView* avatarButton = [avatarButtonController_ view];
  if ([self shouldShowAvatar]) {
    CGFloat badgeXOffset = -kAvatarRightOffset;
    CGFloat badgeYOffset = 0;
    CGFloat buttonHeight = NSHeight([avatarButton frame]);

    if ([self shouldUseNewAvatarButton]) {
      // The fullscreen icon is displayed to the right of the avatar button.
      if (![self isFullscreen])
        badgeXOffset -= kFullscreenIconWidth;
      // Center the button vertically on the tabstrip.
      badgeYOffset = (tabStripHeight - buttonHeight) / 2;
    } else {
      // Actually place the badge *above* |maxY|, by +2 to miss the divider.
      badgeYOffset = 2 * [[avatarButton superview] cr_lineWidth];
    }

    [avatarButton setFrameSize:NSMakeSize(NSWidth([avatarButton frame]),
        std::min(buttonHeight, tabStripHeight))];
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
  if (base::mac::IsOSLionOrLater() &&
      [[self window] isKindOfClass:[FramedBrowserWindow class]]) {
    FramedBrowserWindow* window =
        static_cast<FramedBrowserWindow*>([self window]);
    rightIndent += -[window fullScreenButtonOriginAdjustment].x;

    // The new avatar is wider than the default indentation, so we need to
    // account for its width.
    if ([self shouldUseNewAvatarButton])
      rightIndent += NSWidth([avatarButton frame]) + kAvatarTabStripShrink;
  } else if ([self shouldShowAvatar]) {
    rightIndent += kAvatarTabStripShrink +
        NSWidth([avatarButton frame]) + kAvatarRightOffset;
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
  // info bar.
  return [bookmarkBarController_ isInState:BookmarkBar::DETACHED] ||
         [bookmarkBarController_ isAnimatingToState:BookmarkBar::DETACHED] ||
         [bookmarkBarController_ isAnimatingFromState:BookmarkBar::DETACHED];
}

- (CGFloat)layoutBookmarkBarAtMinX:(CGFloat)minX
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
    if (WebContents* contents =
            browser_->tab_strip_model()->GetActiveWebContents()) {
      if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
        rwhv->WindowFrameChanged();
    }
  }
}

- (void)updateRoundedBottomCorners {
  [[self tabContentArea] setRoundedBottomCorners:![self isFullscreen]];
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

  // Ditto for the content view.
  base::scoped_nsobject<NSView> contentView(
      [[sourceWindow contentView] retain]);
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
    NSView* avatarButtonView = [avatarButtonController_ view];

    [avatarButtonView removeFromSuperview];
    [avatarButtonView setHidden:YES];  // Will be shown in layout.
    [[destWindow cr_windowView] addSubview:avatarButtonView];
  }

  // Add the tab strip after setting the content view and moving the incognito
  // badge (if any), so that the tab strip will be on top (in the z-order).
  if ([self hasTabStrip])
    [[destWindow cr_windowView] addSubview:tabStripView];

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
  DCHECK(permissionBubbleCocoa_);

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center removeObserver:self
                    name:NSWindowWillCloseNotification
                  object:[notification object]];
  [self releaseBarVisibilityForOwner:[notification object]
                       withAnimation:YES
                               delay:YES];
}

- (void)setPresentationModeInternal:(BOOL)presentationMode
                      forceDropdown:(BOOL)forceDropdown {
  if (presentationMode == [self inPresentationMode])
    return;

  if (presentationMode) {
    BOOL fullscreen_for_tab =
        browser_->fullscreen_controller()->IsWindowFullscreenForTabOrPending();
    BOOL kiosk_mode =
        CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode);
    BOOL showDropdown = !fullscreen_for_tab &&
        !kiosk_mode &&
        (forceDropdown || [self floatingBarHasFocus]);
    presentationModeController_.reset(
        [[PresentationModeController alloc] initWithBrowserController:self]);

    if (permissionBubbleCocoa_ && permissionBubbleCocoa_->IsVisible()) {
      DCHECK(permissionBubbleCocoa_->window());
      // A visible permission bubble will force the dropdown to remain visible.
      [self lockBarVisibilityForOwner:permissionBubbleCocoa_->window()
                        withAnimation:NO
                                delay:NO];
      showDropdown = YES;
      // Register to be notified when the permission bubble is closed, to
      // allow fullscreen to hide the dropdown.
      NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
      [center addObserver:self
                 selector:@selector(permissionBubbleWindowWillClose:)
                     name:NSWindowWillCloseNotification
                   object:permissionBubbleCocoa_->window()];
    }
    if (showDropdown) {
      // Turn on layered mode for the window's root view for the entry
      // animation.  Without this, the OS fullscreen animation for entering
      // fullscreen mode does not correctly draw the tab strip.
      // It will be turned off (set back to NO) when the animation finishes,
      // in -windowDidEnterFullScreen:.
      // Leaving wantsLayer on for the duration of presentation mode causes
      // performance issues when the dropdown is animated in/out.  It also does
      // not seem to be required for the exit animation.
      [[[self window] cr_windowView] setWantsLayer:YES];
    }
    NSView* contentView = [[self window] contentView];
    [presentationModeController_ enterPresentationModeForContentView:contentView
                                 showDropdown:showDropdown];
  } else {
    [presentationModeController_ exitPresentationMode];
    presentationModeController_.reset();
  }

  [self adjustUIForPresentationMode:presentationMode];
  [self layoutSubviews];
}

- (void)enterImmersiveFullscreen {
  // |-isFullscreen:| will return YES from here onwards.
  enteringFullscreen_ = YES;  // Set to NO by |-windowDidEnterFullScreen:|.

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

  // When simplified fullscreen is enabled, do not enter presentation mode.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableSimplifiedFullscreen)) {
    // TODO(rohitrao): Add code to manage the menubar here.
  } else {
    [self adjustUIForPresentationMode:YES];
    [self setPresentationModeInternal:YES forceDropdown:NO];
  }

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

  // When simplified fullscreen is enabled, the menubar status is managed
  // directly by BWC.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableSimplifiedFullscreen)) {
    // TODO(rohitrao): Add code to manage the menubar here.
  }

  [self windowWillExitFullScreen:nil];

  [self moveViewsForImmersiveFullscreen:NO
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
  [self updateSubviewZOrder:fullscreen];
  [self updateAllowOverlappingViews:fullscreen];
}

- (void)showFullscreenExitBubbleIfNecessary {
  // This method is called in response to
  // |-updateFullscreenExitBubbleURL:bubbleType:|. If we're in the middle of the
  // transition into fullscreen (i.e., using the System Fullscreen API), do not
  // show the bubble because it will cause visual jank
  // (http://crbug.com/130649). This will be called again as part of
  // |-windowDidEnterFullScreen:|, so arrange to do that work then instead.
  if (enteringFullscreen_)
    return;

  [self hideOverlayIfPossibleWithAnimation:NO delay:NO];

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
  if (notification)  // For System Fullscreen when non-nil.
    [self registerForContentViewResizeNotifications];

  NSWindow* window = [self window];
  savedRegularWindowFrame_ = [window frame];
  BOOL mode = enteringPresentationMode_ ||
       browser_->fullscreen_controller()->IsWindowFullscreenForTabOrPending();
  enteringFullscreen_ = YES;
  [self setPresentationModeInternal:mode forceDropdown:NO];
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
        [window.contentView setHidden:YES];
      }
    }
  }

  if (notification)  // For System Fullscreen when non-nil.
    [self deregisterForContentViewResizeNotifications];
  enteringFullscreen_ = NO;
  enteringPresentationMode_ = NO;

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableSimplifiedFullscreen) &&
      fullscreenUrl_.is_empty()) {
    fullscreenModeController_.reset([[FullscreenModeController alloc]
        initWithBrowserWindowController:self]);
  }

  [self showFullscreenExitBubbleIfNecessary];
  browser_->WindowFullscreenStateChanged();
  [[[self window] cr_windowView] setWantsLayer:NO];
  [self updateRoundedBottomCorners];
}

- (void)windowWillExitFullScreen:(NSNotification*)notification {
  if (notification)  // For System Fullscreen when non-nil.
    [self registerForContentViewResizeNotifications];
  fullscreenModeController_.reset();
  [self destroyFullscreenExitBubbleIfNecessary];
  [self setPresentationModeInternal:NO forceDropdown:NO];
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
  if (notification)  // For System Fullscreen when non-nil.
    [self deregisterForContentViewResizeNotifications];
  browser_->WindowFullscreenStateChanged();
  [self updateRoundedBottomCorners];
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

- (void)hideOverlayIfPossibleWithAnimation:(BOOL)animation delay:(BOOL)delay {
  if (!barVisibilityUpdatesEnabled_ || [barVisibilityLocks_ count])
    return;
  [presentationModeController_ ensureOverlayHiddenWithAnimation:animation
                                                          delay:delay];
}

- (CGFloat)toolbarDividerOpacity {
  return [bookmarkBarController_ toolbarDividerOpacity];
}

- (void)updateSubviewZOrder:(BOOL)inPresentationMode {
  NSView* contentView = [[self window] contentView];
  NSView* toolbarView = [toolbarController_ view];

  if (inPresentationMode) {
    // Toolbar is above tab contents so that it can slide down from top of
    // screen.
    [contentView cr_ensureSubview:toolbarView
                     isPositioned:NSWindowAbove
                       relativeTo:[self tabContentArea]];
  } else {
    // Toolbar is below tab contents so that the info bar arrow can appear above
    // it.
    [contentView cr_ensureSubview:toolbarView
                     isPositioned:NSWindowBelow
                       relativeTo:[self tabContentArea]];
  }

  // The bookmark bar is always below the toolbar.
  [contentView cr_ensureSubview:[bookmarkBarController_ view]
                   isPositioned:NSWindowBelow
                     relativeTo:toolbarView];

  if (inPresentationMode) {
    // In presentation mode the info bar is below all other views.
    [contentView cr_ensureSubview:[infoBarContainerController_ view]
                     isPositioned:NSWindowBelow
                       relativeTo:[self tabContentArea]];
  } else {
    // Above the toolbar but still below tab contents. Similar to the bookmark
    // bar, this allows Instant results to be above the info bar.
    [contentView cr_ensureSubview:[infoBarContainerController_ view]
                     isPositioned:NSWindowAbove
                       relativeTo:toolbarView];
  }

  // The find bar is above everything.
  if (findBarCocoaController_) {
    NSView* relativeView = nil;
    if (inPresentationMode)
      relativeView = toolbarView;
    else
      relativeView = [self tabContentArea];
    [contentView cr_ensureSubview:[findBarCocoaController_ view]
                     isPositioned:NSWindowAbove
                       relativeTo:relativeView];
  }

  if (floatingBarBackingView_) {
    if ([floatingBarBackingView_ cr_isBelowView:[self tabContentArea]])
      [floatingBarBackingView_ removeFromSuperview];
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

- (BOOL)shouldAllowOverlappingViews:(BOOL)inPresentationMode {
  if (inPresentationMode)
    return YES;

  if (findBarCocoaController_ &&
      ![[findBarCocoaController_ findBarView] isHidden]) {
    return YES;
  }

  if (overlappedViewCount_)
    return YES;

  return NO;
}

- (void)updateAllowOverlappingViews:(BOOL)inPresentationMode {
  WebContents* contents = browser_->tab_strip_model()->GetActiveWebContents();
  if (!contents)
    return;

  BOOL allowOverlappingViews =
      [self shouldAllowOverlappingViews:inPresentationMode];

  // The rendering path with overlapping views disabled causes bugs when
  // transitioning between composited and non-composited mode.
  // http://crbug.com/279472
  allowOverlappingViews = YES;
  contents->SetAllowOverlappingViews(allowOverlappingViews);

  WebContents* devTools = DevToolsWindow::GetInTabWebContents(contents, NULL);
  if (devTools)
    devTools->SetAllowOverlappingViews(allowOverlappingViews);
}

- (void)updateInfoBarTipVisibility {
  // If there's no toolbar then hide the infobar tip.
  [infoBarContainerController_
      setShouldSuppressTopInfoBarTip:![self hasToolbar]];
}

@end  // @implementation BrowserWindowController(Private)
