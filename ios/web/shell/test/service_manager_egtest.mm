// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#import "ios/testing/wait_util.h"
#include "ios/web/public/service_manager_connection.h"
#import "ios/web/shell/test/earl_grey/shell_earl_grey.h"
#import "ios/web/shell/test/earl_grey/web_shell_test_case.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/test/echo/public/interfaces/echo.mojom.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char* kTestInput = "Livingston, I presume";

// Callback passed to echo::mojom::Echo::EchoString(). Verifies that the echoed
// string has the expected value and sets |echo_callback_called_flag| to true
// to indicate that the callback was invoked. |echo| is passed simply to ensure
// that our connection to the Echo implementation remains alive long enough for
// the callback to reach us.
void OnEchoString(echo::mojom::EchoPtr echo,
                  bool* echo_callback_called_flag,
                  const std::string& echoed_input) {
  GREYAssert(kTestInput == echoed_input,
             @"Unexpected string passed to echo callback: %s",
             echoed_input.c_str());
  *echo_callback_called_flag = true;
}

// Waits until the callback to echo::mojom::Echo::EchoString() is invoked (as
// signalled by that callback setting |echo_callback_called_flag| to true).
void WaitForEchoStringCallback(bool* echo_callback_called_flag) {
  GREYCondition* condition =
      [GREYCondition conditionWithName:@"Wait for echo string callback"
                                 block:^BOOL {
                                   return *echo_callback_called_flag;
                                 }];
  GREYAssert([condition waitWithTimeout:testing::kWaitForUIElementTimeout],
             @"Failed waiting for echo callback");
}
}

// Service Manager test cases for the web shell.
@interface ServiceManagerTestCase : WebShellTestCase
@end

@implementation ServiceManagerTestCase

// Tests that it is possible to connect to an embedded service that was
// registered by web_shell.
- (void)testConnectionToEmbeddedService {
  // Connect to the echo service and bind an Echo instance.
  echo::mojom::EchoPtr echo;
  web::ServiceManagerConnection* connection =
      web::ServiceManagerConnection::Get();
  DCHECK(connection);
  connection->GetConnector()->BindInterface("echo", mojo::MakeRequest(&echo));

  // Call EchoString, making sure to keep our end of the connection alive
  // until the callback is received.
  echo::mojom::Echo* raw_echo = echo.get();
  bool echo_callback_called = false;
  raw_echo->EchoString(kTestInput,
                       base::BindOnce(&OnEchoString, base::Passed(&echo),
                                      &echo_callback_called));

  WaitForEchoStringCallback(&echo_callback_called);
}

@end
