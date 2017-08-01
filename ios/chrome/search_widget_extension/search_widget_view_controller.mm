// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/search_widget_extension/search_widget_view_controller.h"

#import <NotificationCenter/NotificationCenter.h>

#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/open_from_clipboard/clipboard_recent_content_impl_ios.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/search_widget_extension/copied_url_view.h"
#import "ios/chrome/search_widget_extension/search_widget_view.h"
#import "ios/chrome/search_widget_extension/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Using GURL in the extension is not wanted as it includes ICU which makes the
// extension binary much larger; therefore, ios/chrome/common/x_callback_url.h
// cannot be used. This class makes a very basic use of x-callback-url, so no
// full implementation is required.
NSString* const kXCallbackURLHost = @"x-callback-url";
}  // namespace

@interface SearchWidgetViewController ()<SearchWidgetViewActionTarget>
@property(nonatomic, weak) SearchWidgetView* widgetView;
@property(nonatomic, strong) NSURL* copiedURL;
@property(nonatomic, strong)
    ClipboardRecentContentImplIOS* clipboardRecentContent;

// Updates the widget with latest data from the clipboard. Returns whether any
// visual updates occurred.
- (BOOL)updateWidget;
// Opens the main application with the given |command|.
- (void)openAppWithCommand:(NSString*)command;
// Opens the main application with the given |command| and |parameter|.
- (void)openAppWithCommand:(NSString*)command parameter:(NSString*)parameter;
// Returns the dictionary of commands to pass via user defaults to open the main
// application for a given |command| and |parameter|.
+ (NSDictionary*)dictForCommand:(NSString*)command
                      parameter:(NSString*)parameter;

@end

@implementation SearchWidgetViewController

@synthesize widgetView = _widgetView;
@synthesize copiedURL = _copiedURL;
@synthesize clipboardRecentContent = _clipboardRecentContent;

- (instancetype)init {
  self = [super init];
  if (self) {
    _clipboardRecentContent = [[ClipboardRecentContentImplIOS alloc]
           initWithMaxAge:1 * 60 * 60
        authorizedSchemes:[NSSet setWithObjects:@"http", @"https", nil]
             userDefaults:app_group::GetGroupUserDefaults()
                 delegate:nil];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UIVibrancyEffect* primary;
  UIVibrancyEffect* secondary;
  CGFloat height =
      self.extensionContext
          ? [self.extensionContext
                widgetMaximumSizeForDisplayMode:NCWidgetDisplayModeCompact]
                .height
          : 110;
  if (base::ios::IsRunningOnIOS10OrLater()) {
    primary = [UIVibrancyEffect widgetPrimaryVibrancyEffect];
    secondary = [UIVibrancyEffect widgetSecondaryVibrancyEffect];
  } else {
    primary = [UIVibrancyEffect notificationCenterVibrancyEffect];
    secondary = [UIVibrancyEffect notificationCenterVibrancyEffect];
  }

  // A local variable is necessary here as the property is declared weak and the
  // object would be deallocated before being retained by the addSubview call.
  SearchWidgetView* widgetView = [[SearchWidgetView alloc]
         initWithActionTarget:self
        primaryVibrancyEffect:primary
      secondaryVibrancyEffect:secondary
                compactHeight:height
             initiallyCompact:([self.extensionContext
                                       widgetActiveDisplayMode] ==
                               NCWidgetDisplayModeCompact)];
  self.widgetView = widgetView;
  [self.view addSubview:self.widgetView];

  if (base::ios::IsRunningOnIOS10OrLater()) {
    self.extensionContext.widgetLargestAvailableDisplayMode =
        NCWidgetDisplayModeExpanded;
  }

  self.widgetView.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:ui_util::CreateSameConstraints(
                                              self.view, self.widgetView)];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self updateWidget];
}

- (void)widgetPerformUpdateWithCompletionHandler:
    (void (^)(NCUpdateResult))completionHandler {
  completionHandler([self updateWidget] ? NCUpdateResultNewData
                                        : NCUpdateResultNoData);
}

- (BOOL)updateWidget {
  NSURL* url = [_clipboardRecentContent recentURLFromClipboard];

  if (![url isEqual:self.copiedURL]) {
    self.copiedURL = url;
    [self.widgetView setCopiedURLString:self.copiedURL.absoluteString];
    return YES;
  }
  return NO;
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  BOOL isCompact = [self.extensionContext widgetActiveDisplayMode] ==
                   NCWidgetDisplayModeCompact;

  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> _Nonnull context) {
        [self.widgetView showMode:isCompact];
      }
                      completion:nil];
}

#pragma mark - NCWidgetProviding

- (void)widgetActiveDisplayModeDidChange:(NCWidgetDisplayMode)activeDisplayMode
                         withMaximumSize:(CGSize)maxSize {
  switch (activeDisplayMode) {
    case NCWidgetDisplayModeCompact:
      self.preferredContentSize = maxSize;
      break;
    case NCWidgetDisplayModeExpanded:
      self.preferredContentSize =
          CGSizeMake(maxSize.width, [self.widgetView widgetHeight]);
      break;
  }
}

// Implementing this method removes the leading edge inset for iOS version < 10.
// TODO(crbug.com/720490): Remove implementation when dropping ios9 support.
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_10_0
- (UIEdgeInsets)widgetMarginInsetsForProposedMarginInsets:
    (UIEdgeInsets)defaultMa‌​rginInsets {
  return UIEdgeInsetsZero;
}
#endif

#pragma mark - WidgetViewActionTarget

- (void)openSearch:(id)sender {
  [self openAppWithCommand:base::SysUTF8ToNSString(
                               app_group::kChromeAppGroupFocusOmniboxCommand)];
}

- (void)openIncognito:(id)sender {
  [self
      openAppWithCommand:base::SysUTF8ToNSString(
                             app_group::kChromeAppGroupIncognitoSearchCommand)];
}

- (void)openVoice:(id)sender {
  [self openAppWithCommand:base::SysUTF8ToNSString(
                               app_group::kChromeAppGroupVoiceSearchCommand)];
}

- (void)openQRCode:(id)sender {
  [self openAppWithCommand:base::SysUTF8ToNSString(
                               app_group::kChromeAppGroupQRScannerCommand)];
}

- (void)openCopiedURL:(id)sender {
  DCHECK(self.copiedURL);
  [self openAppWithCommand:base::SysUTF8ToNSString(
                               app_group::kChromeAppGroupOpenURLCommand)
                 parameter:self.copiedURL.absoluteString];
}

#pragma mark - internal

- (void)openAppWithCommand:(NSString*)command {
  return [self openAppWithCommand:command parameter:nil];
}

- (void)openAppWithCommand:(NSString*)command parameter:(NSString*)parameter {
  NSUserDefaults* sharedDefaults =
      [[NSUserDefaults alloc] initWithSuiteName:app_group::ApplicationGroup()];
  NSString* defaultsKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandPreference);
  [sharedDefaults
      setObject:[SearchWidgetViewController dictForCommand:command
                                                 parameter:parameter]
         forKey:defaultsKey];
  [sharedDefaults synchronize];

  NSString* scheme = base::mac::ObjCCast<NSString>([[NSBundle mainBundle]
      objectForInfoDictionaryKey:@"KSChannelChromeScheme"]);
  if (!scheme)
    return;

  NSURLComponents* urlComponents = [NSURLComponents new];
  urlComponents.scheme = scheme;
  urlComponents.host = kXCallbackURLHost;
  urlComponents.path = [NSString
      stringWithFormat:@"/%@", base::SysUTF8ToNSString(
                                   app_group::kChromeAppGroupXCallbackCommand)];

  NSURL* openURL = [urlComponents URL];
  [self.extensionContext openURL:openURL completionHandler:nil];
}

+ (NSDictionary*)dictForCommand:(NSString*)command
                      parameter:(NSString*)parameter {
  NSString* timePrefKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTimePreference);
  NSString* appPrefKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandAppPreference);
  NSString* commandPrefKey = base::SysUTF8ToNSString(
      app_group::kChromeAppGroupCommandCommandPreference);

  if (parameter) {
    NSString* paramPrefKey = base::SysUTF8ToNSString(
        app_group::kChromeAppGroupCommandParameterPreference);
    return @{
      timePrefKey : [NSDate date],
      appPrefKey : @"TodayExtension",
      commandPrefKey : command,
      paramPrefKey : parameter,
    };
  }
  return @{
    timePrefKey : [NSDate date],
    appPrefKey : @"TodayExtension",
    commandPrefKey : command,
  };
}

@end
