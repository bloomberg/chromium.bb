// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_view_interaction_test_util.h"

#import <Foundation/Foundation.h>

#include "ios/testing/earl_grey/wait_util.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#include "ios/web/web_state/web_state_impl.h"

using web::NavigationManager;

namespace web {
namespace test {

void TapWebViewElementWithId(web::WebState* web_state,
                             const std::string& element_id) {
  RunActionOnWebViewElementWithId(web_state, element_id, CLICK);
}

void RunActionOnWebViewElementWithId(web::WebState* web_state,
                                     const std::string& element_id,
                                     ElementAction action) {
  CRWWebController* web_controller =
      static_cast<WebStateImpl*>(web_state)->GetWebController();
  const char* jsAction = nullptr;
  switch (action) {
    case CLICK:
      jsAction = ".click()";
      break;
    case FOCUS:
      jsAction = ".focus();";
      break;
  }
  NSString* script =
      [NSString stringWithFormat:@"document.getElementById('%s')%s",
                                 element_id.c_str(), jsAction];
  __block bool did_complete = false;
  [web_controller executeUserJavaScript:script
                      completionHandler:^(id, NSError*) {
                        did_complete = true;
                      }];

  testing::WaitUntilCondition(testing::kWaitForJSCompletionTimeout, ^{
    return did_complete;
  });
}

}  // namespace test
}  // namespace web
