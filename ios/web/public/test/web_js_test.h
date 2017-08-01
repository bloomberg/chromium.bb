// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_WEB_JS_TEST_H_
#define IOS_WEB_PUBLIC_TEST_WEB_JS_TEST_H_

#import <Foundation/Foundation.h>

#import "base/mac/bundle_locations.h"
#import "base/mac/scoped_nsobject.h"
#import "testing/gtest_mac.h"

namespace web {

// Base fixture mixin for testing JavaScripts.
template <class WebTestT>
class WebJsTest : public WebTestT {
 public:
  WebJsTest(NSArray* java_script_paths)
      : java_script_paths_([java_script_paths copy]) {}

 protected:
  // Loads |html| and inject JavaScripts at |javaScriptPaths_|.
  void LoadHtmlAndInject(NSString* html) {
    WebTestT::LoadHtml(html);
    Inject();
  }

  // Returns an id representation of the JavaScript's evaluation results;
  // the JavaScript is passed in as a |format| and its arguments.
  id ExecuteJavaScriptWithFormat(NSString* format, ...)
      __attribute__((format(__NSString__, 2, 3)));

  // Helper method that EXPECTs the |java_script| evaluation results on each
  // element obtained by scripts in |get_element_javas_cripts|; the expected
  // result is the corresponding entry in |expected_results|.
  void ExecuteJavaScriptOnElementsAndCheck(NSString* java_script,
                                           NSArray* get_element_java_scripts,
                                           NSArray* expected_results);

  // Helper method that EXPECTs the |java_script| evaluation results on each
  // element obtained by JavaScripts in |get_element_java_scripts|. The
  // expected results are boolean and are true only for elements in
  // |get_element_java_scripts_expecting_true| which is subset of
  // |get_element_java_scripts|.
  void ExecuteBooleanJavaScriptOnElementsAndCheck(
      NSString* java_script,
      NSArray* get_element_java_scripts,
      NSArray* get_element_java_scripts_expecting_true);

 private:
  // Injects JavaScript at |java_script_paths_|.
  void Inject();

  base::scoped_nsobject<NSArray> java_script_paths_;
};

template <class WebTestT>
void WebJsTest<WebTestT>::Inject() {
  // Main web injection should have occurred.
  ASSERT_NSEQ(@"object", WebTestT::ExecuteJavaScript(@"typeof __gCrWeb"));

  for (NSString* java_script_path in java_script_paths_.get()) {
    NSString* path =
        [base::mac::FrameworkBundle() pathForResource:java_script_path
                                               ofType:@"js"];
    WebTestT::ExecuteJavaScript([NSString
        stringWithContentsOfFile:path
                        encoding:NSUTF8StringEncoding
                           error:nil]);
  }
}

template <class WebTestT>
id WebJsTest<WebTestT>::ExecuteJavaScriptWithFormat(NSString* format, ...) {
  va_list args;
  va_start(args, format);
  base::scoped_nsobject<NSString> java_script(
      [[NSString alloc] initWithFormat:format arguments:args]);
  va_end(args);

  return WebTestT::ExecuteJavaScript(java_script);
}

template <class WebTestT>
void WebJsTest<WebTestT>::ExecuteJavaScriptOnElementsAndCheck(
    NSString* java_script,
    NSArray* get_element_java_scripts,
    NSArray* expected_results) {
  for (NSUInteger i = 0; i < get_element_java_scripts.count; ++i) {
    EXPECT_NSEQ(
        expected_results[i],
        ExecuteJavaScriptWithFormat(java_script, get_element_java_scripts[i]));
  }
}

template <class WebTestT>
void WebJsTest<WebTestT>::ExecuteBooleanJavaScriptOnElementsAndCheck(
    NSString* java_script,
    NSArray* get_element_java_scripts,
    NSArray* get_element_java_scripts_expecting_true) {
  for (NSString* get_element_java_script in get_element_java_scripts) {
    BOOL expected = [get_element_java_scripts_expecting_true
        containsObject:get_element_java_script];
    EXPECT_NSEQ(@(expected), ExecuteJavaScriptWithFormat(
                                 java_script, get_element_java_script))
        << [NSString stringWithFormat:@"%@ on %@ should return %d", java_script,
                                      get_element_java_script, expected];
  }
}

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_WEB_JS_TEST_H_
