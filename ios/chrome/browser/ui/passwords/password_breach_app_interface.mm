// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_breach_app_interface.h"

#import "ios/chrome/browser/ui/commands/password_breach_commands.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PasswordBreachAppInterface

+ (void)showPasswordBreach {
  auto handler = chrome_test_util::HandlerForActiveBrowser();
  auto leakType = password_manager::CreateLeakType(
      password_manager::IsSaved(true), password_manager::IsReused(false),
      password_manager::IsSyncing(true));
  [(id<PasswordBreachCommands>)handler
      showPasswordBreachForLeakType:leakType
                                URL:GURL("example.com")];
}

@end
