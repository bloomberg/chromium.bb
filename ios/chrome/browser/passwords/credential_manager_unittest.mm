// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/credential_manager.h"

#include <memory>
#include <utility>

#include "base/mac/bind_objc_block.h"
#include "base/memory/ref_counted.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/chrome/browser/passwords/js_credential_manager.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "url/gurl.h"

using testing::Return;

namespace {
// Type of a function invoked when a promise is resolved.
typedef void (^ResolvePromiseBlock)(NSInteger request_id,
                                    const web::Credential& credential);
}  // namespace

// A helper to mock methods that have C++ object parameters.
@interface MockJSCredentialManager : OCMockComplexTypeHelper
@end

@implementation MockJSCredentialManager
- (void)resolvePromiseWithRequestID:(NSInteger)requestID
                         credential:(const web::Credential&)credential
                  completionHandler:(void (^)(BOOL))completionHandler {
  static_cast<ResolvePromiseBlock>([self blockForSelector:_cmd])(requestID,
                                                                 credential);
  completionHandler(YES);
}
@end

namespace {

const char kTestURL[] = "https://foo.com/login";

// Returns a test credential.
autofill::PasswordForm GetTestPasswordForm1(bool zero_click_allowed) {
  autofill::PasswordForm form;
  form.origin = GURL(kTestURL);
  form.signon_realm = form.origin.spec();
  form.username_value = base::ASCIIToUTF16("foo");
  form.password_value = base::ASCIIToUTF16("bar");
  form.skip_zero_click = !zero_click_allowed;
  form.type = autofill::PasswordForm::Type::TYPE_API;
  return form;
}

// Returns a test credential matching |GetTestPasswordForm1()|.
web::Credential GetTestWebCredential1(bool zero_click_allowed) {
  web::Credential credential;
  autofill::PasswordForm form(GetTestPasswordForm1(zero_click_allowed));
  credential.type = web::CredentialType::CREDENTIAL_TYPE_PASSWORD;
  credential.id = form.username_value;
  credential.password = form.password_value;
  return credential;
}

// Returns a different test credential.
autofill::PasswordForm GetTestPasswordForm2(bool zero_click_allowed) {
  autofill::PasswordForm form;
  form.origin = GURL(kTestURL);
  form.signon_realm = form.origin.spec();
  form.username_value = base::ASCIIToUTF16("baz");
  form.password_value = base::ASCIIToUTF16("bah");
  form.skip_zero_click = !zero_click_allowed;
  return form;
}

// Returns a test credential matching |GetTestPasswordForm2()|.
web::Credential GetTestWebCredential2(bool zero_click_allowed) {
  web::Credential credential;
  autofill::PasswordForm form(GetTestPasswordForm2(zero_click_allowed));
  credential.type = web::CredentialType::CREDENTIAL_TYPE_PASSWORD;
  credential.id = form.username_value;
  credential.password = form.password_value;
  return credential;
}

typedef BOOL (^StringPredicate)(NSString*);

// Returns a block that takes a string argument and returns whether it is equal
// to |string|.
StringPredicate EqualsString(const char* string) {
  return [[^BOOL(NSString* other) {
    return [base::SysUTF8ToNSString(string) isEqualToString:other];
  } copy] autorelease];
}

// A stub PasswordManagerClient for testing.
class StubPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  StubPasswordManagerClient()
      : password_manager::StubPasswordManagerClient(),
        password_store_(nullptr) {
    prefs_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableAutosignin, false);
    password_bubble_experiment::RegisterPrefs(prefs_.registry());
  }

  ~StubPasswordManagerClient() override {
    if (password_store_)
      password_store_->ShutdownOnUIThread();
  }

  MOCK_CONST_METHOD0(IsSavingAndFillingEnabledForCurrentPage, bool());

  void SetPasswordStore(
      scoped_refptr<password_manager::PasswordStore> password_store) {
    password_store_ = password_store;
  }

  password_manager::PasswordStore* GetPasswordStore() const override {
    return password_store_.get();
  }

  void SetUserChosenCredential(
      const password_manager::CredentialInfo& credential) {
    user_chosen_credential_ = credential;
  }

  bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
      const GURL& origin,
      const CredentialsCallback& callback) override {
    return false;
  }

  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<password_manager::PasswordFormManager> form_to_save,
      password_manager::CredentialSourceType type,
      bool update_password) override {
    if (!update_password)
      saved_form_ = std::move(form_to_save);
    return true;
  }

  PrefService* GetPrefs() override { return &prefs_; }

  password_manager::PasswordFormManager* saved_form() {
    return saved_form_.get();
  }

 private:
  // PrefService for testing.
  TestingPrefServiceSimple prefs_;

  // PasswordStore for testing.
  scoped_refptr<password_manager::PasswordStore> password_store_;

  // The password form shown to the user for saving.
  std::unique_ptr<password_manager::PasswordFormManager> saved_form_;

  // The credential to be returned to callers of PromptUserToChooseCredentials.
  password_manager::CredentialInfo user_chosen_credential_;

  DISALLOW_COPY_AND_ASSIGN(StubPasswordManagerClient);
};

// Tests for CredentialManager.
class CredentialManagerTest : public web::WebTestWithWebState {
 public:
  CredentialManagerTest() {}
  ~CredentialManagerTest() override {}

  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    id originalMock =
        [OCMockObject niceMockForClass:[JSCredentialManager class]];
    mock_js_credential_manager_.reset([[MockJSCredentialManager alloc]
        initWithRepresentedObject:originalMock]);
    credential_manager_.reset(new CredentialManager(
        web_state(), &stub_client_, &stub_driver_,
        static_cast<id>(mock_js_credential_manager_.get())));
    LoadHtml(@"", GURL(kTestURL));
  }

  // Sets up an expectation that the promise identified by |request_id| will be
  // resolved with |credential|. |verified| must point to a variable that will
  // be checked by the caller to ensure that the expectations were run. (This
  // is necessary because OCMock doesn't handle methods with C++ object
  // parameters.)
  void ExpectPromiseResolved(bool* verified,
                             int request_id,
                             const web::Credential& credential) {
    SEL selector =
        @selector(resolvePromiseWithRequestID:credential:completionHandler:);
    web::Credential strong_credential(credential);
    [mock_js_credential_manager_
                  onSelector:selector
        callBlockExpectation:^(NSInteger block_request_id,
                               const web::Credential& block_credential) {
          EXPECT_EQ(request_id, block_request_id);
          EXPECT_EQ(strong_credential.type, block_credential.type);
          EXPECT_EQ(strong_credential.id, block_credential.id);
          EXPECT_EQ(strong_credential.name, block_credential.name);
          EXPECT_EQ(strong_credential.avatar_url, block_credential.avatar_url);
          EXPECT_EQ(strong_credential.password, block_credential.password);
          EXPECT_EQ(strong_credential.federation_origin.Serialize(),
                    block_credential.federation_origin.Serialize());
          *verified = true;
        }];
  }

  // Same as |ExpectPromiseResolved(bool*, int, const web::Credential&)| but
  // does not expect a credential to be passed.
  void ExpectPromiseResolved(int request_id) {
    [[[mock_js_credential_manager_ representedObject] expect]
        resolvePromiseWithRequestID:request_id
                  completionHandler:nil];
  }

  // Clears the expectations set up by |ExpectPromiseResolved()|.
  void ClearPromiseResolutionExpectations() {
    SEL selector =
        @selector(resolvePromiseWithRequestID:credential:completionHandler:);
    [mock_js_credential_manager_ removeBlockExpectationOnSelector:selector];
  }

  // Sets up an expectation that the promise identified by |request_id| will be
  // rejected with the given |error_type| and |message|. The caller should use
  // EXPECT_OCMOCK_VERIFY to verify that the expectations were run.
  void ExpectPromiseRejected(int request_id,
                             const char* error_type,
                             const char* message) {
    [[[mock_js_credential_manager_ representedObject] expect]
        rejectPromiseWithRequestID:request_id
                         errorType:[OCMArg
                                       checkWithBlock:EqualsString(error_type)]
                           message:[OCMArg checkWithBlock:EqualsString(message)]
                 completionHandler:[OCMArg any]];
  }

 protected:
  // Mock for PasswordManagerClient.
  StubPasswordManagerClient stub_client_;

  // Stub for PasswordManagerDriver.
  password_manager::StubPasswordManagerDriver stub_driver_;

  // Mock for JSCredentialManager.
  base::scoped_nsobject<MockJSCredentialManager> mock_js_credential_manager_;

  // CredentialManager for testing.
  std::unique_ptr<CredentialManager> credential_manager_;

 private:
  explicit CredentialManagerTest(const CredentialManagerTest&) = delete;
  CredentialManagerTest& operator=(const CredentialManagerTest&) = delete;
};

// Tests that a credential request is rejected properly when the PasswordStore
// is unavailable.
TEST_F(CredentialManagerTest, RequestRejectedWhenPasswordStoreUnavailable) {
  // Clear the password store.
  stub_client_.SetPasswordStore(nullptr);

  // Requesting a credential should reject the request with an error.
  const int request_id = 0;
  ExpectPromiseRejected(request_id,
                        kCredentialsPasswordStoreUnavailableErrorType,
                        kCredentialsPasswordStoreUnavailableErrorMessage);
  credential_manager_->CredentialsRequested(request_id, GURL(kTestURL), false,
                                            std::vector<std::string>(), true);

  // Pump the message loop and verify.
  WaitForBackgroundTasks();
  EXPECT_OCMOCK_VERIFY([mock_js_credential_manager_ representedObject]);
}

// Tests that a credential request is rejected when another request is pending.
TEST_F(CredentialManagerTest, RequestRejectedWhenExistingRequestIsPending) {
  // Set a password store, but prevent requests from completing.
  stub_client_.SetPasswordStore(new password_manager::TestPasswordStore);

  // Make an initial request. Don't pump the message loop, so that the task
  // doesn't complete. Expect the request to resolve with an empty credential
  // after the message loop is pumped.
  const int first_request_id = 0;
  bool first_verified = false;
  ExpectPromiseResolved(&first_verified, first_request_id, web::Credential());
  credential_manager_->CredentialsRequested(first_request_id, GURL(kTestURL),
                                            false, std::vector<std::string>(),
                                            true);

  // Making a second request and then pumping the message loop should reject the
  // request with an error.
  const int second_request_id = 0;
  ExpectPromiseRejected(second_request_id, kCredentialsPendingRequestErrorType,
                        kCredentialsPendingRequestErrorMessage);
  credential_manager_->CredentialsRequested(second_request_id, GURL(kTestURL),
                                            false, std::vector<std::string>(),
                                            true);

  // Pump the message loop and verify.
  WaitForBackgroundTasks();
  EXPECT_TRUE(first_verified);
  EXPECT_OCMOCK_VERIFY([mock_js_credential_manager_ representedObject]);
}

// Tests that a zero-click credential request is resolved properly with an empty
// credential when zero-click sign-in is disabled.
TEST_F(CredentialManagerTest,
       ZeroClickRequestResolvedWithEmptyCredentialWhenZeroClickSignInDisabled) {
  // Set a password store, but request a zero-click credential with zero-click
  // disabled.
  stub_client_.SetPasswordStore(new password_manager::TestPasswordStore);
  const bool zero_click = true;
  static_cast<TestingPrefServiceSimple*>(stub_client_.GetPrefs())
      ->SetUserPref(password_manager::prefs::kCredentialsEnableAutosignin,
                    new base::FundamentalValue(!zero_click));

  // Requesting a zero-click credential should immediately resolve the request
  // with an empty credential.
  const int request_id = 0;
  bool verified = false;
  ExpectPromiseResolved(&verified, request_id, web::Credential());
  credential_manager_->CredentialsRequested(
      request_id, GURL(kTestURL), zero_click, std::vector<std::string>(), true);

  // Pump the message loop and verify.
  WaitForBackgroundTasks();
  EXPECT_TRUE(verified);
}

// Tests that a credential request is properly resolved with an empty credential
// when no credentials are available.
TEST_F(CredentialManagerTest,
       RequestResolvedWithEmptyCredentialWhenNoneAvailable) {
  // Set a password store with no credentials, enable zero-click, and request a
  // zero-click credential.
  stub_client_.SetPasswordStore(new password_manager::TestPasswordStore);
  const bool zero_click = true;
  static_cast<TestingPrefServiceSimple*>(stub_client_.GetPrefs())
      ->SetUserPref(password_manager::prefs::kCredentialsEnableAutosignin,
                    new base::FundamentalValue(zero_click));

  // Requesting a zero-click credential should try to retrieve PasswordForms
  // from the PasswordStore and resolve the request with an empty Credential
  // when none are found.
  const int request_id = 0;
  bool verified = false;
  ExpectPromiseResolved(&verified, request_id, web::Credential());
  credential_manager_->CredentialsRequested(
      request_id, GURL(kTestURL), zero_click, std::vector<std::string>(), true);

  // Pump the message loop and verify.
  WaitForBackgroundTasks();
  EXPECT_TRUE(verified);
}

// Tests that a zero-click credential request properly resolves.
TEST_F(CredentialManagerTest, ZeroClickRequestResolved) {
  // Set a password store with a credential, enable zero-click, and request a
  // zero-click credential.
  scoped_refptr<password_manager::TestPasswordStore> store(
      new password_manager::TestPasswordStore);
  const bool zero_click = true;
  store->AddLogin(GetTestPasswordForm1(zero_click));
  stub_client_.SetPasswordStore(store);
  static_cast<TestingPrefServiceSimple*>(stub_client_.GetPrefs())
      ->SetUserPref(password_manager::prefs::kCredentialsEnableAutosignin,
                    new base::FundamentalValue(zero_click));
  // Without the first run experience, the zero-click credentials won't be
  // passed to the site.
  static_cast<TestingPrefServiceSimple*>(stub_client_.GetPrefs())
      ->SetUserPref(
          password_manager::prefs::kWasAutoSignInFirstRunExperienceShown,
          new base::FundamentalValue(true));
  WaitForBackgroundTasks();

  // Requesting a zero-click credential should retrieve a PasswordForm from the
  // PasswordStore and resolve the request with a corresponding Credential.
  const int request_id = 0;
  bool verified = false;
  ExpectPromiseResolved(&verified, request_id,
                        GetTestWebCredential1(zero_click));
  credential_manager_->CredentialsRequested(
      request_id, GURL(kTestURL), zero_click, std::vector<std::string>(), true);

  // Pump the message loop and verify.
  WaitForBackgroundTasks();
  EXPECT_TRUE(verified);
}

// Tests that a credential request properly resolves.
// TODO(crbug.com/598851): Reenabled this test.
TEST_F(CredentialManagerTest, DISABLED_RequestResolved) {
  // Set a password store with two credentials and set a credential to be
  // returned by the PasswordManagerClient as if chosen by the user.
  scoped_refptr<password_manager::TestPasswordStore> store(
      new password_manager::TestPasswordStore);
  const bool zero_click = false;
  store->AddLogin(GetTestPasswordForm1(zero_click));
  store->AddLogin(GetTestPasswordForm2(zero_click));
  stub_client_.SetPasswordStore(store);
  stub_client_.SetUserChosenCredential(password_manager::CredentialInfo(
      GetTestPasswordForm2(zero_click),
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD));
  WaitForBackgroundTasks();

  // Request a credential. The request should be resolved with the credential
  // set on the stub client.
  const int request_id = 0;
  bool verified = false;
  ExpectPromiseResolved(&verified, request_id,
                        GetTestWebCredential2(zero_click));
  credential_manager_->CredentialsRequested(
      request_id, GURL(kTestURL), zero_click, std::vector<std::string>(), true);

  // Pump the message loop and verify.
  WaitForBackgroundTasks();
  EXPECT_TRUE(verified);
}

// Tests that two requests back-to-back succeed when they wait to be resolved.
// TODO(crbug.com/598851): Reenable this test.
TEST_F(CredentialManagerTest, DISABLED_ConsecutiveRequestsResolve) {
  // Set a password store with two credentials and set a credential to be
  // returned by the PasswordManagerClient as if chosen by the user.
  scoped_refptr<password_manager::TestPasswordStore> store(
      new password_manager::TestPasswordStore);
  const bool zero_click = false;
  store->AddLogin(GetTestPasswordForm1(zero_click));
  stub_client_.SetPasswordStore(store);
  stub_client_.SetUserChosenCredential(password_manager::CredentialInfo(
      GetTestPasswordForm1(zero_click),
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD));
  WaitForBackgroundTasks();

  // Request a credential. The request should be resolved with the credential
  // set on the stub client.
  const int first_request_id = 0;
  bool first_verified = false;
  ExpectPromiseResolved(&first_verified, first_request_id,
                        GetTestWebCredential1(zero_click));
  credential_manager_->CredentialsRequested(first_request_id, GURL(kTestURL),
                                            zero_click,
                                            std::vector<std::string>(), true);

  // Pump the message loop and verify.
  WaitForBackgroundTasks();
  EXPECT_TRUE(first_verified);

  ClearPromiseResolutionExpectations();

  // Make a second request. It should be resolved again.
  const int second_request_id = 1;
  bool second_verified = false;
  ExpectPromiseResolved(&second_verified, second_request_id,
                        GetTestWebCredential1(zero_click));
  credential_manager_->CredentialsRequested(second_request_id, GURL(kTestURL),
                                            zero_click,
                                            std::vector<std::string>(), true);

  // Pump the message loop and verify.
  WaitForBackgroundTasks();
  EXPECT_TRUE(second_verified);
}

// Tests that notifySignedIn prompts the user to save a password.
TEST_F(CredentialManagerTest,
       SignInResolvesAndPromptsUserWhenSavingEnabledAndNotBlacklisted) {
  // Set a password store so the PasswordFormManager can retrieve credentials.
  scoped_refptr<password_manager::TestPasswordStore> store(
      new password_manager::TestPasswordStore);
  stub_client_.SetPasswordStore(store);
  const bool saving_enabled = true;
  EXPECT_CALL(stub_client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillOnce(Return(saving_enabled))
      .WillOnce(Return(saving_enabled));

  // Notify the browser that the user signed in.
  const int request_id = 0;
  ExpectPromiseResolved(request_id);
  const bool zero_click = true;
  credential_manager_->SignedIn(request_id, GURL(kTestURL),
                                GetTestWebCredential1(zero_click));

  // Pump the message loop and verify.
  WaitForBackgroundTasks();
  autofill::PasswordForm expected_observed_form(
      GetTestPasswordForm1(zero_click));
  expected_observed_form.username_value.clear();
  expected_observed_form.password_value.clear();
  EXPECT_EQ(expected_observed_form, stub_client_.saved_form()->observed_form());
}

// Tests that notifySignedIn doesn't prompt the user to save a password when the
// password manager is disabled for the current page.
TEST_F(CredentialManagerTest,
       SignInResolvesAndDoesNotPromptsUserWhenSavingDisabledAndNotBlacklisted) {
  // Disable saving.
  scoped_refptr<password_manager::TestPasswordStore> store(
      new password_manager::TestPasswordStore);
  stub_client_.SetPasswordStore(store);
  const bool saving_enabled = false;
  EXPECT_CALL(stub_client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillOnce(Return(saving_enabled));

  // Notify the browser that the user signed in.
  const int request_id = 0;
  ExpectPromiseResolved(request_id);
  const bool zero_click = false;
  credential_manager_->SignedIn(request_id, GURL(kTestURL),
                                GetTestWebCredential1(zero_click));

  // Pump the message loop and verify that no form was saved.
  WaitForBackgroundTasks();
  EXPECT_FALSE(stub_client_.saved_form());
}

// Tests that notifySignedIn doesn't prompt the user to save a password when the
// submitted form is blacklisted by the password manager.
TEST_F(CredentialManagerTest,
       SignInResolvesAndDoesNotPromptUserWhenSavingEnabledAndBlacklisted) {
  // Disable saving.
  scoped_refptr<password_manager::TestPasswordStore> store(
      new password_manager::TestPasswordStore);
  stub_client_.SetPasswordStore(store);
  const bool saving_enabled = true;
  EXPECT_CALL(stub_client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillOnce(Return(saving_enabled))
      .WillOnce(Return(saving_enabled));

  // Save the credential that will be signed in and mark it blacklisted.
  const bool zero_click = false;
  autofill::PasswordForm blacklisted_form(GetTestPasswordForm1(zero_click));
  blacklisted_form.blacklisted_by_user = true;
  store->AddLogin(blacklisted_form);
  WaitForBackgroundTasks();

  // Notify the browser that the user signed in.
  const int request_id = 0;
  ExpectPromiseResolved(request_id);
  credential_manager_->SignedIn(request_id, GURL(kTestURL),
                                GetTestWebCredential1(zero_click));

  // Pump the message loop and verify that no form was saved.
  WaitForBackgroundTasks();
  EXPECT_FALSE(stub_client_.saved_form());
}

// Tests that notifySignedOut marks credentials as non-zero-click.
TEST_F(CredentialManagerTest, SignOutResolvesAndMarksFormsNonZeroClick) {
  // Create two zero-click credentials for the current page.
  std::string current_origin(credential_manager_->GetOrigin().spec());
  autofill::PasswordForm form1(GetTestPasswordForm1(true));
  form1.signon_realm = current_origin;
  autofill::PasswordForm form2(GetTestPasswordForm2(true));
  form2.signon_realm = current_origin;

  // Add both credentials to the store.
  scoped_refptr<password_manager::TestPasswordStore> store(
      new password_manager::TestPasswordStore);
  stub_client_.SetPasswordStore(store);
  store->AddLogin(form1);
  store->AddLogin(form2);
  WaitForBackgroundTasks();

  // Check that both credentials in the store are zero-click.
  EXPECT_EQ(1U, store->stored_passwords().size());
  const std::vector<autofill::PasswordForm>& passwords_before_signout =
      store->stored_passwords().find(current_origin)->second;
  EXPECT_EQ(2U, passwords_before_signout.size());
  for (const autofill::PasswordForm& form : passwords_before_signout)
    EXPECT_FALSE(form.skip_zero_click);

  // Sign out the current origin, which has credentials, and a second origin,
  // which doesnt.
  const int first_request_id = 0;
  ExpectPromiseResolved(first_request_id);
  credential_manager_->SignedOut(first_request_id,
                                 credential_manager_->GetOrigin());
  const int second_request_id = 1;
  ExpectPromiseResolved(second_request_id);
  credential_manager_->SignedOut(second_request_id, GURL("https://foo.com"));

  // Pump the message loop to let the promises resolve. Check that the
  // credentials in the store are now non-zero-click and that signing out an
  // origin with no credentials had no effect.
  WaitForBackgroundTasks();
  EXPECT_EQ(1U, store->stored_passwords().size());
  const std::vector<autofill::PasswordForm>& passwords_after_signout =
      store->stored_passwords().find(current_origin)->second;
  EXPECT_EQ(2U, passwords_after_signout.size());
  for (const autofill::PasswordForm& form : passwords_after_signout)
    EXPECT_TRUE(form.skip_zero_click);
}

}  // namespace
