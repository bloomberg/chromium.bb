// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"

#include <algorithm>

#include "app/mac/nsimage_cache.h"
#include "base/mac/mac_util.h"
#include "base/memory/singleton.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/accelerators_cocoa.h"
#import "chrome/browser/ui/cocoa/background_gradient_view.h"
#import "chrome/browser/ui/cocoa/encoding_menu_controller_delegate_mac.h"
#import "chrome/browser/ui/cocoa/extensions/browser_action_button.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_container_view.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"
#import "chrome/browser/ui/cocoa/gradient_button_cell.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field_editor.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/menu_button.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#import "chrome/browser/ui/cocoa/toolbar/back_forward_menu_controller.h"
#import "chrome/browser/ui/cocoa/toolbar/reload_button.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_button.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_view.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#import "chrome/browser/ui/cocoa/wrench_menu/wrench_menu_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/toolbar/wrench_menu_model.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/accelerator_cocoa.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image.h"
#include "ui/gfx/rect.h"

namespace {

// Names of images in the bundle for buttons.
NSString* const kBackButtonImageName = @"back_Template.pdf";
NSString* const kForwardButtonImageName = @"forward_Template.pdf";
NSString* const kReloadButtonReloadImageName = @"reload_Template.pdf";
NSString* const kReloadButtonStopImageName = @"stop_Template.pdf";
NSString* const kHomeButtonImageName = @"home_Template.pdf";
NSString* const kWrenchButtonImageName = @"tools_Template.pdf";

// Height of the toolbar in pixels when the bookmark bar is closed.
const CGFloat kBaseToolbarHeight = 35.0;

// The minimum width of the location bar in pixels.
const CGFloat kMinimumLocationBarWidth = 100.0;

// The duration of any animation that occurs within the toolbar in seconds.
const CGFloat kAnimationDuration = 0.2;

// The amount of left padding that the wrench menu should have.
const CGFloat kWrenchMenuLeftPadding = 3.0;

}  // namespace

@interface ToolbarController(Private)
- (void)addAccessibilityDescriptions;
- (void)initCommandStatus:(CommandUpdater*)commands;
- (void)prefChanged:(std::string*)prefName;
- (BackgroundGradientView*)backgroundGradientView;
- (void)toolbarFrameChanged;
- (void)pinLocationBarToLeftOfBrowserActionsContainerAndAnimate:(BOOL)animate;
- (void)maintainMinimumLocationBarWidth;
- (void)adjustBrowserActionsContainerForNewWindow:(NSNotification*)notification;
- (void)browserActionsContainerDragged:(NSNotification*)notification;
- (void)browserActionsContainerDragFinished:(NSNotification*)notification;
- (void)browserActionsVisibilityChanged:(NSNotification*)notification;
- (void)adjustLocationSizeBy:(CGFloat)dX animate:(BOOL)animate;
- (void)badgeWrenchMenuIfNeeded;
@end

namespace ToolbarControllerInternal {

// A C++ delegate that handles the accelerators in the wrench menu.
class WrenchAcceleratorDelegate : public ui::AcceleratorProvider {
 public:
  virtual bool GetAcceleratorForCommandId(int command_id,
      ui::Accelerator* accelerator_generic) {
    // Downcast so that when the copy constructor is invoked below, the key
    // string gets copied, too.
    ui::AcceleratorCocoa* out_accelerator =
        static_cast<ui::AcceleratorCocoa*>(accelerator_generic);
    AcceleratorsCocoa* keymap = AcceleratorsCocoa::GetInstance();
    const ui::AcceleratorCocoa* accelerator =
        keymap->GetAcceleratorForCommand(command_id);
    if (accelerator) {
      *out_accelerator = *accelerator;
      return true;
    }
    return false;
  }
};

// A class registered for C++ notifications. This is used to detect changes in
// preferences and upgrade available notifications. Bridges the notification
// back to the ToolbarController.
class NotificationBridge : public NotificationObserver {
 public:
  explicit NotificationBridge(ToolbarController* controller)
      : controller_(controller) {
    registrar_.Add(this, NotificationType::UPGRADE_RECOMMENDED,
                   NotificationService::AllSources());
  }

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::PREF_CHANGED:
        [controller_ prefChanged:Details<std::string>(details).ptr()];
        break;
      case NotificationType::UPGRADE_RECOMMENDED:
        [controller_ badgeWrenchMenuIfNeeded];
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  ToolbarController* controller_;  // weak, owns us

  NotificationRegistrar registrar_;
};

}  // namespace ToolbarControllerInternal

@implementation ToolbarController

- (id)initWithModel:(ToolbarModel*)model
           commands:(CommandUpdater*)commands
            profile:(Profile*)profile
            browser:(Browser*)browser
     resizeDelegate:(id<ViewResizer>)resizeDelegate
       nibFileNamed:(NSString*)nibName {
  DCHECK(model && commands && profile && [nibName length]);
  if ((self = [super initWithNibName:nibName
                              bundle:base::mac::MainAppBundle()])) {
    toolbarModel_ = model;
    commands_ = commands;
    profile_ = profile;
    browser_ = browser;
    resizeDelegate_ = resizeDelegate;
    hasToolbar_ = YES;
    hasLocationBar_ = YES;

    // Register for notifications about state changes for the toolbar buttons
    commandObserver_.reset(new CommandObserverBridge(self, commands));
    commandObserver_->ObserveCommand(IDC_BACK);
    commandObserver_->ObserveCommand(IDC_FORWARD);
    commandObserver_->ObserveCommand(IDC_RELOAD);
    commandObserver_->ObserveCommand(IDC_HOME);
    commandObserver_->ObserveCommand(IDC_BOOKMARK_PAGE);
  }
  return self;
}

- (id)initWithModel:(ToolbarModel*)model
           commands:(CommandUpdater*)commands
            profile:(Profile*)profile
            browser:(Browser*)browser
     resizeDelegate:(id<ViewResizer>)resizeDelegate {
  if ((self = [self initWithModel:model
                         commands:commands
                          profile:profile
                          browser:browser
                   resizeDelegate:resizeDelegate
                     nibFileNamed:@"Toolbar"])) {
  }
  return self;
}


- (void)dealloc {
  // Unset ViewIDs of toolbar elements.
  // ViewIDs of |toolbarView|, |reloadButton_|, |locationBar_| and
  // |browserActionsContainerView_| are handled by themselves.
  view_id_util::UnsetID(backButton_);
  view_id_util::UnsetID(forwardButton_);
  view_id_util::UnsetID(homeButton_);
  view_id_util::UnsetID(wrenchButton_);

  // Make sure any code in the base class which assumes [self view] is
  // the "parent" view continues to work.
  hasToolbar_ = YES;
  hasLocationBar_ = YES;

  [[NSNotificationCenter defaultCenter] removeObserver:self];

  if (trackingArea_.get())
    [[self view] removeTrackingArea:trackingArea_.get()];
  [super dealloc];
}

// Called after the view is done loading and the outlets have been hooked up.
// Now we can hook up bridges that rely on UI objects such as the location
// bar and button state.
- (void)awakeFromNib {
  // A bug in AppKit (<rdar://7298597>, <http://openradar.me/7298597>) causes
  // images loaded directly from nibs in a framework to not get their "template"
  // flags set properly. Thus, despite the images being set on the buttons in
  // the xib, we must set them in code.
  [backButton_ setImage:app::mac::GetCachedImageWithName(kBackButtonImageName)];
  [forwardButton_ setImage:
      app::mac::GetCachedImageWithName(kForwardButtonImageName)];
  [reloadButton_ setImage:
      app::mac::GetCachedImageWithName(kReloadButtonReloadImageName)];
  [homeButton_ setImage:
      app::mac::GetCachedImageWithName(kHomeButtonImageName)];
  [wrenchButton_ setImage:
      app::mac::GetCachedImageWithName(kWrenchButtonImageName)];
  [self badgeWrenchMenuIfNeeded];

  [wrenchButton_ setOpenMenuOnClick:YES];

  [backButton_ setShowsBorderOnlyWhileMouseInside:YES];
  [forwardButton_ setShowsBorderOnlyWhileMouseInside:YES];
  [reloadButton_ setShowsBorderOnlyWhileMouseInside:YES];
  [homeButton_ setShowsBorderOnlyWhileMouseInside:YES];
  [wrenchButton_ setShowsBorderOnlyWhileMouseInside:YES];

  [backButton_ setHandleMiddleClick:YES];
  [forwardButton_ setHandleMiddleClick:YES];
  [reloadButton_ setHandleMiddleClick:YES];
  [homeButton_ setHandleMiddleClick:YES];

  [self initCommandStatus:commands_];
  locationBarView_.reset(new LocationBarViewMac(locationBar_,
                                                commands_, toolbarModel_,
                                                profile_, browser_));
  [locationBar_ setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
  // Register pref observers for the optional home and page/options buttons
  // and then add them to the toolbar based on those prefs.
  notificationBridge_.reset(
      new ToolbarControllerInternal::NotificationBridge(self));
  PrefService* prefs = profile_->GetPrefs();
  showHomeButton_.Init(prefs::kShowHomeButton, prefs,
                       notificationBridge_.get());
  [self showOptionalHomeButton];
  [self installWrenchMenu];

  // Create the controllers for the back/forward menus.
  backMenuController_.reset([[BackForwardMenuController alloc]
          initWithBrowser:browser_
                modelType:BACK_FORWARD_MENU_TYPE_BACK
                   button:backButton_]);
  forwardMenuController_.reset([[BackForwardMenuController alloc]
          initWithBrowser:browser_
                modelType:BACK_FORWARD_MENU_TYPE_FORWARD
                   button:forwardButton_]);

  // For a popup window, the toolbar is really just a location bar
  // (see override for [ToolbarController view], below).  When going
  // fullscreen, we remove the toolbar controller's view from the view
  // hierarchy.  Calling [locationBar_ removeFromSuperview] when going
  // fullscreen causes it to get released, making us unhappy
  // (http://crbug.com/18551).  We avoid the problem by incrementing
  // the retain count of the location bar; use of the scoped object
  // helps us remember to release it.
  locationBarRetainer_.reset([locationBar_ retain]);
  trackingArea_.reset(
      [[CrTrackingArea alloc] initWithRect:NSZeroRect // Ignored
                                   options:NSTrackingMouseMoved |
                                           NSTrackingInVisibleRect |
                                           NSTrackingMouseEnteredAndExited |
                                           NSTrackingActiveAlways
                              proxiedOwner:self
                                  userInfo:nil]);
  NSView* toolbarView = [self view];
  [toolbarView addTrackingArea:trackingArea_.get()];

  // If the user has any Browser Actions installed, the container view for them
  // may have to be resized depending on the width of the toolbar frame.
  [toolbarView setPostsFrameChangedNotifications:YES];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(toolbarFrameChanged)
             name:NSViewFrameDidChangeNotification
           object:toolbarView];

  // Set ViewIDs for toolbar elements which don't have their dedicated class.
  // ViewIDs of |toolbarView|, |reloadButton_|, |locationBar_| and
  // |browserActionsContainerView_| are handled by themselves.
  view_id_util::SetID(backButton_, VIEW_ID_BACK_BUTTON);
  view_id_util::SetID(forwardButton_, VIEW_ID_FORWARD_BUTTON);
  view_id_util::SetID(homeButton_, VIEW_ID_HOME_BUTTON);
  view_id_util::SetID(wrenchButton_, VIEW_ID_APP_MENU);

  [self addAccessibilityDescriptions];
}

- (void)addAccessibilityDescriptions {
  // Set accessibility descriptions. http://openradar.appspot.com/7496255
  NSString* description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_BACK);
  [[backButton_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
  description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_FORWARD);
  [[forwardButton_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
  description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_RELOAD);
  [[reloadButton_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
  description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_HOME);
  [[homeButton_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
  description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_LOCATION);
  [[locationBar_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
  description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_APP);
  [[wrenchButton_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
}

- (void)mouseExited:(NSEvent*)theEvent {
  [[hoveredButton_ cell] setMouseInside:NO animate:YES];
  [hoveredButton_ release];
  hoveredButton_ = nil;
}

- (NSButton*)hoverButtonForEvent:(NSEvent*)theEvent {
  NSButton* targetView = (NSButton*)[[self view]
                                     hitTest:[theEvent locationInWindow]];

  // Only interpret the view as a hoverButton_ if it's both button and has a
  // button cell that cares.  GradientButtonCell derived cells care.
  if (([targetView isKindOfClass:[NSButton class]]) &&
      ([[targetView cell]
         respondsToSelector:@selector(setMouseInside:animate:)]))
    return targetView;
  return nil;
}

- (void)mouseMoved:(NSEvent*)theEvent {
  NSButton* targetView = [self hoverButtonForEvent:theEvent];
  if (hoveredButton_ != targetView) {
    [[hoveredButton_ cell] setMouseInside:NO animate:YES];
    [[targetView cell] setMouseInside:YES animate:YES];
    [hoveredButton_ release];
    hoveredButton_ = [targetView retain];
  }
}

- (void)mouseEntered:(NSEvent*)event {
  [self mouseMoved:event];
}

- (LocationBarViewMac*)locationBarBridge {
  return locationBarView_.get();
}

- (void)focusLocationBar:(BOOL)selectAll {
  if (locationBarView_.get())
    locationBarView_->FocusLocation(selectAll ? true : false);
}

// Called when the state for a command changes to |enabled|. Update the
// corresponding UI element.
- (void)enabledStateChangedForCommand:(NSInteger)command enabled:(BOOL)enabled {
  NSButton* button = nil;
  switch (command) {
    case IDC_BACK:
      button = backButton_;
      break;
    case IDC_FORWARD:
      button = forwardButton_;
      break;
    case IDC_HOME:
      button = homeButton_;
      break;
  }
  [button setEnabled:enabled];
}

// Init the enabled state of the buttons on the toolbar to match the state in
// the controller.
- (void)initCommandStatus:(CommandUpdater*)commands {
  [backButton_ setEnabled:commands->IsCommandEnabled(IDC_BACK) ? YES : NO];
  [forwardButton_
      setEnabled:commands->IsCommandEnabled(IDC_FORWARD) ? YES : NO];
  [reloadButton_ setEnabled:YES];
  [homeButton_ setEnabled:commands->IsCommandEnabled(IDC_HOME) ? YES : NO];
}

- (void)updateToolbarWithContents:(TabContents*)tab
               shouldRestoreState:(BOOL)shouldRestore {
  locationBarView_->Update(tab, shouldRestore ? true : false);

  [locationBar_ updateCursorAndToolTipRects];

  if (browserActionsController_.get()) {
    [browserActionsController_ update];
  }
}

- (void)setStarredState:(BOOL)isStarred {
  locationBarView_->SetStarred(isStarred ? true : false);
}

- (void)setIsLoading:(BOOL)isLoading force:(BOOL)force {
  [reloadButton_ setIsLoading:isLoading force:force];
}

- (void)setHasToolbar:(BOOL)toolbar hasLocationBar:(BOOL)locBar {
  [self view];  // Force nib loading.

  hasToolbar_ = toolbar;

  // If there's a toolbar, there must be a location bar.
  DCHECK((toolbar && locBar) || !toolbar);
  hasLocationBar_ = toolbar ? YES : locBar;

  // Decide whether to hide/show based on whether there's a location bar.
  [[self view] setHidden:!hasLocationBar_];

  // Make location bar not editable when in a pop-up.
  locationBarView_->SetEditable(toolbar);
}

- (NSView*)view {
  if (hasToolbar_)
    return [super view];
  return locationBar_;
}

// (Private) Returns the backdrop to the toolbar.
- (BackgroundGradientView*)backgroundGradientView {
  // We really do mean |[super view]|; see our override of |-view|.
  DCHECK([[super view] isKindOfClass:[BackgroundGradientView class]]);
  return (BackgroundGradientView*)[super view];
}

- (id)customFieldEditorForObject:(id)obj {
  if (obj == locationBar_) {
    // Lazilly construct Field editor, Cocoa UI code always runs on the
    // same thread, so there shoudn't be a race condition here.
    if (autocompleteTextFieldEditor_.get() == nil) {
      autocompleteTextFieldEditor_.reset(
          [[AutocompleteTextFieldEditor alloc] init]);
    }

    // This needs to be called every time, otherwise notifications
    // aren't sent correctly.
    DCHECK(autocompleteTextFieldEditor_.get());
    [autocompleteTextFieldEditor_.get() setFieldEditor:YES];
    return autocompleteTextFieldEditor_.get();
  }
  return nil;
}

// Returns an array of views in the order of the outlets above.
- (NSArray*)toolbarViews {
  return [NSArray arrayWithObjects:backButton_, forwardButton_, reloadButton_,
             homeButton_, wrenchButton_, locationBar_,
             browserActionsContainerView_, nil];
}

// Moves |rect| to the right by |delta|, keeping the right side fixed by
// shrinking the width to compensate. Passing a negative value for |deltaX|
// moves to the left and increases the width.
- (NSRect)adjustRect:(NSRect)rect byAmount:(CGFloat)deltaX {
  NSRect frame = NSOffsetRect(rect, deltaX, 0);
  frame.size.width -= deltaX;
  return frame;
}

// Show or hide the home button based on the pref.
- (void)showOptionalHomeButton {
  // Ignore this message if only showing the URL bar.
  if (!hasToolbar_)
    return;
  BOOL hide = showHomeButton_.GetValue() ? NO : YES;
  if (hide == [homeButton_ isHidden])
    return;  // Nothing to do, view state matches pref state.

  // Always shift the text field by the width of the home button minus one pixel
  // since the frame edges of each button are right on top of each other. When
  // hiding the button, reverse the direction of the movement (to the left).
  CGFloat moveX = [homeButton_ frame].size.width - 1.0;
  if (hide)
    moveX *= -1;  // Reverse the direction of the move.

  [locationBar_ setFrame:[self adjustRect:[locationBar_ frame]
                                 byAmount:moveX]];
  [homeButton_ setHidden:hide];
}

// Install the menu wrench buttons. Calling this repeatedly is inexpensive so it
// can be done every time the buttons are shown.
- (void)installWrenchMenu {
  if (wrenchMenuModel_.get())
    return;
  acceleratorDelegate_.reset(
      new ToolbarControllerInternal::WrenchAcceleratorDelegate());

  wrenchMenuModel_.reset(new WrenchMenuModel(
      acceleratorDelegate_.get(), browser_));
  [wrenchMenuController_ setModel:wrenchMenuModel_.get()];
  [wrenchMenuController_ setUseWithPopUpButtonCell:YES];
  [wrenchButton_ setAttachedMenu:[wrenchMenuController_ menu]];
}

- (WrenchMenuController*)wrenchMenuController {
  return wrenchMenuController_;
}

- (void)badgeWrenchMenuIfNeeded {

  int badgeResource = 0;
  if (UpgradeDetector::GetInstance()->notify_upgrade()) {
    badgeResource = IDR_UPDATE_BADGE;
  } else {
    // No badge - clear the badge if one is already set.
    if ([[wrenchButton_ cell] overlayImage])
      [[wrenchButton_ cell] setOverlayImage:nil];
    return;
  }

  NSImage* badge =
      ResourceBundle::GetSharedInstance().GetNativeImageNamed(badgeResource);
  NSImage* wrenchImage =
      app::mac::GetCachedImageWithName(kWrenchButtonImageName);
  NSSize wrenchImageSize = [wrenchImage size];
  NSSize badgeSize = [badge size];

  scoped_nsobject<NSImage> overlayImage(
      [[NSImage alloc] initWithSize:wrenchImageSize]);

  // Draw badge in the upper right corner of the button.
  NSPoint overlayPosition =
      NSMakePoint(wrenchImageSize.width - badgeSize.width,
                  wrenchImageSize.height - badgeSize.height);

  [overlayImage lockFocus];
  [badge drawAtPoint:overlayPosition
            fromRect:NSZeroRect
           operation:NSCompositeSourceOver
            fraction:1.0];
  [overlayImage unlockFocus];

  [[wrenchButton_ cell] setOverlayImage:overlayImage];
}

- (void)prefChanged:(std::string*)prefName {
  if (!prefName) return;
  if (*prefName == prefs::kShowHomeButton) {
    [self showOptionalHomeButton];
  }
}

- (void)createBrowserActionButtons {
  if (!browserActionsController_.get()) {
    browserActionsController_.reset([[BrowserActionsController alloc]
            initWithBrowser:browser_
              containerView:browserActionsContainerView_]);
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(browserActionsContainerDragged:)
               name:kBrowserActionGrippyDraggingNotification
             object:browserActionsController_];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(browserActionsContainerDragFinished:)
               name:kBrowserActionGrippyDragFinishedNotification
             object:browserActionsController_];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(browserActionsVisibilityChanged:)
               name:kBrowserActionVisibilityChangedNotification
             object:browserActionsController_];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(adjustBrowserActionsContainerForNewWindow:)
               name:NSWindowDidBecomeKeyNotification
             object:[[self view] window]];
  }
  CGFloat containerWidth = [browserActionsContainerView_ isHidden] ? 0.0 :
      NSWidth([browserActionsContainerView_ frame]);
  if (containerWidth > 0.0)
    [self adjustLocationSizeBy:(containerWidth * -1) animate:NO];
}

- (void)adjustBrowserActionsContainerForNewWindow:
    (NSNotification*)notification {
  [self toolbarFrameChanged];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSWindowDidBecomeKeyNotification
              object:[[self view] window]];
}

- (void)browserActionsContainerDragged:(NSNotification*)notification {
  CGFloat locationBarWidth = NSWidth([locationBar_ frame]);
  locationBarAtMinSize_ = locationBarWidth <= kMinimumLocationBarWidth;
  [browserActionsContainerView_ setCanDragLeft:!locationBarAtMinSize_];
  [browserActionsContainerView_ setGrippyPinned:locationBarAtMinSize_];
  [self adjustLocationSizeBy:
      [browserActionsContainerView_ resizeDeltaX] animate:NO];
}

- (void)browserActionsContainerDragFinished:(NSNotification*)notification {
  [browserActionsController_ resizeContainerAndAnimate:YES];
  [self pinLocationBarToLeftOfBrowserActionsContainerAndAnimate:YES];
}

- (void)browserActionsVisibilityChanged:(NSNotification*)notification {
  [self pinLocationBarToLeftOfBrowserActionsContainerAndAnimate:NO];
}

- (void)pinLocationBarToLeftOfBrowserActionsContainerAndAnimate:(BOOL)animate {
  CGFloat locationBarXPos = NSMaxX([locationBar_ frame]);
  CGFloat leftDistance;

  if ([browserActionsContainerView_ isHidden]) {
    CGFloat edgeXPos = [wrenchButton_ frame].origin.x;
    leftDistance = edgeXPos - locationBarXPos - kWrenchMenuLeftPadding;
  } else {
    NSRect containerFrame = animate ?
        [browserActionsContainerView_ animationEndFrame] :
        [browserActionsContainerView_ frame];

    leftDistance = containerFrame.origin.x - locationBarXPos;
  }
  if (leftDistance != 0.0)
    [self adjustLocationSizeBy:leftDistance animate:animate];
}

- (void)maintainMinimumLocationBarWidth {
  CGFloat locationBarWidth = NSWidth([locationBar_ frame]);
  locationBarAtMinSize_ = locationBarWidth <= kMinimumLocationBarWidth;
  if (locationBarAtMinSize_) {
    CGFloat dX = kMinimumLocationBarWidth - locationBarWidth;
    [self adjustLocationSizeBy:dX animate:NO];
  }
}

- (void)toolbarFrameChanged {
  // Do nothing if the frame changes but no Browser Action Controller is
  // present.
  if (!browserActionsController_.get())
    return;

  [self maintainMinimumLocationBarWidth];

  if (locationBarAtMinSize_) {
    // Once the grippy is pinned, leave it until it is explicity un-pinned.
    [browserActionsContainerView_ setGrippyPinned:YES];
    NSRect containerFrame = [browserActionsContainerView_ frame];
    // Determine how much the container needs to move in case it's overlapping
    // with the location bar.
    CGFloat dX = NSMaxX([locationBar_ frame]) - containerFrame.origin.x;
    containerFrame = NSOffsetRect(containerFrame, dX, 0);
    containerFrame.size.width -= dX;
    [browserActionsContainerView_ setFrame:containerFrame];
  } else if (!locationBarAtMinSize_ &&
      [browserActionsContainerView_ grippyPinned]) {
    // Expand out the container until it hits the saved size, then unpin the
    // grippy.
    // Add 0.1 pixel so that it doesn't hit the minimum width codepath above.
    CGFloat dX = NSWidth([locationBar_ frame]) -
        (kMinimumLocationBarWidth + 0.1);
    NSRect containerFrame = [browserActionsContainerView_ frame];
    containerFrame = NSOffsetRect(containerFrame, -dX, 0);
    containerFrame.size.width += dX;
    CGFloat savedContainerWidth = [browserActionsController_ savedWidth];
    if (NSWidth(containerFrame) >= savedContainerWidth) {
      containerFrame = NSOffsetRect(containerFrame,
          NSWidth(containerFrame) - savedContainerWidth, 0);
      containerFrame.size.width = savedContainerWidth;
      [browserActionsContainerView_ setGrippyPinned:NO];
    }
    [browserActionsContainerView_ setFrame:containerFrame];
    [self pinLocationBarToLeftOfBrowserActionsContainerAndAnimate:NO];
  }
}

- (void)adjustLocationSizeBy:(CGFloat)dX animate:(BOOL)animate {
  // Ensure that the location bar is in its proper place.
  NSRect locationFrame = [locationBar_ frame];
  locationFrame.size.width += dX;

  if (!animate) {
    [locationBar_ setFrame:locationFrame];
    return;
  }

  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext] setDuration:kAnimationDuration];
  [[locationBar_ animator] setFrame:locationFrame];
  [NSAnimationContext endGrouping];
}

- (NSPoint)bookmarkBubblePoint {
  return locationBarView_->GetBookmarkBubblePoint();
}

- (CGFloat)desiredHeightForCompression:(CGFloat)compressByHeight {
  // With no toolbar, just ignore the compression.
  return hasToolbar_ ? kBaseToolbarHeight - compressByHeight :
                       NSHeight([locationBar_ frame]);
}

- (void)setDividerOpacity:(CGFloat)opacity {
  BackgroundGradientView* view = [self backgroundGradientView];
  [view setShowsDivider:(opacity > 0 ? YES : NO)];

  // We may not have a toolbar view (e.g., popup windows only have a location
  // bar).
  if ([view isKindOfClass:[ToolbarView class]]) {
    ToolbarView* toolbarView = (ToolbarView*)view;
    [toolbarView setDividerOpacity:opacity];
  }
}

- (BrowserActionsController*)browserActionsController {
  return browserActionsController_.get();
}

// (URLDropTargetController protocol)
- (void)dropURLs:(NSArray*)urls inView:(NSView*)view at:(NSPoint)point {
  // TODO(viettrungluu): This code is more or less copied from the code in
  // |TabStripController|. I'll refactor this soon to make it common and expand
  // its capabilities (e.g., allow text DnD).
  if ([urls count] < 1) {
    NOTREACHED();
    return;
  }

  // TODO(viettrungluu): dropping multiple URLs?
  if ([urls count] > 1)
    NOTIMPLEMENTED();

  // Get the first URL and fix it up.
  GURL url(URLFixerUpper::FixupURL(
      base::SysNSStringToUTF8([urls objectAtIndex:0]), std::string()));

  browser_->GetSelectedTabContents()->OpenURL(url, GURL(), CURRENT_TAB,
                                              PageTransition::TYPED);
}

// (URLDropTargetController protocol)
- (void)dropText:(NSString*)text inView:(NSView*)view at:(NSPoint)point {
  // TODO(viettrungluu): This code is more or less copied from the code in
  // |TabStripController|. I'll refactor this soon to make it common and expand
  // its capabilities (e.g., allow text DnD).

  // If the input is plain text, classify the input and make the URL.
  AutocompleteMatch match;
  browser_->profile()->GetAutocompleteClassifier()->Classify(
      base::SysNSStringToUTF16(text), string16(), false, &match, NULL);
  GURL url(match.destination_url);

  browser_->GetSelectedTabContents()->OpenURL(url, GURL(), CURRENT_TAB,
                                              PageTransition::TYPED);
}

// (URLDropTargetController protocol)
- (void)indicateDropURLsInView:(NSView*)view at:(NSPoint)point {
  // Do nothing.
}

// (URLDropTargetController protocol)
- (void)hideDropURLsIndicatorInView:(NSView*)view {
  // Do nothing.
}

@end
