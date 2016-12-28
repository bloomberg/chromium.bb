// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_view_interaction_test_util.h"

#import "base/mac/bind_objc_block.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/web_state/crw_web_view_scroll_view_proxy.h"
#import "ios/web/web_state/crw_web_view_proxy_impl.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"

using web::NavigationManager;

namespace web {
namespace test {

enum ElementAction {
  ELEMENT_ACTION_CLICK,
  ELEMENT_ACTION_FOCUS,
  ELEMENT_ACTION_SUBMIT
};

std::unique_ptr<base::Value> ExecuteJavaScript(web::WebState* web_state,
                                               const std::string& script) {
  __block std::unique_ptr<base::Value> result;
  __block bool did_finish = false;
  web_state->ExecuteJavaScript(base::UTF8ToUTF16(script),
                               base::BindBlock(^(const base::Value* value) {
                                 if (value)
                                   result = value->CreateDeepCopy();
                                 did_finish = true;
                               }));

  bool completed = testing::WaitUntilConditionOrTimeout(
      testing::kWaitForJSCompletionTimeout, ^{
        return did_finish;
      });
  if (!completed) {
    return nullptr;
  }

  // As result is marked __block, this return call does a copy and not a move
  // (marking the variable as __block mean it is allocated in the block object
  // and not the stack). Since the "return std::move()" pattern is discouraged
  // use a local variable.
  //
  // Fixes the following compilation failure:
  //   ../web_view_matchers.mm:ll:cc: error: call to implicitly-deleted copy
  //       constructor of 'std::unique_ptr<base::Value>'
  std::unique_ptr<base::Value> stack_result = std::move(result);
  return stack_result;
}

CGRect GetBoundingRectOfElementWithId(web::WebState* web_state,
                                      const std::string& element_id) {
  std::string kGetBoundsScript =
      "(function() {"
      "  var element = document.getElementById('" +
      element_id +
      "');"
      "  if (!element)"
      "    return {'error': 'Element " +
      element_id +
      " not found'};"
      "  var rect = element.getBoundingClientRect();"
      "  return {"
      "      'left': rect.left,"
      "      'top': rect.top,"
      "      'width': rect.right - rect.left,"
      "      'height': rect.bottom - rect.top,"
      "    };"
      "})();";

  __block base::DictionaryValue const* rect = nullptr;

  bool found =
      testing::WaitUntilConditionOrTimeout(testing::kWaitForUIElementTimeout, ^{
        std::unique_ptr<base::Value> value =
            ExecuteJavaScript(web_state, kGetBoundsScript);
        base::DictionaryValue* dictionary = nullptr;
        if (value && value->GetAsDictionary(&dictionary)) {
          std::string error;
          if (dictionary->GetString("error", &error)) {
            DLOG(ERROR) << "Error getting rect: " << error << ", retrying..";
          } else {
            rect = dictionary->DeepCopy();
            return true;
          }
        }
        return false;
      });

  if (!found)
    return CGRectNull;

  double left, top, width, height;
  if (!(rect->GetDouble("left", &left) && rect->GetDouble("top", &top) &&
        rect->GetDouble("width", &width) &&
        rect->GetDouble("height", &height))) {
    return CGRectNull;
  }

  CGFloat scale = [[web_state->GetWebViewProxy() scrollViewProxy] zoomScale];

  return CGRectMake(left * scale, top * scale, width * scale, height * scale);
}

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

  testing::WaitUntilConditionOrTimeout(testing::kWaitForJSCompletionTimeout, ^{
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
