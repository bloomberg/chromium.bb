// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_touch_bar.h"

#include <memory>

#include "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_member.h"
#include "components/search_engines/util.h"
#include "components/toolbar/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/vector_icons/vector_icons.h"

namespace {

// The touch bar actions that are being recorded in a histogram. These values
// should not be re-ordered or removed.
enum TouchBarAction {
  BACK = 0,
  FORWARD,
  STOP,
  RELOAD,
  HOME,
  SEARCH,
  STAR,
  NEW_TAB,
  TOUCH_BAR_ACTION_COUNT
};

// The touch bar's identifier.
const NSTouchBarCustomizationIdentifier kBrowserWindowTouchBarId =
    @"BrowserWindowTouchBarId";

// Touch bar items identifiers.
const NSTouchBarItemIdentifier kBackForwardTouchId = @"BackForwardTouchId";
const NSTouchBarItemIdentifier kReloadOrStopTouchId = @"ReloadOrStopTouchId";
const NSTouchBarItemIdentifier kHomeTouchId = @"HomeTouchId";
const NSTouchBarItemIdentifier kSearchTouchId = @"SearchTouchId";
const NSTouchBarItemIdentifier kStarTouchId = @"StarTouchId";
const NSTouchBarItemIdentifier kNewTabTouchId = @"NewTabTouchId";

// The button indexes in the back and forward segment control.
const int kBackSegmentIndex = 0;
const int kForwardSegmentIndex = 1;

// Touch bar icon colors values.
const SkColor kTouchBarDefaultIconColor = SK_ColorWHITE;
const SkColor kTouchBarStarActiveColor = gfx::kGoogleBlue500;

// The size of the touch bar icons.
const int kTouchBarIconSize = 16;

// The width of the search button in the touch bar.
const int kSearchBtnWidthWithHomeBtn = 205;
const int kSearchBtnWidthWithoutHomeBtn = 280;

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
                               SkColor color = kTouchBarDefaultIconColor) {
  NSButton* button =
      [NSButton buttonWithImage:CreateNSImageFromIcon(icon, color)
                         target:owner
                         action:@selector(executeCommand:)];
  button.tag = command;
  return button;
}

TouchBarAction TouchBarActionFromCommand(int command) {
  switch (command) {
    case IDC_BACK:
      return TouchBarAction::BACK;
    case IDC_FORWARD:
      return TouchBarAction::FORWARD;
    case IDC_STOP:
      return TouchBarAction::STOP;
    case IDC_RELOAD:
      return TouchBarAction::RELOAD;
    case IDC_HOME:
      return TouchBarAction::HOME;
    case IDC_FOCUS_LOCATION:
      return TouchBarAction::SEARCH;
    case IDC_BOOKMARK_PAGE:
      return TouchBarAction::STAR;
    case IDC_NEW_TAB:
      return TouchBarAction::NEW_TAB;
    default:
      NOTREACHED();
      return TouchBarAction::TOUCH_BAR_ACTION_COUNT;
  }
}

// Logs the sample's UMA metrics into the DefaultTouchBar.Metrics histogram.
void LogTouchBarUMA(int command) {
  UMA_HISTOGRAM_ENUMERATION("TouchBar.Default.Metrics",
                            TouchBarActionFromCommand(command),
                            TOUCH_BAR_ACTION_COUNT);
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
}

// Creates and return the back and forward segmented buttons.
- (NSView*)backOrForwardTouchBarView;

// Creates and returns the search button.
- (NSView*)searchTouchBarView;
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

  base::scoped_nsobject<NSTouchBar> touchBar(
      [[NSClassFromString(@"NSTouchBar") alloc] init]);
  NSArray* touchBarItemIdentifiers;
  if (showHomeButton_.GetValue()) {
    touchBarItemIdentifiers = @[
      kBackForwardTouchId, kReloadOrStopTouchId, kHomeTouchId, kSearchTouchId,
      kStarTouchId, kNewTabTouchId
    ];
  } else {
    touchBarItemIdentifiers = @[
      kBackForwardTouchId, kReloadOrStopTouchId, kSearchTouchId, kStarTouchId,
      kNewTabTouchId
    ];
  }

  [touchBar setCustomizationIdentifier:kBrowserWindowTouchBarId];
  [touchBar setDefaultItemIdentifiers:touchBarItemIdentifiers];
  [touchBar setCustomizationAllowedItemIdentifiers:touchBarItemIdentifiers];
  [touchBar setDelegate:self];

  return touchBar.autorelease();
}

- (NSTouchBarItem*)touchBar:(NSTouchBar*)touchBar
      makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier {
  if (!touchBar)
    return nil;

  base::scoped_nsobject<NSCustomTouchBarItem> touchBarItem([[NSClassFromString(
      @"NSCustomTouchBarItem") alloc] initWithIdentifier:identifier]);
  if ([identifier isEqualTo:kBackForwardTouchId]) {
    [touchBarItem setView:[self backOrForwardTouchBarView]];
  } else if ([identifier isEqualTo:kReloadOrStopTouchId]) {
    const gfx::VectorIcon& icon =
        isPageLoading_ ? kNavigateStopIcon : kNavigateReloadIcon;
    int command_id = isPageLoading_ ? IDC_STOP : IDC_RELOAD;
    [touchBarItem setView:CreateTouchBarButton(icon, self, command_id)];
  } else if ([identifier isEqualTo:kHomeTouchId]) {
    [touchBarItem
        setView:CreateTouchBarButton(kNavigateHomeIcon, self, IDC_HOME)];
  } else if ([identifier isEqualTo:kNewTabTouchId]) {
    [touchBarItem setView:CreateTouchBarButton(kNewTabMacTouchbarIcon, self,
                                               IDC_NEW_TAB)];
  } else if ([identifier isEqualTo:kStarTouchId]) {
    const gfx::VectorIcon& icon =
        isStarred_ ? toolbar::kStarActiveIcon : toolbar::kStarIcon;
    SkColor iconColor =
        isStarred_ ? kTouchBarStarActiveColor : kTouchBarDefaultIconColor;
    [touchBarItem
        setView:CreateTouchBarButton(icon, self, IDC_BOOKMARK_PAGE, iconColor)];
  } else if ([identifier isEqualTo:kSearchTouchId]) {
    [touchBarItem setView:[self searchTouchBarView]];
  }

  return touchBarItem.autorelease();
}

- (NSView*)backOrForwardTouchBarView {
  NSArray* images = @[
    CreateNSImageFromIcon(ui::kBackArrowIcon),
    CreateNSImageFromIcon(ui::kForwardArrowIcon)
  ];

  NSSegmentedControl* control = [NSSegmentedControl
      segmentedControlWithImages:images
                    trackingMode:NSSegmentSwitchTrackingMomentary
                          target:self
                          action:@selector(backOrForward:)];
  control.segmentStyle = NSSegmentStyleSeparated;
  [control setEnabled:commandUpdater_->IsCommandEnabled(IDC_BACK)
           forSegment:kBackSegmentIndex];
  [control setEnabled:commandUpdater_->IsCommandEnabled(IDC_FORWARD)
           forSegment:kForwardSegmentIndex];
  return control;
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
        gfx::CreateVectorIcon(kGoogleSearchMacTouchbarIcon, kTouchBarIconSize,
                              gfx::kPlaceholderColor),
        base::mac::GetSRGBColorSpace());
  } else {
    image = CreateNSImageFromIcon(ui::kSearchIcon);
  }

  NSButton* searchButton =
      [NSButton buttonWithTitle:base::SysUTF16ToNSString(title)
                          image:image
                         target:self
                         action:@selector(executeCommand:)];
  searchButton.imageHugsTitle = YES;
  searchButton.tag = IDC_FOCUS_LOCATION;
  int width = showHomeButton_.GetValue() ? kSearchBtnWidthWithHomeBtn
                                         : kSearchBtnWidthWithoutHomeBtn;
  [searchButton.widthAnchor constraintEqualToConstant:width].active = YES;
  return searchButton;
}

- (void)backOrForward:(id)sender {
  NSSegmentedControl* control = sender;
  int command =
      [control selectedSegment] == kBackSegmentIndex ? IDC_BACK : IDC_FORWARD;
  LogTouchBarUMA(command);
  commandUpdater_->ExecuteCommand(command);
}

- (void)executeCommand:(id)sender {
  int command = [sender tag];
  LogTouchBarUMA(command);
  commandUpdater_->ExecuteCommand(command);
}

@end
