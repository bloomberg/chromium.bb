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
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/browser/avatar_button_controller.h"
#import "chrome/browser/ui/cocoa/fast_resize_view.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/floating_bar_backing_view.h"
#import "chrome/browser/ui/cocoa/framed_browser_window.h"
#import "chrome/browser/ui/cocoa/fullscreen_window.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_container_controller.h"
#import "chrome/browser/ui/cocoa/presentation_mode_controller.h"
#import "chrome/browser/ui/cocoa/status_bubble_mac.h"
#import "chrome/browser/ui/cocoa/tab_contents/previewable_contents_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_view.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#import "ui/base/cocoa/focus_tracker.h"
#include "ui/base/ui_base_types.h"

using content::RenderWidgetHostView;
using content::WebContents;

// Forward-declare symbols that are part of the 10.6 SDK.
#if !defined(MAC_OS_X_VERSION_10_6) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6

enum {
  NSApplicationPresentationDefault                    = 0,
  NSApplicationPresentationAutoHideDock               = (1 <<  0),
  NSApplicationPresentationHideDock                   = (1 <<  1),
  NSApplicationPresentationAutoHideMenuBar            = (1 <<  2),
  NSApplicationPresentationHideMenuBar                = (1 <<  3),
};
typedef NSUInteger NSApplicationPresentationOptions;

#endif  // MAC_OS_X_VERSION_10_6

namespace {

// Space between the incognito badge and the right edge of the window.
const CGFloat kAvatarRightOffset = 4;

// The amount by which to shrink the tab strip (on the right) when the
// incognito badge is present.
const CGFloat kAvatarTabStripShrink = 18;

// The amount by which to shift the avatar to the right if on Lion.
const CGFloat kAvatarShiftForLion = 20;

// Insets for the location bar, used when the full toolbar is hidden.
// TODO(viettrungluu): We can argue about the "correct" insetting; I like the
// following best, though arguably 0 inset is better/more correct.
const CGFloat kLocBarLeftRightInset = 1;
const CGFloat kLocBarTopInset = 0;
const CGFloat kLocBarBottomInset = 1;

}  // namespace

@implementation BrowserWindowController(Private)

// Create the appropriate tab strip controller based on whether or not side
// tabs are enabled.
- (void)createTabStripController {
  Class factory = [TabStripController class];

  DCHECK([previewableContentsController_ activeContainer]);
  DCHECK([[previewableContentsController_ activeContainer] window]);
  tabStripController_.reset([[factory alloc]
      initWithView:[self tabStripView]
        switchView:[previewableContentsController_ activeContainer]
           browser:browser_.get()
          delegate:self]);
}

- (void)createAndInstallPresentationModeToggleButton {
  DCHECK(base::mac::IsOSLionOrLater());
  if (presentationModeToggleButton_.get())
    return;

  // TODO(rohitrao): Make this button prettier.
  presentationModeToggleButton_.reset(
      [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 25, 25)]);
  NSButton* button = presentationModeToggleButton_.get();
  [button setButtonType:NSMomentaryLightButton];
  [button setBezelStyle:NSRegularSquareBezelStyle];
  [button setBordered:NO];
  [[button cell] setHighlightsBy:NSContentsCellMask];
  [[button cell] setShowsStateBy:NSContentsCellMask];
  [button setImage:[NSImage imageNamed:NSImageNameIChatTheaterTemplate]];
  [button setTarget:self];
  [button setAction:@selector(togglePresentationModeForLionOrLater:)];
  [[[[self window] contentView] superview] addSubview:button];
}

- (void)saveWindowPositionIfNeeded {
  if (!browser_->ShouldSaveWindowPlacement())
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
  browser_->SaveWindowPlacement(bounds, show_state);

  // |windowScreen| can be nil (for example, if the monitor arrangement was
  // changed while in fullscreen mode).  If we see a nil screen, return without
  // saving.
  // TODO(rohitrao): We should just not save anything for fullscreen windows.
  // http://crbug.com/36479.
  if (!windowScreen)
    return;

  // Only save main window information to preferences.
  PrefService* prefs = browser_->profile()->GetPrefs();
  if (!prefs || browser_ != BrowserList::GetLastActive())
    return;

  // Save the current work area, in flipped coordinates.
  gfx::Rect workArea(NSRectToCGRect([windowScreen visibleFrame]));
  workArea.set_y(monitorFrame.size.height - workArea.y() - workArea.height());

  DictionaryPrefUpdate update(prefs, browser_->GetWindowPlacementKey().c_str());
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
  switch ([bookmarkBarController_ visualState]) {
    case bookmarks::kShowingState: {
      NSRect bookmarkBarFrame = [[bookmarkBarController_ view] frame];
      defaultSheetRect.origin.y = bookmarkBarFrame.origin.y;
      break;
    }
    case bookmarks::kHiddenState:
    case bookmarks::kDetachedState: {
      NSRect toolbarFrame = [[toolbarController_ view] frame];
      defaultSheetRect.origin.y = toolbarFrame.origin.y;
      break;
    }
    case bookmarks::kInvalidState:
    default:
      NOTREACHED();
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

  BOOL inPresentationMode = [self inPresentationMode];
  CGFloat floatingBarHeight = [self floatingBarHeight];
  // In presentation mode, |yOffset| accounts for the sliding position of the
  // floating bar and the extra offset needed to dodge the menu bar.
  CGFloat yOffset = inPresentationMode ?
      (std::floor((1 - floatingBarShownFraction_) * floatingBarHeight) -
          [presentationModeController_ floatingBarVerticalOffset]) : 0;
  CGFloat maxY = NSMaxY(contentBounds) + yOffset;

  CGFloat overlayMaxY =
      NSMaxY([window frame]) +
      std::floor((1 - floatingBarShownFraction_) * floatingBarHeight);
  [self layoutPresentationModeToggleAtOverlayMaxX:NSMaxX([window frame])
                                      overlayMaxY:overlayMaxY];

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

  // If we're not displaying the bookmark bar below the infobar, then it goes
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
  [toolbarController_
      setDividerOpacity:[bookmarkBarController_ toolbarDividerOpacity]];
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

- (void)layoutPresentationModeToggleAtOverlayMaxX:(CGFloat)maxX
                                      overlayMaxY:(CGFloat)maxY {
  // Lay out the presentation mode toggle button at the very top of the
  // tab strip.
  if ([self shouldShowPresentationModeToggle]) {
    [self createAndInstallPresentationModeToggleButton];

    NSPoint origin =
        NSMakePoint(maxX - NSWidth([presentationModeToggleButton_ frame]),
                    maxY - NSHeight([presentationModeToggleButton_ frame]));
    [presentationModeToggleButton_ setFrameOrigin:origin];
  } else {
    [presentationModeToggleButton_ removeFromSuperview];
    presentationModeToggleButton_.reset();
  }
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

  // Calculate the right indentation.  The default indentation built into the
  // tabstrip leaves enough room for the fullscreen button or presentation mode
  // toggle button on Lion.  On non-Lion systems, the default indentation also
  // looks fine.  If an avatar button is present, indent enough to account for
  // its width.
  const CGFloat possibleExtraShiftForLion =
      base::mac::IsOSLionOrLater() ? kAvatarShiftForLion : 0;

  CGFloat rightIndent = 0;
  if ([self shouldShowAvatar])
    rightIndent += (kAvatarTabStripShrink + possibleExtraShiftForLion);
  [tabStripController_ setRightIndentForControls:rightIndent];

  // Go ahead and layout the tabs.
  [tabStripController_ layoutTabsWithoutAnimation];

  // Now lay out incognito badge together with the tab strip.
  if ([self shouldShowAvatar]) {
    NSView* avatarButton = [avatarButtonController_ view];
    CGFloat buttonHeight = std::min(
        static_cast<CGFloat>(profiles::kAvatarIconHeight), tabStripHeight);
    [avatarButton setFrameSize:NSMakeSize(profiles::kAvatarIconWidth,
                                          buttonHeight)];

    // Actually place the badge *above* |maxY|, by +2 to miss the divider.  On
    // Lion or later, shift the badge left to move it away from the fullscreen
    // button.
    CGFloat badgeOffset = kAvatarRightOffset + possibleExtraShiftForLion;
    NSPoint origin =
        NSMakePoint(width - NSWidth([avatarButton frame]) - badgeOffset,
                    maxY + 2);
    [avatarButton setFrameOrigin:origin];
    [avatarButton setHidden:NO];  // Make sure it's shown.
  }

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
  return [bookmarkBarController_ isInState:bookmarks::kDetachedState] ||
      [bookmarkBarController_ isAnimatingToState:bookmarks::kDetachedState] ||
      [bookmarkBarController_ isAnimatingFromState:bookmarks::kDetachedState];
}

- (CGFloat)layoutBookmarkBarAtMinX:(CGFloat)minX
                              maxY:(CGFloat)maxY
                             width:(CGFloat)width {
  NSView* bookmarkBarView = [bookmarkBarController_ view];
  NSRect bookmarkBarFrame = [bookmarkBarView frame];
  BOOL oldHidden = [bookmarkBarView isHidden];
  BOOL newHidden = ![self isBookmarkBarVisible];
  if (oldHidden != newHidden)
    [bookmarkBarView setHidden:newHidden];
  bookmarkBarFrame.origin.x = minX;
  bookmarkBarFrame.origin.y = maxY - NSHeight(bookmarkBarFrame);
  bookmarkBarFrame.size.width = width;
  [bookmarkBarView setFrame:bookmarkBarFrame];
  maxY -= NSHeight(bookmarkBarFrame);

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
      BOOL aboveBookmarkBar = [self placeBookmarkBarBelowInfoBar];

      // Insert it into the view hierarchy if necessary.
      if (![floatingBarBackingView_ superview] ||
          aboveBookmarkBar != floatingBarAboveBookmarkBar_) {
        NSView* contentView = [[self window] contentView];
        // z-order gets messed up unless we explicitly remove the floatingbar
        // view and re-add it.
        [floatingBarBackingView_ removeFromSuperview];
        [contentView addSubview:floatingBarBackingView_
                     positioned:(aboveBookmarkBar ?
                                     NSWindowAbove : NSWindowBelow)
                     relativeTo:[bookmarkBarController_ view]];
        floatingBarAboveBookmarkBar_ = aboveBookmarkBar;
      }

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
    if (WebContents* contents = browser_->GetSelectedWebContents()) {
      if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
        rwhv->WindowFrameChanged();
    }
  }
}

- (BOOL)shouldShowBookmarkBar {
  DCHECK(browser_.get());
  return browser_->profile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar) ?
      YES : NO;
}

- (BOOL)shouldShowDetachedBookmarkBar {
  DCHECK(browser_.get());
  TabContentsWrapper* tab = browser_->GetSelectedTabContentsWrapper();
  return (tab && tab->bookmark_tab_helper()->ShouldShowBookmarkBar() &&
          ![previewableContentsController_ isShowingPreview]);
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

- (BOOL)shouldUsePresentationModeWhenEnteringFullscreen {
  return browser_->profile()->GetPrefs()->GetBoolean(
      prefs::kPresentationModeEnabled);
}

- (void)setShouldUsePresentationModeWhenEnteringFullscreen:(BOOL)flag {
  browser_->profile()->GetPrefs()->SetBoolean(
      prefs::kPresentationModeEnabled, flag);
}

- (BOOL)shouldShowPresentationModeToggle {
  return base::mac::IsOSLionOrLater() && [self isFullscreen];
}

- (void)moveViewsForFullscreenForSnowLeopardOrEarlier:(BOOL)fullscreen
    regularWindow:(NSWindow*)regularWindow
    fullscreenWindow:(NSWindow*)fullscreenWindow {
  // This method is only for Snow Leopard and earlier.
  DCHECK(base::mac::IsOSSnowLeopardOrEarlier());

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

  // Destroy the tab strip's sheet controller.  We will recreate it in the new
  // window when needed.
  [tabStripController_ destroySheetController];

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
    BOOL fullscreen_for_tab = browser_->IsFullscreenForTabOrPending();
    BOOL showDropdown = !fullscreen_for_tab &&
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

- (void)enterFullscreenForSnowLeopardOrEarlier {
  DCHECK(base::mac::IsOSSnowLeopardOrEarlier());

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

  [self moveViewsForFullscreenForSnowLeopardOrEarlier:YES
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

- (void)exitFullscreenForSnowLeopardOrEarlier {
  DCHECK(base::mac::IsOSSnowLeopardOrEarlier());

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

  [self moveViewsForFullscreenForSnowLeopardOrEarlier:NO
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

  // Adjust the infobar container. In fullscreen, it needs to be below all
  // top chrome elements so it only sits atop the web contents. When in normal
  // mode, it needs to draw over the bookmark bar and part of the toolbar.
  [[infoBarContainerController_ view] removeFromSuperview];
  NSView* infoBarDest = [[self window] contentView];
  [infoBarDest addSubview:[infoBarContainerController_ view]
               positioned:fullscreen ? NSWindowBelow : NSWindowAbove
               relativeTo:fullscreen ? nil
                                     : [toolbarController_ view]];
}

- (void)showFullscreenExitBubbleIfNecessary {
  if (!browser_->IsFullscreenForTabOrPending()) {
    return;
  }

  [presentationModeController_ ensureOverlayHiddenWithAnimation:NO delay:NO];

  fullscreenExitBubbleController_.reset(
      [[FullscreenExitBubbleController alloc]
          initWithOwner:self
                browser:browser_.get()
                    url:fullscreenUrl_
             bubbleType:fullscreenBubbleType_]);
  NSView* contentView = [[self window] contentView];
  CGFloat maxWidth = NSWidth([contentView frame]);
  CGFloat maxY = NSMaxY([[[self window] contentView] frame]);
  [fullscreenExitBubbleController_
      positionInWindowAtTop:maxY width:maxWidth];
  [fullscreenExitBubbleController_ showWindow];
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
  BOOL mode = [self shouldUsePresentationModeWhenEnteringFullscreen];
  mode = mode || browser_->IsFullscreenForTabOrPending();
  enteringFullscreen_ = YES;
  [self setPresentationModeInternal:mode forceDropdown:NO];
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification {
  if (base::mac::IsOSLionOrLater())
    [self deregisterForContentViewResizeNotifications];
  enteringFullscreen_ = NO;
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

@end  // @implementation BrowserWindowController(Private)
