// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_controller_private.h"

#include <cmath>

#import "base/auto_reset.h"
#include "base/command_line.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window_state.h"
#import "chrome/browser/ui/cocoa/browser_window_layout.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"
#import "chrome/browser/ui/cocoa/fast_resize_view.h"
#import "chrome/browser/ui/cocoa/framed_browser_window.h"
#include "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#import "chrome/browser/ui/cocoa/tab_contents/overlayable_contents_controller.h"
#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/cocoa/appkit_utils.h"
#import "ui/base/cocoa/focus_tracker.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/ui_base_types.h"

using content::RenderWidgetHostView;
using content::WebContents;

@interface NSWindow (NSPrivateBrowserWindowControllerApis)
// Note: These functions are private, use -[NSObject respondsToSelector:]
// before calling them.
- (NSWindow*)_windowForToolbar;
@end

@implementation BrowserWindowController(Private)

- (void)saveWindowPositionIfNeeded {
  if (!chrome::ShouldSaveWindowPlacement(browser_.get()))
    return;

  NSWindow* window = [self window];

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
  CGFloat defaultSheetY = defaultSheetLocation.origin.y;
  // The toolbar is not shown in popup and application modes. The sheet
  // should be located at the top of the window, under the title of the
  // window.
  defaultSheetY = NSMaxY([[window contentView] frame]);

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

  [self updateSubviewZOrder];

  base::scoped_nsobject<BrowserWindowLayout> layout(
      [[BrowserWindowLayout alloc] init]);
  [self applyLayout:layout];

  browser_->GetBubbleManager()->UpdateAllBubbleAnchors();
}

- (void)applyTabStripLayout:(const chrome::TabStripLayout&)layout {
}

- (BOOL)placeBookmarkBarBelowInfoBar {
  return NO;
}

- (void)layoutTabContentArea:(NSRect)newFrame {
  NSView* tabContentView = [self tabContentArea];
  NSRect tabContentFrame = [tabContentView frame];

  tabContentFrame = newFrame;
  [tabContentView setFrame:tabContentFrame];
}

- (void)adjustToolbarAndBookmarkBarForCompression:(CGFloat)compression {
}


- (void)applyLayout:(BrowserWindowLayout*)layout {
  chrome::LayoutOutput output = [layout computeLayout];

  [self layoutTabContentArea:output.contentAreaFrame];
}

- (void)updateSubviewZOrder {
  [self updateSubviewZOrderNormal];
}

- (void)updateSubviewZOrderNormal {
  base::scoped_nsobject<NSMutableArray> subviews([[NSMutableArray alloc] init]);
  if ([self tabContentArea])
    [subviews addObject:[self tabContentArea]];

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

- (WebContents*)webContents {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

- (PermissionRequestManager*)permissionRequestManager {
  if (WebContents* contents = [self webContents])
    return PermissionRequestManager::FromWebContents(contents);
  return nil;
}

@end  // @implementation BrowserWindowController(Private)
