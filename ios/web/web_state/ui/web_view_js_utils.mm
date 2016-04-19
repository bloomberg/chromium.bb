// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/web_view_js_utils.h"

#include <CoreFoundation/CoreFoundation.h>
#import <WebKit/WebKit.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"

namespace {

// Converts result of WKWebView script evaluation to a string.
NSString* StringResultFromWKResult(id result) {
  if (!result)
    return @"";

  CFTypeID result_type = CFGetTypeID(result);
  if (result_type == CFStringGetTypeID())
    return result;

  if (result_type == CFNumberGetTypeID())
    return [result stringValue];

  if (result_type == CFBooleanGetTypeID())
    return [result boolValue] ? @"true" : @"false";

  if (result_type == CFNullGetTypeID())
    return @"";

  // TODO(stuartmorgan): Stringify other types.
  NOTREACHED();
  return nil;
}

}  // namespace

namespace web {

NSString* const kJSEvaluationErrorDomain = @"JSEvaluationError";

std::unique_ptr<base::Value> ValueResultFromWKResult(id wk_result) {
  if (!wk_result)
    return nullptr;

  std::unique_ptr<base::Value> result;
  CFTypeID result_type = CFGetTypeID(wk_result);
  if (result_type == CFStringGetTypeID()) {
    result.reset(new base::StringValue(base::SysNSStringToUTF16(wk_result)));
    DCHECK(result->IsType(base::Value::TYPE_STRING));
  } else if (result_type == CFNumberGetTypeID()) {
    result.reset(new base::FundamentalValue([wk_result doubleValue]));
    DCHECK(result->IsType(base::Value::TYPE_DOUBLE));
  } else if (result_type == CFBooleanGetTypeID()) {
    result.reset(
        new base::FundamentalValue(static_cast<bool>([wk_result boolValue])));
    DCHECK(result->IsType(base::Value::TYPE_BOOLEAN));
  } else if (result_type == CFNullGetTypeID()) {
    result = base::Value::CreateNullValue();
    DCHECK(result->IsType(base::Value::TYPE_NULL));
  } else {
    NOTREACHED();  // Convert other types as needed.
  }
  return result;
}

void EvaluateJavaScript(WKWebView* web_view,
                        NSString* script,
                        JavaScriptCompletion completion_handler) {
  void (^web_view_completion_handler)(id, NSError*) = nil;
  // Do not create a web_view_completion_handler if no |completion_handler| is
  // passed to this function. WKWebView guarantees to call all completion
  // handlers before deallocation. Passing nil as completion handler (when
  // appropriate) may speed up web view deallocation, because there will be no
  // need to call those completion handlers.
  if (completion_handler) {
    web_view_completion_handler = ^(id result, NSError* error) {
      completion_handler(StringResultFromWKResult(result), error);
    };
  }
  ExecuteJavaScript(web_view, script, web_view_completion_handler);
}

void ExecuteJavaScript(WKWebView* web_view,
                       NSString* script,
                       JavaScriptResultBlock completion_handler) {
  DCHECK([script length]);
  if (!web_view && completion_handler) {
    dispatch_async(dispatch_get_main_queue(), ^{
      NSString* error_message =
          @"JS evaluation failed because there is no web view.";
      base::scoped_nsobject<NSError> error([[NSError alloc]
          initWithDomain:kJSEvaluationErrorDomain
                    code:JS_EVALUATION_ERROR_CODE_NO_WEB_VIEW
                userInfo:@{NSLocalizedDescriptionKey : error_message}]);
      completion_handler(nil, error);
    });
    return;
  }

  [web_view evaluateJavaScript:script completionHandler:completion_handler];
}

}  // namespace web
