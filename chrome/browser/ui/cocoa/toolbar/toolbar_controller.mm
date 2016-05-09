// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"

#include <algorithm>

#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/command_observer.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sync/sync_global_error_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/app_menu/app_menu_controller.h"
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
#import "chrome/browser/ui/cocoa/toolbar/app_toolbar_button.h"
#import "chrome/browser/ui/cocoa/toolbar/app_toolbar_button_cell.h"
#import "chrome/browser/ui/cocoa/toolbar/back_forward_menu_controller.h"
#import "chrome/browser/ui/cocoa/toolbar/reload_button_cocoa.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_button_cocoa.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_view_cocoa.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_badge_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#import "ui/base/cocoa/menu_controller.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

namespace {

// Duration of the toolbar animation.
const NSTimeInterval kToolBarAnimationDuration = 0.12;

// The height of the location bar in Material Design.
const CGFloat kMaterialDesignLocationBarHeight = 28;

// The padding between the top of the toolbar and the top of the
// location bar.
const CGFloat kMaterialDesignLocationBarPadding = 2;

// The padding between Material Design elements and the edges of the toolbar.
const CGFloat kMaterialDesignElementPadding = 4;

// Toolbar buttons are 24x24 and centered in a 28x28 space, so there is a 2pt-
// wide inset.
const CGFloat kMaterialDesignButtonInset = 2;

// The y-offset of the browser actions container from the location bar.
const CGFloat kMaterialDesignContainerYOffset = 2;

// The minimum width of the location bar in pixels.
const CGFloat kMinimumLocationBarWidth = 100.0;

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
// Height of the location bar. Used for animating the toolbar in and out when
// the location bar is displayed stand-alone for bookmark apps.
+ (CGFloat)locationBarHeight;
// Return the amount of left padding that the app menu should have.
+ (CGFloat)appMenuLeftPadding;
- (void)cleanUp;
- (void)addAccessibilityDescriptions;
- (void)initCommandStatus:(CommandUpdater*)commands;
- (void)prefChanged:(const std::string&)prefName;
- (ToolbarView*)toolbarView;
// Height of the toolbar in pixels when the bookmark bar is closed.
- (CGFloat)baseToolbarHeight;
- (void)toolbarFrameChanged;
- (void)showLocationBarOnly;
- (void)pinLocationBarToLeftOfBrowserActionsContainerAndAnimate:(BOOL)animate;
- (void)maintainMinimumLocationBarWidth;
- (void)adjustBrowserActionsContainerForNewWindow:(NSNotification*)notification;
- (void)browserActionsContainerDragged:(NSNotification*)notification;
- (void)browserActionsVisibilityChanged:(NSNotification*)notification;
- (void)browserActionsContainerWillAnimate:(NSNotification*)notification;
- (void)adjustLocationSizeBy:(CGFloat)dX animate:(BOOL)animate;
- (void)updateAppMenuButtonSeverity:(AppMenuIconPainter::Severity)severity
                            animate:(BOOL)animate;
@end

namespace ToolbarControllerInternal {

// A C++ bridge class that handles listening for updates to commands and
// passing them back to ToolbarController. ToolbarController will create one of
// these bridges, pass them to CommandUpdater::AddCommandObserver, and then wait
// for update notifications, delivered via
// -enabledStateChangedForCommand:enabled:.
class CommandObserverBridge : public CommandObserver {
 public:
  explicit CommandObserverBridge(ToolbarController* observer)
      : observer_(observer) {
    DCHECK(observer_);
  }

 protected:
  // Overridden from CommandObserver
  void EnabledStateChangedForCommand(int command, bool enabled) override {
    [observer_ enabledStateChangedForCommand:command enabled:enabled];
  }

 private:
  ToolbarController* observer_;  // weak, owns me

  DISALLOW_COPY_AND_ASSIGN(CommandObserverBridge);
};

// A class registered for C++ notifications. This is used to detect changes in
// preferences and upgrade available notifications. Bridges the notification
// back to the ToolbarController.
class NotificationBridge : public AppMenuBadgeController::Delegate {
 public:
  explicit NotificationBridge(ToolbarController* controller)
      : controller_(controller),
        badge_controller_([controller browser]->profile(), this) {
  }
  ~NotificationBridge() override {}

  void UpdateBadgeSeverity() {
    badge_controller_.UpdateDelegate();
  }

  void UpdateBadgeSeverity(AppMenuBadgeController::BadgeType type,
                           AppMenuIconPainter::Severity severity,
                           bool animate) override {
    [controller_ updateAppMenuButtonSeverity:severity animate:animate];
  }

  void OnPreferenceChanged(const std::string& pref_name) {
    [controller_ prefChanged:pref_name];
  }

 private:
  ToolbarController* controller_;  // weak, owns us

  AppMenuBadgeController badge_controller_;

  DISALLOW_COPY_AND_ASSIGN(NotificationBridge);
};

}  // namespace ToolbarControllerInternal

@implementation ToolbarController

@synthesize browser = browser_;

+ (CGFloat)locationBarHeight {
  if (!ui::MaterialDesignController::IsModeMaterial()) {
    return 29;
  }

  return kMaterialDesignLocationBarHeight;
}

+ (CGFloat)appMenuLeftPadding {
  if (!ui::MaterialDesignController::IsModeMaterial()) {
    return 3;
  }

  return kMaterialDesignElementPadding;
}

+ (CGFloat)materialDesignButtonInset {
  return kMaterialDesignButtonInset;
}

- (id)initWithCommands:(CommandUpdater*)commands
               profile:(Profile*)profile
               browser:(Browser*)browser
        resizeDelegate:(id<ViewResizer>)resizeDelegate {
  DCHECK(commands && profile);
  if ((self = [super initWithNibName:@"Toolbar"
                              bundle:base::mac::FrameworkBundle()])) {
    commands_ = commands;
    profile_ = profile;
    browser_ = browser;
    hasToolbar_ = YES;
    hasLocationBar_ = YES;

    // Register for notifications about state changes for the toolbar buttons
    commandObserver_.reset(
        new ToolbarControllerInternal::CommandObserverBridge(self));

    commands->AddCommandObserver(IDC_BACK, commandObserver_.get());
    commands->AddCommandObserver(IDC_FORWARD, commandObserver_.get());
    commands->AddCommandObserver(IDC_RELOAD, commandObserver_.get());
    commands->AddCommandObserver(IDC_HOME, commandObserver_.get());
    commands->AddCommandObserver(IDC_BOOKMARK_PAGE, commandObserver_.get());
    // NOTE: Don't remove the command observers. ToolbarController is
    // autoreleased at about the same time as the CommandUpdater (owned by the
    // Browser), so |commands_| may not be valid any more.

    [[self toolbarView] setResizeDelegate:resizeDelegate];

    // Start global error services now so we badge the menu correctly.
    SyncGlobalErrorFactory::GetForProfile(profile);
  }
  return self;
}

// Called after the view is done loading and the outlets have been hooked up.
// Now we can hook up bridges that rely on UI objects such as the location bar
// and button state. -viewDidLoad is the recommended way to do this in 10.10
// SDK. When running on 10.10 or above -awakeFromNib still works but for some
// reason is not guaranteed to be called (http://crbug.com/526276), so implement
// both.
- (void)awakeFromNib {
  [self viewDidLoad];
}

- (void)viewDidLoad {
  // When linking and running on 10.10+, both -awakeFromNib and -viewDidLoad may
  // be called, don't initialize twice.
  if (locationBarView_) {
    DCHECK(base::mac::IsOSYosemiteOrLater());
    return;
  }

  // Make Material Design layout adjustments to the NIB items.
  bool isModeMaterial = ui::MaterialDesignController::IsModeMaterial();
  if (isModeMaterial) {
    ToolbarView* toolbarView = [self toolbarView];
    NSRect toolbarBounds = [toolbarView bounds];
    NSSize toolbarButtonSize = [ToolbarButton toolbarButtonSize];

    // Set the toolbar height.
    NSRect frame = [toolbarView frame];
    frame.size.height = [self baseToolbarHeight];
    [toolbarView setFrame:frame];

    NSRect backButtonFrame = [backButton_ frame];
    backButtonFrame.origin.x =
        kMaterialDesignElementPadding + kMaterialDesignButtonInset;
    backButtonFrame.origin.y = NSMaxY(toolbarBounds) -
        kMaterialDesignElementPadding - toolbarButtonSize.height;
    backButtonFrame.size = toolbarButtonSize;
    [backButton_ setFrame:backButtonFrame];

    NSRect forwardButtonFrame = [forwardButton_ frame];
    forwardButtonFrame.origin.x =
        NSMaxX(backButtonFrame) + 2 * kMaterialDesignButtonInset;
    forwardButtonFrame.origin.y = backButtonFrame.origin.y;
    forwardButtonFrame.size = toolbarButtonSize;
    [forwardButton_ setFrame:forwardButtonFrame];

    NSRect reloadButtonFrame = [reloadButton_ frame];
    reloadButtonFrame.origin.x =
        NSMaxX(forwardButtonFrame) + 2 * kMaterialDesignButtonInset;
    reloadButtonFrame.origin.y = forwardButtonFrame.origin.y;
    reloadButtonFrame.size = toolbarButtonSize;
    [reloadButton_ setFrame:reloadButtonFrame];

    NSRect homeButtonFrame = [homeButton_ frame];
    homeButtonFrame.origin.x =
        NSMaxX(reloadButtonFrame) + 2 * kMaterialDesignButtonInset;
    homeButtonFrame.origin.y = reloadButtonFrame.origin.y;
    homeButtonFrame.size = toolbarButtonSize;
    [homeButton_ setFrame:homeButtonFrame];

    // Replace the app button from the nib with an AppToolbarButton instance for
    // Material Design.
    AppToolbarButton* newMenuButton =
        [[[AppToolbarButton alloc] initWithFrame:[appMenuButton_ frame]]
            autorelease];
    [newMenuButton setAutoresizingMask:[appMenuButton_ autoresizingMask]];
    [[appMenuButton_ superview] addSubview:newMenuButton];
    [appMenuButton_ removeFromSuperview];
    appMenuButton_ = newMenuButton;

    // Adjust the menu button's position.
    NSRect menuButtonFrame = [appMenuButton_ frame];
    CGFloat menuButtonFrameMaxX =
        NSMaxX(toolbarBounds) - [ToolbarController appMenuLeftPadding];
    menuButtonFrame.origin.x =
        menuButtonFrameMaxX - kMaterialDesignButtonInset -
            toolbarButtonSize.width;
    menuButtonFrame.origin.y = homeButtonFrame.origin.y;
    menuButtonFrame.size = toolbarButtonSize;
    [appMenuButton_ setFrame:menuButtonFrame];

    // Adjust the size and location on the location bar to take up the
    // space between the reload and menu buttons.
    NSRect locationBarFrame = [locationBar_ frame];
    locationBarFrame.origin.x = NSMaxX(homeButtonFrame) +
        kMaterialDesignButtonInset;
    locationBarFrame.origin.y = NSMaxY(toolbarBounds) -
        kMaterialDesignLocationBarPadding - kMaterialDesignLocationBarHeight;
    locationBarFrame.size.width =
        menuButtonFrame.origin.x -
            locationBarFrame.origin.x;
    locationBarFrame.size.height = kMaterialDesignLocationBarHeight;
    [locationBar_ setFrame:locationBarFrame];

    // Correctly position the extension buttons' container view.
    NSRect containerFrame = [browserActionsContainerView_ frame];
    containerFrame.size.width += kMaterialDesignButtonInset;
    containerFrame.origin.y =
        locationBarFrame.origin.y + kMaterialDesignContainerYOffset;
    containerFrame.size.height = toolbarButtonSize.height;
    [browserActionsContainerView_ setFrame:containerFrame];
  } else {
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

    [[appMenuButton_ cell] setImageID:IDR_TOOLS
                       forButtonState:image_button_cell::kDefaultState];
    [[appMenuButton_ cell] setImageID:IDR_TOOLS_H
                       forButtonState:image_button_cell::kHoverState];
    [[appMenuButton_ cell] setImageID:IDR_TOOLS_P
                       forButtonState:image_button_cell::kPressedState];

    // Adjust the toolbar height if running on Retina - see the comment in
    // -baseToolbarHeight.
    CGFloat toolbarHeight = [self baseToolbarHeight];
    ToolbarView* toolbarView = [self toolbarView];
    NSRect toolbarFrame = [toolbarView frame];
    if (toolbarFrame.size.height != toolbarHeight) {
      toolbarFrame.size.height = toolbarHeight;
      [toolbarView setFrame:toolbarFrame];
    }
  }

  notificationBridge_.reset(
      new ToolbarControllerInternal::NotificationBridge(self));
  notificationBridge_->UpdateBadgeSeverity();

  [appMenuButton_ setOpenMenuOnClick:YES];

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
  [locationBar_ setFont:[NSFont systemFontOfSize:14]];
  if (!isModeMaterial) {
    [locationBar_ setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
  }

  // Register pref observers for the optional home and page/options buttons
  // and then add them to the toolbar based on those prefs.
  PrefService* prefs = profile_->GetPrefs();
  showHomeButton_.Init(
      prefs::kShowHomeButton, prefs,
      base::Bind(
          &ToolbarControllerInternal::NotificationBridge::OnPreferenceChanged,
          base::Unretained(notificationBridge_.get())));
  [self showOptionalHomeButton];
  [self installAppMenu];

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
  view_id_util::SetID(appMenuButton_, VIEW_ID_APP_MENU);

  [self addAccessibilityDescriptions];
}

- (void)dealloc {
  [self cleanUp];
  [super dealloc];
}

- (void)browserWillBeDestroyed {
  // Clear resize delegate so it doesn't get called during stopAnimation, and
  // stop any in-flight animation.
  [[self toolbarView] setResizeDelegate:nil];
  [[self toolbarView] stopAnimation];

  // Pass this call onto other reference counted objects.
  [backMenuController_ browserWillBeDestroyed];
  [forwardMenuController_ browserWillBeDestroyed];
  [browserActionsController_ browserWillBeDestroyed];
  [appMenuController_ browserWillBeDestroyed];

  [self cleanUp];
}

- (void)cleanUp {
  // Unset ViewIDs of toolbar elements.
  // ViewIDs of |toolbarView|, |reloadButton_|, |locationBar_| and
  // |browserActionsContainerView_| are handled by themselves.
  view_id_util::UnsetID(backButton_);
  view_id_util::UnsetID(forwardButton_);
  view_id_util::UnsetID(homeButton_);
  view_id_util::UnsetID(appMenuButton_);

  // Make sure any code in the base class which assumes [self view] is
  // the "parent" view continues to work.
  hasToolbar_ = YES;
  hasLocationBar_ = YES;

  [[NSNotificationCenter defaultCenter] removeObserver:self];

  if (trackingArea_.get()) {
    [[self view] removeTrackingArea:trackingArea_.get()];
    [trackingArea_.get() clearOwner];
    trackingArea_.reset();
  }

  // Destroy owned objects that hold a weak Browser*.
  locationBarView_.reset();
  browserActionsContainerDelegate_.reset();
  browser_ = nullptr;
}

- (void)addAccessibilityDescriptions {
  // Set accessibility descriptions. http://openradar.appspot.com/7496255
  NSString* description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_BACK);
  [[backButton_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
  NSString* helpTag = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_TOOLTIP_BACK);
  [[backButton_ cell]
      accessibilitySetOverrideValue:helpTag
                       forAttribute:NSAccessibilityHelpAttribute];

  description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_FORWARD);
  [[forwardButton_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
  helpTag = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_TOOLTIP_FORWARD);
  [[forwardButton_ cell]
      accessibilitySetOverrideValue:helpTag
                       forAttribute:NSAccessibilityHelpAttribute];

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
  [[appMenuButton_ cell]
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

- (void)locationBarWasAddedToWindow {
  // Allow the |locationBarView_| to update itself to match the browser window
  // theme.
  locationBarView_->OnAddedToWindow();
}

- (void)focusLocationBar:(BOOL)selectAll {
  if (locationBarView_.get()) {
    locationBarView_->FocusLocation(selectAll ? true : false);
  }
}

// Called when the state for a command changes to |enabled|. Update the
// corresponding UI element.
- (void)enabledStateChangedForCommand:(int)command enabled:(bool)enabled {
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

- (void)zoomChangedForActiveTab:(BOOL)canShowBubble {
  locationBarView_->ZoomChangedForActiveTab(
      canShowBubble && ![appMenuController_ isMenuOpen]);
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

  // Make location bar not editable when in a pop-up or an app window.
  locationBarView_->SetEditable(toolbar);

  // If necessary, resize the location bar and hide the toolbar icons to display
  // the toolbar with only the location bar inside it.
  if (!hasToolbar_ && hasLocationBar_)
    [self showLocationBarOnly];
}

// (Private) Returns the backdrop to the toolbar as a ToolbarView.
- (ToolbarView*)toolbarView{
  return base::mac::ObjCCastStrict<ToolbarView>([self view]);
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
                                   homeButton_, appMenuButton_, locationBar_,
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
  CGFloat moveX = [homeButton_ frame].size.width;
  if (!ui::MaterialDesignController::IsModeMaterial()) {
    moveX -= 1.0;
  }
  if (hide)
    moveX *= -1;  // Reverse the direction of the move.

  [locationBar_ setFrame:[self adjustRect:[locationBar_ frame]
                                 byAmount:moveX]];
  [homeButton_ setHidden:hide];
}

// Install the app menu buttons. Calling this repeatedly is inexpensive so it
// can be done every time the buttons are shown.
- (void)installAppMenu {
  if (appMenuController_.get())
    return;

  appMenuController_.reset(
      [[AppMenuController alloc] initWithBrowser:browser_]);
  [appMenuController_ setUseWithPopUpButtonCell:YES];
  [appMenuButton_ setAttachedMenu:[appMenuController_ menu]];
}

- (void)updateAppMenuButtonSeverity:(AppMenuIconPainter::Severity)severity
                            animate:(BOOL)animate {
  if (!ui::MaterialDesignController::IsModeMaterial()) {
    AppToolbarButtonCell* cell =
        base::mac::ObjCCastStrict<AppToolbarButtonCell>([appMenuButton_ cell]);
    [cell setSeverity:severity shouldAnimate:animate];
    return;
  }
  AppToolbarButton* appMenuButton =
      base::mac::ObjCCastStrict<AppToolbarButton>(appMenuButton_);
  [appMenuButton setSeverity:severity shouldAnimate:animate];
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
  if (![browserActionsContainerView_ isHidden])
    [self pinLocationBarToLeftOfBrowserActionsContainerAndAnimate:NO];
}

- (void)updateVisibility:(BOOL)visible withAnimation:(BOOL)animate {
  CGFloat newHeight = visible ? [ToolbarController locationBarHeight] : 0;

  // Perform the animation, which will cause the BrowserWindowController to
  // resize this view in the browser layout as required.
  if (animate) {
    [[self toolbarView] animateToNewHeight:newHeight
                                  duration:kToolBarAnimationDuration];
  } else {
    [[self toolbarView] setHeight:newHeight];
  }
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
  [self pinLocationBarToLeftOfBrowserActionsContainerAndAnimate:NO];
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
    CGFloat edgeXPos = [appMenuButton_ frame].origin.x;
    leftDistance = edgeXPos - locationBarXPos -
        [ToolbarController appMenuLeftPadding];
  } else {
    leftDistance = NSMinX([browserActionsContainerView_ animationEndFrame]) -
        locationBarXPos;
    // Equalize the distance between the location bar and the first extension
    // button, and the distance between the location bar and home/reload button.
    if (ui::MaterialDesignController::IsModeMaterial()) {
      leftDistance -= kMaterialDesignButtonInset;
    }
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
    if (!ui::MaterialDesignController::IsModeMaterial()) {
      CGFloat elementTopPadding =
          kMaterialDesignElementPadding + kMaterialDesignButtonInset;
      // Pre-Material Design, this value is calculated from the values in
      // Toolbar.xib: the height of the toolbar (35) minus the height of the
      // child elements (29) minus the y-origin of the elements (4).
      elementTopPadding = 2;
      containerFrame.origin.y =
          NSHeight([[self view] frame]) - NSHeight(containerFrame) -
          elementTopPadding;
    } else {
      containerFrame.origin.y =
          [locationBar_ frame].origin.y + kMaterialDesignContainerYOffset;
    }
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

// Hide the back, forward, reload, home, and app menu buttons of the toolbar.
// This allows the location bar to occupy the entire width. There is no way to
// undo this operation, and once it is called, no other programmatic changes
// to the toolbar or location bar width should be made. This message is
// invalid if the toolbar is shown or the location bar is hidden.
- (void)showLocationBarOnly {
  // -showLocationBarOnly is only ever called once, shortly after
  // initialization, so the regular buttons should all be visible.
  DCHECK(!hasToolbar_ && hasLocationBar_);
  DCHECK(![backButton_ isHidden]);

  // Ensure the location bar fills the toolbar.
  NSRect toolbarFrame = [[self view] frame];
  toolbarFrame.size.height = [ToolbarController locationBarHeight];
  [[self view] setFrame:toolbarFrame];

  [locationBar_ setFrame:NSMakeRect(0, 0, NSWidth([[self view] frame]),
                                    [ToolbarController locationBarHeight])];

  [backButton_ setHidden:YES];
  [forwardButton_ setHidden:YES];
  [reloadButton_ setHidden:YES];
  [appMenuButton_ setHidden:YES];
  [homeButton_ setHidden:YES];
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
  NSRect frame = appMenuButton_.frame;
  NSPoint point = NSMakePoint(NSMidX(frame), NSMinY(frame));
  // Inset to account for the whitespace around the hotdogs.
  point.y += app_menu_controller::kAppMenuBubblePointOffsetY;
  return [self.view convertPoint:point toView:nil];
}

- (NSPoint)managePasswordsBubblePoint {
  return locationBarView_->GetManagePasswordsBubblePoint();
}

- (NSPoint)saveCreditCardBubblePoint {
  return locationBarView_->GetSaveCreditCardBubblePoint();
}

- (NSPoint)translateBubblePoint {
  return locationBarView_->GetTranslateBubblePoint();
}

- (CGFloat)baseToolbarHeight {
  // Height of the toolbar in pixels when the bookmark bar is closed.
  const bool kIsModeMaterial = ui::MaterialDesignController::IsModeMaterial();
  const CGFloat kBaseToolbarHeightNormal = kIsModeMaterial ? 37 : 35;

  // Not all lines are drawn at 2x normal height when running on Retina, which
  // causes the toolbar controls to be visually 1pt too high within the toolbar
  // area. It's not possible to adjust the control y-positions by 0.5pt and have
  // them appear 0.5pt lower (they are still drawn at their original locations),
  // so instead shave off 1pt from the bottom of the toolbar. Note that there's
  // an offsetting change in -[BookmarkBarController preferredHeight] to
  // maintain the proper spacing between bookmark icons and toolbar items. See
  // https://crbug.com/326245 .
  const CGFloat kLineWidth = [[self view] cr_lineWidth];
  const BOOL kIsRetina = (kLineWidth < 1);
  BOOL reduceHeight = YES;

  // If Material Design and Retina, no height adjustment is needed.
  if (kIsModeMaterial && kIsRetina) {
    reduceHeight = NO;
  }

  return reduceHeight ? kBaseToolbarHeightNormal - 1
                      : kBaseToolbarHeightNormal;
}

- (CGFloat)desiredHeightForCompression:(CGFloat)compressByHeight {
  // With no toolbar, just ignore the compression.
  if (!hasToolbar_)
    return NSHeight([locationBar_ frame]);

  return [self baseToolbarHeight] - compressByHeight;
}

- (void)setDividerOpacity:(CGFloat)opacity {
  ToolbarView* toolbarView = [self toolbarView];
  [toolbarView setShowsDivider:(opacity > 0 ? YES : NO)];
  [toolbarView setDividerOpacity:opacity];
  [toolbarView setNeedsDisplay:YES];
}

- (BrowserActionsController*)browserActionsController {
  return browserActionsController_.get();
}

- (NSView*)appMenuButton {
  return appMenuButton_;
}

- (AppMenuController*)appMenuController {
  return appMenuController_.get();
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
  GURL url(url_formatter::FixupURL(
      base::SysNSStringToUTF8([urls objectAtIndex:0]), std::string()));

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
