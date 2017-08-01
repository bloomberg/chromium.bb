// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/content_widget_extension/content_widget_view_controller.h"

#import <NotificationCenter/NotificationCenter.h>

#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/ntp/ntp_tile.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/chrome/content_widget_extension/content_widget_view.h"

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

@interface ContentWidgetViewController ()
@property(nonatomic, weak) ContentWidgetView* widgetView;
@property(nonatomic, strong) NSArray<NTPTile*>* sites;

// Updates the widget with latest data. Returns whether any visual updates
// occurred.
- (BOOL)updateWidget;
// Opens the main application with the given |URL|.
- (void)openAppWithURL:(NSString*)URL;
@end

@implementation ContentWidgetViewController

@synthesize sites = _sites;
@synthesize widgetView = _widgetView;

- (instancetype)init {
  self = [super init];
  if (self) {
    NSUserDefaults* sharedDefaults = [[NSUserDefaults alloc]
        initWithSuiteName:app_group::ApplicationGroup()];
    _sites = [NSKeyedUnarchiver
        unarchiveObjectWithData:[sharedDefaults
                                    objectForKey:app_group::kSuggestedItems]];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // A local variable is necessary here as the property is declared weak and the
  // object would be deallocated before being retained by the addSubview call.
  ContentWidgetView* widgetView = [[ContentWidgetView alloc] init];
  self.widgetView = widgetView;
  [self.view addSubview:self.widgetView];

  if (base::ios::IsRunningOnIOS10OrLater()) {
    self.extensionContext.widgetLargestAvailableDisplayMode =
        NCWidgetDisplayModeExpanded;
  }

  self.widgetView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.view.leadingAnchor
        constraintEqualToAnchor:self.widgetView.leadingAnchor],
    [self.view.trailingAnchor
        constraintEqualToAnchor:self.widgetView.trailingAnchor],
    [self.view.topAnchor constraintEqualToAnchor:self.widgetView.topAnchor],
    [self.view.bottomAnchor
        constraintEqualToAnchor:self.widgetView.bottomAnchor]
  ]];
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

#pragma mark - NCWidgetProviding

- (void)widgetActiveDisplayModeDidChange:(NCWidgetDisplayMode)activeDisplayMode
                         withMaximumSize:(CGSize)maxSize {
  BOOL isVariableHeight = (activeDisplayMode == NCWidgetDisplayModeExpanded);

  // If the widget's height is not variable, the preferredContentSize is the
  // maxSize. Widgets cannot be shrunk, and this ensures the view will lay
  // itself out according to the actual screen size. (This is only likely to
  // happen if the accessibility option for larger font is used.) If the widget
  // is not a fixed size, if the fitting size for the widget's contents is
  // larger than the maximum size for the current widget display mode, this
  // maximum size is used for the widget. Otherwise, the preferredContentSize is
  // set to the fitting size so that the widget gets the correct height.
  if (isVariableHeight) {
    CGSize fittingSize = [self.widgetView
        systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];
    if (fittingSize.height < maxSize.height) {
      self.preferredContentSize = fittingSize;
      return;
    }
  }
  self.preferredContentSize = maxSize;
}

// Implementing this method removes the leading edge inset for iOS version < 10.
// TODO(crbug.com/720490): Remove implementation when dropping ios9 support.
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_10_0
- (UIEdgeInsets)widgetMarginInsetsForProposedMarginInsets:
    (UIEdgeInsets)defaultMa‌​rginInsets {
  return UIEdgeInsetsZero;
}
#endif

#pragma mark - internal

- (BOOL)updateWidget {
  NSUserDefaults* sharedDefaults =
      [[NSUserDefaults alloc] initWithSuiteName:app_group::ApplicationGroup()];
  NSMutableArray<NTPTile*>* newSites = [NSKeyedUnarchiver
      unarchiveObjectWithData:[sharedDefaults
                                  objectForKey:app_group::kSuggestedItems]];

  if (newSites == self.sites) {
    return NO;
  }
  return YES;
}

- (void)openAppWithURL:(NSString*)URL {
  NSUserDefaults* sharedDefaults =
      [[NSUserDefaults alloc] initWithSuiteName:app_group::ApplicationGroup()];
  NSString* defaultsKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandPreference);

  NSString* timePrefKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTimePreference);
  NSString* appPrefKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandAppPreference);
  NSString* commandPrefKey = base::SysUTF8ToNSString(
      app_group::kChromeAppGroupCommandCommandPreference);
  NSString* paramPrefKey = base::SysUTF8ToNSString(
      app_group::kChromeAppGroupCommandParameterPreference);

  NSDictionary* commandDict = @{
    timePrefKey : [NSDate date],
    appPrefKey : @"TodayExtension",
    commandPrefKey :
        base::SysUTF8ToNSString(app_group::kChromeAppGroupOpenURLCommand),
    paramPrefKey : URL,
  };

  [sharedDefaults setObject:commandDict forKey:defaultsKey];
  [sharedDefaults synchronize];

  NSString* scheme = base::mac::ObjCCast<NSString>([[NSBundle mainBundle]
      objectForInfoDictionaryKey:@"KSChannelChromeScheme"]);
  if (!scheme)
    return;

  NSURLComponents* urlComponents = [NSURLComponents new];
  urlComponents.scheme = scheme;
  urlComponents.host = kXCallbackURLHost;
  urlComponents.path = [@"/"
      stringByAppendingString:base::SysUTF8ToNSString(
                                  app_group::kChromeAppGroupXCallbackCommand)];

  NSURL* openURL = [urlComponents URL];
  [self.extensionContext openURL:openURL completionHandler:nil];
}

@end
