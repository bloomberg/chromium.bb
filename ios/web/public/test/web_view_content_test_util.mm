// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_view_content_test_util.h"

#import <Foundation/Foundation.h>

#import "ios/testing/wait_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"

namespace {
// Script that returns document.body as a string.
char kGetDocumentBodyJavaScript[] =
    "document.body ? document.body.textContent : null";
}

using testing::WaitUntilConditionOrTimeout;
using testing::kWaitForUIElementTimeout;

namespace web {
namespace test {

bool IsWebViewContainingText(web::WebState* web_state,
                             const std::string& text) {
  std::unique_ptr<base::Value> value =
      web::test::ExecuteJavaScript(web_state, kGetDocumentBodyJavaScript);
  std::string body;
  if (value && value->GetAsString(&body)) {
    return body.find(text) != std::string::npos;
  }
  return false;
}

bool WaitForWebViewContainingText(web::WebState* web_state, std::string text) {
  return WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
    return IsWebViewContainingText(web_state, text);
  });
}

}  // namespace test
}  // namespace web
