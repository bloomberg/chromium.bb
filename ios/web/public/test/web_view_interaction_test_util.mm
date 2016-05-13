// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_view_interaction_test_util.h"

#import <Foundation/Foundation.h>

#include "base/mac/bind_objc_block.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "ios/testing/earl_grey/wait_util.h"

using web::NavigationManager;

namespace web {
namespace test {

void TapWebViewElementWithId(web::WebState* web_state,
                             const std::string& element_id) {
  const char kJsClick[] = "document.getElementById('%s').click()";
  __block bool did_complete = false;
  web_state->ExecuteJavaScript(
      base::UTF8ToUTF16(base::StringPrintf(kJsClick, element_id.c_str())),
      base::BindBlock(^(const base::Value*) {
        did_complete = true;
      }));

  // TODO(crbug.com/610837): Re-factor wait code into common location.
  NSDate* deadline = [NSDate
      dateWithTimeIntervalSinceNow:testing::kWaitForJSCompletionTimeout];
  while (([[NSDate date] compare:deadline] != NSOrderedDescending) &&
         !did_complete) {
    base::test::ios::SpinRunLoopWithMaxDelay(
        base::TimeDelta::FromSecondsD(testing::kSpinDelaySeconds));
  }
}

}  // namespace test
}  // namespace web