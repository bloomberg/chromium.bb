// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_TEST_WEB_VIEW_TEST_UTIL_H_
#define IOS_WEB_VIEW_TEST_WEB_VIEW_TEST_UTIL_H_

#import <Foundation/Foundation.h>

#include "base/compiler_specific.h"

@class CWVWebView;

namespace ios_web_view {
namespace test {

// Creates web view with default configuration and frame equal to screen bounds.
CWVWebView* CreateWebView() WARN_UNUSED_RESULT;

// Loads |URL| in |web_view| and waits until the load completes. Asserts if
// loading does not complete.
bool LoadUrl(CWVWebView* web_view, NSURL* url) WARN_UNUSED_RESULT;

// Returns whether the element with |element_id| in the passed |web_view| has
// been tapped using a JavaScript click() event.
bool TapWebViewElementWithId(CWVWebView* web_view,
                             NSString* element_id) WARN_UNUSED_RESULT;

// Waits until |script| is executed and synchronously returns the evaluation
// result.
id EvaluateJavaScript(CWVWebView* web_view, NSString* script, NSError** error);

// Waits for |web_view| to contain |text|. Returns false if the condition is not
// met within a timeout.
bool WaitForWebViewContainingTextOrTimeout(CWVWebView* web_view,
                                           NSString* text) WARN_UNUSED_RESULT;

// Waits until |web_view| stops loading. Returns false if the condition is not
// met within a timeout.
bool WaitForWebViewLoadCompletionOrTimeout(CWVWebView* web_view)
    WARN_UNUSED_RESULT;

}  // namespace test
}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_TEST_WEB_VIEW_TEST_UTIL_H_
