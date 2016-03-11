// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_WEB_VIEW_JS_UTILS_H_
#define IOS_WEB_WEB_STATE_UI_WEB_VIEW_JS_UTILS_H_

#import <Foundation/Foundation.h>

#include "ios/web/public/block_types.h"

@class UIWebView;
@class WKWebView;

// This file contains functions that asynchronously evaluate JavaScript on
// WKWebView/UIWebView and provide result in the same format. Call
// |completion_handler| with results of the evaluation or an NSError if there is
// an error. The |completion_handler| can be nil.
namespace web {

// The domain for JS evaluation NSErrors in web.
extern NSString* const kJSEvaluationErrorDomain;

// The type of errors that can occur while evaluating JS.
enum JSEvaluationErrorCode {
  // No web view present to evaluate JS.
  JS_EVALUATION_ERROR_CODE_NO_WEB_VIEW = 1,
};

// The error code for JS evaluation NSErrors.
extern const NSInteger kJSEvaluationErrorCode;

// Asynchronous adaptor to evaluate JavaScript on UIWebView. Provides evaluation
// result as it is, without modifications.
void EvaluateJavaScript(UIWebView* web_view,
                        NSString* script,
                        JavaScriptCompletion completion_handler);

// Evaluates JavaScript on WKWebView. Provides evaluation result in the same
// format as UIWebView. If the web view cannot evaluate JS at the moment,
// |completion_handler| is called with an NSError.
void EvaluateJavaScript(WKWebView* web_view,
                        NSString* script,
                        JavaScriptCompletion completion_handler);

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_UI_WEB_VIEW_JS_UTILS_H_
