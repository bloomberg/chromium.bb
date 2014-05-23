// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/panels/panel_window_controller_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"  // IDC_*
#include "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/browser_command_executor.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/panels/mouse_drag_controller.h"
#import "chrome/browser/ui/cocoa/panels/panel_cocoa.h"
#import "chrome/browser/ui/cocoa/panels/panel_titlebar_view_cocoa.h"
#import "chrome/browser/ui/cocoa/panels/panel_utils_cocoa.h"
#import "chrome/browser/ui/cocoa/tab_contents/favicon_util_mac.h"
#import "chrome/browser/ui/cocoa/tab_contents/tab_contents_controller.h"
#import "chrome/browser/ui/cocoa/sprite_view.h"
#include "chrome/browser/ui/panels/panel_bounds_animation.h"
#include "chrome/browser/ui/panels/panel_collection.h"
#include "chrome/browser/ui/panels/panel_constants.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/stacked_panel_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/encoding_menu_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

using content::WebContents;

const int kMinimumWindowSize = 1;
const double kBoundsAnimationSpeedPixelsPerSecond = 1000;
const double kBoundsAnimationMaxDurationSeconds = 0.18;

// Edge thickness to trigger user resizing via system, in screen pixels.
const double kWidthOfMouseResizeArea = 15.0;

@interface PanelWindowControllerCocoa (PanelsCanBecomeKey)
// Internal helper method for extracting the total number of panel windows
// from the panel manager. Used to decide if panel can become the key window.
- (int)numPanels;
@end

@implementation PanelWindowCocoaImpl
// The panels cannot be reduced to 3-px windows on the edge of the screen
// active area (above Dock). Default constraining logic makes at least a height
// of the titlebar visible, so the user could still grab it. We do 'restore'
// differently, and minimize panels to 3 px. Hence the need to override the
// constraining logic.
- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen *)screen {
  return frameRect;
}

// Prevent panel window from becoming key - for example when it is minimized.
// Panel windows use a higher priority NSWindowLevel to ensure they are always
// visible, causing the OS to prefer panel windows when selecting a window
// to make the key window. To counter this preference, we override
// -[NSWindow:canBecomeKeyWindow] to restrict when the panel can become the
// key window to a limited set of scenarios, such as when cycling through
// windows, when panels are the only remaining windows, when an event
// triggers window activation, etc. The panel may also be prevented from
// becoming the key window, regardless of the above scenarios, such as when
// a panel is minimized.
- (BOOL)canBecomeKeyWindow {
  // Give precedence to controller preventing activation of the window.
  PanelWindowControllerCocoa* controller =
      base::mac::ObjCCast<PanelWindowControllerCocoa>([self windowController]);
  if (![controller canBecomeKeyWindow])
    return NO;

  BrowserCrApplication* app = base::mac::ObjCCast<BrowserCrApplication>(
      [BrowserCrApplication sharedApplication]);

  // A Panel window can become the key window only in limited scenarios.
  // This prevents the system from always preferring a Panel window due
  // to its higher priority NSWindowLevel when selecting a window to make key.
  return ([app isHandlingSendEvent]  && [[app currentEvent] window] == self) ||
      [controller activationRequestedByPanel] ||
      [app isCyclingWindows] ||
      [app previousKeyWindow] == self ||
      [[app windows] count] == static_cast<NSUInteger>([controller numPanels]);
}

- (void)performMiniaturize:(id)sender {
  [[self windowController] minimizeButtonClicked:0];
}

- (void)mouseMoved:(NSEvent*)event {
  // Cocoa does not support letting the application determine the edges that
  // can trigger the user resizing. To work around this, we track the mouse
  // location. When it is close to the edge/corner where the user resizing
  // is not desired, we force the min and max size of the window to be same
  // as current window size. For all other cases, we restore the min and max
  // size.
  PanelWindowControllerCocoa* controller =
      base::mac::ObjCCast<PanelWindowControllerCocoa>([self windowController]);
  NSRect frame = [self frame];
  if ([controller canResizeByMouseAtCurrentLocation]) {
    // Mac window server limits window sizes to 10000.
    NSSize maxSize = NSMakeSize(10000, 10000);

    // If the user is resizing a stacked panel by its bottom edge, make sure its
    // height cannot grow more than what the panel below it could offer. This is
    // because growing a stacked panel by y amount will shrink the panel below
    // it by same amount and we do not want the panel below it being shrunk to
    // be smaller than the titlebar.
    Panel* panel = [controller panel];
    NSPoint point = [NSEvent mouseLocation];
    if (point.y < NSMinY(frame) + kWidthOfMouseResizeArea && panel->stack()) {
      Panel* belowPanel = panel->stack()->GetPanelBelow(panel);
      if (belowPanel && !belowPanel->IsMinimized()) {
        maxSize.height = panel->GetBounds().height() +
            belowPanel->GetBounds().height() - panel::kTitlebarHeight;
      }
    }

    // Enable the user-resizing by setting both min and max size to the right
    // values.
    [self setMinSize:NSMakeSize(panel::kPanelMinWidth,
                                panel::kPanelMinHeight)];
    [self setMaxSize:maxSize];
  } else {
    // Disable the user-resizing by setting both min and max size to be same as
    // current window size.
    [self setMinSize:frame.size];
    [self setMaxSize:frame.size];
  }

  [super mouseMoved:event];
}
@end

// ChromeEventProcessingWindow expects its controller to implement the
// BrowserCommandExecutor protocol.
@interface PanelWindowControllerCocoa (InternalAPI) <BrowserCommandExecutor>

// BrowserCommandExecutor methods.
- (void)executeCommand:(int)command;

@end

@implementation PanelWindowControllerCocoa (InternalAPI)

// This gets called whenever a browser-specific keyboard shortcut is performed
// in the Panel window. We simply swallow all those events.
- (void)executeCommand:(int)command {}

@end

@implementation PanelWindowControllerCocoa

- (id)initWithPanel:(PanelCocoa*)window {
  NSString* nibpath =
      [base::mac::FrameworkBundle() pathForResource:@"Panel" ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    windowShim_.reset(window);
    animateOnBoundsChange_ = YES;
    canBecomeKeyWindow_ = YES;
    activationRequestedByPanel_ = NO;
  }
  return self;
}

- (Panel*)panel {
  return windowShim_->panel();
}

- (void)awakeFromNib {
  NSWindow* window = [self window];

  DCHECK(window);
  DCHECK(titlebar_view_);
  DCHECK_EQ(self, [window delegate]);

  [self updateWindowLevel];

  [self updateWindowCollectionBehavior];

  [titlebar_view_ attach];

  // Set initial size of the window to match the size of the panel to give
  // the renderer the proper size to work with earlier, avoiding a resize
  // after the window is revealed.
  gfx::Rect panelBounds = windowShim_->panel()->GetBounds();
  NSRect frame = [window frame];
  frame.size.width = panelBounds.width();
  frame.size.height = panelBounds.height();
  [window setFrame:frame display:NO];

  // MacOS will turn the user-resizing to the user-dragging if the direction of
  // the dragging is orthogonal to the direction of the arrow cursor. We do not
  // want this since it will bypass our dragging logic. The panel window is
  // still draggable because we track and handle the dragging in our custom way.
  [[self window] setMovable:NO];

  [self updateTrackingArea];
}

- (void)updateWebContentsViewFrame {
  content::WebContents* webContents = windowShim_->panel()->GetWebContents();
  if (!webContents)
    return;

  // Compute the size of the web contents view. Don't assume it's similar to the
  // size of the contentView, because the contentView is managed by the Cocoa
  // to be (window - standard titlebar), while we have taller custom titlebar
  // instead. In coordinate system of window's contentView.
  NSRect contentFrame = [self contentRectForFrameRect:[[self window] frame]];
  contentFrame.origin = NSZeroPoint;

  NSView* contentView = webContents->GetNativeView();
  if (!NSEqualRects([contentView frame], contentFrame))
    [contentView setFrame:contentFrame];
}

- (void)disableWebContentsViewAutosizing {
  [[[self window] contentView] setAutoresizesSubviews:NO];
}

- (void)enableWebContentsViewAutosizing {
  [self updateWebContentsViewFrame];
  [[[self window] contentView] setAutoresizesSubviews:YES];
}

- (void)revealAnimatedWithFrame:(const NSRect&)frame {
  NSWindow* window = [self window];  // This ensures loading the nib.

  // Disable subview resizing while resizing the window to avoid renderer
  // resizes during intermediate stages of animation.
  [self disableWebContentsViewAutosizing];

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
  WebContents* web_contents = windowShim_->panel()->GetWebContents();
  // RWHV may be NULL in unit tests.
  if (web_contents && web_contents->GetRenderWidgetHostView())
    web_contents->GetRenderWidgetHostView()->SetActive(false);

  // This will re-enable the content resizing after it finishes.
  [self setPanelFrame:frame animate:YES];
}

- (void)updateTitleBar {
  NSString* newTitle = base::SysUTF16ToNSString(
      windowShim_->panel()->GetWindowTitle());
  pendingWindowTitle_.reset(
      [BrowserWindowUtils scheduleReplaceOldTitle:pendingWindowTitle_.get()
                                     withNewTitle:newTitle
                                        forWindow:[self window]]);
  [titlebar_view_ setTitle:newTitle];
  [self updateIcon];
}

- (void)updateIcon {
  base::scoped_nsobject<NSView> iconView;
  if (throbberShouldSpin_) {
    // If the throbber is spinning now, no need to replace it.
    if ([[titlebar_view_ icon] isKindOfClass:[SpriteView class]])
      return;

    NSImage* iconImage =
        ResourceBundle::GetSharedInstance().GetNativeImageNamed(
            IDR_THROBBER).ToNSImage();
    SpriteView* spriteView = [[SpriteView alloc] init];
    [spriteView setImage:iconImage];
    iconView.reset(spriteView);
  } else {
    const gfx::Image& page_icon = windowShim_->panel()->GetCurrentPageIcon();
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    NSRect iconFrame = [[titlebar_view_ icon] frame];
    NSImage* iconImage = page_icon.IsEmpty() ?
        rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON).ToNSImage() :
        page_icon.ToNSImage();
    NSImageView* imageView = [[NSImageView alloc] initWithFrame:iconFrame];
    [imageView setImage:iconImage];
    iconView.reset(imageView);
  }
  [titlebar_view_ setIcon:iconView];
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

- (void)updateTitleBarMinimizeRestoreButtonVisibility {
  Panel* panel = windowShim_->panel();
  [titlebar_view_ setMinimizeButtonVisibility:panel->CanShowMinimizeButton()];
  [titlebar_view_ setRestoreButtonVisibility:panel->CanShowRestoreButton()];
}

- (void)webContentsInserted:(WebContents*)contents {
  NSView* view = contents->GetNativeView();
  [[[self window] contentView] addSubview:view];
  [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  [self enableWebContentsViewAutosizing];
}

- (void)webContentsDetached:(WebContents*)contents {
  [contents->GetNativeView() removeFromSuperview];
}

- (PanelTitlebarViewCocoa*)titlebarView {
  return titlebar_view_;
}

// Called to validate menu and toolbar items when this window is key. All the
// items we care about have been set with the |-commandDispatch:|
// action and a target of FirstResponder in IB.
// Delegate to the NSApp delegate if Panel does not care about the command or
// shortcut, to make sure the global items in Chrome main app menu still work.
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  if ([item action] == @selector(commandDispatch:)) {
    NSInteger tag = [item tag];
    CommandUpdater* command_updater = windowShim_->panel()->command_updater();
    if (command_updater->SupportsCommand(tag))
      return command_updater->IsCommandEnabled(tag);
    else
      return [[NSApp delegate] validateUserInterfaceItem:item];
  }
  return NO;
}

// Called when the user picks a menu or toolbar item when this window is key.
// Calls through to the panel object to execute the command or delegates up.
- (void)commandDispatch:(id)sender {
  DCHECK(sender);
  NSInteger tag = [sender tag];
  CommandUpdater* command_updater = windowShim_->panel()->command_updater();
  if (command_updater->SupportsCommand(tag))
    windowShim_->panel()->ExecuteCommandIfEnabled(tag);
  else
    [[NSApp delegate] commandDispatch:sender];
}

// Handler for the custom Close button.
- (void)closePanel {
  windowShim_->panel()->Close();
}

// Handler for the custom Minimize button.
- (void)minimizeButtonClicked:(int)modifierFlags {
  Panel* panel = windowShim_->panel();
  panel->OnMinimizeButtonClicked((modifierFlags & NSShiftKeyMask) ?
                                 panel::APPLY_TO_ALL : panel::NO_MODIFIER);
}

// Handler for the custom Restore button.
- (void)restoreButtonClicked:(int)modifierFlags {
  Panel* panel = windowShim_->panel();
  panel->OnRestoreButtonClicked((modifierFlags & NSShiftKeyMask) ?
                                panel::APPLY_TO_ALL : panel::NO_MODIFIER);
}

// Called when the user wants to close the panel or from the shutdown process.
// The Panel object is in control of whether or not we're allowed to close. It
// may defer closing due to several states, such as onbeforeUnload handlers
// needing to be fired. If closing is deferred, the Panel will handle the
// processing required to get us to the closing state and (by watching for
// the web content going away) will again call to close the window when it's
// finally ready.
- (BOOL)windowShouldClose:(id)sender {
  Panel* panel = windowShim_->panel();
  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!panel->ShouldCloseWindow())
    return NO;

  if (panel->GetWebContents()) {
    // Terminate any playing animations.
    [self terminateBoundsAnimation];
    animateOnBoundsChange_ = NO;
    // Make panel close the web content, allowing the renderer to shut down
    // and call us back again.
    panel->OnWindowClosing();
    return NO;
  }

  // No web content; it's ok to close the window.
  return YES;
}

// When windowShouldClose returns YES (or if controller receives direct 'close'
// signal), window will be unconditionally closed. Clean up.
- (void)windowWillClose:(NSNotification*)notification {
  DCHECK(!windowShim_->panel()->GetWebContents());
  // Avoid callbacks from a nonblocking animation in progress, if any.
  [self terminateBoundsAnimation];
  windowShim_->DidCloseNativeWindow();
  // Call |-autorelease| after a zero-length delay to avoid deadlock from
  // code in the current run loop that waits on PANEL_CLOSED notification.
  // The notification is sent when this object is freed, but this object
  // cannot be freed until the current run loop completes.
  [self performSelector:@selector(autorelease)
             withObject:nil
             afterDelay:0];
}

- (void)startDrag:(NSPoint)mouseLocation {
  // Convert from Cocoa's screen coordinates to platform-indepedent screen
  // coordinates because PanelManager method takes platform-indepedent screen
  // coordinates.
  windowShim_->panel()->manager()->StartDragging(
      windowShim_->panel(),
      cocoa_utils::ConvertPointFromCocoaCoordinates(mouseLocation));
}

- (void)endDrag:(BOOL)cancelled {
  windowShim_->panel()->manager()->EndDragging(cancelled);
}

- (void)drag:(NSPoint)mouseLocation {
  // Convert from Cocoa's screen coordinates to platform-indepedent screen
  // coordinates because PanelManager method takes platform-indepedent screen
  // coordinates.
  windowShim_->panel()->manager()->Drag(
      cocoa_utils::ConvertPointFromCocoaCoordinates(mouseLocation));
}

- (void)setPanelFrame:(NSRect)frame
              animate:(BOOL)animate {
  BOOL jumpToDestination = (!animateOnBoundsChange_ || !animate);

  // If no animation is in progress, apply bounds change instantly.
  if (jumpToDestination && ![self isAnimatingBounds]) {
    [[self window] setFrame:frame display:YES animate:NO];
    return;
  }

  NSDictionary *windowResize = [NSDictionary dictionaryWithObjectsAndKeys:
      [self window], NSViewAnimationTargetKey,
      [NSValue valueWithRect:frame], NSViewAnimationEndFrameKey, nil];
  NSArray *animations = [NSArray arrayWithObjects:windowResize, nil];

  // If an animation is in progress, update the animation with new target
  // bounds. Also, set the destination frame bounds to the new value.
  if (jumpToDestination && [self isAnimatingBounds]) {
    [boundsAnimation_ setViewAnimations:animations];
    [[self window] setFrame:frame display:YES animate:NO];
    return;
  }

  // Will be enabled back in animationDidEnd callback.
  [self disableWebContentsViewAutosizing];

  // Terminate previous animation, if it is still playing.
  [self terminateBoundsAnimation];

  boundsAnimation_ =
      [[NSViewAnimation alloc] initWithViewAnimations:animations];
  [boundsAnimation_ setDelegate:self];

  NSRect currentFrame = [[self window] frame];
  // Compute duration. We use constant speed of animation, however if the change
  // is too large, we clip the duration (effectively increasing speed) to
  // limit total duration of animation. This makes 'small' transitions fast.
  // 'distance' is the max travel between 4 potentially traveling corners.
  double distanceX = std::max(std::abs(NSMinX(currentFrame) - NSMinX(frame)),
                              std::abs(NSMaxX(currentFrame) - NSMaxX(frame)));
  double distanceY = std::max(std::abs(NSMinY(currentFrame) - NSMinY(frame)),
                              std::abs(NSMaxY(currentFrame) - NSMaxY(frame)));
  double distance = std::max(distanceX, distanceY);
  double duration = std::min(distance / kBoundsAnimationSpeedPixelsPerSecond,
                             kBoundsAnimationMaxDurationSeconds);
  // Detect animation that happens when expansion state is set to MINIMIZED
  // and there is relatively big portion of the panel to hide from view.
  // Initialize animation differently in this case, using fast-pause-slow
  // method, see below for more details.
  if (distanceY > 0 &&
      windowShim_->panel()->expansion_state() == Panel::MINIMIZED) {
    animationStopToShowTitlebarOnly_ = 1.0 -
        (windowShim_->panel()->TitleOnlyHeight() - NSHeight(frame)) / distanceY;
    if (animationStopToShowTitlebarOnly_ > 0.7) {  // Relatively big movement.
      playingMinimizeAnimation_ = YES;
      duration = 1.5;
    }
  }
  [boundsAnimation_ setDuration: PanelManager::AdjustTimeInterval(duration)];
  [boundsAnimation_ setFrameRate:0.0];
  [boundsAnimation_ setAnimationBlockingMode: NSAnimationNonblocking];
  [boundsAnimation_ startAnimation];
}

- (float)animation:(NSAnimation*)animation
  valueForProgress:(NSAnimationProgress)progress {
  return PanelBoundsAnimation::ComputeAnimationValue(
      progress, playingMinimizeAnimation_, animationStopToShowTitlebarOnly_);
}

- (void)cleanupAfterAnimation {
  playingMinimizeAnimation_ = NO;
  if (!windowShim_->panel()->IsMinimized())
    [self enableWebContentsViewAutosizing];
}

- (void)animationDidEnd:(NSAnimation*)animation {
  [self cleanupAfterAnimation];

  // Only invoke this callback from animationDidEnd, since animationDidStop can
  // be called when we interrupt/restart animation which is in progress.
  // We only need this notification when animation indeed finished moving
  // the panel bounds.
  Panel* panel = windowShim_->panel();
  panel->manager()->OnPanelAnimationEnded(panel);
}

- (void)animationDidStop:(NSAnimation*)animation {
  [self cleanupAfterAnimation];
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

- (void)onTitlebarMouseClicked:(int)modifierFlags {
  Panel* panel = windowShim_->panel();
  panel->OnTitlebarClicked((modifierFlags & NSShiftKeyMask) ?
                           panel::APPLY_TO_ALL : panel::NO_MODIFIER);
}

- (void)onTitlebarDoubleClicked:(int)modifierFlags {
  // Double-clicking is only allowed to minimize docked panels.
  Panel* panel = windowShim_->panel();
  if (panel->collection()->type() != PanelCollection::DOCKED ||
      panel->IsMinimized())
    return;
  [self minimizeButtonClicked:modifierFlags];
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
  // We need to activate the controls (in the "WebView"). To do this, get the
  // selected WebContents's RenderWidgetHostView and tell it to activate.
  if (WebContents* contents = windowShim_->panel()->GetWebContents()) {
    if (content::RenderWidgetHostView* rwhv =
        contents->GetRenderWidgetHostView())
      rwhv->SetActive(true);
  }

  windowShim_->panel()->OnActiveStateChanged(true);

  // Make the window user-resizable when it gains the focus.
  [[self window] setStyleMask:
      [[self window] styleMask] | NSResizableWindowMask];
}

- (void)windowDidResignKey:(NSNotification*)notification {
  // If our app is still active and we're still the key window, ignore this
  // message, since it just means that a menu extra (on the "system status bar")
  // was activated; we'll get another |-windowDidResignKey| if we ever really
  // lose key window status.
  if ([NSApp isActive] && ([NSApp keyWindow] == [self window]))
    return;

  [self onWindowDidResignKey];
}

- (void)windowWillStartLiveResize:(NSNotification*)notification {
  // Check if the user-resizing is allowed for the triggering edge/corner.
  // This is an extra safe guard because we are not able to track the mouse
  // movement outside the window and Cocoa could trigger the user-resizing
  // when the mouse moves a bit outside the edge/corner.
  if (![self canResizeByMouseAtCurrentLocation])
    return;
  userResizing_ = YES;
  windowShim_->panel()->OnPanelStartUserResizing();
}

- (void)windowDidEndLiveResize:(NSNotification*)notification {
  if (!userResizing_)
    return;
  userResizing_ = NO;

  Panel* panel = windowShim_->panel();
  panel->OnPanelEndUserResizing();

  gfx::Rect newBounds =
      cocoa_utils::ConvertRectFromCocoaCoordinates([[self window] frame]);
  if (windowShim_->panel()->GetBounds() == newBounds)
    return;
  windowShim_->set_cached_bounds_directly(newBounds);

  panel->IncreaseMaxSize(newBounds.size());
  panel->set_full_size(newBounds.size());

  panel->collection()->RefreshLayout();
}

- (NSSize)windowWillResize:(NSWindow*)sender toSize:(NSSize)newSize {
  // As an extra safe guard, we avoid the user resizing if it is deemed not to
  // be allowed (see comment in windowWillStartLiveResize).
  if ([[self window] inLiveResize] && !userResizing_)
    return [[self window] frame].size;
  return newSize;
}

- (void)windowDidResize:(NSNotification*)notification {
  Panel* panel = windowShim_->panel();
  if (userResizing_) {
    panel->collection()->OnPanelResizedByMouse(
        panel,
        cocoa_utils::ConvertRectFromCocoaCoordinates([[self window] frame]));
  }

  [self updateTrackingArea];

  if (![self isAnimatingBounds] ||
      panel->collection()->type() != PanelCollection::DOCKED)
    return;

  // Remove the web contents view from the view hierarchy when the panel is not
  // taller than the titlebar. Put it back when the panel grows taller than
  // the titlebar. Note that RenderWidgetHostViewMac works for the case that
  // the web contents view does not exist in the view hierarchy (i.e. the tab
  // is not the main one), but it does not work well, like causing occasional
  // crashes (http://crbug.com/265932), if the web contents view is made hidden.
  //
  // This is needed when the docked panels are being animated. When the
  // animation starts, the contents view autosizing is disabled. After the
  // animation ends, the contents view autosizing is reenabled and the frame
  // of contents view is updated. Thus it is likely that the contents view will
  // overlap with the titlebar view when the panel shrinks to be very small.
  // The implementation of the web contents view assumes that it will never
  // overlap with another view in order to paint the web contents view directly.
  content::WebContents* webContents = panel->GetWebContents();
  if (!webContents)
    return;
  NSView* contentView = webContents->GetNativeView();
  if (NSHeight([self contentRectForFrameRect:[[self window] frame]]) <= 0) {
    // No need to retain the view before it is removed from its superview
    // because WebContentsView keeps a reference to this view.
    if ([contentView superview])
      [contentView removeFromSuperview];
  } else {
    if (![contentView superview]) {
      [[[self window] contentView] addSubview:contentView];

      // When the web contents view is put back, we need to tell its render
      // widget host view to accept focus.
      content::RenderWidgetHostView* rwhv =
          webContents->GetRenderWidgetHostView();
      if (rwhv) {
        [[self window] makeFirstResponder:rwhv->GetNativeView()];
        rwhv->SetActive([[self window] isMainWindow]);
      }
    }
  }
}

- (void)activate {
  // Activate the window. -|windowDidBecomeKey:| will be called when
  // window becomes active.
  base::AutoReset<BOOL> pin(&activationRequestedByPanel_, true);
  [BrowserWindowUtils activateWindowForController:self];
}

- (void)deactivate {
  if (![[self window] isMainWindow])
    return;

  [NSApp deactivate];

  // Cocoa does not support deactivating a NSWindow explicitly. To work around
  // this, we call orderOut and orderFront to force the window to lose its key
  // window state.

  // Before doing this, we need to disable screen updates to prevent flickering.
  NSDisableScreenUpdates();

  // If a panel is in stacked mode, the window has a background parent window.
  // We need to detach it from its parent window before applying the ordering
  // change and then put it back because otherwise tha background parent window
  // might show up.
  NSWindow* parentWindow = [[self window] parentWindow];
  if (parentWindow)
    [parentWindow removeChildWindow:[self window]];

  [[self window] orderOut:nil];
  [[self window] orderFront:nil];

  if (parentWindow)
    [parentWindow addChildWindow:[self window] ordered:NSWindowAbove];

  NSEnableScreenUpdates();

  // Though the above workaround causes the window to lose its key window state,
  // it does not trigger the system to call windowDidResignKey.
  [self onWindowDidResignKey];
}

- (void)onWindowDidResignKey {
  // We need to deactivate the controls (in the "WebView"). To do this, get the
  // selected WebContents's RenderWidgetHostView and tell it to deactivate.
  if (WebContents* contents = windowShim_->panel()->GetWebContents()) {
    if (content::RenderWidgetHostView* rwhv =
        contents->GetRenderWidgetHostView())
      rwhv->SetActive(false);
  }

  windowShim_->panel()->OnActiveStateChanged(false);

  // Make the window not user-resizable when it loses the focus. This is to
  // solve the problem that the bottom edge of the active panel does not
  // trigger the user-resizing if this panel stacks with another inactive
  // panel at the bottom.
  [[self window] setStyleMask:
      [[self window] styleMask] & ~NSResizableWindowMask];
}

- (void)preventBecomingKeyWindow:(BOOL)prevent {
  canBecomeKeyWindow_ = !prevent;
}

- (void)fullScreenModeChanged:(bool)isFullScreen {
  [self updateWindowLevel];

  // If the panel is not always on top, its z-order should not be affected if
  // some other window enters fullscreen mode.
  if (!windowShim_->panel()->IsAlwaysOnTop())
    return;

  // The full-screen window is in normal level and changing the panel window
  // to same normal level will not move it below the full-screen window. Thus
  // we need to reorder the panel window.
  if (isFullScreen)
    [[self window] orderOut:nil];
  else
    [[self window] orderFrontRegardless];
}

- (BOOL)canBecomeKeyWindow {
  // Panel can only gain focus if it is expanded. Minimized panels do not
  // participate in Cmd-~ rotation.
  // TODO(dimich): If it will be ever desired to expand/focus the Panel on
  // keyboard navigation or via main menu, the care should be taken to avoid
  // cases when minimized Panel is getting keyboard input, invisibly.
  return canBecomeKeyWindow_;
}

- (int)numPanels {
  return windowShim_->panel()->manager()->num_panels();
}

- (BOOL)activationRequestedByPanel {
  return activationRequestedByPanel_;
}

- (void)updateWindowLevel {
  [self updateWindowLevel:windowShim_->panel()->IsMinimized()];
}

- (void)updateWindowLevel:(BOOL)panelIsMinimized {
  if (![self isWindowLoaded])
    return;
  Panel* panel = windowShim_->panel();
  if (!panel->IsAlwaysOnTop()) {
    [[self window] setLevel:NSNormalWindowLevel];
    return;
  }
  // If we simply use NSStatusWindowLevel (25) for all docked panel windows,
  // IME composition windows for things like CJK languages appear behind panels.
  // Pre 10.7, IME composition windows have a window level of 19, which is
  // lower than the dock at level 20. Since we want panels to appear on top of
  // the dock, it is impossible to enforce an ordering where IME > panel > dock,
  // since IME < dock.
  // On 10.7, IME composition windows and the dock both live at level 20, so we
  // use the same window level for panels. Since newly created windows appear at
  // the top of their window level, panels are typically on top of the dock, and
  // the IME composition window correctly draws over the panel.
  // An autohide dock causes problems though: since it's constantly being
  // revealed, it ends up drawing on top of other windows at the same level.
  // While this is OK for expanded panels, it makes minimized panels impossible
  // to activate. As a result, we still use NSStatusWindowLevel for minimized
  // panels, since it's impossible to compose IME text in them anyway.
  if (panelIsMinimized) {
    [[self window] setLevel:NSStatusWindowLevel];
    return;
  }
  [[self window] setLevel:NSDockWindowLevel];
}

- (void)updateWindowCollectionBehavior {
  if (![self isWindowLoaded])
    return;
  NSWindowCollectionBehavior collectionBehavior =
      NSWindowCollectionBehaviorParticipatesInCycle;
  if (windowShim_->panel()->IsAlwaysOnTop())
    collectionBehavior |= NSWindowCollectionBehaviorCanJoinAllSpaces;
  [[self window] setCollectionBehavior:collectionBehavior];
}

- (void)updateTrackingArea {
  NSView* superview = [[[self window] contentView] superview];

  if (trackingArea_.get())
    [superview removeTrackingArea:trackingArea_.get()];

  trackingArea_.reset(
          [[CrTrackingArea alloc] initWithRect:[superview bounds]
                                       options:NSTrackingInVisibleRect |
                                               NSTrackingMouseMoved |
                                               NSTrackingActiveInKeyWindow
                                         owner:superview
                                      userInfo:nil]);
  [superview addTrackingArea:trackingArea_.get()];
}

- (void)showShadow:(BOOL)show {
  if (![self isWindowLoaded])
    return;
  [[self window] setHasShadow:show];
}

- (void)miniaturize {
  [[self window] miniaturize:nil];
}

- (BOOL)isMiniaturized {
  return [[self window] isMiniaturized];
}

- (BOOL)canResizeByMouseAtCurrentLocation {
  panel::Resizability resizability = windowShim_->panel()->CanResizeByMouse();
  NSRect frame = [[self window] frame];
  NSPoint point = [NSEvent mouseLocation];

  if (point.y < NSMinY(frame) + kWidthOfMouseResizeArea) {
    if (point.x < NSMinX(frame) + kWidthOfMouseResizeArea &&
        (resizability & panel::RESIZABLE_BOTTOM_LEFT) == 0) {
      return NO;
    }
    if (point.x > NSMaxX(frame) - kWidthOfMouseResizeArea &&
        (resizability & panel::RESIZABLE_BOTTOM_RIGHT) == 0) {
      return NO;
    }
    if ((resizability & panel::RESIZABLE_BOTTOM) == 0)
      return NO;
  } else if (point.y > NSMaxY(frame) - kWidthOfMouseResizeArea) {
    if (point.x < NSMinX(frame) + kWidthOfMouseResizeArea &&
        (resizability & panel::RESIZABLE_TOP_LEFT) == 0) {
      return NO;
    }
    if (point.x > NSMaxX(frame) - kWidthOfMouseResizeArea &&
        (resizability & panel::RESIZABLE_TOP_RIGHT) == 0) {
      return NO;
    }
    if ((resizability & panel::RESIZABLE_TOP) == 0)
      return NO;
  } else {
    if (point.x < NSMinX(frame) + kWidthOfMouseResizeArea &&
        (resizability & panel::RESIZABLE_LEFT) == 0) {
      return NO;
    }
    if (point.x > NSMaxX(frame) - kWidthOfMouseResizeArea &&
        (resizability & panel::RESIZABLE_RIGHT) == 0) {
      return NO;
    }
  }
  return YES;
}

// We have custom implementation of these because our titlebar height is custom
// and does not match the standard one.
- (NSRect)frameRectForContentRect:(NSRect)contentRect {
  // contentRect is in contentView coord system. We should add a titlebar on top
  // and then convert to the windows coord system.
  contentRect.size.height += panel::kTitlebarHeight;
  NSRect frameRect = [[[self window] contentView] convertRect:contentRect
                                                       toView:nil];
  return frameRect;
}

- (NSRect)contentRectForFrameRect:(NSRect)frameRect {
  NSRect contentRect = [[[self window] contentView] convertRect:frameRect
                                                       fromView:nil];
  contentRect.size.height -= panel::kTitlebarHeight;
  if (contentRect.size.height < 0)
    contentRect.size.height = 0;
  return contentRect;
}

@end
