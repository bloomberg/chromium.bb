// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_touch_bar.h"

#include <memory>

#include "base/mac/availability.h"
#include "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_member.h"
#include "components/search_engines/util.h"
#include "components/strings/grit/components_strings.h"
#include "components/toolbar/vector_icons.h"
#include "components/url_formatter/url_formatter.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/cocoa/touch_bar_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {


// Touch bar identifiers.
NSString* const kBrowserWindowTouchBarId = @"browser-window";
NSString* const kTabFullscreenTouchBarId = @"tab-fullscreen";

// Touch bar items identifiers.
NSString* const kBackForwardTouchId = @"BACK-FWD";
NSString* const kReloadOrStopTouchId = @"RELOAD-STOP";
NSString* const kHomeTouchId = @"HOME";
NSString* const kSearchTouchId = @"SEARCH";
NSString* const kStarTouchId = @"BOOKMARK";
NSString* const kNewTabTouchId = @"NEW-TAB";
NSString* const kExitFullscreenTouchId = @"EXIT-FULLSCREEN";
NSString* const kFullscreenOriginLabelTouchId = @"FULLSCREEN-ORIGIN-LABEL";

// The button indexes in the back and forward segment control.
const int kBackSegmentIndex = 0;
const int kForwardSegmentIndex = 1;

// Touch bar icon colors values.
const SkColor kTouchBarDefaultIconColor = SK_ColorWHITE;
const SkColor kTouchBarStarActiveColor = gfx::kGoogleBlue500;

// The size of the touch bar icons.
const int kTouchBarIconSize = 16;

// The min width of the search button in the touch bar.
const int kSearchBtnMinWidth = 205;

// Creates an NSImage from the given VectorIcon.
NSImage* CreateNSImageFromIcon(const gfx::VectorIcon& icon,
                               SkColor color = kTouchBarDefaultIconColor) {
  return NSImageFromImageSkiaWithColorSpace(
      gfx::CreateVectorIcon(icon, kTouchBarIconSize, color),
      base::mac::GetSRGBColorSpace());
}

// Creates a NSButton for the touch bar.
NSButton* CreateTouchBarButton(const gfx::VectorIcon& icon,
                               BrowserWindowTouchBar* owner,
                               int command,
                               int tooltip_id,
                               SkColor color = kTouchBarDefaultIconColor) {
  NSButton* button =
      [NSButton buttonWithImage:CreateNSImageFromIcon(icon, color)
                         target:owner
                         action:@selector(executeCommand:)];
  button.tag = command;
  [button setAccessibilityLabel:l10n_util::GetNSString(tooltip_id)];
  return button;
}

ui::TouchBarAction TouchBarActionFromCommand(int command) {
  switch (command) {
    case IDC_BACK:
      return ui::TouchBarAction::BACK;
    case IDC_FORWARD:
      return ui::TouchBarAction::FORWARD;
    case IDC_STOP:
      return ui::TouchBarAction::STOP;
    case IDC_RELOAD:
      return ui::TouchBarAction::RELOAD;
    case IDC_HOME:
      return ui::TouchBarAction::HOME;
    case IDC_FOCUS_LOCATION:
      return ui::TouchBarAction::SEARCH;
    case IDC_BOOKMARK_PAGE:
      return ui::TouchBarAction::STAR;
    case IDC_NEW_TAB:
      return ui::TouchBarAction::NEW_TAB;
    default:
      NOTREACHED();
      return ui::TouchBarAction::TOUCH_BAR_ACTION_COUNT;
  }
}

// A class registered for C++ notifications. This is used to detect changes in
// the home button preferences and update the Touch Bar.
class HomePrefNotificationBridge {
 public:
  explicit HomePrefNotificationBridge(BrowserWindowController* bwc)
      : bwc_(bwc) {}

  ~HomePrefNotificationBridge() {}

  void UpdateTouchBar() { [bwc_ invalidateTouchBar]; }

 private:
  BrowserWindowController* bwc_;  // Weak.

  DISALLOW_COPY_AND_ASSIGN(HomePrefNotificationBridge);
};

}  // namespace

@interface BrowserWindowTouchBar () {
  // Used to execute commands such as navigating back and forward.
  CommandUpdater* commandUpdater_;  // Weak, owned by Browser.

  // The browser associated with the touch bar.
  Browser* browser_;  // Weak.

  BrowserWindowController* bwc_;  // Weak, own us.

  // Used to monitor the optional home button pref.
  BooleanPrefMember showHomeButton_;

  // Used to receive and handle notifications for the home button pref.
  std::unique_ptr<HomePrefNotificationBridge> notificationBridge_;

  // The stop/reload button in the touch bar.
  base::scoped_nsobject<NSButton> reloadStopButton_;

  // The back/forward segmented control in the touch bar.
  base::scoped_nsobject<NSSegmentedControl> backForwardControl_;

  // The starred button in the touch bar.
  base::scoped_nsobject<NSButton> starredButton_;
}

// Creates and returns a touch bar for tab fullscreen mode.
- (NSTouchBar*)createTabFullscreenTouchBar API_AVAILABLE(macos(10.12.2));

// Sets up the back and forward segmented control.
- (void)setupBackForwardControl;

// Methods to update controls on the touch bar. Called when creating the
// touch bar or the page load state has been updated.
- (void)updateReloadStopButton;
- (void)updateBackForwardControl;
- (void)updateStarredButton;

// Creates and returns the search button.
- (NSView*)searchTouchBarView API_AVAILABLE(macos(10.12));
@end

@implementation BrowserWindowTouchBar

@synthesize isPageLoading = isPageLoading_;
@synthesize isStarred = isStarred_;

- (instancetype)initWithBrowser:(Browser*)browser
        browserWindowController:(BrowserWindowController*)bwc {
  if ((self = [self init])) {
    DCHECK(browser);
    commandUpdater_ = browser->command_controller()->command_updater();
    browser_ = browser;
    bwc_ = bwc;

    notificationBridge_.reset(new HomePrefNotificationBridge(bwc_));
    PrefService* prefs = browser->profile()->GetPrefs();
    showHomeButton_.Init(
        prefs::kShowHomeButton, prefs,
        base::Bind(&HomePrefNotificationBridge::UpdateTouchBar,
                   base::Unretained(notificationBridge_.get())));
  }

  return self;
}

- (NSTouchBar*)makeTouchBar {
  if (!base::FeatureList::IsEnabled(features::kBrowserTouchBar))
    return nil;

  // When in tab fullscreen, we should show a touch bar containing only
  // items associated with that mode. Since the toolbar is hidden, only
  // the option to exit fullscreen should show up.
  if ([bwc_ isFullscreenForTabContentOrExtension])
    return [self createTabFullscreenTouchBar];

  base::scoped_nsobject<NSTouchBar> touchBar([[ui::NSTouchBar() alloc] init]);
  [touchBar
      setCustomizationIdentifier:ui::GetTouchBarId(kBrowserWindowTouchBarId)];
  [touchBar setDelegate:self];

  NSMutableArray* customIdentifiers = [NSMutableArray arrayWithCapacity:7];
  NSMutableArray* defaultIdentifiers = [NSMutableArray arrayWithCapacity:6];

  NSArray* touchBarItems = @[
    kBackForwardTouchId, kReloadOrStopTouchId, kHomeTouchId, kSearchTouchId,
    kStarTouchId, kNewTabTouchId
  ];

  for (NSString* item in touchBarItems) {
    NSString* itemIdentifier =
        ui::GetTouchBarItemId(kBrowserWindowTouchBarId, item);
    [customIdentifiers addObject:itemIdentifier];

    // Don't add the home button if it's not shown in the toolbar.
    if (showHomeButton_.GetValue() || ![item isEqualTo:kHomeTouchId])
      [defaultIdentifiers addObject:itemIdentifier];
  }

  [customIdentifiers addObject:NSTouchBarItemIdentifierFlexibleSpace];

  [touchBar setDefaultItemIdentifiers:defaultIdentifiers];
  [touchBar setCustomizationAllowedItemIdentifiers:customIdentifiers];

  return touchBar.autorelease();
}

- (NSTouchBarItem*)touchBar:(NSTouchBar*)touchBar
      makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier
    API_AVAILABLE(macos(10.12.2)) {
  if (!touchBar)
    return nil;

  base::scoped_nsobject<NSCustomTouchBarItem> touchBarItem(
      [[ui::NSCustomTouchBarItem() alloc] initWithIdentifier:identifier]);
  if ([identifier hasSuffix:kBackForwardTouchId]) {
    [self updateBackForwardControl];
    [touchBarItem setView:backForwardControl_.get()];
    [touchBarItem setCustomizationLabel:
                      l10n_util::GetNSString(
                          IDS_TOUCH_BAR_BACK_FORWARD_CUSTOMIZATION_LABEL)];
  } else if ([identifier hasSuffix:kReloadOrStopTouchId]) {
    [self updateReloadStopButton];
    [touchBarItem setView:reloadStopButton_.get()];
    [touchBarItem setCustomizationLabel:
                      l10n_util::GetNSString(
                          IDS_TOUCH_BAR_STOP_RELOAD_CUSTOMIZATION_LABEL)];
  } else if ([identifier hasSuffix:kHomeTouchId]) {
    [touchBarItem setView:CreateTouchBarButton(kNavigateHomeIcon, self,
                                               IDC_HOME, IDS_ACCNAME_HOME)];
    [touchBarItem
        setCustomizationLabel:l10n_util::GetNSString(
                                  IDS_TOUCH_BAR_HOME_CUSTOMIZATION_LABEL)];
  } else if ([identifier hasSuffix:kNewTabTouchId]) {
    [touchBarItem
        setView:CreateTouchBarButton(kNewTabMacTouchbarIcon, self, IDC_NEW_TAB,
                                     IDS_TOOLTIP_NEW_TAB)];
    [touchBarItem
        setCustomizationLabel:l10n_util::GetNSString(
                                  IDS_TOUCH_BAR_NEW_TAB_CUSTOMIZATION_LABEL)];
  } else if ([identifier hasSuffix:kStarTouchId]) {
    [self updateStarredButton];
    [touchBarItem setView:starredButton_.get()];
    [touchBarItem
        setCustomizationLabel:l10n_util::GetNSString(
                                  IDS_TOUCH_BAR_BOOKMARK_CUSTOMIZATION_LABEL)];
  } else if ([identifier hasSuffix:kSearchTouchId]) {
    if (@available(macOS 10.12, *)) {
      [touchBarItem setView:[self searchTouchBarView]];
      [touchBarItem setCustomizationLabel:l10n_util::GetNSString(
                                              IDS_TOUCH_BAR_GOOGLE_SEARCH)];
    } else {
      NOTREACHED();
    }
  } else if ([identifier hasSuffix:kFullscreenOriginLabelTouchId]) {
    content::WebContents* contents =
        browser_->tab_strip_model()->GetActiveWebContents();

    if (!contents)
      return nil;

    // Strip the trailing slash.
    url::Parsed parsed;
    base::string16 displayText = url_formatter::FormatUrl(
        contents->GetLastCommittedURL(),
        url_formatter::kFormatUrlOmitTrailingSlashOnBareHostname,
        net::UnescapeRule::SPACES, &parsed, nullptr, nullptr);

    base::scoped_nsobject<NSMutableAttributedString> attributedString(
        [[NSMutableAttributedString alloc]
            initWithString:base::SysUTF16ToNSString(displayText)]);

    if (parsed.path.is_nonempty()) {
      size_t pathIndex = parsed.path.begin;
      [attributedString
          addAttribute:NSForegroundColorAttributeName
                 value:OmniboxViewMac::BaseTextColor(true)
                 range:NSMakeRange(pathIndex,
                                   [attributedString length] - pathIndex)];
    }

    [touchBarItem
        setView:[NSTextField labelWithAttributedString:attributedString.get()]];
  } else if ([identifier hasSuffix:kExitFullscreenTouchId]) {
    return nil;
  }

  return touchBarItem.autorelease();
}

- (NSTouchBar*)createTabFullscreenTouchBar API_AVAILABLE(macos(10.12.2)) {
  base::scoped_nsobject<NSTouchBar> touchBar([[ui::NSTouchBar() alloc] init]);
  [touchBar setDelegate:self];

  if ([touchBar respondsToSelector:
      @selector(setEscapeKeyReplacementItemIdentifier:)]) {
    NSString* exitIdentifier =
        ui::GetTouchBarItemId(kTabFullscreenTouchBarId, kExitFullscreenTouchId);
    [touchBar setEscapeKeyReplacementItemIdentifier:exitIdentifier];
    [touchBar setDefaultItemIdentifiers:@[ ui::GetTouchBarItemId(
                                            kTabFullscreenTouchBarId,
                                            kFullscreenOriginLabelTouchId) ]];

    base::scoped_nsobject<NSCustomTouchBarItem> touchBarItem(
        [[ui::NSCustomTouchBarItem() alloc] initWithIdentifier:exitIdentifier]);

    [touchBarItem
        setView:[NSButton buttonWithTitle:l10n_util::GetNSString(
                                              IDS_TOUCH_BAR_EXIT_FULLSCREEN)
                                   target:self
                                   action:@selector(exitFullscreenForTab:)]];
    [touchBar
        setTemplateItems:[NSSet setWithObject:touchBarItem.autorelease()]];
  }

  return touchBar.autorelease();
}

- (void)setupBackForwardControl {
  NSMutableArray* images = [NSMutableArray arrayWithArray:@[
    CreateNSImageFromIcon(vector_icons::kBackArrowIcon),
    CreateNSImageFromIcon(vector_icons::kForwardArrowIcon)
  ]];

  // Offset the icons so that it matches the height of the other Touch Bar
  // items.
  const int kIconYOffset = 2;
  for (NSUInteger i = 0; i < [images count]; i++) {
    NSImage* image = [images objectAtIndex:i];
    NSSize size = [image size];
    size.height += kIconYOffset;

    NSImage* offsettedImage = [[[NSImage alloc] initWithSize:size] autorelease];
    [offsettedImage lockFocus];
    [image drawInRect:NSMakeRect(0, 0, size.width, size.height - kIconYOffset)];
    [offsettedImage unlockFocus];
    [images replaceObjectAtIndex:i withObject:offsettedImage];
  }

  NSSegmentedControl* control = [NSSegmentedControl
      segmentedControlWithImages:images
                    trackingMode:NSSegmentSwitchTrackingMomentary
                          target:self
                          action:@selector(backOrForward:)];

  // Use the accessibility protocol to get the children.
  // Use NSAccessibilityUnignoredDescendant to be sure we start with
  // the correct object.
  id segmentElement = NSAccessibilityUnignoredDescendant(control);
  NSArray* segments = [segmentElement
      accessibilityAttributeValue:NSAccessibilityChildrenAttribute];
  NSEnumerator* e = [segments objectEnumerator];
  [[e nextObject]
      accessibilitySetOverrideValue:l10n_util::GetNSString(IDS_ACCNAME_BACK)
                       forAttribute:NSAccessibilityTitleAttribute];
  [[e nextObject]
      accessibilitySetOverrideValue:l10n_util::GetNSString(IDS_ACCNAME_FORWARD)
                       forAttribute:NSAccessibilityTitleAttribute];

  backForwardControl_.reset([control retain]);
}

- (void)updateReloadStopButton {
  const gfx::VectorIcon& icon =
      isPageLoading_ ? kNavigateStopIcon : vector_icons::kReloadIcon;
  int commandId = isPageLoading_ ? IDC_STOP : IDC_RELOAD;
  int tooltipId = isPageLoading_ ? IDS_TOOLTIP_STOP : IDS_TOOLTIP_RELOAD;

  if (!reloadStopButton_) {
    reloadStopButton_.reset(
        [CreateTouchBarButton(icon, self, commandId, tooltipId) retain]);
    return;
  }

  [reloadStopButton_
      setImage:CreateNSImageFromIcon(icon, kTouchBarDefaultIconColor)];
  [reloadStopButton_ setTag:commandId];
  [reloadStopButton_ setAccessibilityLabel:l10n_util::GetNSString(tooltipId)];
}

- (void)updateBackForwardControl {
  if (!backForwardControl_)
    [self setupBackForwardControl];

  if (@available(macOS 10.10, *))
    [backForwardControl_ setSegmentStyle:NSSegmentStyleSeparated];

  [backForwardControl_ setEnabled:commandUpdater_->IsCommandEnabled(IDC_BACK)
                       forSegment:kBackSegmentIndex];
  [backForwardControl_ setEnabled:commandUpdater_->IsCommandEnabled(IDC_FORWARD)
                       forSegment:kForwardSegmentIndex];
}

- (void)updateStarredButton {
  const gfx::VectorIcon& icon =
      isStarred_ ? toolbar::kStarActiveIcon : toolbar::kStarIcon;
  SkColor iconColor =
      isStarred_ ? kTouchBarStarActiveColor : kTouchBarDefaultIconColor;
  int tooltipId = isStarred_ ? IDS_TOOLTIP_STARRED : IDS_TOOLTIP_STAR;
  if (!starredButton_) {
    starredButton_.reset([CreateTouchBarButton(icon, self, IDC_BOOKMARK_PAGE,
                                               tooltipId, iconColor) retain]);
    return;
  }

  [starredButton_ setImage:CreateNSImageFromIcon(icon, iconColor)];
  [starredButton_ setAccessibilityLabel:l10n_util::GetNSString(tooltipId)];
}

- (NSView*)searchTouchBarView {
  TemplateURLService* templateUrlService =
      TemplateURLServiceFactory::GetForProfile(browser_->profile());
  const TemplateURL* defaultProvider =
      templateUrlService->GetDefaultSearchProvider();
  BOOL isGoogle =
      defaultProvider->GetEngineType(templateUrlService->search_terms_data()) ==
      SEARCH_ENGINE_GOOGLE;

  base::string16 title =
      isGoogle ? l10n_util::GetStringUTF16(IDS_TOUCH_BAR_GOOGLE_SEARCH)
               : l10n_util::GetStringFUTF16(IDS_TOUCH_BAR_SEARCH,
                                            defaultProvider->short_name());

  NSImage* image;
  if (isGoogle) {
    image = NSImageFromImageSkiaWithColorSpace(
        gfx::CreateVectorIcon(kGoogleGLogoIcon, kTouchBarIconSize,
                              gfx::kPlaceholderColor),
        base::mac::GetSRGBColorSpace());
  } else {
    image = CreateNSImageFromIcon(vector_icons::kSearchIcon);
  }

  NSButton* searchButton =
      [NSButton buttonWithTitle:base::SysUTF16ToNSString(title)
                          image:image
                         target:self
                         action:@selector(executeCommand:)];
  searchButton.imageHugsTitle = YES;
  searchButton.tag = IDC_FOCUS_LOCATION;
  [searchButton.widthAnchor
      constraintGreaterThanOrEqualToConstant:kSearchBtnMinWidth]
      .active = YES;
  [searchButton
      setContentHuggingPriority:1.0
                 forOrientation:NSLayoutConstraintOrientationHorizontal];
  return searchButton;
}

- (void)backOrForward:(id)sender {
  NSSegmentedControl* control = sender;
  int command =
      [control selectedSegment] == kBackSegmentIndex ? IDC_BACK : IDC_FORWARD;
  LogTouchBarUMA(TouchBarActionFromCommand(command));
  commandUpdater_->ExecuteCommand(command);
}

- (void)exitFullscreenForTab:(id)sender {
  browser_->exclusive_access_manager()
      ->fullscreen_controller()
      ->ExitExclusiveAccessIfNecessary();
}

- (void)executeCommand:(id)sender {
  int command = [sender tag];
  ui::LogTouchBarUMA(TouchBarActionFromCommand(command));
  commandUpdater_->ExecuteCommand(command);
}

- (void)setIsPageLoading:(BOOL)isPageLoading {
  isPageLoading_ = isPageLoading;
  [self updateReloadStopButton];
  [self updateBackForwardControl];
  [self updateStarredButton];
}

@end
