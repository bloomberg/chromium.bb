// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/widget_extension/widget_view_controller.h"

#import <NotificationCenter/NotificationCenter.h>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/chrome/common/x_callback_url.h"
#import "ios/chrome/widget_extension/widget_view.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WidgetViewController ()<WidgetViewActionTarget>
@property(nonatomic, weak) WidgetView* widgetView;
@end

@implementation WidgetViewController

@synthesize widgetView = _widgetView;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // A local variable is necessary here as the property is declared weak and the
  // object would be deallocated before being retained by the addSubview call.
  WidgetView* widgetView = [[WidgetView alloc] initWithActionTarget:self];
  self.widgetView = widgetView;
  [self.view addSubview:self.widgetView];

  [self.widgetView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [NSLayoutConstraint activateConstraints:@[
    [self.widgetView.leadingAnchor
        constraintEqualToAnchor:[self.view leadingAnchor]],
    [self.widgetView.trailingAnchor
        constraintEqualToAnchor:[self.view trailingAnchor]],
    [self.widgetView.heightAnchor
        constraintEqualToAnchor:[self.view heightAnchor]],
    [self.widgetView.widthAnchor
        constraintEqualToAnchor:[self.view widthAnchor]]
  ]];
}

- (void)openApp:(id)sender {
  NSUserDefaults* sharedDefaults =
      [[NSUserDefaults alloc] initWithSuiteName:app_group::ApplicationGroup()];
  NSString* defaultsKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandPreference);
  [sharedDefaults setObject:[WidgetViewController commandDict]
                     forKey:defaultsKey];
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

+ (NSDictionary*)commandDict {
  NSString* command =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupFocusOmniboxCommand);
  NSString* timePrefKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTimePreference);
  NSString* appPrefKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandAppPreference);
  NSString* commandPrefKey = base::SysUTF8ToNSString(
      app_group::kChromeAppGroupCommandCommandPreference);
  return @{
    timePrefKey : [NSDate date],
    appPrefKey : @"TodayExtension",
    commandPrefKey : command,
  };
}

@end
