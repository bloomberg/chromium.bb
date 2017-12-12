// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/fakes/test_web_state_observer.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

using FormJsTest = WebTestWithWebState;

// Tests that keyup event correctly delivered to WebStateObserver.
TEST_F(FormJsTest, KeyUpEvent) {
  TestWebStateObserver observer(web_state());
  LoadHtml(@"<p></p>");
  ASSERT_FALSE(observer.form_activity_info());
  ExecuteJavaScript(@"document.dispatchEvent(new KeyboardEvent('keyup'));");
  TestFormActivityInfo* info = observer.form_activity_info();
  ASSERT_TRUE(info);
  EXPECT_EQ("keyup", info->form_activity.type);
  EXPECT_FALSE(info->form_activity.input_missing);
}

// Tests that focus event correctly delivered to WebStateObserver.
TEST_F(FormJsTest, FocusMainFrame) {
  TestWebStateObserver observer(web_state());
  LoadHtml(
      @"<form>"
       "<input type='text' name='username' id='id1'>"
       "<input type='password' name='password' id='id2'>"
       "</form>");
  ASSERT_FALSE(observer.form_activity_info());
  ExecuteJavaScript(@"document.getElementById('id1').focus();");
  TestFormActivityInfo* info = observer.form_activity_info();
  ASSERT_TRUE(info);
  EXPECT_EQ("focus", info->form_activity.type);
  EXPECT_FALSE(info->form_activity.input_missing);
}

// Tests that submit event correctly delivered to WebStateObserver.
TEST_F(FormJsTest, FormSubmitMainFrame) {
  TestWebStateObserver observer(web_state());
  LoadHtml(
      @"<form id='form1'>"
       "<input type='password'>"
       "<input type='submit' id='submit_input'/>"
       "</form>");
  ASSERT_FALSE(observer.submit_document_info());
  ExecuteJavaScript(@"document.getElementById('submit_input').click();");
  TestSubmitDocumentInfo* info = observer.submit_document_info();
  ASSERT_TRUE(info);
  EXPECT_EQ("form1", info->form_name);
}

}  // namespace web
