// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activities/send_tab_to_self_activity.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/send_tab_to_self_command.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kSendTabToSelfActivityType =
    @"com.google.chrome.sendTabToSelfActivity";

}  // namespace

@interface SendTabToSelfActivity ()
// The dispatcher that handles when the activity is performed.
@property(nonatomic, weak, readonly) id<BrowserCommands> dispatcher;
@end

@implementation SendTabToSelfActivity

@synthesize dispatcher = _dispatcher;

+ (NSString*)activityIdentifier {
  return kSendTabToSelfActivityType;
}

- (instancetype)initWithDispatcher:(id<BrowserCommands>)dispatcher {
  if (self = [super init]) {
    // TODO(crbug.com/944596): Set the target device id.
    _dispatcher = dispatcher;
  }
  return self;
}

#pragma mark - UIActivity

- (NSString*)activityType {
  return [[self class] activityIdentifier];
}

- (NSString*)activityTitle {
  return l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
}

- (UIImage*)activityImage {
  return [UIImage imageNamed:@"activity_services_send_tab_to_self"];
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  return YES;
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  SendTabToSelfCommand* command =
      [[SendTabToSelfCommand alloc] initWithTargetDeviceId:@""];
  [_dispatcher sendTabToSelf:command];
  [self activityDidFinish:YES];
}

@end