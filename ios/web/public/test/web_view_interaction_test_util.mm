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

enum ElementAction {
  ELEMENT_ACTION_CLICK,
  ELEMENT_ACTION_FOCUS,
  ELEMENT_ACTION_SUBMIT
};

// Returns whether the Javascript action specified by |action| ran on
// |element_id| in the passed |web_state|.
bool RunActionOnWebViewElementWithId(web::WebState* web_state,
                                     const std::string& element_id,
                                     ElementAction action) {
  CRWWebController* web_controller =
      static_cast<WebStateImpl*>(web_state)->GetWebController();
  const char* js_action = nullptr;
  switch (action) {
    case ELEMENT_ACTION_CLICK:
      js_action = ".click()";
      break;
    case ELEMENT_ACTION_FOCUS:
      js_action = ".focus();";
      break;
    case ELEMENT_ACTION_SUBMIT:
      js_action = ".submit();";
      break;
  }
  NSString* script = [NSString
      stringWithFormat:@"(function() {"
                        "  var element = document.getElementById('%s');"
                        "  if (element) {"
                        "    element%s;"
                        "    return true;"
                        "  }"
                        "  return false;"
                        "})();",
                       element_id.c_str(), js_action];
  __block bool did_complete = false;
  __block bool element_found = false;
  [web_controller executeUserJavaScript:script
                      completionHandler:^(id result, NSError*) {
                        did_complete = true;
                        element_found = [result boolValue];
                      }];

  testing::WaitUntilCondition(testing::kWaitForJSCompletionTimeout, ^{
    return did_complete;
  });

  return element_found;
}

bool TapWebViewElementWithId(web::WebState* web_state,
                             const std::string& element_id) {
  return RunActionOnWebViewElementWithId(web_state, element_id,
                                         ELEMENT_ACTION_CLICK);
}

bool FocusWebViewElementWithId(web::WebState* web_state,
                               const std::string& element_id) {
  return RunActionOnWebViewElementWithId(web_state, element_id,
                                         ELEMENT_ACTION_FOCUS);
}

bool SubmitWebViewFormWithId(web::WebState* web_state,
                             const std::string& form_id) {
  return RunActionOnWebViewElementWithId(web_state, form_id,
                                         ELEMENT_ACTION_SUBMIT);
}

}  // namespace test
}  // namespace web
