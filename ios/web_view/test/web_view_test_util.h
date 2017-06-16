// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_TEST_WEB_VIEW_TEST_UTIL_H_
#define IOS_WEB_VIEW_TEST_WEB_VIEW_TEST_UTIL_H_

#import <Foundation/Foundation.h>

@class CWVWebView;

namespace ios_web_view {
namespace test {

// Creates web view with default configuration and frame equal to screen bounds.
CWVWebView* CreateWebView();

// Returns whether the element with |element_id| in the passed |web_view| has
// been tapped using a JavaScript click() event.
bool TapChromeWebViewElementWithId(CWVWebView* web_view, NSString* element_id);

// Waits until |script| is executed and synchronously returns the evaluation
// result.
id EvaluateJavaScript(CWVWebView* web_view, NSString* script, NSError** error);

// Waits for |web_view| to contain |text|. Returns false if the condition is not
// met within a timeout.
bool WaitForWebViewContainingTextOrTimeout(CWVWebView* web_view,
                                           NSString* text);

}  // namespace test
}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_TEST_WEB_VIEW_TEST_UTIL_H_
