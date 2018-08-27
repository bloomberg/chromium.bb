// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_controller_helper.h"

#include <stddef.h>

#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#import "components/password_manager/ios/js_password_manager.h"
#import "components/password_manager/ios/password_controller_helper.h"
#include "ios/web/public/test/fakes/test_web_client.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::PasswordForm;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {
// Returns a string containing the JavaScript loaded from a
// bundled resource file with the given name (excluding extension).
NSString* GetPageScript(NSString* script_file_name) {
  EXPECT_NE(nil, script_file_name);
  NSString* path =
      [base::mac::FrameworkBundle() pathForResource:script_file_name
                                             ofType:@"js"];
  EXPECT_NE(nil, path);
  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  EXPECT_EQ(nil, error);
  EXPECT_NE(nil, content);
  return content;
}

class TestWebClientWithScript : public web::TestWebClient {
 public:
  NSString* GetDocumentStartScriptForMainFrame(
      web::BrowserState* browser_state) const override {
    return GetPageScript(@"test_bundle");
  }
};
}

@interface PasswordControllerHelper (Testing)

// Provides access to JavaScript Manager for testing with mocks.
@property(readonly) JsPasswordManager* jsPasswordManager;

// Provides access to the method below for testing with mocks.
- (void)extractSubmittedPasswordForm:(const std::string&)formName
                   completionHandler:
                       (void (^)(BOOL found,
                                 const PasswordForm& form))completionHandler;

@end

class PasswordControllerHelperTest : public web::WebTestWithWebState {
 public:
  PasswordControllerHelperTest()
      : web::WebTestWithWebState(std::make_unique<TestWebClientWithScript>()) {}

  ~PasswordControllerHelperTest() override = default;

  void SetUp() override {
    WebTestWithWebState::SetUp();
    helper_ = [[PasswordControllerHelper alloc] initWithWebState:web_state()
                                                        delegate:nil];
  }

  void TearDown() override {
    WaitForBackgroundTasks();
    helper_ = nil;
    web::WebTestWithWebState::TearDown();
  }

 protected:
  // Returns an identifier for the |form_index|th form in the page.
  std::string GetFormId(int form_index) {
    NSString* kGetFormIdScript =
        @"__gCrWeb.form.getFormIdentifier("
         "    document.querySelectorAll('form')[%d]);";
    return base::SysNSStringToUTF8(ExecuteJavaScript(
        [NSString stringWithFormat:kGetFormIdScript, form_index]));
  }

  // PasswordControllerHelper for testing.
  PasswordControllerHelper* helper_;

  DISALLOW_COPY_AND_ASSIGN(PasswordControllerHelperTest);
};

struct GetSubmittedPasswordFormTestData {
  // HTML String of the form.
  NSString* html_string;
  // Javascript to submit the form.
  NSString* java_script;
  // 0 based index of the form on the page to submit.
  const int index_of_the_form_to_submit;
  // True if expected to find the form on submission.
  const bool expected_form_found;
  // Expected username element.
  const char* expected_username_element;
};

// Check that HTML forms are captured and converted correctly into
// PasswordForms on submission.
TEST_F(PasswordControllerHelperTest, GetSubmittedPasswordForm) {
  // clang-format off
  const GetSubmittedPasswordFormTestData test_data[] = {
    // Two forms with no explicit names.
    {
      @"<form action='javascript:;'>"
      "<input type='text' name='user1' value='user1'>"
      "<input type='password' name='pass1' value='pw1'>"
      "</form>"
      "<form action='javascript:;'>"
      "<input type='text' name='user2' value='user2'>"
      "<input type='password' name='pass2' value='pw2'>"
      "<input type='submit' id='s2'>"
      "</form>",
      @"document.getElementById('s2').click()",
      1, true, "user2"
    },
    // Two forms with explicit names.
    {
      @"<form name='test2a' action='javascript:;'>"
      "<input type='text' name='user1' value='user1'>"
      "<input type='password' name='pass1' value='pw1'>"
      "<input type='submit' id='s1'>"
      "</form>"
      "<form name='test2b' action='javascript:;' value='user2'>"
      "<input type='text' name='user2'>"
      "<input type='password' name='pass2' value='pw2'>"
      "</form>",
      @"document.getElementById('s1').click()",
      0, true, "user1"
    },
    // No password forms.
    {
      @"<form action='javascript:;'>"
      "<input type='text' name='user1' value='user1'>"
      "<input type='text' name='not_pass1' value='text1'>"
      "<input type='submit' id='s1'>"
      "</form>",
      @"document.getElementById('s1').click()",
      0, false, nullptr
    },
    // Form with quotes in the form and field names.
    {
      @"<form name=\"foo'\" action='javascript:;'>"
      "<input type='text' name=\"user1'\" value='user1'>"
      "<input type='password' id='s1' name=\"pass1'\" value='pw2'>"
      "</form>",
      @"document.getElementById('s1').click()",
      0, true, "user1'"
    },
  };
  // clang-format on

  for (const GetSubmittedPasswordFormTestData& data : test_data) {
    SCOPED_TRACE(testing::Message() << "for html_string=" << data.html_string
                                    << " and java_script=" << data.java_script
                                    << " and index_of_the_form_to_submit="
                                    << data.index_of_the_form_to_submit);
    LoadHtml(data.html_string);
    ExecuteJavaScript(data.java_script);
    __block BOOL block_was_called = NO;
    id completion_handler = ^(BOOL found, const PasswordForm& form) {
      block_was_called = YES;
      ASSERT_EQ(data.expected_form_found, found);
      if (data.expected_form_found) {
        EXPECT_EQ(base::ASCIIToUTF16(data.expected_username_element),
                  form.username_element);
      }
    };
    [helper_
        extractSubmittedPasswordForm:GetFormId(data.index_of_the_form_to_submit)
                   completionHandler:completion_handler];
    EXPECT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
          return block_was_called;
        }));
  }
}
