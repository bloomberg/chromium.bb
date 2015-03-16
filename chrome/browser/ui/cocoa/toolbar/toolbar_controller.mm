// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"

#include <algorithm>

#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/memory/singleton.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/background_gradient_view.h"
#include "chrome/browser/ui/cocoa/drag_util.h"
#import "chrome/browser/ui/cocoa/extensions/browser_action_button.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_container_view.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"
#import "chrome/browser/ui/cocoa/gradient_button_cell.h"
#import "chrome/browser/ui/cocoa/image_button_cell.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field_editor.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/menu_button.h"
#import "chrome/browser/ui/cocoa/toolbar/back_forward_menu_controller.h"
#import "chrome/browser/ui/cocoa/toolbar/reload_button_cocoa.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_button_cocoa.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_view_cocoa.h"
#import "chrome/browser/ui/cocoa/toolbar/wrench_toolbar_button_cell.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#import "chrome/browser/ui/cocoa/wrench_menu/wrench_menu_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/wrench_menu_badge_controller.h"
#include "chrome/browser/ui/toolbar/wrench_menu_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/autocomplete_match.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_fixer/url_fixer.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#import "ui/base/cocoa/menu_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

namespace {

// Height of the toolbar in pixels when the bookmark bar is closed.
const CGFloat kBaseToolbarHeightNormal = 35.0;

// The padding above the toolbar elements. This is calculated from the values
// in Toolbar.xib: the height of the toolbar (35) minus the height of the child
// elements (29) minus the y-origin of the elements (4).
const CGFloat kToolbarElementTopPadding = 2.0;

// The minimum width of the location bar in pixels.
const CGFloat kMinimumLocationBarWidth = 100.0;

// The amount of left padding that the wrench menu should have.
const CGFloat kWrenchMenuLeftPadding = 3.0;

class BrowserActionsContainerDelegate :
    public BrowserActionsContainerViewSizeDelegate {
 public:
  BrowserActionsContainerDelegate(
      AutocompleteTextField* location_bar,
      BrowserActionsContainerView* browser_actions_container_view);
  ~BrowserActionsContainerDelegate() override;

 private:
  // BrowserActionsContainerSizeDelegate:
  CGFloat GetMaxAllowedWidth() override;

  AutocompleteTextField* location_bar_;
  BrowserActionsContainerView* browser_actions_container_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionsContainerDelegate);
};

BrowserActionsContainerDelegate::BrowserActionsContainerDelegate(
    AutocompleteTextField* location_bar,
    BrowserActionsContainerView* browser_actions_container_view)
    : location_bar_(location_bar),
      browser_actions_container_(browser_actions_container_view) {
  [browser_actions_container_ setDelegate:this];
}

BrowserActionsContainerDelegate::~BrowserActionsContainerDelegate() {
  [browser_actions_container_ setDelegate:nil];
}

CGFloat BrowserActionsContainerDelegate::GetMaxAllowedWidth() {
  CGFloat location_bar_flex =
      NSWidth([location_bar_ frame]) - kMinimumLocationBarWidth;
  return NSWidth([browser_actions_container_ frame]) + location_bar_flex;
}

}  // namespace

@interface ToolbarController()
@property(assign, nonatomic) Browser* browser;
- (void)addAccessibilityDescriptions;
- (void)initCommandStatus:(CommandUpdater*)commands;
- (void)prefChanged:(const std::string&)prefName;
- (BackgroundGradientView*)backgroundGradientView;
- (void)toolbarFrameChanged;
- (void)pinLocationBarToLeftOfBrowserActionsContainerAndAnimate:(BOOL)animate;
- (void)maintainMinimumLocationBarWidth;
- (void)adjustBrowserActionsContainerForNewWindow:(NSNotification*)notification;
- (void)browserActionsContainerDragged:(NSNotification*)notification;
- (void)browserActionsVisibilityChanged:(NSNotification*)notification;
- (void)browserActionsContainerWillAnimate:(NSNotification*)notification;
- (void)adjustLocationSizeBy:(CGFloat)dX animate:(BOOL)animate;
- (void)updateWrenchButtonSeverity:(WrenchIconPainter::Severity)severity
                           animate:(BOOL)animate;
@end

namespace ToolbarControllerInternal {

// A class registered for C++ notifications. This is used to detect changes in
// preferences and upgrade available notifications. Bridges the notification
// back to the ToolbarController.
class NotificationBridge : public WrenchMenuBadgeController::Delegate {
 public:
  explicit NotificationBridge(ToolbarController* controller)
      : controller_(controller),
        badge_controller_([controller browser]->profile(), this) {
  }
  ~NotificationBridge() override {}

  void UpdateBadgeSeverity() {
    badge_controller_.UpdateDelegate();
  }

  void UpdateBadgeSeverity(WrenchMenuBadgeController::BadgeType type,
                           WrenchIconPainter::Severity severity,
                           bool animate) override {
    [controller_ updateWrenchButtonSeverity:severity animate:animate];
  }

  void OnPreferenceChanged(const std::string& pref_name) {
    [controller_ prefChanged:pref_name];
  }

 private:
  ToolbarController* controller_;  // weak, owns us

  WrenchMenuBadgeController badge_controller_;

  DISALLOW_COPY_AND_ASSIGN(NotificationBridge);
};

}  // namespace ToolbarControllerInternal

@implementation ToolbarController

@synthesize browser = browser_;

- (id)initWithCommands:(CommandUpdater*)commands
               profile:(Profile*)profile
               browser:(Browser*)browser
        resizeDelegate:(id<ViewResizer>)resizeDelegate
          nibFileNamed:(NSString*)nibName {
  DCHECK(commands && profile && [nibName length]);
  if ((self = [super initWithNibName:nibName
                              bundle:base::mac::FrameworkBundle()])) {
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

- (id)initWithCommands:(CommandUpdater*)commands
               profile:(Profile*)profile
               browser:(Browser*)browser
        resizeDelegate:(id<ViewResizer>)resizeDelegate {
  if ((self = [self initWithCommands:commands
                             profile:profile
                             browser:browser
                      resizeDelegate:resizeDelegate
                        nibFileNamed:@"Toolbar"])) {
  }
  return self;
}


- (void)dealloc {
  browserActionsContainerDelegate_.reset();

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
  [[backButton_ cell] setImageID:IDR_BACK
                  forButtonState:image_button_cell::kDefaultState];
  [[backButton_ cell] setImageID:IDR_BACK_H
                  forButtonState:image_button_cell::kHoverState];
  [[backButton_ cell] setImageID:IDR_BACK_P
                  forButtonState:image_button_cell::kPressedState];
  [[backButton_ cell] setImageID:IDR_BACK_D
                  forButtonState:image_button_cell::kDisabledState];

  [[forwardButton_ cell] setImageID:IDR_FORWARD
                     forButtonState:image_button_cell::kDefaultState];
  [[forwardButton_ cell] setImageID:IDR_FORWARD_H
                     forButtonState:image_button_cell::kHoverState];
  [[forwardButton_ cell] setImageID:IDR_FORWARD_P
                     forButtonState:image_button_cell::kPressedState];
  [[forwardButton_ cell] setImageID:IDR_FORWARD_D
                     forButtonState:image_button_cell::kDisabledState];

  [[reloadButton_ cell] setImageID:IDR_RELOAD
                    forButtonState:image_button_cell::kDefaultState];
  [[reloadButton_ cell] setImageID:IDR_RELOAD_H
                    forButtonState:image_button_cell::kHoverState];
  [[reloadButton_ cell] setImageID:IDR_RELOAD_P
                    forButtonState:image_button_cell::kPressedState];

  [[homeButton_ cell] setImageID:IDR_HOME
                  forButtonState:image_button_cell::kDefaultState];
  [[homeButton_ cell] setImageID:IDR_HOME_H
                  forButtonState:image_button_cell::kHoverState];
  [[homeButton_ cell] setImageID:IDR_HOME_P
                  forButtonState:image_button_cell::kPressedState];

  [[wrenchButton_ cell] setImageID:IDR_TOOLS
                    forButtonState:image_button_cell::kDefaultState];
  [[wrenchButton_ cell] setImageID:IDR_TOOLS_H
                    forButtonState:image_button_cell::kHoverState];
  [[wrenchButton_ cell] setImageID:IDR_TOOLS_P
                    forButtonState:image_button_cell::kPressedState];

  notificationBridge_.reset(
      new ToolbarControllerInternal::NotificationBridge(self));
  notificationBridge_->UpdateBadgeSeverity();

  [wrenchButton_ setOpenMenuOnClick:YES];

  [backButton_ setOpenMenuOnRightClick:YES];
  [forwardButton_ setOpenMenuOnRightClick:YES];

  [backButton_ setHandleMiddleClick:YES];
  [forwardButton_ setHandleMiddleClick:YES];
  [reloadButton_ setHandleMiddleClick:YES];
  [homeButton_ setHandleMiddleClick:YES];

  [self initCommandStatus:commands_];
  [reloadButton_ setCommandUpdater:commands_];

  locationBarView_.reset(new LocationBarViewMac(locationBar_, commands_,
                                                profile_, browser_));
  [locationBar_ setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];

  // Register pref observers for the optional home and page/options buttons
  // and then add them to the toolbar based on those prefs.
  PrefService* prefs = profile_->GetPrefs();
  showHomeButton_.Init(
      prefs::kShowHomeButton, prefs,
      base::Bind(
          &ToolbarControllerInternal::NotificationBridge::OnPreferenceChanged,
          base::Unretained(notificationBridge_.get())));
  [self showOptionalHomeButton];
  [self installWrenchMenu];

  [self pinLocationBarToLeftOfBrowserActionsContainerAndAnimate:NO];

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
                                     owner:self
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
  [[hoveredButton_ cell] setIsMouseInside:NO];
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
         respondsToSelector:@selector(setIsMouseInside:)]))
    return targetView;
  return nil;
}

- (void)mouseMoved:(NSEvent*)theEvent {
  NSButton* targetView = [self hoverButtonForEvent:theEvent];
  if (hoveredButton_ != targetView) {
    [[hoveredButton_ cell] setIsMouseInside:NO];
    [[targetView cell] setIsMouseInside:YES];
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
  if (locationBarView_.get()) {
    locationBarView_->FocusLocation(selectAll ? true : false);
  }
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

- (void)updateToolbarWithContents:(WebContents*)tab {
  locationBarView_->Update(tab);

  [locationBar_ updateMouseTracking];

  if (browserActionsController_.get()) {
    [browserActionsController_ update];
  }

  BOOL needReloadMenu = chrome::IsDebuggerAttachedToCurrentTab(browser_);
  [reloadButton_ setMenuEnabled:needReloadMenu];
}

- (void)resetTabState:(WebContents*)tab {
  locationBarView_->ResetTabState(tab);
}

- (void)setStarredState:(BOOL)isStarred {
  locationBarView_->SetStarred(isStarred);
}

- (void)setTranslateIconLit:(BOOL)on {
  locationBarView_->SetTranslateIconLit(on);
}

- (void)setOverflowedToolbarActionWantsToRun:(BOOL)overflowedActionWantsToRun {
  WrenchToolbarButtonCell* cell =
      base::mac::ObjCCastStrict<WrenchToolbarButtonCell>([wrenchButton_ cell]);
  [cell setOverflowedToolbarActionWantsToRun:overflowedActionWantsToRun];
}

- (void)zoomChangedForActiveTab:(BOOL)canShowBubble {
  locationBarView_->ZoomChangedForActiveTab(
      canShowBubble && ![wrenchMenuController_ isMenuOpen]);
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
    if (base::mac::IsOSSnowLeopard()) {
      // Manually transferring the drawsBackground and backgroundColor
      // properties is necessary to ensure anti-aliased text on 10.6.
      [autocompleteTextFieldEditor_
          setDrawsBackground:[locationBar_ drawsBackground]];
      [autocompleteTextFieldEditor_
          setBackgroundColor:[locationBar_ backgroundColor]];
    }
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
  if (wrenchMenuController_.get())
    return;

  wrenchMenuController_.reset(
      [[WrenchMenuController alloc] initWithBrowser:browser_]);
  [wrenchMenuController_ setUseWithPopUpButtonCell:YES];
  [wrenchButton_ setAttachedMenu:[wrenchMenuController_ menu]];
}

- (void)updateWrenchButtonSeverity:(WrenchIconPainter::Severity)severity
                           animate:(BOOL)animate {
  WrenchToolbarButtonCell* cell =
      base::mac::ObjCCastStrict<WrenchToolbarButtonCell>([wrenchButton_ cell]);
  [cell setSeverity:severity shouldAnimate:animate];
}

- (void)prefChanged:(const std::string&)prefName {
  if (prefName == prefs::kShowHomeButton) {
    [self showOptionalHomeButton];
  }
}

- (void)createBrowserActionButtons {
  if (!browserActionsController_.get()) {
    browserActionsContainerDelegate_.reset(
        new BrowserActionsContainerDelegate(locationBar_,
                                            browserActionsContainerView_));
    browserActionsController_.reset([[BrowserActionsController alloc]
            initWithBrowser:browser_
              containerView:browserActionsContainerView_
             mainController:nil]);
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(browserActionsContainerDragged:)
               name:kBrowserActionGrippyDraggingNotification
             object:browserActionsContainerView_];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(browserActionsVisibilityChanged:)
               name:kBrowserActionVisibilityChangedNotification
             object:browserActionsController_];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(browserActionsContainerWillAnimate:)
               name:kBrowserActionsContainerWillAnimate
             object:browserActionsContainerView_];
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
  [self adjustLocationSizeBy:
      [browserActionsContainerView_ resizeDeltaX] animate:NO];
}

- (void)browserActionsVisibilityChanged:(NSNotification*)notification {
  [self pinLocationBarToLeftOfBrowserActionsContainerAndAnimate:NO];
}

- (void)browserActionsContainerWillAnimate:(NSNotification*)notification {
  [self pinLocationBarToLeftOfBrowserActionsContainerAndAnimate:YES];
}

- (void)pinLocationBarToLeftOfBrowserActionsContainerAndAnimate:(BOOL)animate {
  CGFloat locationBarXPos = NSMaxX([locationBar_ frame]);
  CGFloat leftDistance = 0.0;

  if ([browserActionsContainerView_ isHidden]) {
    CGFloat edgeXPos = [wrenchButton_ frame].origin.x;
    leftDistance = edgeXPos - locationBarXPos - kWrenchMenuLeftPadding;
  } else {
    leftDistance = NSMinX([browserActionsContainerView_ animationEndFrame]) -
        locationBarXPos;
  }
  if (leftDistance != 0.0)
    [self adjustLocationSizeBy:leftDistance animate:animate];
  else
    [locationBar_ stopAnimation];
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

  if ([browserActionsContainerView_ isAnimating]) {
    // If the browser actions container is animating, we need to stop it first,
    // because the frame it's animating for could be incorrect with the new
    // bounds (if, for instance, the bookmark bar was added).
    // This will advance to the end of the animation, so we also need to adjust
    // it afterwards.
    [browserActionsContainerView_ stopAnimation];
    NSRect containerFrame = [browserActionsContainerView_ frame];
    containerFrame.origin.y =
        NSHeight([[self view] frame]) - NSHeight(containerFrame) -
        kToolbarElementTopPadding;
    [browserActionsContainerView_ setFrame:containerFrame];
    [self pinLocationBarToLeftOfBrowserActionsContainerAndAnimate:NO];
  }

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
    CGFloat savedContainerWidth =
        [browserActionsController_ preferredSize].width();
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

  [locationBar_ stopAnimation];

  if (animate)
    [locationBar_ animateToFrame:locationFrame];
  else
    [locationBar_ setFrame:locationFrame];
}

- (NSPoint)bookmarkBubblePoint {
  if (locationBarView_->IsStarEnabled())
    return locationBarView_->GetBookmarkBubblePoint();

  // Grab bottom middle of hotdogs.
  NSRect frame = wrenchButton_.frame;
  NSPoint point = NSMakePoint(NSMidX(frame), NSMinY(frame));
  // Inset to account for the whitespace around the hotdogs.
  point.y += wrench_menu_controller::kWrenchBubblePointOffsetY;
  return [self.view convertPoint:point toView:nil];
}

- (NSPoint)managePasswordsBubblePoint {
  return locationBarView_->GetManagePasswordsBubblePoint();
}

- (NSPoint)translateBubblePoint {
  return locationBarView_->GetTranslateBubblePoint();
}

- (CGFloat)desiredHeightForCompression:(CGFloat)compressByHeight {
  // With no toolbar, just ignore the compression.
  if (!hasToolbar_)
    return NSHeight([locationBar_ frame]);

  return kBaseToolbarHeightNormal - compressByHeight;
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

  [view setNeedsDisplay:YES];
}

- (BrowserActionsController*)browserActionsController {
  return browserActionsController_.get();
}

- (NSView*)wrenchButton {
  return wrenchButton_;
}

- (WrenchMenuController*)wrenchMenuController {
  return wrenchMenuController_.get();
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
  GURL url(url_fixer::FixupURL(base::SysNSStringToUTF8([urls objectAtIndex:0]),
                               std::string()));

  if (url.SchemeIs(url::kJavaScriptScheme)) {
    browser_->window()->GetLocationBar()->GetOmniboxView()->SetUserText(
          OmniboxView::StripJavascriptSchemas(base::UTF8ToUTF16(url.spec())));
  }
  OpenURLParams params(
      url, Referrer(), CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false);
  browser_->tab_strip_model()->GetActiveWebContents()->OpenURL(params);
}

// (URLDropTargetController protocol)
- (void)dropText:(NSString*)text inView:(NSView*)view at:(NSPoint)point {
  // TODO(viettrungluu): This code is more or less copied from the code in
  // |TabStripController|. I'll refactor this soon to make it common and expand
  // its capabilities (e.g., allow text DnD).

  // If the input is plain text, classify the input and make the URL.
  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(browser_->profile())->Classify(
      base::SysNSStringToUTF16(text), false, false,
      metrics::OmniboxEventProto::BLANK, &match, NULL);
  GURL url(match.destination_url);

  OpenURLParams params(
      url, Referrer(), CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false);
  browser_->tab_strip_model()->GetActiveWebContents()->OpenURL(params);
}

// (URLDropTargetController protocol)
- (void)indicateDropURLsInView:(NSView*)view at:(NSPoint)point {
  // Do nothing.
}

// (URLDropTargetController protocol)
- (void)hideDropURLsIndicatorInView:(NSView*)view {
  // Do nothing.
}

// (URLDropTargetController protocol)
- (BOOL)isUnsupportedDropData:(id<NSDraggingInfo>)info {
  return drag_util::IsUnsupportedDropData(profile_, info);
}

@end
