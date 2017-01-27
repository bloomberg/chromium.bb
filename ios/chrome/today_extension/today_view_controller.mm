// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/today_extension/today_view_controller.h"

#import <CommonCrypto/CommonDigest.h>
#import <NotificationCenter/NotificationCenter.h>
#include <unistd.h>

#include "base/at_exit.h"
#import "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/ios/block_types.h"
#include "base/ios/ios_util.h"
#import "base/ios/weak_nsobject.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#import "base/mac/scoped_block.h"
#import "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics_action.h"
#import "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/sys_info.h"
#include "components/open_from_clipboard/clipboard_recent_content_ios.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/physical_web/physical_web_device.h"
#import "ios/chrome/common/physical_web/physical_web_scanner.h"
#include "ios/chrome/common/x_callback_url.h"
#import "ios/chrome/today_extension/footer_label.h"
#import "ios/chrome/today_extension/lock_screen_state.h"
#import "ios/chrome/today_extension/notification_center_button.h"
#import "ios/chrome/today_extension/physical_web_optin_footer.h"
#import "ios/chrome/today_extension/today_metrics_logger.h"
#include "ios/chrome/today_extension/ui_util.h"
#import "ios/chrome/today_extension/url_table_cell.h"
#include "ios/today_extension/grit/ios_today_extension_strings.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace {

// The different state Physical Web can have at startup.
// Order is so that first 16 states code the four boolean tuple
// (optin, enable, bluetooth, lockscreen) and if user never opted in, states
// 16-19 code the lock and bluetooth state.
enum PhysicalWebInitialState {
  OPTOUT_DISABLE_BTOFF_UNLOCK,
  OPTOUT_DISABLE_BTOFF_LOCK,
  OPTOUT_DISABLE_BTON_UNLOCK,
  OPTOUT_DISABLE_BTON_LOCK,
  OPTOUT_ENABLE_BTOFF_UNLOCK,
  OPTOUT_ENABLE_BTOFF_LOCK,
  OPTOUT_ENABLE_BTON_UNLOCK,
  OPTOUT_ENABLE_BTON_LOCK,
  OPTIN_DISABLE_BTOFF_UNLOCK,
  OPTIN_DISABLE_BTOFF_LOCK,
  OPTIN_DISABLE_BTON_UNLOCK,
  OPTIN_DISABLE_BTON_LOCK,
  OPTIN_ENABLE_BTOFF_UNLOCK,
  OPTIN_ENABLE_BTOFF_LOCK,
  OPTIN_ENABLE_BTON_UNLOCK,
  OPTIN_ENABLE_BTON_LOCK,
  NEVEROPTED_BTOFF_UNLOCK,
  NEVEROPTED_BTOFF_LOCK,
  NEVEROPTED_BTON_UNLOCK,
  NEVEROPTED_BTON_LOCK,
  PHYSICAL_WEB_INITIAL_STATE_COUNT,

  // Helper flag values
  LOCKED_FLAG = 1 << 0,
  BLUETOOTH_FLAG = 1 << 1,
  PHYSICAL_WEB_ACTIVE_FLAG = 1 << 2,
  PHYSICAL_WEB_OPTED_IN_FLAG = 1 << 3,
  PHYSICAL_WEB_OPTED_IN_UNDECIDED_FLAG = 1 << 4,
};

enum PhysicalWebState {
  PHYSICAL_WEB_DISABLE,
  PHYSICAL_WEB_INITIAL_SCANNING,
  PHYSICAL_WEB_SCANNING,
  PHYSICAL_WEB_FROZEN,
  PHYSICAL_WEB_STATE_COUNT
};

// Global exit manager for LazyInstance and message loops. It is needed to
// enable the metrics logs.
base::AtExitManager* g_at_exit_ = nullptr;

const CGFloat kPhysicalWebInitialScanningDelay = 2;
const CGFloat kPhysicalWebRefreshDelay = 2;
const CGFloat kPhysicalWebScanningDelay = 5;

const int kMaxNumberOfPhysicalWebItem = 2;

// Setting to track if user ever interacted with physical web.
NSString* const kPhysicalWebInitialStateDonePreference =
    @"PhysicalInitialStateDone";

// Setting to track if physical web has been turned off by the user.
NSString* const kPhysicalWebDisabledPreference = @"PhysicalWebDisabled";

// Setting to track if user opted in for physical web.
NSString* const kPhysicalWebOptedInPreference = @"PhysicalWebOptedIn";

}  // namespace

@interface TodayViewController ()<LockScreenStateDelegate,
                                  NCWidgetProviding,
                                  PhysicalWebScannerDelegate,
                                  UITableViewDataSource>

// Loads the current locale .pak file for localization.
- (void)loadLocalization;

// Whether all the physical web devices are displayed (YES) or only
// |kMaxNumberOfPhysicalWebItem| (NO).
@property(nonatomic, assign) BOOL displayAllPhysicalWebItems;

// Returns the string contained in the OS pasteboard if it contains a valid URL.
// Returns nil otherwise.
- (NSString*)pasteURLString;

// Updates the URL displayed in the "Open Copied Link" button.
- (void)updatePasteURLButton;

// Sets the footer label that is displayed in the widget.
- (void)setFooterLabel:(FooterLabel)footerLabel forceUpdate:(BOOL)force;

// Computes the height needed by the whole notification center widget with the
// context (orientation, number of beacons...).
- (CGFloat)widgetHeight;

// Change the widget height to |height| if |self isWidgetExpandable| is true;
- (void)setHeight:(CGFloat)height;

// Returns whether the height of the widget can be changed.
- (BOOL)isWidgetExpandable;

// Computes the height needed by the |_urlsTable| table view.
- (CGFloat)urlsTableHeight;

// Refreshes the data and redraws the widget.
- (void)refreshWidget;

// Sets settings wether physical web is enabled.
- (void)setPhysicalWebEnabled:(BOOL)enabled;

// Starts the physical web scanner.
- (void)startPhysicalWeb;

// Stops the physical web scanner. Hide the beacons in the table.
- (void)stopPhysicalWeb;

// Handler for the "New Tab" button. Sends a new tab order to Chrome.
- (void)newTab:(id)sender;

// Handler for the "Voice Search" button. Sends a voice search order to Chrome.
- (void)voiceSearch:(id)sender;

// Called when "Open Copied Link" is tapped. Sends an open url order to Chrome
// to open |url|.
- (void)openClipboardURLInChrome:(NSString*)url;

// Called when a physical web button is tapped. Sends an open url order to
// Chrome to open |url|.
- (void)openPhysicalWebURLInChrome:(NSString*)url;

// Sends an order to Chrome to open |url|.
- (void)openURLInChrome:(NSString*)url;

// Opens Chrome with an x-callback-url with command "app-group-command". The
// |command| and |parameter| are passed via a shared sandbox NSDictionary.
- (void)sendToChromeCommand:(NSString*)command
              withParameter:(NSString*)parameter;

// Creates (or reuses) an autoreleased URLTableCell to contain the pasteboard
// URL.
- (URLTableCell*)cellForPasteboardURL;

// Creates (or reuses) an autoreleased URLTableCell to contain the "Show more
// beacons" button.
- (URLTableCell*)cellForShowMore;

// Creates (or reuses) an autoreleased URLTableCell to contain the physical web
// URL. |index| is the index of the PhysicalWebDevice in |_scanner devices|
// table.
- (URLTableCell*)cellForPhysicalWebURLAtIndex:(NSInteger)index;

// Sends an histogram coding the initial state of the four variables:
// - bluetooth on/off
// - lock screen locked/unlocked
// - physical web enabled/disabled
// - physical web opted in/opted out/not yet decided.
- (void)reportInitialState;

@end

@implementation TodayViewController {
  base::scoped_nsobject<NotificationCenterButton> _newTabButton;
  base::scoped_nsobject<NotificationCenterButton> _voiceSearchButton;
  base::scoped_nsobject<UIView> _containerView;
  base::scoped_nsobject<UILabel> _emptyWidgetLabel;
  base::scoped_nsobject<UIStackView> _buttonsView;
  base::scoped_nsobject<UIStackView> _contentStackView;
  base::scoped_nsobject<NSLayoutConstraint> _tableViewHeight;

  base::scoped_nsobject<UITableView> _urlsTable;
  base::scoped_nsobject<PhysicalWebScanner> _scanner;
  base::scoped_nsobject<NSString> _pasteURL;
  base::scoped_nsprotocol<id<FooterLabel>> _footerLabel;

  CGFloat _defaultLeadingMarginInset;

  NSInteger _maxNumberOfURLs;
  BOOL _displayAllPhysicalWebItems;
  BOOL _physicalWebDetected;

  // Whether the histogram giving the initial state was sent.
  BOOL _initialStateReported;

  // Whether physical web is active (the user enabled it). The scanning for
  // devices can be started.
  BOOL _physicalWebActive;

  // Whether the |_scanner| actually started scanning for devices.
  BOOL _physicalWebRunning;

  // Whether the user has ever seen a beacon and interacted with physical web.
  // If not, don't show any UI if there is no beacon around.
  BOOL _physicalWebInInitialState;

  // Whether the user opted in. Queries to resolve the URLs title can be issued.
  BOOL _physicalWebOptedIn;

  // Whether bluetooth is on. Default to NO, until notification that the
  // bluetooth is on is received.
  BOOL _bluetoothIsOn;

  PhysicalWebState _physicalWebState;
  FooterLabel _currentFooterLabel;

  // A boolean to track if the widget is currently on screen or not.
  BOOL _hidden;

  // Whether a refresh of the widget is scheduled.
  BOOL _refreshScheduled;

  // Whether the widget is displayed in notification center (NO) or as a
  // shortcut widget (YES).
  BOOL _displayedInShortcutMode;

  // The Recent clipboard service that handles the clipboard timeout.
  std::unique_ptr<ClipboardRecentContentIOS> _clipboardRecentContent;
}

@synthesize displayAllPhysicalWebItems = _displayAllPhysicalWebItems;

- (NSString*)pasteURLString {
  GURL pasteURL;
  _clipboardRecentContent->GetRecentURLFromClipboard(&pasteURL);

  if (pasteURL.is_valid() && pasteURL.SchemeIsHTTPOrHTTPS()) {
    return base::SysUTF8ToNSString(pasteURL.spec());
  }
  return nil;
}

- (void)loadView {
  static dispatch_once_t initialization_token;
  dispatch_once(&initialization_token, ^{
    if (!g_at_exit_)
      g_at_exit_ = new base::AtExitManager;
    base::CommandLine::Init(0, nullptr);
    base::FilePath path = base::FilePath(
        base::SysNSStringToUTF8([[NSBundle mainBundle] resourcePath]));
    path = path.DirName().DirName().AppendASCII("icudtl.dat");
    DCHECK(access(path.value().c_str(), F_OK) != -1);
    base::ios::OverridePathOfEmbeddedICU(path.value().c_str());
    base::i18n::InitializeICU();
    [self loadLocalization];
  });

  _defaultLeadingMarginInset = ui_util::kDefaultLeadingMarginInset;

  if (base::ios::IsRunningOnIOS10OrLater()) {
    [[self extensionContext]
        setWidgetLargestAvailableDisplayMode:NCWidgetDisplayModeExpanded];
  }
  _clipboardRecentContent.reset(new ClipboardRecentContentIOS(
      std::string(), app_group::GetGroupUserDefaults()));
  TodayMetricsLogger::GetInstance()->RecordUserAction(
      base::UserMetricsAction("TodayExtension.ExtensionInitialized"));

  _physicalWebInInitialState = ![[NSUserDefaults standardUserDefaults]
      boolForKey:kPhysicalWebInitialStateDonePreference];

  _physicalWebActive = ![[NSUserDefaults standardUserDefaults]
      boolForKey:kPhysicalWebDisabledPreference];
  _physicalWebOptedIn = [[NSUserDefaults standardUserDefaults]
      boolForKey:kPhysicalWebOptedInPreference];

  _containerView.reset([[UIView alloc] initWithFrame:CGRectZero]);
  [_containerView setTranslatesAutoresizingMaskIntoConstraints:NO];
  self.view = _containerView.get();

  // Sets a transparent image as layer to prevent iOS from optimizing out the
  // touch events on the transparent part of the widget.
  UIGraphicsBeginImageContextWithOptions(CGSizeMake(1, 1), NO, 0);
  UIImage* img = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  self.view.layer.contents = (id)[img CGImage];

  _maxNumberOfURLs = NSIntegerMax;
  [self updatePasteURLButton];
  [self setHeight:[self widgetHeight]];

  _newTabButton.reset([[NotificationCenterButton alloc]
        initWithTitle:l10n_util::GetNSString(
                          IDS_IOS_NEW_TAB_TITLE_TODAY_EXTENSION)
                 icon:@"todayview_new_tab"
               target:self
               action:@selector(newTab:)
      backgroundColor:ui_util::BackgroundColor()
             inkColor:ui_util::InkColor()
           titleColor:[UIColor blackColor]]);
  [_newTabButton setButtonSpacesSeparator:ui_util::kUIButtonSeparator
                               frontShift:ui_util::kUIButtonFrontShift
                        horizontalPadding:0
                          verticalPadding:0];
  [_newTabButton setCornerRadius:ui_util::kUIButtonCornerRadius];

  _voiceSearchButton.reset([[NotificationCenterButton alloc]
        initWithTitle:l10n_util::GetNSString(
                          IDS_IOS_VOICE_SEARCH_TODAY_EXTENSION_TITLE)
                 icon:@"todayview_voice_search"
               target:self
               action:@selector(voiceSearch:)
      backgroundColor:ui_util::BackgroundColor()
             inkColor:ui_util::InkColor()
           titleColor:[UIColor blackColor]]);
  [_voiceSearchButton setButtonSpacesSeparator:ui_util::kUIButtonSeparator
                                    frontShift:ui_util::kUIButtonFrontShift
                             horizontalPadding:0
                               verticalPadding:0];
  [_voiceSearchButton setCornerRadius:ui_util::kUIButtonCornerRadius];

  _buttonsView.reset([[UIStackView alloc]
      initWithArrangedSubviews:@[ _newTabButton, _voiceSearchButton ]]);

  [_buttonsView setAxis:UILayoutConstraintAxisHorizontal];
  [_buttonsView setDistribution:UIStackViewDistributionFillEqually];
  [_buttonsView setSpacing:ui_util::kFirstLineButtonMargin];
  [_buttonsView setLayoutMarginsRelativeArrangement:YES];
  [_buttonsView setTranslatesAutoresizingMaskIntoConstraints:NO];

  [[_buttonsView heightAnchor]
      constraintEqualToConstant:ui_util::kFirstLineHeight]
      .active = YES;

  CGFloat chromeIconXOffset =
      _defaultLeadingMarginInset + ui_util::ChromeIconOffset();
  CGFloat firstLineOuterMargin =
      chromeIconXOffset - ui_util::kFirstLineButtonMargin;
  [_buttonsView
      setLayoutMargins:UIEdgeInsetsMake(ui_util::kFirstLineButtonMargin,
                                        firstLineOuterMargin,
                                        ui_util::kFirstLineButtonMargin,
                                        firstLineOuterMargin)];

  _urlsTable.reset([[UITableView alloc] initWithFrame:CGRectZero]);
  [_urlsTable setDataSource:self];
  [_urlsTable setRowHeight:ui_util::kSecondLineHeight];
  [_urlsTable setSeparatorStyle:UITableViewCellSeparatorStyleNone];
  _tableViewHeight.reset(
      [[[_urlsTable heightAnchor] constraintEqualToConstant:0] retain]);
  [_tableViewHeight setActive:YES];

  _contentStackView.reset([[UIStackView alloc]
      initWithArrangedSubviews:@[ _buttonsView, _urlsTable ]]);
  [[_urlsTable widthAnchor]
      constraintEqualToAnchor:[_contentStackView widthAnchor]]
      .active = YES;
  [_contentStackView setAxis:UILayoutConstraintAxisVertical];
  [_contentStackView setDistribution:UIStackViewDistributionFill];
  [_contentStackView setSpacing:0];
  [_contentStackView setLayoutMarginsRelativeArrangement:NO];
  [_contentStackView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_containerView addSubview:_contentStackView];
  [[_contentStackView topAnchor]
      constraintEqualToAnchor:[_containerView topAnchor]]
      .active = YES;
  [[_contentStackView widthAnchor]
      constraintEqualToAnchor:[_containerView widthAnchor]]
      .active = YES;
  [[_contentStackView centerXAnchor]
      constraintEqualToAnchor:[_containerView centerXAnchor]]
      .active = YES;

  if (base::ios::IsRunningOnIOS10OrLater()) {
    _emptyWidgetLabel.reset([[UILabel alloc] initWithFrame:CGRectZero]);
    [_emptyWidgetLabel
        setText:l10n_util::GetNSString(IDS_IOS_EMPTY_TODAY_EXTENSION_TEXT)];
    [_emptyWidgetLabel setFont:[UIFont systemFontOfSize:16]];
    [_emptyWidgetLabel setTextColor:ui_util::emptyLabelColor()];
    [_emptyWidgetLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_containerView addSubview:_emptyWidgetLabel];
    [NSLayoutConstraint activateConstraints:@[
      [[_emptyWidgetLabel centerXAnchor]
          constraintEqualToAnchor:[_containerView centerXAnchor]],
      [[_emptyWidgetLabel centerYAnchor]
          constraintEqualToAnchor:[_containerView centerYAnchor]
                         constant:ui_util::kEmptyLabelYOffset]
    ]];
    [_emptyWidgetLabel setHidden:YES];
  }

  _hidden = NO;
  [self refreshWidget];
}

- (void)loadLocalization {
  NSArray* languageList = [[NSBundle mainBundle] preferredLocalizations];
  NSString* firstLocale = [languageList objectAtIndex:0];

  if (!firstLocale) {
    firstLocale = @"en";
  }
  base::FilePath resource_path([[base::mac::FrameworkBundle()
      pathForResource:@"locale"
               ofType:@"pak"
          inDirectory:@""
      forLocalization:firstLocale] fileSystemRepresentation]);
  ResourceBundle::InitSharedInstanceWithPakPath(resource_path);
}

- (void)updatePasteURLButton {
  NSString* pasteURLString = [self pasteURLString];
  if ([pasteURLString isEqualToString:_pasteURL])
    return;
  _pasteURL.reset([pasteURLString copy]);
  if (_pasteURL) {
    TodayMetricsLogger::GetInstance()->RecordUserAction(
        base::UserMetricsAction("TodayExtension.CopiedURLDisplayed"));
  }
  [self refreshWidget];
}

- (void)setHeight:(CGFloat)height {
  if (![self isWidgetExpandable]) {
    return;
  }

  CGSize size = CGSizeMake(0, height);
  if (base::ios::IsRunningOnIOS10OrLater()) {
    size = [self.extensionContext
        widgetMaximumSizeForDisplayMode:[self.extensionContext
                                                widgetActiveDisplayMode]];
    CGSize minSize = [self.extensionContext
        widgetMaximumSizeForDisplayMode:NCWidgetDisplayModeCompact];
    size.height = MIN(height, size.height);
    // Empirically, widget has to be bigger in Expanded mode than in Compact
    // mode.
    // If it is not the case, some resize instructions can be lost.
    // These tests have been done on iPhone 7 on iOS10.0 and 10.1.
    size.height = MAX(size.height, minSize.height + 1);
  }
  if (self.preferredContentSize.height == size.height) {
    // If the height is already that size, avoid trigger UI updates.
    return;
  }
  self.preferredContentSize = size;
}

- (BOOL)isWidgetExpandable {
  if (base::ios::IsRunningOnIOS10OrLater()) {
    return [self.extensionContext widgetActiveDisplayMode] ==
           NCWidgetDisplayModeExpanded;
  }
  return YES;
}

- (CGFloat)widgetHeight {
  if (_hidden) {
    return ui_util::kFirstLineHeight;
  }
  CGFloat height = 0;
  if (!_displayedInShortcutMode)
    height += ui_util::kFirstLineHeight;
  return height + [self urlsTableHeight] +
         [_footerLabel heightForWidth:[_containerView frame].size.width];
}

- (CGFloat)urlsTableHeight {
  return [self tableView:_urlsTable numberOfRowsInSection:0] *
         ui_util::kSecondLineHeight;
}

- (void)scheduleRefreshWidget {
  if (_refreshScheduled)
    return;

  _refreshScheduled = YES;
  [self performSelector:@selector(refreshWidget)
             withObject:nil
             afterDelay:kPhysicalWebRefreshDelay];
}

- (void)refreshWidget {
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector(refreshWidget)
                                             object:nil];
  _refreshScheduled = NO;
  [_urlsTable reloadData];
  [_tableViewHeight setConstant:[self urlsTableHeight]];
  [self.view setNeedsLayout];
  CGFloat height = [self widgetHeight];
  BOOL empty = height == 0;
  [_emptyWidgetLabel setHidden:!empty];
  [self setHeight:height];
}

- (void)setFooterLabel:(FooterLabel)footerLabel forceUpdate:(BOOL)force {
  if (footerLabel == _currentFooterLabel && !force)
    return;
  if (footerLabel == PW_OPTIN_DIALOG &&
      _currentFooterLabel != PW_OPTIN_DIALOG) {
    TodayMetricsLogger::GetInstance()->RecordUserAction(
        base::UserMetricsAction("PhysicalWeb.OptinDisplayed"));
  }

  _currentFooterLabel = footerLabel;
  [[_footerLabel view] removeFromSuperview];
  base::WeakNSObject<TodayViewController> weakSelf(self);
  base::mac::ScopedBlock<ProceduralBlock> learnMoreBlock;
  base::mac::ScopedBlock<ProceduralBlock> turnOffPhysicalWeb;
  base::mac::ScopedBlock<ProceduralBlock> turnOnPhysicalWeb;
  base::mac::ScopedBlock<ProceduralBlock> optInPhysicalWeb;
  base::mac::ScopedBlock<ProceduralBlock> optOutPhysicalWeb;

  learnMoreBlock.reset(
      ^{
        [weakSelf learnMore];
      },
      base::scoped_policy::RETAIN);
  if (![[LockScreenState sharedInstance] isScreenLocked]) {
    turnOffPhysicalWeb.reset(
        ^{
          [weakSelf setPhysicalWebEnabled:NO];
        },
        base::scoped_policy::RETAIN);

    turnOnPhysicalWeb.reset(
        ^{
          [weakSelf setPhysicalWebEnabled:YES];
        },
        base::scoped_policy::RETAIN);

    optInPhysicalWeb.reset(
        ^{
          [weakSelf physicalWebOptIn];
        },
        base::scoped_policy::RETAIN);

    optOutPhysicalWeb.reset(
        ^{
          [weakSelf physicalWebOptOut];
        },
        base::scoped_policy::RETAIN);
  }

  switch (footerLabel) {
    case NO_FOOTER_LABEL:
      _footerLabel.reset();
      break;
    case PW_IS_OFF_FOOTER_LABEL:
      _footerLabel.reset([[PWIsOffFooterLabel alloc]
          initWithLearnMoreBlock:learnMoreBlock
                     turnOnBlock:turnOnPhysicalWeb]);
      break;
    case PW_IS_ON_FOOTER_LABEL:
      _footerLabel.reset([[PWIsOnFooterLabel alloc]
          initWithLearnMoreBlock:learnMoreBlock
                    turnOffBlock:turnOffPhysicalWeb]);
      break;
    case PW_SCANNING_FOOTER_LABEL:
      _footerLabel.reset([[PWScanningFooterLabel alloc]
          initWithLearnMoreBlock:learnMoreBlock
                    turnOffBlock:turnOffPhysicalWeb]);
      break;
    case PW_OPTIN_DIALOG:
      _footerLabel.reset([[PhysicalWebOptInFooter alloc]
          initWithLeftInset:_defaultLeadingMarginInset
             learnMoreBlock:learnMoreBlock
                optinAction:optInPhysicalWeb
              dismissAction:optOutPhysicalWeb]);
      break;
    case PW_BT_OFF_FOOTER_LABEL:
      _footerLabel.reset(
          [[PWBTOffFooterLabel alloc] initWithLearnMoreBlock:learnMoreBlock]);
      break;
    case FOOTER_LABEL_COUNT:
      NOTREACHED();
      break;
  }
  if (_footerLabel) {
    [_contentStackView addArrangedSubview:[_footerLabel view]];
    [[[_footerLabel view] widthAnchor]
        constraintEqualToAnchor:[_contentStackView widthAnchor]]
        .active = YES;
    [[[_footerLabel view] centerXAnchor]
        constraintEqualToAnchor:[_contentStackView centerXAnchor]]
        .active = YES;
    [[[_footerLabel view] bottomAnchor]
        constraintEqualToAnchor:[self view].bottomAnchor]
        .active = YES;
  }
  [self refreshWidget];
}

- (void)learnMore {
  [self openURLInChrome:
            @"https://support.google.com/chrome/?p=chrome_physical_web"];
}

- (void)setPhysicalWebEnabled:(BOOL)enabled {
  if (enabled == _physicalWebActive)
    return;
  _physicalWebActive = enabled;
  [[NSUserDefaults standardUserDefaults]
      setBool:!enabled
       forKey:kPhysicalWebDisabledPreference];
  if (enabled) {
    [self startPhysicalWeb];
  } else {
    [self stopPhysicalWeb];
  }
}

- (void)lockScreenStateDidChange:(LockScreenState*)lockScreenState {
  [self updatePhysicalWebFooterForceUpdate:YES];
}

- (void)newTab:(id)sender {
  TodayMetricsLogger::GetInstance()->RecordUserAction(
      base::UserMetricsAction("TodayExtension.NewTabPressed"));

  NSString* command =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupNewTabCommand);
  [self sendToChromeCommand:command withParameter:nil];
}

- (void)voiceSearch:(id)sender {
  TodayMetricsLogger::GetInstance()->RecordUserAction(
      base::UserMetricsAction("TodayExtension.VoiceSearchPressed"));
  NSString* command =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupVoiceSearchCommand);
  [self sendToChromeCommand:command withParameter:nil];
}

- (void)openClipboardURLInChrome:(NSString*)url {
  TodayMetricsLogger::GetInstance()->RecordUserAction(
      base::UserMetricsAction("TodayExtension.OpenClipboardPressed"));
  [self openURLInChrome:url];
}

- (void)openPhysicalWebURLInChrome:(NSString*)url {
  TodayMetricsLogger::GetInstance()->RecordUserAction(
      base::UserMetricsAction("TodayExtension.PhysicalWebPressed"));
  TodayMetricsLogger::GetInstance()->RecordUserAction(
      base::UserMetricsAction("PhysicalWeb.UrlSelected"));
  [self openURLInChrome:url];
}

- (void)openURLInChrome:(NSString*)url {
  TodayMetricsLogger::GetInstance()->RecordUserAction(
      base::UserMetricsAction("TodayExtension.ActionTriggered"));
  GURL pasteURL(base::SysNSStringToUTF8(url));
  if (!pasteURL.is_valid()) {
    return;
  }
  NSString* command =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupOpenURLCommand);
  [self sendToChromeCommand:command withParameter:url];
}

- (void)sendToChromeCommand:(NSString*)command
              withParameter:(NSString*)parameter {
  base::scoped_nsobject<NSUserDefaults> sharedDefaults(
      [[NSUserDefaults alloc] initWithSuiteName:app_group::ApplicationGroup()]);

  base::scoped_nsobject<NSMutableDictionary> commandDictionary(
      [[NSMutableDictionary alloc] init]);
  [commandDictionary
      setObject:[NSDate date]
         forKey:base::SysUTF8ToNSString(
                    app_group::kChromeAppGroupCommandTimePreference)];
  [commandDictionary
      setObject:@"TodayExtension"
         forKey:base::SysUTF8ToNSString(
                    app_group::kChromeAppGroupCommandAppPreference)];

  [commandDictionary
      setObject:command
         forKey:base::SysUTF8ToNSString(
                    app_group::kChromeAppGroupCommandCommandPreference)];

  if (parameter) {
    [commandDictionary
        setObject:parameter
           forKey:base::SysUTF8ToNSString(
                      app_group::kChromeAppGroupCommandParameterPreference)];
  }
  [sharedDefaults setObject:commandDictionary
                     forKey:base::SysUTF8ToNSString(
                                app_group::kChromeAppGroupCommandPreference)];
  [sharedDefaults synchronize];

  NSString* scheme = base::mac::ObjCCast<NSString>([[NSBundle mainBundle]
      objectForInfoDictionaryKey:@"KSChannelChromeScheme"]);
  if (!scheme)
    return;
  const GURL openURL =
      CreateXCallbackURL(base::SysNSStringToUTF8(scheme),
                         app_group::kChromeAppGroupXCallbackCommand);
  [self.extensionContext openURL:net::NSURLWithGURL(openURL)
               completionHandler:nil];
}

- (void)startPhysicalWeb {
  if (_physicalWebRunning)
    return;
  _physicalWebRunning = YES;

  // Reset scanner to reset previously detected devices.
  [_scanner stop];
  _scanner.reset([[PhysicalWebScanner alloc] initWithDelegate:self]);
  if (_physicalWebOptedIn) {
    [_scanner setNetworkRequestEnabled:YES];
  }
  _physicalWebState = PHYSICAL_WEB_INITIAL_SCANNING;
  _displayAllPhysicalWebItems = NO;
  [self updatePhysicalWebFooterForceUpdate:NO];
  [self refreshWidget];
  [_scanner start];
  // Refresh the UI after 2 seconds.
  [self performSelector:@selector(physicalWebEndOfInitialScanning)
             withObject:nil
             afterDelay:kPhysicalWebInitialScanningDelay];
}

- (void)physicalWebEndOfInitialScanning {
  _physicalWebState = PHYSICAL_WEB_SCANNING;
  if (_physicalWebDetected) {
    [self refreshWidget];
  }
  // After 5 seconds, stop scanning and refresh the UI.
  [self performSelector:@selector(physicalWebEndOfScanning)
             withObject:nil
             afterDelay:kPhysicalWebScanningDelay];
}

- (void)physicalWebEndOfScanning {
  [_scanner stop];
  _physicalWebState = PHYSICAL_WEB_FROZEN;
  if (_physicalWebOptedIn || !_physicalWebDetected) {
    [self updatePhysicalWebFooterForceUpdate:NO];
    [self refreshWidget];
  }
}

- (void)stopPhysicalWeb {
  _physicalWebRunning = NO;
  _physicalWebDetected = NO;
  _refreshScheduled = NO;
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  _physicalWebState = PHYSICAL_WEB_DISABLE;
  [_scanner stop];
  _scanner.reset();
  [self updatePhysicalWebFooterForceUpdate:NO];
  [self refreshWidget];
}

- (FooterLabel)footerForCurrentPhysicalWebState {
  if (_hidden) {
    return NO_FOOTER_LABEL;
  }

  if (!_bluetoothIsOn) {
    if (_physicalWebActive && _physicalWebOptedIn) {
      return PW_BT_OFF_FOOTER_LABEL;
    }
    return NO_FOOTER_LABEL;
  }

  // Bluetooth is on.
  if (!_physicalWebActive) {
    return PW_IS_OFF_FOOTER_LABEL;
  }

  if (!_physicalWebOptedIn) {
    // User did not opt in. Show opt-in screen if devices are detected.
    if (_physicalWebDetected) {
      return PW_OPTIN_DIALOG;
    } else {
      if (_physicalWebInInitialState) {
        return NO_FOOTER_LABEL;
      } else {
        return PW_IS_ON_FOOTER_LABEL;
      }
    }
  }

  if (_physicalWebState == PHYSICAL_WEB_FROZEN) {
    return PW_IS_ON_FOOTER_LABEL;
  } else {
    return PW_SCANNING_FOOTER_LABEL;
  }
  NOTREACHED();
}

- (void)updatePhysicalWebFooterForceUpdate:(BOOL)force {
  [self setFooterLabel:[self footerForCurrentPhysicalWebState]
           forceUpdate:force];
}

- (void)physicalWebOptOut {
  _physicalWebOptedIn = NO;
  _physicalWebInInitialState = NO;
  [self setPhysicalWebEnabled:NO];
  [[NSUserDefaults standardUserDefaults] setBool:NO
                                          forKey:kPhysicalWebOptedInPreference];
  [[NSUserDefaults standardUserDefaults]
      setBool:YES
       forKey:kPhysicalWebInitialStateDonePreference];
}

- (void)physicalWebOptIn {
  [[NSUserDefaults standardUserDefaults] setBool:YES
                                          forKey:kPhysicalWebOptedInPreference];
  [[NSUserDefaults standardUserDefaults]
      setBool:YES
       forKey:kPhysicalWebInitialStateDonePreference];
  _physicalWebInInitialState = NO;
  _physicalWebOptedIn = YES;
  [self stopPhysicalWeb];
  [self startPhysicalWeb];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  _displayedInShortcutMode = NO;
  if (base::ios::IsRunningOnIOS10OrLater()) {
    CGSize maxHeightExpanded = [self.extensionContext
        widgetMaximumSizeForDisplayMode:NCWidgetDisplayModeExpanded];
    CGSize maxHeightCompact = [self.extensionContext
        widgetMaximumSizeForDisplayMode:NCWidgetDisplayModeCompact];
    _displayedInShortcutMode =
        maxHeightExpanded.height == maxHeightCompact.height;
    [_buttonsView setHidden:_displayedInShortcutMode];
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  _hidden = NO;
  _initialStateReported = NO;
  [[LockScreenState sharedInstance] setDelegate:self];
  _pasteURL.reset();
  [self updatePasteURLButton];
  TodayMetricsLogger::GetInstance()->RecordUserAction(
      base::UserMetricsAction("TodayExtension.ExtensionDisplayed"));
  [_scanner stop];
  if (!_displayedInShortcutMode || !_physicalWebInInitialState) {
    _scanner.reset([[PhysicalWebScanner alloc] initWithDelegate:self]);
  }
  _physicalWebRunning = NO;
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  if (_physicalWebRunning) {
    UMA_HISTOGRAM_COUNTS_100("PhysicalWeb.TotalBeaconsDetected",
                             [[_scanner devices] count]);
  }
  TodayMetricsLogger::GetInstance()->RecordUserAction(
      base::UserMetricsAction("TodayExtension.ExtensionDismissed"));

  _hidden = YES;
  [[LockScreenState sharedInstance] setDelegate:nil];
  [self setFooterLabel:NO_FOOTER_LABEL forceUpdate:NO];
  [self stopPhysicalWeb];
  [self refreshWidget];
  if (base::ios::IsRunningOnIOS10OrLater()) {
    // Prepare for next display whch can be on Shortcut mode.
    [_buttonsView setHidden:YES];
  }
}

- (void)scannerUpdatedDevices:(PhysicalWebScanner*)scanner {
  _physicalWebDetected =
      [_scanner unresolvedBeaconsCount] + [[_scanner devices] count] > 0;
  if (!_physicalWebOptedIn && _physicalWebDetected) {
    [self updatePhysicalWebFooterForceUpdate:NO];
    return;
  }
  if (_physicalWebState == PHYSICAL_WEB_SCANNING) {
    [self scheduleRefreshWidget];
  }
}

- (void)reportInitialState {
  if (_initialStateReported)
    return;

  _initialStateReported = YES;
  int state =
      [[LockScreenState sharedInstance] isScreenLocked] ? LOCKED_FLAG : 0;
  state |= (_bluetoothIsOn ? BLUETOOTH_FLAG : 0);
  if (!_physicalWebInInitialState) {
    state |= (_physicalWebActive ? PHYSICAL_WEB_ACTIVE_FLAG : 0);
    state |= (_physicalWebOptedIn ? PHYSICAL_WEB_OPTED_IN_FLAG : 0);
  } else {
    state |= PHYSICAL_WEB_OPTED_IN_UNDECIDED_FLAG;
  }
  DCHECK(state < PHYSICAL_WEB_INITIAL_STATE_COUNT);
  UMA_HISTOGRAM_ENUMERATION("PhysicalWeb.InitialState", state,
                            PHYSICAL_WEB_INITIAL_STATE_COUNT);
}

- (void)scannerBluetoothStatusUpdated:(PhysicalWebScanner*)scanner {
  _bluetoothIsOn = [scanner bluetoothEnabled];
  [self reportInitialState];

  if (_bluetoothIsOn && _physicalWebActive) {
    [self startPhysicalWeb];
  } else {
    [self stopPhysicalWeb];
  }
  [self updatePhysicalWebFooterForceUpdate:NO];
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  DCHECK(tableView == _urlsTable.get());
  DCHECK(section == 0);
  if (_hidden)
    return 0;
  NSInteger rowCount = [[_scanner devices] count];
  if (!_displayAllPhysicalWebItems && rowCount > kMaxNumberOfPhysicalWebItem) {
    // Add one row for the "Show more" button.
    rowCount = kMaxNumberOfPhysicalWebItem + 1;
  }
  if (_physicalWebState == PHYSICAL_WEB_INITIAL_SCANNING) {
    rowCount = 0;
  }
  if (_pasteURL)
    rowCount++;
  if (rowCount > _maxNumberOfURLs)
    rowCount = _maxNumberOfURLs;
  return rowCount;
}

- (URLTableCell*)cellForPasteboardURL {
  NSString* pasteboardReusableID = @"PasteboardCell";
  URLTableCell* cell = base::mac::ObjCCast<URLTableCell>(
      [_urlsTable dequeueReusableCellWithIdentifier:pasteboardReusableID]);
  if (cell) {
    [cell setTitle:l10n_util::GetNSString(
                       IDS_IOS_OPEN_COPIED_LINK_TODAY_EXTENSION)
               url:_pasteURL];

  } else {
    base::WeakNSObject<TodayViewController> weakSelf(self);
    URLActionBlock action = ^(NSString* url) {
      [weakSelf openClipboardURLInChrome:url];
    };
    cell = [[[URLTableCell alloc]
          initWithTitle:l10n_util::GetNSString(
                            IDS_IOS_OPEN_COPIED_LINK_TODAY_EXTENSION)
                    url:_pasteURL
                   icon:@"todayview_clipboard"
              leftInset:_defaultLeadingMarginInset
        reuseIdentifier:pasteboardReusableID
                  block:action] autorelease];
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
  }
  return cell;
}

- (URLTableCell*)cellForShowMore {
  NSString* showMoreReusableID = @"ShowMoreCell";
  URLTableCell* cell = base::mac::ObjCCast<URLTableCell>(
      [_urlsTable dequeueReusableCellWithIdentifier:showMoreReusableID]);
  NSString* title = l10n_util::GetNSString(
      IDS_IOS_PYSICAL_WEB_TODAY_EXTENSION_SHOW_MORE_BEACONS);
  if (cell) {
    [cell setTitle:title url:@""];
  } else {
    base::WeakNSObject<TodayViewController> weakSelf(self);
    URLActionBlock action = ^(NSString* url) {
      [weakSelf setDisplayAllPhysicalWebItems:YES];
      [weakSelf refreshWidget];
    };
    cell = [[[URLTableCell alloc] initWithTitle:title
                                            url:@""
                                           icon:@""
                                      leftInset:_defaultLeadingMarginInset
                                reuseIdentifier:showMoreReusableID
                                          block:action] autorelease];
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
  }
  return cell;
}

- (URLTableCell*)cellForPhysicalWebURLAtIndex:(NSInteger)index {
  NSString* physicalWebReusableID = @"PhysicalWebCell";
  URLTableCell* cell = base::mac::ObjCCast<URLTableCell>(
      [_urlsTable dequeueReusableCellWithIdentifier:physicalWebReusableID]);
  PhysicalWebDevice* device = [[_scanner devices] objectAtIndex:index];
  if (cell) {
    [cell setTitle:[device title] url:[[device url] absoluteString]];
  } else {
    base::WeakNSObject<TodayViewController> weakSelf(self);
    URLActionBlock action = ^(NSString* url) {
      [weakSelf openPhysicalWebURLInChrome:url];
    };
    cell = [[[URLTableCell alloc] initWithTitle:[device title]
                                            url:[[device url] absoluteString]
                                           icon:@"todayview_physical_web"
                                      leftInset:_defaultLeadingMarginInset
                                reuseIdentifier:physicalWebReusableID
                                          block:action] autorelease];
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
  }
  return cell;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK(tableView == _urlsTable.get());
  NSInteger indexRequested = [indexPath row];
  NSInteger lastRowIndex =
      [self tableView:tableView numberOfRowsInSection:0] - 1;

  DCHECK(indexRequested >= 0 && indexRequested <= lastRowIndex);

  URLTableCell* cell = nil;
  if (_pasteURL) {
    if (indexRequested == 0) {
      cell = [self cellForPasteboardURL];
    }
    indexRequested--;
  }
  if (!cell && indexRequested >= kMaxNumberOfPhysicalWebItem &&
      !_displayAllPhysicalWebItems) {
    cell = [self cellForShowMore];
  }
  if (!cell) {
    cell = [self cellForPhysicalWebURLAtIndex:indexRequested];
  }
  [cell setSeparatorVisible:[indexPath row] != lastRowIndex ||
                            _currentFooterLabel == PW_OPTIN_DIALOG];
  return cell;
}

#pragma mark - NCWidgetProviding

- (void)widgetActiveDisplayModeDidChange:(NCWidgetDisplayMode)activeDisplayMode
                         withMaximumSize:(CGSize)maxSize {
  if (activeDisplayMode == NCWidgetDisplayModeExpanded) {
    // If in NCWidgetDisplayModeExpanded mode, we can change the size of the
    // widget.
    [self setHeight:[self widgetHeight]];
  } else {
    // If in NCWidgetDisplayModeCompact mode, the size has to be
    // |NCWidgetDisplayModeCompact.maxsize|. Set the preferredContentSize so
    // next time we want to check the size, the value is correct.
    // Directly call |setPreferredContentSize:| as widget is not expandable at
    // this time.
    [self setPreferredContentSize:maxSize];
  }
}

- (void)widgetPerformUpdateWithCompletionHandler:
    (void (^)(NCUpdateResult))completionHandler {
  completionHandler(NCUpdateResultNewData);
}

- (UIEdgeInsets)widgetMarginInsetsForProposedMarginInsets:
    (UIEdgeInsets)defaultMarginInsets {
  DCHECK(!base::ios::IsRunningOnIOS10OrLater());
  if (!UIEdgeInsetsEqualToEdgeInsets(defaultMarginInsets, UIEdgeInsetsZero)) {
    if (ui_util::IsRTL()) {
      _defaultLeadingMarginInset = defaultMarginInsets.right;
    } else {
      _defaultLeadingMarginInset = defaultMarginInsets.left;
    }
  }
  return UIEdgeInsetsZero;
}

@end
