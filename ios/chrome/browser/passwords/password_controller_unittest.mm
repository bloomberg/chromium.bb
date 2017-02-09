// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_controller.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/mac/bind_objc_block.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/values.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_controller.h"
#import "ios/chrome/browser/autofill/form_suggestion_controller.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/passwords/js_password_manager.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state/web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/OCMock/OCPartialMockObject.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::PasswordForm;
using autofill::PasswordFormFillData;
using testing::Return;

namespace {

class MockWebState : public web::TestWebState {
 public:
  MOCK_CONST_METHOD0(GetBrowserState, web::BrowserState*(void));
};

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  explicit MockPasswordManagerClient(password_manager::PasswordStore* store)
      : store_(store) {}

  ~MockPasswordManagerClient() override = default;

  MOCK_CONST_METHOD0(GetLogManager, password_manager::LogManager*(void));

  PrefService* GetPrefs() override { return &prefs_; }

  password_manager::PasswordStore* GetPasswordStore() const override {
    return store_;
  }

 private:
  TestingPrefServiceSimple prefs_;
  password_manager::PasswordStore* const store_;
};

class MockLogManager : public password_manager::LogManager {
 public:
  MOCK_CONST_METHOD1(LogSavePasswordProgress, void(const std::string& text));
  MOCK_CONST_METHOD0(IsLoggingActive, bool(void));

  // Methods not important for testing.
  void OnLogRouterAvailabilityChanged(bool router_can_be_used) override {}
  void SetSuspended(bool suspended) override {}
};

// Creates PasswordController with the given |web_state| and a mock client
// using the given |store|. If not null, |weak_client| is filled with a
// non-owning pointer to the created client. The created controller is
// returned.
PasswordController* CreatePasswordController(
    web::WebState* web_state,
    password_manager::PasswordStore* store,
    MockPasswordManagerClient** weak_client) {
  auto client = base::MakeUnique<MockPasswordManagerClient>(store);
  if (weak_client)
    *weak_client = client.get();
  return [[PasswordController alloc] initWithWebState:web_state
                                  passwordsUiDelegate:nil
                                               client:std::move(client)];
}

}  // namespace

@interface PasswordController (
    Testing)<CRWWebStateObserver, FormSuggestionProvider>

- (void)findPasswordFormsWithCompletionHandler:
    (void (^)(const std::vector<PasswordForm>&))completionHandler;

- (void)extractSubmittedPasswordForm:(const std::string&)formName
                   completionHandler:
                       (void (^)(BOOL found,
                                 const PasswordForm& form))completionHandler;

- (void)fillPasswordForm:(const PasswordFormFillData&)formData
       completionHandler:(void (^)(BOOL))completionHandler;

- (BOOL)getPasswordForm:(PasswordForm*)form
         fromDictionary:(const base::DictionaryValue*)dictionary
                pageURL:(const GURL&)pageLocation;

// Provides access to JavaScript Manager for testing with mocks.
@property(readonly) JsPasswordManager* passwordJsManager;

@end

// Real FormSuggestionController is wrapped to register the addition of
// suggestions.
@interface PasswordsTestSuggestionController : FormSuggestionController

@property(nonatomic, copy) NSArray* suggestions;

@end

@implementation PasswordsTestSuggestionController

@synthesize suggestions = _suggestions;

- (void)updateKeyboardWithSuggestions:(NSArray*)suggestions {
  self.suggestions = suggestions;
}


@end

class PasswordControllerTest : public web::WebTestWithWebState {
 public:
  PasswordControllerTest()
      : store_(new testing::NiceMock<password_manager::MockPasswordStore>()) {}

  ~PasswordControllerTest() override { store_->ShutdownOnUIThread(); }

  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    passwordController_ =
        CreatePasswordController(web_state(), store_.get(), nullptr);
    @autoreleasepool {
      // Make sure the temporary array is released after SetUp finishes,
      // otherwise [passwordController_ suggestionProvider] will be retained
      // until PlatformTest teardown, at which point all Chrome objects are
      // already gone and teardown may access invalid memory.
      suggestionController_ = [[PasswordsTestSuggestionController alloc]
          initWithWebState:web_state()
                 providers:@[ [passwordController_ suggestionProvider] ]];
      accessoryViewController_ = [[FormInputAccessoryViewController alloc]
          initWithWebState:web_state()
                 providers:@[ [suggestionController_ accessoryViewProvider] ]];
    }
  }

 protected:
  // Helper method for PasswordControllerTest.DontFillReadonly. Tries to load
  // |html| and find and fill there a form with hard-coded form data. Returns
  // YES on success, NO otherwise.
  BOOL BasicFormFill(NSString* html);

  // Retrieve the current suggestions from suggestionController_ sorted in
  // alphabetical order according to their value properties.
  NSArray* GetSortedSuggestionValues() {
    NSMutableArray* suggestion_values = [NSMutableArray array];
    for (FormSuggestion* suggestion in [suggestionController_ suggestions])
      [suggestion_values addObject:suggestion.value];
    return [suggestion_values
        sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
  }

  // Returns an identifier for the |form_number|th form in the page.
  std::string FormName(int form_number) {
    NSString* kFormNamingScript =
        @"__gCrWeb.common.getFormIdentifier("
         "    document.querySelectorAll('form')[%d]);";
    return base::SysNSStringToUTF8(ExecuteJavaScript(
        [NSString stringWithFormat:kFormNamingScript, form_number]));
  }

  // Sets up a partial mock that intercepts calls to the selector
  // -fillPasswordForm:withUsername:password:completionHandler: to the
  // PasswordController's JavaScript manager. For the first
  // |target_failure_count| calls, skips the invocation of the real JavaScript
  // manager, giving the effect that password form fill failed. As soon as
  // |failure_count| reaches |target_failure_count|, stop the partial mock
  // and let the original JavaScript manager execute.
  void SetFillPasswordFormFailureCount(int target_failure_count) {
    id original_manager = [passwordController_ passwordJsManager];
    OCPartialMockObject* failing_manager =
        [OCMockObject partialMockForObject:original_manager];
    __block int failure_count = 0;
    void (^fail_invocation)(NSInvocation*) = ^(NSInvocation* invocation) {
      if (failure_count >= target_failure_count) {
        [failing_manager stopMocking];
        [invocation invokeWithTarget:original_manager];
      } else {
        ++failure_count;
        // Fetches the completion handler from |invocation| and calls it with
        // failure status.
        __unsafe_unretained void (^completionHandler)(BOOL);
        const NSInteger kArgOffset = 1;
        const NSInteger kCompletionHandlerArgIndex = 4;
        [invocation getArgument:&completionHandler
                        atIndex:(kCompletionHandlerArgIndex + kArgOffset)];
        ASSERT_TRUE(completionHandler);
        completionHandler(NO);
      }
    };
    [[[failing_manager stub] andDo:fail_invocation]
         fillPasswordForm:[OCMArg any]
             withUsername:[OCMArg any]
                 password:[OCMArg any]
        completionHandler:[OCMArg any]];
  }

  // SuggestionController for testing.
  PasswordsTestSuggestionController* suggestionController_;

  // FormInputAccessoryViewController for testing.
  FormInputAccessoryViewController* accessoryViewController_;

  // PasswordController for testing.
  PasswordController* passwordController_;

  scoped_refptr<password_manager::PasswordStore> store_;
};

struct PasswordFormTestData {
  const char* const page_location;
  const char* const json_string;
  const char* const expected_origin;
  const char* const expected_action;
  const char* const expected_username_element;
  const char* const expected_username_value;
  const char* const expected_new_password_element;
  const char* const expected_new_password_value;
  const char* const expected_old_password_element;
  const char* const expected_old_password_value;
};

// Check that given a serialization of a PasswordForm, the controller is able
// to create the corresponding PasswordForm object.
TEST_F(PasswordControllerTest, PopulatePasswordFormWithDictionary) {
  // clang-format off
  PasswordFormTestData test_data[] = {
    // One username element, one password element.  URLs contain extra
    // parts: username/password, query, reference, which are all expected
    // to be stripped off. The password is recognized as an old password.
    {
      "http://john:doe@fakedomain.com/foo/bar?baz=quz#foobar",
      "{ \"action\": \"some/action?to=be&or=not#tobe\","
          "\"usernameElement\": \"account\","
          "\"usernameValue\": \"fakeaccount\","
          "\"name\": \"signup\","
          "\"origin\": \"http://john:doe@fakedomain.com/foo/bar\","
          "\"passwords\": ["
              "{ \"element\": \"secret\"," "\"value\": \"fakesecret\" },"
          "]}",
      "http://fakedomain.com/foo/bar",
      "http://fakedomain.com/foo/some/action",
      "account",
      "fakeaccount",
      "",
      "",
      "secret",
      "fakesecret",
    },
    // One username element, one password element. Population should fail
    // due to an origin mismatch.
    {
      "http://john:doe@fakedomain.com/foo/bar?baz=quz#foobar",
      "{ \"action\": \"some/action?to=be&or=not#tobe\","
          "\"usernameElement\": \"account\","
          "\"usernameValue\": \"fakeaccount\","
          "\"name\": \"signup\","
          "\"origin\": \"http://john:doe@realdomainipromise.com/foo/bar\","
          "\"passwords\": ["
              "{ \"element\": \"secret\"," "\"value\": \"fakesecret\" },"
          "]}",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
      "",
    },
    // One username element, two password elements.  Since both password
    // values are the same, we are assuming that the webpage asked the user
    // to enter the password twice for confirmation.
    {
      "http://fakedomain.com/foo",
      "{ \"action\": \"http://anotherdomain.com/some_action\","
          "\"usernameElement\": \"account\","
          "\"usernameValue\": \"fakeaccount\","
          "\"name\": \"signup\","
          "\"origin\": \"http://fakedomain.com/foo\","
          "\"passwords\": ["
              "{ \"element\": \"secret\"," "\"value\": \"fakesecret\" },"
              "{ \"element\": \"confirm\"," "\"value\": \"fakesecret\" },"
          "]}",
      "http://fakedomain.com/foo",
      "http://anotherdomain.com/some_action",
      "account",
      "fakeaccount",
      "secret",
      "fakesecret",
      "",
      "",
    },
    // One username element, two password elements.  The password
    // values are different, so we are assuming that the webpage asked the user
    // to enter the old password and new password.
    {
      "http://fakedomain.com/foo",
      "{ \"action\": \"\","
          "\"usernameElement\": \"account\","
          "\"usernameValue\": \"fakeaccount\","
          "\"name\": \"signup\","
          "\"origin\": \"http://fakedomain.com/foo\","
          "\"passwords\": ["
              "{ \"element\": \"old\"," "\"value\": \"oldsecret\" },"
              "{ \"element\": \"new\"," "\"value\": \"newsecret\" },"
          "]}",
      "http://fakedomain.com/foo",
      "http://fakedomain.com/foo",
      "account",
      "fakeaccount",
      "new",
      "newsecret",
      "old",
      "oldsecret",
    },
    // One username element, three password elements.  All passwords
    // are the same. Password population should fail because this configuration
    // does not make sense.
    {
      "http://fakedomain.com",
      "{ \"action\": \"\","
          "\"usernameElement\": \"account\","
          "\"usernameValue\": \"fakeaccount\","
          "\"name\": \"signup\","
          "\"origin\": \"http://fakedomain.com/foo\","
          "\"passwords\": ["
              "{ \"element\": \"pass1\"," "\"value\": \"word\" },"
              "{ \"element\": \"pass2\"," "\"value\": \"word\" },"
              "{ \"element\": \"pass3\"," "\"value\": \"word\" },"
          "]}",
      "http://fakedomain.com/",
      "http://fakedomain.com/",
      "account",
      "fakeaccount",
      "",
      "",
      "",
      "",
    },
    // One username element, three password elements.  Two passwords are
    // the same followed by a different one.  Assuming that the duplicated
    // password is the old one.
    {
      "http://fakedomain.com",
      "{ \"action\": \"\","
          "\"usernameElement\": \"account\","
          "\"usernameValue\": \"fakeaccount\","
          "\"name\": \"signup\","
          "\"origin\": \"http://fakedomain.com/foo\","
          "\"passwords\": ["
              "{ \"element\": \"pass1\"," "\"value\": \"word1\" },"
              "{ \"element\": \"pass2\"," "\"value\": \"word1\" },"
              "{ \"element\": \"pass3\"," "\"value\": \"word3\" },"
          "]}",
      "http://fakedomain.com/",
      "http://fakedomain.com/",
      "account",
      "fakeaccount",
      "pass3",
      "word3",
      "pass1",
      "word1",
    },
    // One username element, three password elements.  A password is
    // follwed by two duplicate ones.  Assuming that the duplicated
    // password is the new one.
    {
      "http://fakedomain.com",
      "{ \"action\": \"\","
          "\"usernameElement\": \"account\","
          "\"usernameValue\": \"fakeaccount\","
          "\"name\": \"signup\","
          "\"origin\": \"http://fakedomain.com/foo\","
          "\"passwords\": ["
              "{ \"element\": \"pass1\"," "\"value\": \"word1\" },"
              "{ \"element\": \"pass2\"," "\"value\": \"word2\" },"
              "{ \"element\": \"pass3\"," "\"value\": \"word2\" },"
          "]}",
      "http://fakedomain.com/",
      "http://fakedomain.com/",
      "account",
      "fakeaccount",
      "pass2",
      "word2",
      "pass1",
      "word1",
    },
  };
  // clang-format on

  for (const PasswordFormTestData& data : test_data) {
    SCOPED_TRACE(testing::Message()
                 << "for page_location=" << data.page_location
                 << " and json_string=" << data.json_string);
    std::unique_ptr<base::Value> json_data(
        base::JSONReader::Read(data.json_string, true));
    const base::DictionaryValue* json_dict = nullptr;
    ASSERT_TRUE(json_data->GetAsDictionary(&json_dict));
    PasswordForm form;
    [passwordController_ getPasswordForm:&form
                          fromDictionary:json_dict
                                 pageURL:GURL(data.page_location)];
    EXPECT_STREQ(data.expected_origin, form.origin.spec().c_str());
    EXPECT_STREQ(data.expected_action, form.action.spec().c_str());
    EXPECT_EQ(base::ASCIIToUTF16(data.expected_username_element),
              form.username_element);
    EXPECT_EQ(base::ASCIIToUTF16(data.expected_username_value),
              form.username_value);
    EXPECT_EQ(base::ASCIIToUTF16(data.expected_new_password_element),
              form.new_password_element);
    EXPECT_EQ(base::ASCIIToUTF16(data.expected_new_password_value),
              form.new_password_value);
    EXPECT_EQ(base::ASCIIToUTF16(data.expected_old_password_element),
              form.password_element);
    EXPECT_EQ(base::ASCIIToUTF16(data.expected_old_password_value),
              form.password_value);
  }
};

struct FindPasswordFormTestData {
  NSString* html_string;
  const bool expected_form_found;
  const char* const expected_username_element;
  const char* const expected_password_element;
};

// TODO(crbug.com/403705) This test is flaky.
// Check that HTML forms are converted correctly into PasswordForms.
TEST_F(PasswordControllerTest, FLAKY_FindPasswordFormsInView) {
  // clang-format off
  FindPasswordFormTestData test_data[] = {
    // Normal form: a username and a password element.
    {
      @"<form>"
          "<input type='text' name='user0'>"
          "<input type='password' name='pass0'>"
          "</form>",
      true, "user0", "pass0"
    },
    // User name is captured as an email address (HTML5).
    {
      @"<form>"
          "<input type='email' name='email1'>"
          "<input type='password' name='pass1'>"
          "</form>",
      true, "email1", "pass1"
    },
    // No username element.
    {
      @"<form>"
          "<input type='password' name='user2'>"
          "<input type='password' name='pass2'>"
          "</form>",
      true, "", "user2"
    },
    // No username element before password.
    {
      @"<form>"
          "<input type='password' name='pass3'>"
          "<input type='text' name='user3'>"
          "</form>",
      true, "", "pass3"
    },
    // Disabled username element.
    {
      @"<form>"
          "<input type='text' name='user4' disabled='disabled'>"
          "<input type='password' name='pass4'>"
          "</form>",
      true, "", "pass4"
    },
    // Username element has autocomplete='off'.
    {
      @"<form>"
          "<input type='text' name='user5' AUTOCOMPLETE='off'>"
          "<input type='password' name='pass5'>"
          "</form>",
      true, "user5", "pass5"
    },
    // No password element.
    {
      @"<form>"
          "<input type='text' name='user6'>"
          "<input type='text' name='pass6'>"
          "</form>",
      false, nullptr, nullptr
    },
    // Disabled password element.
    {
      @"<form>"
          "<input type='text' name='user7'>"
          "<input type='password' name='pass7' disabled='disabled'>"
          "</form>",
      false, nullptr, nullptr
    },
    // Password element has autocomplete='off'.
    {
      @"<form>"
          "<input type='text' name='user8'>"
          "<input type='password' name='pass8' AUTOCOMPLETE='OFF'>"
          "</form>",
      true, "user8", "pass8"
    },
    // Form element has autocomplete='off'.
    {
      @"<form autocomplete='off'>"
          "<input type='text' name='user9'>"
          "<input type='password' name='pass9'>"
          "</form>",
      true, "user9", "pass9"
    },
  };
  // clang-format on

  for (const FindPasswordFormTestData& data : test_data) {
    SCOPED_TRACE(testing::Message() << "for html_string=" << data.html_string);
    LoadHtml(data.html_string);
    __block std::vector<PasswordForm> forms;
    __block BOOL block_was_called = NO;
    [passwordController_ findPasswordFormsWithCompletionHandler:^(
                             const std::vector<PasswordForm>& result) {
      block_was_called = YES;
      forms = result;
    }];
    base::test::ios::WaitUntilCondition(^bool() {
      return block_was_called;
    });
    if (data.expected_form_found) {
      ASSERT_EQ(1U, forms.size());
      EXPECT_EQ(base::ASCIIToUTF16(data.expected_username_element),
                forms[0].username_element);
      EXPECT_EQ(base::ASCIIToUTF16(data.expected_password_element),
                forms[0].password_element);
    } else {
      ASSERT_TRUE(forms.empty());
    }
  }
}

struct GetSubmittedPasswordFormTestData {
  NSString* html_string;
  NSString* java_script;
  const int number_of_forms_to_submit;
  const bool expected_form_found;
  const char* expected_username_element;
};

// TODO(crbug.com/403705) This test is flaky.
// Check that HTML forms are captured and converted correctly into
// PasswordForms on submission.
TEST_F(PasswordControllerTest, FLAKY_GetSubmittedPasswordForm) {
  // clang-format off
  GetSubmittedPasswordFormTestData test_data[] = {
    // Two forms with no explicit names.
    {
      @"<form action='javascript:;'>"
          "<input type='text' name='user1'>"
          "<input type='password' name='pass1'>"
          "</form>"
          "<form action='javascript:;'>"
          "<input type='text' name='user2'>"
          "<input type='password' name='pass2'>"
          "<input type='submit' id='s2'>"
          "</form>",
      @"document.getElementById('s2').click()",
      1, true, "user2"
    },
    // Two forms with explicit names.
    {
      @"<form name='test2a' action='javascript:;'>"
          "<input type='text' name='user1'>"
          "<input type='password' name='pass1'>"
          "<input type='submit' id='s1'>"
          "</form>"
          "<form name='test2b' action='javascript:;'>"
          "<input type='text' name='user2'>"
          "<input type='password' name='pass2'>"
          "</form>",
      @"document.getElementById('s1').click()",
      0, true, "user1"
    },
    // No password forms.
    {
      @"<form action='javascript:;'>"
          "<input type='text' name='user1'>"
          "<input type='text' name='pass1'>"
          "<input type='submit' id='s1'>"
          "</form>",
      @"document.getElementById('s1').click()",
      0, false, nullptr
    },
    // Form with quotes in the form and field names.
    {
      @"<form name=\"foo'\" action='javascript:;'>"
          "<input type='text' name=\"user1'\">"
          "<input type='password' id='s1' name=\"pass1'\">"
          "</form>",
      @"document.getElementById('s1').click()",
      0, true, "user1'"
    },
  };
  // clang-format on

  for (const GetSubmittedPasswordFormTestData& data : test_data) {
    SCOPED_TRACE(testing::Message() << "for html_string=" << data.html_string
                                    << " and java_script=" << data.java_script
                                    << " and number_of_forms_to_submit="
                                    << data.number_of_forms_to_submit);
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
    [passwordController_
        extractSubmittedPasswordForm:FormName(data.number_of_forms_to_submit)
                   completionHandler:completion_handler];
    base::test::ios::WaitUntilCondition(^bool() {
      return block_was_called;
    });
  }
}

// Populates |form_data| with test values.
void SetPasswordFormFillData(PasswordFormFillData& form_data,
                             const std::string& origin,
                             const std::string& action,
                             const char* username_field,
                             const char* username_value,
                             const char* password_field,
                             const char* password_value,
                             const char* additional_username,
                             const char* additional_password,
                             bool wait_for_username) {
  form_data.origin = GURL(origin);
  form_data.action = GURL(action);
  autofill::FormFieldData username;
  username.name = base::UTF8ToUTF16(username_field);
  username.value = base::UTF8ToUTF16(username_value);
  form_data.username_field = username;
  autofill::FormFieldData password;
  password.name = base::UTF8ToUTF16(password_field);
  password.value = base::UTF8ToUTF16(password_value);
  form_data.password_field = password;
  if (additional_username) {
    autofill::PasswordAndRealm additional_password_data;
    additional_password_data.password = base::UTF8ToUTF16(additional_password);
    additional_password_data.realm.clear();
    form_data.additional_logins.insert(
        std::pair<base::string16, autofill::PasswordAndRealm>(
            base::UTF8ToUTF16(additional_username), additional_password_data));
  }
  form_data.wait_for_username = wait_for_username;
}

// Test HTML page.  It contains several password forms.  Tests autofill
// them and verify that the right ones are autofilled.
static NSString* kHtmlWithMultiplePasswordForms =
    @"<form>"
     "<input id='un0' type='text' name='u0'>"
     "<input id='pw0' type='password' name='p0'>"
     "</form>"
     "<form action='?query=yes#reference'>"
     "<input id='un1' type='text' name='u1'>"
     "<input id='pw1' type='password' name='p1'>"
     "</form>"
     "<form action='http://some_other_action'>"
     "<input id='un2' type='text' name='u2'>"
     "<input id='pw2' type='password' name='p2'>"
     "</form>"
     "<form>"
     "<input id='un3' type='text' name='u3'>"
     "<input id='pw3' type='password' name='p3'>"
     "<input id='pw3' type='password' name='p3'>"
     "</form>"
     "<form>"
     "<input id='un4' type='text' name='u4'>"
     "<input id='pw4' type='password' name='p4'>"
     "</form>"
     "<form>"
     "<input id='un5' type='text' name='u4'>"
     "<input id='pw5' type='password' name='p4'>"
     "</form>"
     "<form name=\"f6'\">"
     "<input id=\"un6'\" type='text' name=\"u6'\">"
     "<input id=\"pw6'\" type='password' name=\"p6'\">"
     "</form>";

// A script that resets all text fields.
static NSString* kClearInputFieldsScript =
    @"var inputs = document.getElementsByTagName('input');"
     "for(var i = 0; i < inputs.length; i++){"
     "  inputs[i].value = '';"
     "}";

// A script that we run after autofilling forms.  It returns
// ids and values of all non-empty fields.
static NSString* kInputFieldValueVerificationScript =
    @"var result='';"
     "var inputs = document.getElementsByTagName('input');"
     "for(var i = 0; i < inputs.length; i++){"
     "  var input = inputs[i];"
     "  if (input.value) {"
     "    result += input.id + '=' + input.value +';';"
     "  }"
     "}; result";

struct FillPasswordFormTestData {
  const std::string origin;
  const std::string action;
  const char* username_field;
  const char* username_value;
  const char* password_field;
  const char* password_value;
  const BOOL should_succeed;
  NSString* expected_result;
};

// Test that filling password forms works correctly.
TEST_F(PasswordControllerTest, FillPasswordForm) {
  LoadHtml(kHtmlWithMultiplePasswordForms);

  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"__gCrWeb.hasPasswordField()"));

  const std::string base_url = BaseUrl();
  // clang-format off
  FillPasswordFormTestData test_data[] = {
    // Basic test: one-to-one match on the first password form.
    {
      base_url,
      base_url,
      "u0",
      "test_user",
      "p0",
      "test_password",
      YES,
      @"un0=test_user;pw0=test_password;"
    },
    // Multiple forms match: they should all be autofilled.
    {
      base_url,
      base_url,
      "u4",
      "test_user",
      "p4",
      "test_password",
      YES,
      @"un4=test_user;pw4=test_password;un5=test_user;pw5=test_password;"
    },
    // The form matches despite a different action: the only difference
    // is a query and reference.
    {
      base_url,
      base_url,
      "u1",
      "test_user",
      "p1",
      "test_password",
      YES,
      @"un1=test_user;pw1=test_password;"
    },
    // No match because of a different origin.
    {
      "http://someotherfakedomain.com",
      base_url,
      "u0",
      "test_user",
      "p0",
      "test_password",
      NO,
      @""
    },
    // No match because of a different action.
    {
      base_url,
      "http://someotherfakedomain.com",
      "u0",
      "test_user",
      "p0",
      "test_password",
      NO,
      @""
    },
    // No match because some inputs are not in the form.
    {
      base_url,
      base_url,
      "u0",
      "test_user",
      "p1",
      "test_password",
      NO,
      @""
    },
    // No match because there are duplicate inputs in the form.
    {
      base_url,
      base_url,
      "u3",
      "test_user",
      "p3",
      "test_password",
      NO,
      @""
    },
    // Basic test, but with quotes in the names and IDs.
    {
      base_url,
      base_url,
      "u6'",
      "test_user",
      "p6'",
      "test_password",
      YES,
      @"un6'=test_user;pw6'=test_password;"
    },
  };
  // clang-format on

  for (const FillPasswordFormTestData& data : test_data) {
    ExecuteJavaScript(kClearInputFieldsScript);

    PasswordFormFillData form_data;
    SetPasswordFormFillData(form_data, data.origin, data.action,
                            data.username_field, data.username_value,
                            data.password_field, data.password_value, nullptr,
                            nullptr, false);

    __block BOOL block_was_called = NO;
    [passwordController_ fillPasswordForm:form_data
                        completionHandler:^(BOOL success) {
                          block_was_called = YES;
                          EXPECT_EQ(data.should_succeed, success);
                        }];
    base::test::ios::WaitUntilCondition(^bool() {
      return block_was_called;
    });

    id result = ExecuteJavaScript(kInputFieldValueVerificationScript);
    EXPECT_NSEQ(data.expected_result, result);
  }
}

// Tests that a form is found and the found form is filled in with the given
// username and password.
TEST_F(PasswordControllerTest, FindAndFillOnePasswordForm) {
  LoadHtml(@"<form><input id='un' type='text' name='u'>"
            "<input id='pw' type='password' name='p'></form>");
  __block int call_counter = 0;
  __block int success_counter = 0;
  [passwordController_ findAndFillPasswordForms:@"john.doe@gmail.com"
                                       password:@"super!secret"
                              completionHandler:^(BOOL complete) {
                                ++call_counter;
                                if (complete)
                                  ++success_counter;
                              }];
  base::test::ios::WaitUntilCondition(^{
    return call_counter == 1;
  });
  EXPECT_EQ(1, success_counter);
  id result = ExecuteJavaScript(kInputFieldValueVerificationScript);
  EXPECT_NSEQ(@"un=john.doe@gmail.com;pw=super!secret;", result);
}

// Tests that multiple forms on the same page are found and filled.
// This test includes an mock injected failure on form filling to verify
// that completion handler is called with the proper values.
TEST_F(PasswordControllerTest, FindAndFillMultiplePasswordForms) {
  // Fails the first call to fill password form.
  SetFillPasswordFormFailureCount(1);
  LoadHtml(@"<form><input id='u1' type='text' name='un1'>"
            "<input id='p1' type='password' name='pw1'></form>"
            "<form><input id='u2' type='text' name='un2'>"
            "<input id='p2' type='password' name='pw2'></form>"
            "<form><input id='u3' type='text' name='un3'>"
            "<input id='p3' type='password' name='pw3'></form>");
  __block int call_counter = 0;
  __block int success_counter = 0;
  [passwordController_ findAndFillPasswordForms:@"john.doe@gmail.com"
                                       password:@"super!secret"
                              completionHandler:^(BOOL complete) {
                                ++call_counter;
                                if (complete)
                                  ++success_counter;
                                LOG(INFO) << "HANDLER call " << call_counter
                                          << " success " << success_counter;
                              }];
  // There should be 3 password forms and only 2 successfully filled forms.
  base::test::ios::WaitUntilCondition(^{
    return call_counter == 3;
  });
  EXPECT_EQ(2, success_counter);
  id result = ExecuteJavaScript(kInputFieldValueVerificationScript);
  EXPECT_NSEQ(@"u2=john.doe@gmail.com;p2=super!secret;"
               "u3=john.doe@gmail.com;p3=super!secret;",
              result);
}

BOOL PasswordControllerTest::BasicFormFill(NSString* html) {
  LoadHtml(html);
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"__gCrWeb.hasPasswordField()"));
  const std::string base_url = BaseUrl();
  PasswordFormFillData form_data;
  SetPasswordFormFillData(form_data, base_url, base_url, "u0", "test_user",
                          "p0", "test_password", nullptr, nullptr, false);
  __block BOOL block_was_called = NO;
  __block BOOL return_value = NO;
  [passwordController_ fillPasswordForm:form_data
                      completionHandler:^(BOOL success) {
                        block_was_called = YES;
                        return_value = success;
                      }];
  base::test::ios::WaitUntilCondition(^bool() {
    return block_was_called;
  });
  return return_value;
}

// Check that |fillPasswordForm| is not filled if 'readonly' attribute is set
// on either username or password fields.
// TODO(crbug.com/503050): Test is flaky.
TEST_F(PasswordControllerTest, FLAKY_DontFillReadOnly) {
  // Control check that the fill operation will succceed with well-formed form.
  EXPECT_TRUE(BasicFormFill(@"<form>"
                             "<input id='un0' type='text' name='u0'>"
                             "<input id='pw0' type='password' name='p0'>"
                             "</form>"));
  // Form fill should fail with 'readonly' attribute on username.
  EXPECT_FALSE(BasicFormFill(
      @"<form>"
       "<input id='un0' type='text' name='u0' readonly='readonly'>"
       "<input id='pw0' type='password' name='p0'>"
       "</form>"));
  // Form fill should fail with 'readonly' attribute on password.
  EXPECT_FALSE(BasicFormFill(
      @"<form>"
       "<input id='un0' type='text' name='u0'>"
       "<input id='pw0' type='password' name='p0' readonly='readonly'>"
       "</form>"));
}

// An HTML page without a password form.
static NSString* kHtmlWithoutPasswordForm =
    @"<h2>The rain in Spain stays <i>mainly</i> in the plain.</h2>";

// An HTML page containing one password form.  The username input field
// also has custom event handlers.  We need to verify that those event
// handlers are still triggered even though we override them with our own.
static NSString* kHtmlWithPasswordForm =
    @"<form>"
     "<input id='un' type='text' name=\"u'\""
     "  onkeyup='window.onKeyUpCalled_=true'"
     "  onchange='window.onChangeCalled_=true'>"
     "<input id='pw' type='password' name=\"p'\">"
     "</form>";

// A script that resets indicators used to verify that custom event
// handlers are triggered.  It also finds and the username and
// password fields and caches them for future verification.
static NSString* kUsernameAndPasswordTestPreparationScript =
    @"onKeyUpCalled_ = false;"
     "onChangeCalled_ = false;"
     "username_ = document.getElementById('un');"
     "username_.__gCrWebAutofilled = 'false';"
     "password_ = document.getElementById('pw');"
     "password_.__gCrWebAutofilled = 'false';";

// A script that we run after autofilling forms.  It returns
// all values for verification as a single concatenated string.
static NSString* kUsernamePasswordVerificationScript =
    @"var value = username_.value;"
     "var from = username_.selectionStart;"
     "var to = username_.selectionEnd;"
     "value.substr(0, from) + '[' + value.substr(from, to) + ']'"
     "   + value.substr(to, value.length) + '=' + password_.value"
     "   + ', onkeyup=' + onKeyUpCalled_"
     "   + ', onchange=' + onChangeCalled_;";

struct SuggestionTestData {
  std::string description;
  NSArray* eval_scripts;
  NSArray* expected_suggestions;
  NSString* expected_result;
};

// Tests that form activity correctly sends suggestions to the suggestion
// controller.
TEST_F(PasswordControllerTest, SuggestionUpdateTests) {
  LoadHtml(kHtmlWithPasswordForm);
  const std::string base_url = BaseUrl();
  ExecuteJavaScript(kUsernameAndPasswordTestPreparationScript);

  // Initialize |form_data| with test data and an indicator that autofill
  // should not be performed while the user is entering the username so that
  // we can test with an initially-empty username field. Testing with a
  // username field that contains input is performed by a specific test below.
  PasswordFormFillData form_data;
  SetPasswordFormFillData(form_data, base_url, base_url, "u'", "user0", "p'",
                          "password0", "abc", "def", true);
  form_data.name = base::ASCIIToUTF16(FormName(0));

  __block BOOL block_was_called = NO;
  [passwordController_ fillPasswordForm:form_data
                      completionHandler:^(BOOL success) {
                        block_was_called = YES;
                        // Verify that the fill reports failed.
                        EXPECT_FALSE(success);
                      }];
  base::test::ios::WaitUntilCondition(^bool() {
    return block_was_called;
  });

  // Verify that the form has not been autofilled.
  EXPECT_NSEQ(@"[]=, onkeyup=false, onchange=false",
              ExecuteJavaScript(kUsernamePasswordVerificationScript));

  // clang-format off
  SuggestionTestData test_data[] = {
    {
      "Should show all suggestions when focusing empty username field",
      @[(@"var evt = document.createEvent('Events');"
          "evt.initEvent('focus', true, true, window, 1);"
          "username_.dispatchEvent(evt);"),
        @""],
      @[@"abc", @"user0"],
      @"[]=, onkeyup=false, onchange=false"
    },
    {
      "Should not show suggestions when focusing password field",
      @[(@"var evt = document.createEvent('Events');"
          "evt.initEvent('focus', true, true, window, 1);"
          "password_.dispatchEvent(evt);"),
        @""],
      @[],
      @"[]=, onkeyup=false, onchange=false"
    },
    {
      "Should filter suggestions when focusing username field with input",
      @[(@"username_.value='ab';"
          "var evt = document.createEvent('Events');"
          "evt.initEvent('focus', true, true, window, 1);"
          "username_.dispatchEvent(evt);"),
        @""],
      @[@"abc"],
      @"ab[]=, onkeyup=false, onchange=false"
    },
    {
      "Should filter suggestions while typing",
      @[(@"var evt = document.createEvent('Events');"
          "evt.initEvent('focus', true, true, window, 1);"
          "username_.dispatchEvent(evt);"),
        (@"username_.value='ab';"
          "evt = document.createEvent('Events');"
          "evt.initEvent('keyup', true, true, window, 1);"
          "evt.keyCode = 98;"
          "username_.dispatchEvent(evt);"),
        @""],
      @[@"abc"],
      @"ab[]=, onkeyup=true, onchange=false"
    },
    {
      "Should unfilter suggestions after backspacing",
      @[(@"var evt = document.createEvent('Events');"
          "evt.initEvent('focus', true, true, window, 1);"
          "username_.dispatchEvent(evt);"),
        (@"username_.value='ab';"
          "evt = document.createEvent('Events');"
          "evt.initEvent('keyup', true, true, window, 1);"
          "evt.keyCode = 98;"
          "username_.dispatchEvent(evt);"),
        (@"username_.value='';"
          "evt = document.createEvent('Events');"
          "evt.initEvent('keyup', true, true, window, 1);"
          "evt.keyCode = 8;"
          "username_.dispatchEvent(evt);"),
        @""],
      @[@"abc", @"user0"],
      @"[]=, onkeyup=true, onchange=false"
    },
  };
  // clang-format on

  for (const SuggestionTestData& data : test_data) {
    SCOPED_TRACE(testing::Message()
                 << "for description=" << data.description
                 << " and eval_scripts=" << data.eval_scripts);
    // Prepare the test.
    ExecuteJavaScript(kUsernameAndPasswordTestPreparationScript);

    for (NSString* script in data.eval_scripts) {
      // Trigger events.
      ExecuteJavaScript(script);

      // Pump the run loop so that the host can respond.
      WaitForBackgroundTasks();
    }

    EXPECT_NSEQ(data.expected_suggestions, GetSortedSuggestionValues());
    EXPECT_NSEQ(data.expected_result,
                ExecuteJavaScript(kUsernamePasswordVerificationScript));
    // Clear all suggestions.
    [suggestionController_ setSuggestions:nil];
  }
}

// Tests that selecting a suggestion will fill the corresponding form and field.
TEST_F(PasswordControllerTest, SelectingSuggestionShouldFillPasswordForm) {
  LoadHtml(kHtmlWithPasswordForm);
  const std::string base_url = BaseUrl();
  ExecuteJavaScript(kUsernameAndPasswordTestPreparationScript);

  // Initialize |form_data| with test data and an indicator that autofill
  // should not be performed while the user is entering the username so that
  // we can test with an initially-empty username field.
  PasswordFormFillData form_data;
  SetPasswordFormFillData(form_data, base_url, base_url, "u'", "user0", "p'",
                          "password0", "abc", "def", true);
  form_data.name = base::ASCIIToUTF16(FormName(0));

  __block BOOL block_was_called = NO;
  [passwordController_ fillPasswordForm:form_data
                      completionHandler:^(BOOL success) {
                        block_was_called = YES;
                        // Verify that the fill reports failed.
                        EXPECT_FALSE(success);
                      }];
  base::test::ios::WaitUntilCondition(^bool() {
    return block_was_called;
  });

  // Verify that the form has not been autofilled.
  EXPECT_NSEQ(@"[]=, onkeyup=false, onchange=false",
              ExecuteJavaScript(kUsernamePasswordVerificationScript));

  // Tell PasswordController that a suggestion was selected. It should fill
  // out the password form with the corresponding credentials.
  FormSuggestion* suggestion = [FormSuggestion suggestionWithValue:@"abc"
                                                displayDescription:nil
                                                              icon:nil
                                                        identifier:0];

  block_was_called = NO;
  SuggestionHandledCompletion completion = ^{
    block_was_called = YES;
    EXPECT_NSEQ(@"abc[]=def, onkeyup=false, onchange=false",
                ExecuteJavaScript(kUsernamePasswordVerificationScript));
  };
  [passwordController_ didSelectSuggestion:suggestion
                                  forField:@"u"
                                      form:base::SysUTF8ToNSString(FormName(0))
                         completionHandler:completion];
  base::test::ios::WaitUntilCondition(^bool() {
    return block_was_called;
  });
}

// Tests with invalid inputs.
TEST_F(PasswordControllerTest, CheckIncorrectData) {
  // clang-format off
  std::string invalid_data[] = {
    "{}",

    "{  \"usernameValue\": \"fakeaccount\","
       "\"passwords\": ["
         "{ \"element\": \"secret\"," "\"value\": \"fakesecret\" },"
       "]}",

    "{  \"usernameElement\": \"account\","
       "\"passwords\": ["
         "{ \"element\": \"secret\"," "\"value\": \"fakesecret\" },"
       "]}",

    "{  \"usernameElement\": \"account\","
       "\"usernameValue\": \"fakeaccount\","
    "}",

    "{  \"usernameElement\": \"account\","
       "\"usernameValue\": \"fakeaccount\","
       "\"passwords\": {},"
    "}",

    "{  \"usernameElement\": \"account\","
       "\"usernameValue\": \"fakeaccount\","
       "\"passwords\": ["
       "]}",

    "{  \"usernameElement\": \"account\","
       "\"usernameValue\": \"fakeaccount\","
       "\"passwords\": ["
         "{ \"value\": \"fakesecret\" },"
       "]}",

    "{  \"usernameElement\": \"account\","
       "\"usernameValue\": \"fakeaccount\","
       "\"passwords\": ["
         "{ \"element\": \"secret\" },"
       "]}",
  };
  // clang-format on

  for (const std::string& data : invalid_data) {
    SCOPED_TRACE(testing::Message() << "for data=" << data);
    std::unique_ptr<base::Value> json_data(base::JSONReader::Read(data, true));
    const base::DictionaryValue* json_dict = nullptr;
    ASSERT_TRUE(json_data->GetAsDictionary(&json_dict));
    PasswordForm form;
    BOOL res =
        [passwordController_ getPasswordForm:&form
                              fromDictionary:json_dict
                                     pageURL:GURL("https://www.foo.com/")];
    EXPECT_FALSE(res);
  }
}

// The test case below does not need the heavy fixture from above, but it
// needs to use MockWebState.
TEST(PasswordControllerTestSimple, SaveOnNonHTMLLandingPage) {
  TestChromeBrowserState::Builder builder;
  std::unique_ptr<TestChromeBrowserState> browser_state(builder.Build());
  MockWebState web_state;
  ON_CALL(web_state, GetBrowserState())
      .WillByDefault(testing::Return(browser_state.get()));

  MockPasswordManagerClient* weak_client = nullptr;
  PasswordController* passwordController =
      CreatePasswordController(&web_state, nullptr, &weak_client);
  static_cast<TestingPrefServiceSimple*>(weak_client->GetPrefs())
      ->registry()
      ->RegisterBooleanPref(
          password_manager::prefs::kPasswordManagerSavingEnabled, true);

  // Use a mock LogManager to detect that OnPasswordFormsRendered has been
  // called. TODO(crbug.com/598672): this is a hack, we should modularize the
  // code better to allow proper unit-testing.
  MockLogManager log_manager;
  EXPECT_CALL(log_manager, IsLoggingActive()).WillRepeatedly(Return(true));
  EXPECT_CALL(log_manager,
              LogSavePasswordProgress(
                  "Message: \"PasswordManager::OnPasswordFormsRendered\"\n"));
  EXPECT_CALL(log_manager,
              LogSavePasswordProgress(testing::Ne(
                  "Message: \"PasswordManager::OnPasswordFormsRendered\"\n")))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*weak_client, GetLogManager())
      .WillRepeatedly(Return(&log_manager));

  web_state.SetContentIsHTML(false);
  web_state.SetCurrentURL(GURL("https://example.com"));
  [passwordController webState:&web_state didLoadPageWithSuccess:YES];
}

// Tests that an HTTP page without a password field does not update the SSL
// status to indicate DISPLAYED_PASSWORD_FIELD_ON_HTTP.
TEST_F(PasswordControllerTest, HTTPNoPassword) {
  LoadHtml(kHtmlWithoutPasswordForm, GURL("http://chromium.test"));

  web::SSLStatus ssl_status =
      web_state()->GetNavigationManager()->GetLastCommittedItem()->GetSSL();
  EXPECT_FALSE(ssl_status.content_status &
               web::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
}

// Tests that an HTTP page with a password field updates the SSL status
// to indicate DISPLAYED_PASSWORD_FIELD_ON_HTTP.
TEST_F(PasswordControllerTest, HTTPPassword) {
  LoadHtml(kHtmlWithPasswordForm, GURL("http://chromium.test"));

  web::SSLStatus ssl_status =
      web_state()->GetNavigationManager()->GetLastCommittedItem()->GetSSL();
  EXPECT_TRUE(ssl_status.content_status &
              web::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
}

// Tests that an HTTPS page without a password field does not update the SSL
// status to indicate DISPLAYED_PASSWORD_FIELD_ON_HTTP.
TEST_F(PasswordControllerTest, HTTPSNoPassword) {
  LoadHtml(kHtmlWithoutPasswordForm, GURL("https://chromium.test"));

  web::SSLStatus ssl_status =
      web_state()->GetNavigationManager()->GetLastCommittedItem()->GetSSL();
  EXPECT_FALSE(ssl_status.content_status &
               web::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
}

// Tests that an HTTPS page with a password field does not update the SSL status
// to indicate DISPLAYED_PASSWORD_FIELD_ON_HTTP.
TEST_F(PasswordControllerTest, HTTPSPassword) {
  LoadHtml(kHtmlWithPasswordForm, GURL("https://chromium.test"));

  web::SSLStatus ssl_status =
      web_state()->GetNavigationManager()->GetLastCommittedItem()->GetSSL();
  EXPECT_FALSE(ssl_status.content_status &
               web::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
}
