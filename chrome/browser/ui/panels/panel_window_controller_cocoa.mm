// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_window_controller_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"  // IDC_*
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/event_utils.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_browser_window_cocoa.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_settings_menu_model.h"
#import "chrome/browser/ui/panels/panel_titlebar_view_cocoa.h"
#include "chrome/browser/ui/toolbar/encoding_menu_controller.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"

const int kMinimumWindowSize = 1;
const double kBoundsChangeAnimationDuration = 0.25;

// Replicate specific 10.6 SDK declarations for building with prior SDKs.
#if !defined(MAC_OS_X_VERSION_10_6) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6

enum {
  NSWindowCollectionBehaviorParticipatesInCycle = 1 << 5
};

#endif  // MAC_OS_X_VERSION_10_6

// In UNIT_TESTS, this is set to YES to wait on nonblocking animations.
static BOOL g_reportAnimationStatus = NO;

@implementation PanelWindowControllerCocoa

- (id)initWithBrowserWindow:(PanelBrowserWindowCocoa*)window {
  NSString* nibpath =
      [base::mac::MainAppBundle() pathForResource:@"Panel" ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    windowShim_.reset(window);
    animateOnBoundsChange_ = YES;
  }
  return self;
}

- (ui::ThemeProvider*)themeProvider {
  return ThemeServiceFactory::GetForProfile(windowShim_->browser()->profile());
}

- (ThemedWindowStyle)themedWindowStyle {
  ThemedWindowStyle style = THEMED_POPUP;
  if (windowShim_->browser()->profile()->IsOffTheRecord())
    style |= THEMED_INCOGNITO;
  return style;
}

- (NSPoint)themePatternPhase {
  NSView* windowView = [[[self window] contentView] superview];
  return [BrowserWindowUtils themePatternPhaseFor:windowView withTabStrip:nil];
}

- (void)awakeFromNib {
  NSWindow* window = [self window];

  DCHECK(window);
  DCHECK(titlebar_view_);
  DCHECK_EQ(self, [window delegate]);

  // Using NSModalPanelWindowLevel (8) rather then NSStatusWindowLevel (25)
  // ensures notification balloons on top of regular windows, but below
  // popup menus which are at NSPopUpMenuWindowLevel (101) and Spotlight
  // drop-out, which is at NSStatusWindowLevel-2 (23) for OSX 10.6/7.
  // See http://crbug.com/59878.
  [window setLevel:NSModalPanelWindowLevel];

  if (base::mac::IsOSSnowLeopardOrLater()) {
    [window setCollectionBehavior:
        NSWindowCollectionBehaviorParticipatesInCycle];
  }

  [titlebar_view_ attach];

  // Attach the RenderWigetHostView to the view hierarchy, it will render
  // HTML content.
  [[window contentView] addSubview:[self tabContentsView]];
  [self enableTabContentsViewAutosizing];
}

- (void)disableTabContentsViewAutosizing {
  NSView* tabContentView = [self tabContentsView];
  DCHECK([tabContentView superview] == [[self window] contentView]);
  [tabContentView setAutoresizingMask:NSViewNotSizable];
}

- (void)enableTabContentsViewAutosizing {
  NSView* tabContentView = [self tabContentsView];
  DCHECK([tabContentView superview] == [[self window] contentView]);

  // Parent's bounds is child's frame.
  NSRect frame = [[[self window] contentView] bounds];
  [tabContentView setFrame:frame];
  [tabContentView
      setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
}

- (void)revealAnimatedWithFrame:(const NSRect&)frame {
  NSWindow* window = [self window];  // This ensures loading the nib.

  // Temporarily disable auto-resizing of the TabContents view to make animation
  // smoother and avoid rendering of HTML content at random intermediate sizes.
  [self disableTabContentsViewAutosizing];

  // We grow the window from the bottom up to produce a 'reveal' animation.
  NSRect startFrame = NSMakeRect(NSMinX(frame), NSMinY(frame),
                                 NSWidth(frame), kMinimumWindowSize);
  [window setFrame:startFrame display:NO animate:NO];
  // Shows the window without making it key, on top of its layer, even if
  // Chromium is not an active app.
  [window orderFrontRegardless];
  [window setFrame:frame display:YES animate:YES];

  // Resume auto-resizing of the TabContents view.
  [self enableTabContentsViewAutosizing];
}

- (void)updateTitleBar {
  NSString* newTitle = base::SysUTF16ToNSString(
      windowShim_->browser()->GetWindowTitleForCurrentTab());
  pendingWindowTitle_.reset(
      [BrowserWindowUtils scheduleReplaceOldTitle:pendingWindowTitle_.get()
                                     withNewTitle:newTitle
                                        forWindow:[self window]]);
  [titlebar_view_ setTitle:newTitle];
}

- (void)addFindBar:(FindBarCocoaController*)findBarCocoaController {
  NSView* contentView = [[self window] contentView];
  [contentView addSubview:[findBarCocoaController view]];

  CGFloat maxY = NSMaxY([contentView frame]);
  CGFloat maxWidth = NSWidth([contentView frame]);
  [findBarCocoaController positionFindBarViewAtMaxY:maxY maxWidth:maxWidth];
}

- (NSView*)tabContentsView {
  TabContents* contents = windowShim_->browser()->GetSelectedTabContents();
  CHECK(contents);
  NSView* tabContentView = contents->GetNativeView();
  CHECK(tabContentView);
  return tabContentView;
}

- (PanelTitlebarViewCocoa*)titlebarView {
  return titlebar_view_;
}

// Called to validate menu and toolbar items when this window is key. All the
// items we care about have been set with the |-commandDispatch:| or
// |-commandDispatchUsingKeyModifiers:| actions and a target of FirstResponder
// in IB. If it's not one of those, let it continue up the responder chain to be
// handled elsewhere.
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];
  BOOL enable = NO;
  if (action == @selector(commandDispatch:) ||
      action == @selector(commandDispatchUsingKeyModifiers:)) {
    NSInteger tag = [item tag];
    CommandUpdater* command_updater = windowShim_->browser()->command_updater();
    if (command_updater->SupportsCommand(tag)) {
      enable = command_updater->IsCommandEnabled(tag);
      // Disable commands that do not apply to Panels.
      switch (tag) {
        case IDC_CLOSE_TAB:
        case IDC_FULLSCREEN:
        case IDC_PRESENTATION_MODE:
        case IDC_SYNC_BOOKMARKS:
          enable = NO;
          break;
        default:
          // Special handling for the contents of the Text Encoding submenu. On
          // Mac OS, instead of enabling/disabling the top-level menu item, we
          // enable/disable the submenu's contents (per Apple's HIG).
          EncodingMenuController encoding_controller;
          if (encoding_controller.DoesCommandBelongToEncodingMenu(tag)) {
            enable &= command_updater->IsCommandEnabled(IDC_ENCODING_MENU) ?
                YES : NO;
          }
      }
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
  windowShim_->browser()->ExecuteCommand([sender tag]);
}

// Same as |-commandDispatch:|, but executes commands using a disposition
// determined by the key flags.
- (void)commandDispatchUsingKeyModifiers:(id)sender {
  DCHECK(sender);
  NSEvent* event = [NSApp currentEvent];
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEventWithFlags(
          event, [event modifierFlags]);
  windowShim_->browser()->ExecuteCommandWithDisposition(
      [sender tag], disposition);
}

- (void)executeCommand:(int)command {
  windowShim_->browser()->ExecuteCommandIfEnabled(command);
}

// Handler for the custom Close button.
- (void)closePanel {
  windowShim_->panel()->Close();
}

// Called when the user wants to close the panel or from the shutdown process.
// The Browser object is in control of whether or not we're allowed to close. It
// may defer closing due to several states, such as onbeforeUnload handlers
// needing to be fired. If closing is deferred, the Browser will handle the
// processing required to get us to the closing state and (by watching for
// all the tabs going away) will again call to close the window when it's
// finally ready.
// This callback is only called if the standard Close button is enabled in XIB.
- (BOOL)windowShouldClose:(id)sender {
  Browser* browser = windowShim_->browser();
  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!browser->ShouldCloseWindow())
    return NO;

  if (!browser->tabstrip_model()->empty()) {
    // Tab strip isn't empty. Make browser to close all the tabs, allowing the
    // renderer to shut down and call us back again.
    // The tab strip of Panel is not visible and contains only one tab but
    // it still has to be closed.
    browser->OnWindowClosing();
    return NO;
  }

  // the tab strip is empty, it's ok to close the window
  return YES;
}

// When windowShouldClose returns YES (or if controller receives direct 'close'
// signal), window will be unconditionally closed. Clean up.
- (void)windowWillClose:(NSNotification*)notification {
  DCHECK(windowShim_->browser()->tabstrip_model()->empty());
  // Avoid callbacks from a nonblocking animation in progress, if any.
  [self terminateBoundsAnimation];
  windowShim_->didCloseNativeWindow();
  [self autorelease];
}

- (void)runSettingsMenu:(NSView*)button {
  Panel* panel = windowShim_->panel();
  DCHECK(panel->GetExtension());

  scoped_ptr<PanelSettingsMenuModel> settingsMenuModel(
      new PanelSettingsMenuModel(panel));
  scoped_nsobject<MenuController> settingsMenuController(
      [[MenuController alloc] initWithModel:settingsMenuModel.get()
                     useWithPopUpButtonCell:NO]);

  [NSMenu popUpContextMenu:[settingsMenuController menu]
                 withEvent:[NSApp currentEvent]
                   forView:button];
}

- (void)startDrag {
  animateOnBoundsChange_ = NO;
  windowShim_->panel()->manager()->StartDragging(windowShim_->panel());
}

- (void)endDrag:(BOOL)cancelled {
  animateOnBoundsChange_ = YES;
  windowShim_->panel()->manager()->EndDragging(cancelled);
}

- (void)dragWithDeltaX:(int)deltaX {
  windowShim_->panel()->manager()->Drag(deltaX);
}

- (void)setPanelFrame:(NSRect)frame {
  if (!animateOnBoundsChange_) {
    [[self window] setFrame:frame display:YES animate:NO];
    return;
  }

  NSDictionary *windowResize = [NSDictionary dictionaryWithObjectsAndKeys:
      [self window], NSViewAnimationTargetKey,
      [NSValue valueWithRect:frame], NSViewAnimationEndFrameKey, nil];

  NSArray *animations = [NSArray arrayWithObjects:windowResize, nil];

  // Terminate previous animation, if it is still playing.
  [self terminateBoundsAnimation];
  boundsAnimation_ =
      [[NSViewAnimation alloc] initWithViewAnimations:animations];
  [boundsAnimation_ setDelegate:self];

  [boundsAnimation_ setAnimationBlockingMode: NSAnimationNonblocking];
  [boundsAnimation_ setDuration: kBoundsChangeAnimationDuration];
  [boundsAnimation_ startAnimation];
}

- (void)animationDidEnd:(NSAnimation*)animation {
  if (!g_reportAnimationStatus)
    return;

  NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_BOUNDS_ANIMATIONS_FINISHED,
      Source<Panel>(windowShim_->panel()),
      NotificationService::NoDetails());
}

- (void)terminateBoundsAnimation {
  if (!boundsAnimation_)
    return;
  [boundsAnimation_ stopAnimation];
  [boundsAnimation_ setDelegate:nil];
  [boundsAnimation_ release];
}

// TestingAPI interface implementation

+ (void)enableBoundsAnimationNotifications {
  g_reportAnimationStatus = YES;
}

@end
