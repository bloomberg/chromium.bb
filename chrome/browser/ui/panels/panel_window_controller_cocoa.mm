// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_window_controller_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "chrome/app/chrome_command_ids.h"  // IDC_*
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/event_utils.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#import "chrome/browser/ui/cocoa/tab_contents/favicon_util.h"
#import "chrome/browser/ui/cocoa/tabs/throbber_view.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_browser_window_cocoa.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_settings_menu_model.h"
#import "chrome/browser/ui/panels/panel_titlebar_view_cocoa.h"
#include "chrome/browser/ui/toolbar/encoding_menu_controller.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/mac/nsimage_cache.h"

const int kMinimumWindowSize = 1;
const double kBoundsAnimationSpeedPixelsPerSecond = 1000;
const double kBoundsAnimationMaxDurationSeconds = 0.18;

// Replicate specific 10.6 SDK declarations for building with prior SDKs.
#if !defined(MAC_OS_X_VERSION_10_6) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6

enum {
  NSWindowCollectionBehaviorParticipatesInCycle = 1 << 5
};

#endif  // MAC_OS_X_VERSION_10_6

@implementation PanelWindowCocoaImpl
- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen *)screen {
  return frameRect;
}

- (BOOL)canBecomeKeyWindow {
  PanelWindowControllerCocoa* controller =
      static_cast<PanelWindowControllerCocoa*>([self delegate]);
  return [controller canBecomeKeyWindow];
}
@end


@implementation PanelWindowControllerCocoa

- (id)initWithBrowserWindow:(PanelBrowserWindowCocoa*)window {
  NSString* nibpath =
      [base::mac::MainAppBundle() pathForResource:@"Panel" ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    windowShim_.reset(window);
    animateOnBoundsChange_ = YES;
  }
  contentsController_.reset(
      [[TabContentsController alloc] initWithContents:nil
                                             delegate:nil]);
  return self;
}

- (void)dealloc {
  if (windowTrackingArea_.get()) {
    [[[[self window] contentView] superview]
        removeTrackingArea:windowTrackingArea_.get()];
  }
  [super dealloc];
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

  [window setLevel:NSStatusWindowLevel];

  if (base::mac::IsOSSnowLeopardOrLater()) {
    [window setCollectionBehavior:
        NSWindowCollectionBehaviorParticipatesInCycle];
  }

  [titlebar_view_ attach];

  // Set initial size of the window to match the size of the panel to give
  // the renderer the proper size to work with earlier, avoiding a resize
  // after the window is revealed.
  gfx::Rect panelBounds = windowShim_->GetPanelBounds();
  NSRect frame = [window frame];
  frame.size.width = panelBounds.width();
  frame.size.height = panelBounds.height();
  [window setFrame:frame display:NO];

  [[window contentView] addSubview:[contentsController_ view]];
  [self enableTabContentsViewAutosizing];
}

- (void)mouseEntered:(NSEvent*)event {
  [titlebar_view_ updateSettingsButtonVisibility:YES];
}

- (void)mouseExited:(NSEvent*)event {
  [titlebar_view_ updateSettingsButtonVisibility:NO];
}

- (void)disableTabContentsViewAutosizing {
  [[[self window] contentView] setAutoresizesSubviews:NO];
}

- (void)enableTabContentsViewAutosizing {
  NSView* contentView = [[self window] contentView];
  NSView* controllerView = [contentsController_ view];

  DCHECK([controllerView superview] == contentView);
  DCHECK([controllerView autoresizingMask] & NSViewHeightSizable);
  DCHECK([controllerView autoresizingMask] & NSViewWidthSizable);

  // Parent's bounds is child's frame.
  [controllerView setFrame:[contentView bounds]];
  [contentView setAutoresizesSubviews:YES];
  [contentsController_ ensureContentsVisible];
}

- (void)revealAnimatedWithFrame:(const NSRect&)frame {
  NSWindow* window = [self window];  // This ensures loading the nib.

  // Disable subview resizing while resizing the window to avoid renderer
  // resizes during intermediate stages of animation.
  [self disableTabContentsViewAutosizing];

  // We grow the window from the bottom up to produce a 'reveal' animation.
  NSRect startFrame = NSMakeRect(NSMinX(frame), NSMinY(frame),
                                 NSWidth(frame), kMinimumWindowSize);
  [window setFrame:startFrame display:NO animate:NO];
  // Shows the window without making it key, on top of its layer, even if
  // Chromium is not an active app.
  [window orderFrontRegardless];
  // TODO(dcheng): Temporary hack to work around the fact that
  // orderFrontRegardless causes us to become the first responder. The usual
  // Chrome assumption is that becoming the first responder = you have focus, so
  // we always deactivate the controls here. If we're created as an active
  // panel, we'll get a NSWindowDidBecomeKeyNotification and reactivate the web
  // view properly. See crbug.com/97831 for more details.
  TabContents* tab_contents = [contentsController_ tabContents];
  // RWHV may be NULL in unit tests.
  if (tab_contents && tab_contents->GetRenderWidgetHostView())
    tab_contents->GetRenderWidgetHostView()->SetActive(false);
  [window setFrame:frame display:YES animate:YES];

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
  [self updateIcon];
}

- (void)updateIcon {
  NSView* icon = nil;
  NSRect iconFrame = [[titlebar_view_ icon] frame];
  if (throbberShouldSpin_) {
    // If the throbber is spinning now, no need to replace it.
    if ([[titlebar_view_ icon] isKindOfClass:[ThrobberView class]])
      return;

    NSImage* iconImage =
        ResourceBundle::GetSharedInstance().GetNativeImageNamed(IDR_THROBBER);
    icon = [ThrobberView filmstripThrobberViewWithFrame:iconFrame
                                                  image:iconImage];
  } else {
    NSImage* iconImage = mac::FaviconForTabContents(
        windowShim_->browser()->GetSelectedTabContentsWrapper());
    if (!iconImage)
      iconImage = gfx::GetCachedImageWithName(@"nav.pdf");
    NSImageView* iconView =
        [[[NSImageView alloc] initWithFrame:iconFrame] autorelease];
    [iconView setImage:iconImage];
    icon = iconView;
  }

  [titlebar_view_ setIcon:icon];
}

- (void)updateThrobber:(BOOL)shouldSpin {
  if (throbberShouldSpin_ == shouldSpin)
    return;
  throbberShouldSpin_ = shouldSpin;

  // If the titlebar view has not been attached, bail out.
  if (!titlebar_view_)
    return;

  [self updateIcon];
}

- (void)addFindBar:(FindBarCocoaController*)findBarCocoaController {
  NSView* contentView = [[self window] contentView];
  [contentView addSubview:[findBarCocoaController view]];

  CGFloat maxY = NSMaxY([contentView frame]);
  CGFloat maxWidth = NSWidth([contentView frame]);
  [findBarCocoaController positionFindBarViewAtMaxY:maxY maxWidth:maxWidth];
}

- (void)tabInserted:(TabContents*)contents {
  [contentsController_ changeTabContents:contents];
}

- (void)tabDetached:(TabContents*)contents {
  DCHECK(contents == [contentsController_ tabContents]);
  [contentsController_ changeTabContents:NULL];
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
    // Terminate any playing animations.
    [self terminateBoundsAnimation];
    animateOnBoundsChange_ = NO;
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
  windowShim_->DidCloseNativeWindow();
  // Call |-autorelease| after a zero-length delay to avoid deadlock from
  // code in the current run loop that waits on BROWSER_CLOSED notification.
  // The notification is sent when this object is freed, but this object
  // cannot be freed until the current run loop completes.
  [self performSelector:@selector(autorelease)
             withObject:nil
             afterDelay:0];
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
  // Setup the whole window as the tracking area so that we can get notified
  // when the mouse enters or leaves the window. This will make us be able to
  // show or hide settings button accordingly.
  NSRect localBounds = frame;
  localBounds.origin = NSZeroPoint;
  windowTrackingArea_.reset(
      [[CrTrackingArea alloc] initWithRect:localBounds
                                   options:(NSTrackingMouseEnteredAndExited |
                                            NSTrackingActiveAlways)
                              proxiedOwner:self
                                  userInfo:nil]);
  [windowTrackingArea_.get() clearOwnerWhenWindowWillClose:[self window]];
  [[[[self window] contentView] superview]
      addTrackingArea:windowTrackingArea_.get()];

  if (!animateOnBoundsChange_) {
    [[self window] setFrame:frame display:YES animate:NO];
    return;
  }
  // Will be enabled back in animationDidEnd callback.
  [self disableTabContentsViewAutosizing];

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

  NSRect currentFrame = [[self window] frame];
  // Compute duration. We use constant speed of animation, however if the change
  // is too large, we clip the duration (effectively increasing speed) to
  // limit total duration of animation. This makes 'small' transitions fast.
  // 'distance' is the max travel between 4 potentially traveling corners.
  double distanceX = std::max(abs(NSMinX(currentFrame) - NSMinX(frame)),
                              abs(NSMaxX(currentFrame) - NSMaxX(frame)));
  double distanceY = std::max(abs(NSMinY(currentFrame) - NSMinY(frame)),
                              abs(NSMaxY(currentFrame) - NSMaxY(frame)));
  double distance = std::max(distanceX, distanceY);
  double duration = std::min(distance / kBoundsAnimationSpeedPixelsPerSecond,
                             kBoundsAnimationMaxDurationSeconds);
  [boundsAnimation_ setDuration: duration];
  [boundsAnimation_ startAnimation];
}

- (void)animationDidEnd:(NSAnimation*)animation {
  if (windowShim_->panel()->expansion_state() == Panel::EXPANDED)
    [self enableTabContentsViewAutosizing];

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_BOUNDS_ANIMATIONS_FINISHED,
      content::Source<Panel>(windowShim_->panel()),
      content::NotificationService::NoDetails());
}

- (void)terminateBoundsAnimation {
  if (!boundsAnimation_)
    return;
  [boundsAnimation_ stopAnimation];
  [boundsAnimation_ setDelegate:nil];
  [boundsAnimation_ release];
  boundsAnimation_ = nil;
}

- (BOOL)isAnimatingBounds {
  return boundsAnimation_ && [boundsAnimation_ isAnimating];
}

- (void)onTitlebarMouseClicked {
  Panel* panel = windowShim_->panel();
  Panel::ExpansionState oldExpansionState = panel->expansion_state();

  if (oldExpansionState == Panel::EXPANDED) {
    if ([[self titlebarView] isDrawingAttention]) {
      // Do not minimize if the Panel is drawing attention since user
      // most likely simply wants to reset the 'draw attention' status.
      panel->Activate();
      return;
    }
    panel->SetExpansionState(Panel::MINIMIZED);
    // The Panel class ensures deactivaiton when it is minimized.
  } else {
    panel->SetExpansionState(Panel::EXPANDED);
    panel->Activate();
  }
}

- (int)titlebarHeightInScreenCoordinates {
  NSView* titlebar = [self titlebarView];
  return NSHeight([titlebar convertRect:[titlebar bounds] toView:nil]);
}

// TODO(dcheng): These two selectors are almost copy-and-paste from
// BrowserWindowController. Figure out the appropriate way of code sharing,
// whether it's refactoring more things into BrowserWindowUtils or making a
// common base controller for browser windows.
- (void)windowDidBecomeKey:(NSNotification*)notification {
  BrowserList::SetLastActive(windowShim_->browser());

  // We need to activate the controls (in the "WebView"). To do this, get the
  // selected TabContents's RenderWidgetHostViewMac and tell it to activate.
  if (TabContents* contents = [contentsController_ tabContents]) {
    if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
      rwhv->SetActive(true);
  }

  // If the window becomes key, lets make sure it is expanded and stop
  // drawing attention - since it is ready to accept input, it already has
  // user's attention.
  if ([[self titlebarView] isDrawingAttention]) {
    [[self titlebarView] stopDrawingAttention];
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_CHANGED_ACTIVE_STATUS,
      content::Source<Panel>(windowShim_->panel()),
      content::NotificationService::NoDetails());
}

- (void)windowDidResignKey:(NSNotification*)notification {
  // If our app is still active and we're still the key window, ignore this
  // message, since it just means that a menu extra (on the "system status bar")
  // was activated; we'll get another |-windowDidResignKey| if we ever really
  // lose key window status.
  if ([NSApp isActive] && ([NSApp keyWindow] == [self window]))
    return;

  // We need to deactivate the controls (in the "WebView"). To do this, get the
  // selected TabContents's RenderWidgetHostView and tell it to deactivate.
  if (TabContents* contents = [contentsController_ tabContents]) {
    if (RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView())
      rwhv->SetActive(false);
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_CHANGED_ACTIVE_STATUS,
      content::Source<Panel>(windowShim_->panel()),
      content::NotificationService::NoDetails());
}

- (void)deactivate {
  if (![[self window] isMainWindow])
    return;
  BrowserWindow* browser_window =
      windowShim_->panel()->manager()->GetNextBrowserWindowToActivate(
          windowShim_->panel());

  if (browser_window)
    browser_window->Activate();
  else
    [NSApp deactivate];
}

- (BOOL)canBecomeKeyWindow {
  // Panel can only gain focus if it is expanded. Minimized panels do not
  // participate in Cmd-~ rotation.
  // TODO(dimich): If it will be ever desired to expand/focus the Panel on
  // keyboard navigation or via main menu, the care should be taken to avoid
  // cases when minimized Panel is getting keyboard input, invisibly.
  return windowShim_->panel()->expansion_state() == Panel::EXPANDED;
}

@end
