// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/service_manager_app_interface.h"

#import "base/bind.h"
#import "base/bind_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/service_manager_connection.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/shell/test/app/web_shell_test_util.h"
#import "ios/web/shell/web_usage_controller.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/test/echo/public/mojom/echo.mojom.h"
#include "services/test/user_id/public/mojom/user_id.mojom.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::shell_test_util::GetCurrentWebState;

@implementation ServiceManagerTestAppInterface

+ (void)echoAndLogString:(NSString*)string {
  // Connect to the echo service once and bind an Echo instance.
  static echo::mojom::EchoPtr echo;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    web::ServiceManagerConnection::Get()->GetConnector()->BindInterface(
        "echo", mojo::MakeRequest(&echo));
  });

  std::string UTF8String = base::SysNSStringToUTF8(string);
  echo->EchoString(UTF8String, base::BindOnce(^(const std::string& UTF8Reply) {
                     NSString* reply = base::SysUTF8ToNSString(UTF8Reply);
                     [[self mutableLogs] addObject:reply];
                   }));
}

+ (void)logInstanceGroup {
  // Connect to the user ID service once and bind a UserId instance.
  static user_id::mojom::UserIdPtr userID;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    web::BrowserState::GetConnectorFor(GetCurrentWebState()->GetBrowserState())
        ->BindInterface("user_id", mojo::MakeRequest(&userID));
  });

  // Call GetInstanceGroup(), making sure to keep this end of the connection
  // alive until the callback is received.
  userID->GetInstanceGroup(base::BindOnce(^(const base::Token& token) {
    [[self mutableLogs] addObject:base::SysUTF8ToNSString(token.ToString())];
  }));
}

+ (void)setWebUsageEnabled:(BOOL)enabled {
  // Connect to a mojom::WebUsageController instance associated with
  // current WebState.
  static web::mojom::WebUsageControllerPtr webUsageController;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    GetCurrentWebState()->BindInterfaceRequestFromMainFrame(
        mojo::MakeRequest(&webUsageController));
  });

  webUsageController->SetWebUsageEnabled(
      enabled, base::BindOnce(base::DoNothing::Once()));
}

+ (NSArray*)logs {
  return [[self mutableLogs] copy];
}

+ (void)clearLogs {
  [[self mutableLogs] removeAllObjects];
}
#pragma mark - Private

+ (NSMutableArray*)mutableLogs {
  static NSMutableArray* logs = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    logs = [[NSMutableArray alloc] init];
  });
  return logs;
}

@end
