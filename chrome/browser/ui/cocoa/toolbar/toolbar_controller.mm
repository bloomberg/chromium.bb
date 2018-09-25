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
#include "chrome/browser/recovery/recovery_install_global_error_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/background_gradient_view.h"
#include "chrome/browser/ui/cocoa/drag_util.h"
#import "chrome/browser/ui/cocoa/image_button_cell.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
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
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/web_contents.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#import "ui/base/cocoa/menu_controller.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

namespace {

// Duration of the toolbar animation.
const NSTimeInterval kToolBarAnimationDuration = 0.12;

// The height of the location bar.
const CGFloat kLocationBarHeight = 28;

// The padding between the top of the toolbar and the top of the
// location bar.
const CGFloat kLocationBarPadding = 2;

// The padding between elements and the edges of the toolbar.
const CGFloat kElementPadding = 4;

// Toolbar buttons are 24x24 and centered in a 28x28 space, so there is a 2pt-
// wide inset.
const CGFloat kButtonInset = 2;

}  // namespace

@interface ToolbarController()
@property(assign, nonatomic) Browser* browser;
// Height of the location bar. Used for animating the toolbar in and out when
// the location bar is displayed stand-alone for bookmark apps.
+ (CGFloat)locationBarHeight;
// Return the amount of horizontal padding that the app menu should have on
// each side.
+ (CGFloat)appMenuPadding;
- (void)cleanUp;
- (void)addAccessibilityDescriptions;
- (void)initCommandStatus:(CommandUpdater*)commands;
- (void)prefChanged:(const std::string&)prefName;
// Height of the toolbar in pixels when the bookmark bar is closed.
- (CGFloat)baseToolbarHeight;
- (void)toolbarFrameChanged;
- (void)maintainMinimumLocationBarWidth;
- (void)adjustLocationSizeBy:(CGFloat)dX animate:(BOOL)animate;
- (void)updateAppMenuButtonSeverity:(AppMenuIconController::Severity)severity
                           iconType:(AppMenuIconController::IconType)iconType
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
class NotificationBridge : public AppMenuIconController::Delegate {
 public:
  explicit NotificationBridge(ToolbarController* controller)
      : controller_(controller),
        app_menu_icon_controller_([controller browser]->profile(), this) {}
  ~NotificationBridge() override {}

  void UpdateSeverity() { app_menu_icon_controller_.UpdateDelegate(); }

  void UpdateSeverity(AppMenuIconController::IconType type,
                      AppMenuIconController::Severity severity,
                      bool animate) override {
    [controller_ updateAppMenuButtonSeverity:severity
                                    iconType:type
                                     animate:animate];
  }

  void OnPreferenceChanged(const std::string& pref_name) {
    [controller_ prefChanged:pref_name];
  }

 private:
  ToolbarController* controller_;  // weak, owns us

  AppMenuIconController app_menu_icon_controller_;

  DISALLOW_COPY_AND_ASSIGN(NotificationBridge);
};

}  // namespace ToolbarControllerInternal

@implementation ToolbarController

@synthesize browser = browser_;

+ (CGFloat)locationBarHeight {
  return kLocationBarHeight;
}

+ (CGFloat)appMenuPadding {
  return kElementPadding;
}

+ (CGFloat)materialDesignButtonInset {
  return kButtonInset;
}

- (id)initWithCommands:(CommandUpdater*)commands
               profile:(Profile*)profile
               browser:(Browser*)browser {
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

    // Start global error services now so we badge the menu correctly.
    RecoveryInstallGlobalErrorFactory::GetForProfile(profile);
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
  [self viewDidLoadImpl];
}

- (void)viewDidLoad {
  [self viewDidLoadImpl];
}

- (void)viewDidLoadImpl {
  // When linking and running on 10.10+, both -awakeFromNib and -viewDidLoad may
  // be called, don't initialize twice.
  if (locationBarView_) {
    DCHECK(base::mac::IsAtLeastOS10_10());
    return;
  }

  BOOL isRTL = cocoa_l10n_util::ShouldDoExperimentalRTLLayout();
  NSAutoresizingMaskOptions leadingButtonMask =
      isRTL ? NSViewMinXMargin | NSViewMinYMargin
            : NSViewMaxXMargin | NSViewMinYMargin;
  NSAutoresizingMaskOptions trailingButtonMask =
      isRTL ? NSViewMaxXMargin | NSViewMinYMargin
            : NSViewMinXMargin | NSViewMinYMargin;

  // Make Material Design layout adjustments to the NIB items.
  ToolbarViewCocoa* toolbarView = [self toolbarView];
  NSRect toolbarBounds = [toolbarView bounds];
  NSSize toolbarButtonSize = [ToolbarButtonCocoa toolbarButtonSize];

  // Set the toolbar height.
  NSRect frame = [toolbarView frame];
  frame.size.height = [self baseToolbarHeight];
  [toolbarView setFrame:frame];

  NSArray* leadingButtons =
      @[ backButton_, forwardButton_, reloadButton_, homeButton_ ];
  const CGFloat xStart = kElementPadding + kButtonInset;
  const CGFloat xOffset = toolbarButtonSize.width + kButtonInset * 2;
  const CGFloat yPosition =
      NSMaxY(toolbarBounds) - kElementPadding - toolbarButtonSize.height;
  for (NSUInteger i = 0; i < [leadingButtons count]; i++) {
    NSButton* button = leadingButtons[i];
    NSRect buttonFrame = [button frame];
    buttonFrame.size = toolbarButtonSize;
    buttonFrame.origin.y = yPosition;
    const CGFloat xPosition = xStart + i * xOffset;
    buttonFrame.origin.x =
        isRTL ? NSWidth(frame) - toolbarButtonSize.width - xPosition
              : xPosition;
    [button setFrame:buttonFrame];
    [button setAutoresizingMask:leadingButtonMask];
  }

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
  if (isRTL) {
    menuButtonFrame.origin.x =
        [ToolbarController appMenuPadding] + kButtonInset;
  } else {
    CGFloat menuButtonFrameMaxX =
        NSMaxX(toolbarBounds) - [ToolbarController appMenuPadding];
    menuButtonFrame.origin.x =
        menuButtonFrameMaxX - kButtonInset - toolbarButtonSize.width;
  }
  menuButtonFrame.origin.y = yPosition;
  menuButtonFrame.size = toolbarButtonSize;
  [appMenuButton_ setFrame:menuButtonFrame];
  [appMenuButton_ setAutoresizingMask:trailingButtonMask];

  // Adjust the size and location on the location bar to take up the
  // space between the reload and menu buttons.
  NSRect locationBarFrame = [locationBar_ frame];
  locationBarFrame.origin.x = isRTL
                                  ? NSMaxX(menuButtonFrame) + kButtonInset
                                  : NSMaxX([homeButton_ frame]) + kButtonInset;
  if (![homeButton_ isHidden] && !isRTL) {
    // Ensure proper spacing between the home button and location bar
    locationBarFrame.origin.x += kElementPadding;
  }
  locationBarFrame.origin.y =
      NSMaxY(toolbarBounds) - kLocationBarPadding - kLocationBarHeight;
  CGFloat rightEdge = 0;
  if (isRTL) {
    rightEdge = NSMinX([homeButton_ frame]) - kButtonInset;
    if (![homeButton_ isHidden])
      rightEdge -= kElementPadding;
  } else {
    rightEdge = NSMinX(menuButtonFrame);
  }
  locationBarFrame.size.width = rightEdge - NSMinX(locationBarFrame);

  locationBarFrame.size.height = kLocationBarHeight;
  [locationBar_ setFrame:locationBarFrame];

  notificationBridge_.reset(
      new ToolbarControllerInternal::NotificationBridge(self));
  notificationBridge_->UpdateSeverity();

  [appMenuButton_ setOpenMenuOnClick:YES];

  [backButton_ setOpenMenuOnRightClick:YES];
  [forwardButton_ setOpenMenuOnRightClick:YES];

  [backButton_ setHandleMiddleClick:YES];
  [forwardButton_ setHandleMiddleClick:YES];
  [reloadButton_ setHandleMiddleClick:YES];
  [homeButton_ setHandleMiddleClick:YES];

  [self initCommandStatus:commands_];
  [reloadButton_ setCommandUpdater:commands_];

  locationBarView_.reset(new LocationBarViewMac(commands_, profile_, browser_));

  // Register pref observers for the optional home and page/options buttons
  // and then add them to the toolbar based on those prefs.
  PrefService* prefs = profile_->GetPrefs();
  showHomeButton_.Init(
      prefs::kShowHomeButton, prefs,
      base::Bind(
          &ToolbarControllerInternal::NotificationBridge::OnPreferenceChanged,
          base::Unretained(notificationBridge_.get())));
  [self showOptionalHomeButton];

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
  NSView* parentView = [self view];
  [parentView addTrackingArea:trackingArea_.get()];

  // If the user has any Browser Actions installed, the container view for them
  // may have to be resized depending on the width of the toolbar frame.
  [parentView setPostsFrameChangedNotifications:YES];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(toolbarFrameChanged)
             name:NSViewFrameDidChangeNotification
           object:parentView];

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
  browser_ = nullptr;
}

- (void)addAccessibilityDescriptions {
  // Set accessibility descriptions. http://openradar.appspot.com/7496255
  NSString* description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_BACK);
  [[backButton_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
  NSString* helpTag = l10n_util::GetNSStringWithFixup(IDS_ACCDESCRIPTION_BACK);
  [[backButton_ cell]
      accessibilitySetOverrideValue:helpTag
                       forAttribute:NSAccessibilityHelpAttribute];

  description = l10n_util::GetNSStringWithFixup(IDS_ACCNAME_FORWARD);
  [[forwardButton_ cell]
      accessibilitySetOverrideValue:description
                       forAttribute:NSAccessibilityDescriptionAttribute];
  helpTag = l10n_util::GetNSStringWithFixup(IDS_ACCDESCRIPTION_FORWARD);
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

- (ToolbarViewCocoa*)toolbarView {
  return base::mac::ObjCCastStrict<ToolbarViewCocoa>([self view]);
}

- (LocationBarViewMac*)locationBarBridge {
  return locationBarView_.get();
}

- (void)locationBarWasAddedToWindow {
}

- (BOOL)locationBarHasFocus {
  return NO;
}

- (void)focusLocationBar:(BOOL)selectAll {
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
  BOOL needReloadMenu = chrome::IsDebuggerAttachedToCurrentTab(browser_);
  [reloadButton_ setMenuEnabled:needReloadMenu];
}

- (void)resetTabState:(WebContents*)tab {
}

- (void)setStarredState:(BOOL)isStarred {
}

- (void)setTranslateIconLit:(BOOL)on {
}

- (void)zoomChangedForActiveTab:(BOOL)canShowBubble {
}

- (void)setIsLoading:(BOOL)isLoading force:(BOOL)force {
  [reloadButton_ setIsLoading:isLoading force:force];
}

- (void)setHasToolbar:(BOOL)toolbar hasLocationBar:(BOOL)locBar {
  [self view];  // Force nib loading.

  hasToolbar_ = toolbar;

  hasLocationBar_ = NO;

  // Decide whether to hide/show based on whether there's a location bar.
  [[self view] setHidden:!toolbar];
}

- (id)customFieldEditorForObject:(id)obj {
  return nil;
}

// Returns an array of views, ordered leading to trailing.
- (NSArray*)toolbarViews {
  return @[
    backButton_, forwardButton_, reloadButton_, homeButton_, locationBar_,
    browserActionsContainerView_, appMenuButton_
  ];
}

// Show or hide the home button based on the pref.
- (void)showOptionalHomeButton {
  // Ignore this message if only showing the URL bar.
  if (!hasToolbar_)
    return;
  BOOL hide = showHomeButton_.GetValue() ? NO : YES;
  if (hide == [homeButton_ isHidden])
    return;  // Nothing to do, view state matches pref state.

  [homeButton_ setHidden:hide];
}

- (void)updateAppMenuButtonSeverity:(AppMenuIconController::Severity)severity
                           iconType:(AppMenuIconController::IconType)iconType
                            animate:(BOOL)animate {
  AppToolbarButton* appMenuButton =
      base::mac::ObjCCastStrict<AppToolbarButton>(appMenuButton_);
  [appMenuButton setSeverity:severity iconType:iconType shouldAnimate:animate];
}

- (void)prefChanged:(const std::string&)prefName {
  if (prefName == prefs::kShowHomeButton) {
    [self showOptionalHomeButton];
  }
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

- (void)maintainMinimumLocationBarWidth {
}

- (void)toolbarFrameChanged {
}

- (void)adjustLocationSizeBy:(CGFloat)dX animate:(BOOL)animate {
}

- (NSPoint)bookmarkBubblePoint {
  return [self appMenuBubblePoint];
}

- (NSPoint)appMenuBubblePoint {
  NSRect frame = appMenuButton_.frame;
  NSPoint point = NSMakePoint(NSMaxX(frame), NSMinY(frame));
  return [self.view convertPoint:point toView:nil];
}

- (CGFloat)baseToolbarHeight {
  // Height of the toolbar in pixels when the bookmark bar is closed.
  const CGFloat kBaseToolbarHeightNormal = 37;
  return kBaseToolbarHeightNormal;
}

- (CGFloat)desiredHeightForCompression:(CGFloat)compressByHeight {
  // With no toolbar, just ignore the compression.
  if (!hasToolbar_)
    return NSHeight([locationBar_ frame]);

  return [self baseToolbarHeight] - compressByHeight;
}

- (void)setDividerOpacity:(CGFloat)opacity {
  ToolbarViewCocoa* toolbarView = [self toolbarView];
  [toolbarView setShowsDivider:(opacity > 0 ? YES : NO)];
  [toolbarView setDividerOpacity:opacity];
  [toolbarView setNeedsDisplay:YES];
}

- (NSView*)appMenuButton {
  return appMenuButton_;
}

- (BOOL)isLocationBarFocused {
  return NO;
}

// (URLDropTargetController protocol)
- (void)dropURLs:(NSArray*)urls inView:(NSView*)view at:(NSPoint)point {
  // TODO(viettrungluu): This code is more or less copied from the code in
  // |TabStripControllerCocoa|. I'll refactor this soon to make it common and
  // expand its capabilities (e.g., allow text DnD).
  if ([urls count] < 1) {
    NOTREACHED();
    return;
  }

  for (NSUInteger index = 0; index < [urls count]; index++) {
    // Refactor this code.
    // https://crbug.com/665261.
    GURL url = url_formatter::FixupURL(
        base::SysNSStringToUTF8([urls objectAtIndex:index]), std::string());

    // If the URL isn't valid, don't bother.
    if (!url.is_valid())
      continue;

    // Security: Sanitize text to prevent self-XSS.
    if (url.SchemeIs(url::kJavaScriptScheme))
      continue;

    WindowOpenDisposition disposition;
    if (index == 0)
      disposition = WindowOpenDisposition::CURRENT_TAB;
    else
      disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

    OpenURLParams params(url, Referrer(), disposition,
                         ui::PAGE_TRANSITION_TYPED, false);
    browser_->tab_strip_model()->GetActiveWebContents()->OpenURL(params);
  }
}

// (URLDropTargetController protocol)
- (void)dropText:(NSString*)text inView:(NSView*)view at:(NSPoint)point {
  // TODO(viettrungluu): This code is more or less copied from the code in
  // |TabStripControllerCocoa|. I'll refactor this soon to make it common and
  // expand its capabilities (e.g., allow text DnD).

  // If the input is plain text, classify the input and make the URL.
  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(browser_->profile())->Classify(
      base::SysNSStringToUTF16(text), false, false,
      metrics::OmniboxEventProto::BLANK, &match, NULL);
  GURL url(match.destination_url);

  // Security: Block JavaScript to prevent self-XSS.
  if (url.SchemeIs(url::kJavaScriptScheme))
    return;

  OpenURLParams params(url, Referrer(), WindowOpenDisposition::CURRENT_TAB,
                       ui::PAGE_TRANSITION_TYPED, false);
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
