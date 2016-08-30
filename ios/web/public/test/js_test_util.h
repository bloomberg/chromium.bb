// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_JS_TEST_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_JS_TEST_UTIL_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

@class CRWJSInjectionManager;
@class CRWJSInjectionReceiver;

namespace web {

// These functions synchronously execute JavaScript and return result as id.
// id will be backed up by different classes depending on resulting JS type:
// NSString (string), NSNumber (number or boolean), NSDictionary (object),
// NSArray (array), NSNull (null), NSDate (Date), nil (undefined or execution
// exception).

// Executes JavaScript on the |manager| and returns the result as an id.
id ExecuteJavaScript(CRWJSInjectionManager* manager, NSString* script);

// Executes JavaScript on the |receiver| and returns the result as an id.
id ExecuteJavaScript(CRWJSInjectionReceiver* receiver, NSString* script);

// Executes JavaScript on |web_view| and returns the result as an id.
// |error| can be null and will be updated only if script execution fails.
id ExecuteJavaScript(WKWebView* web_view, NSString* script, NSError** error);

// Executes JavaScript on |web_view| and returns the result as an id.
// Fails if there was an error during script execution.
id ExecuteJavaScript(WKWebView* web_view, NSString* script);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_JS_TEST_UTIL_H_
