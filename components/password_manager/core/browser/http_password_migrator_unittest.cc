// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/http_password_migrator.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using autofill::PasswordForm;
using testing::ElementsAre;
using testing::Pointee;
using testing::SaveArg;
using testing::_;

constexpr char kTestHttpsURL[] = "https://example.org/";
constexpr char kTestHttpURL[] = "http://example.org/";
constexpr char kTestSubdomainHttpURL[] = "http://login.example.org/";

// Creates a dummy http form with some basic arbitrary values.
PasswordForm CreateTestForm() {
  PasswordForm form;
  form.origin = GURL(kTestHttpURL);
  form.signon_realm = form.origin.spec();
  form.action = GURL("https://example.org/action.html");
  form.username_value = base::ASCIIToUTF16("user");
  form.password_value = base::ASCIIToUTF16("password");
  return form;
}

// Creates a dummy http PSL-matching form with some basic arbitrary values.
PasswordForm CreateTestPSLForm() {
  PasswordForm form;
  form.origin = GURL(kTestSubdomainHttpURL);
  form.signon_realm = form.origin.spec();
  form.action = GURL(kTestSubdomainHttpURL);
  form.username_value = base::ASCIIToUTF16("user2");
  form.password_value = base::ASCIIToUTF16("password2");
  form.is_public_suffix_match = true;
  return form;
}

// Creates an Android credential.
PasswordForm CreateAndroidCredential() {
  PasswordForm form;
  form.username_value = base::ASCIIToUTF16("user3");
  form.password_value = base::ASCIIToUTF16("password3");
  form.signon_realm = "android://hash@com.example.android/";
  form.origin = GURL(form.signon_realm);
  form.action = GURL();
  form.is_affiliation_based_match = true;
  return form;
}

class MockConsumer : public HttpPasswordMigrator::Consumer {
 public:
  MOCK_METHOD1(ProcessForms,
               void(const std::vector<autofill::PasswordForm*>& forms));

  void ProcessMigratedForms(
      std::vector<std::unique_ptr<autofill::PasswordForm>> forms) override {
    std::vector<autofill::PasswordForm*> raw_forms(forms.size());
    std::transform(forms.begin(), forms.end(), raw_forms.begin(),
                   [](const std::unique_ptr<autofill::PasswordForm>& form) {
                     return form.get();
                   });
    ProcessForms(raw_forms);
  }
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  explicit MockPasswordManagerClient(PasswordStore* store) : store_(store) {}

  PasswordStore* GetPasswordStore() const override { return store_; }
  MOCK_CONST_METHOD2(PostHSTSQueryForHost,
                     void(const GURL&, const HSTSCallback& callback));

 private:
  PasswordStore* store_;

  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerClient);
};

}  // namespace

class HttpPasswordMigratorTest : public testing::Test {
 public:
  HttpPasswordMigratorTest()
      : mock_store_(new testing::StrictMock<MockPasswordStore>),
        client_(mock_store_.get()) {}

  ~HttpPasswordMigratorTest() override { mock_store_->ShutdownOnUIThread(); }

  MockConsumer& consumer() { return consumer_; }
  MockPasswordStore& store() { return *mock_store_; }
  MockPasswordManagerClient& client() { return client_; }

 protected:
  void TestEmptyStore(bool is_hsts);
  void TestFullStore(bool is_hsts);

 private:
  base::MessageLoop message_loop_;  // Used by mock_store_.
  MockConsumer consumer_;
  scoped_refptr<MockPasswordStore> mock_store_;
  MockPasswordManagerClient client_;

  DISALLOW_COPY_AND_ASSIGN(HttpPasswordMigratorTest);
};

void HttpPasswordMigratorTest::TestEmptyStore(bool is_hsts) {
  PasswordStore::FormDigest form(autofill::PasswordForm::SCHEME_HTML,
                                 kTestHttpURL, GURL(kTestHttpURL));
  EXPECT_CALL(store(), GetLogins(form, _));
  PasswordManagerClient::HSTSCallback callback;
  EXPECT_CALL(client(), PostHSTSQueryForHost(GURL(kTestHttpsURL), _))
      .WillOnce(SaveArg<1>(&callback));
  HttpPasswordMigrator migrator(GURL(kTestHttpsURL), &client(), &consumer());
  callback.Run(is_hsts);

  EXPECT_CALL(consumer(), ProcessForms(std::vector<autofill::PasswordForm*>()));
  migrator.OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>>());
}

void HttpPasswordMigratorTest::TestFullStore(bool is_hsts) {
  PasswordStore::FormDigest form_digest(autofill::PasswordForm::SCHEME_HTML,
                                        kTestHttpURL, GURL(kTestHttpURL));
  EXPECT_CALL(store(), GetLogins(form_digest, _));
  PasswordManagerClient::HSTSCallback callback;
  EXPECT_CALL(client(), PostHSTSQueryForHost(GURL(kTestHttpsURL), _))
      .WillOnce(SaveArg<1>(&callback));
  HttpPasswordMigrator migrator(GURL(kTestHttpsURL), &client(), &consumer());
  callback.Run(is_hsts);

  PasswordForm form = CreateTestForm();
  PasswordForm psl_form = CreateTestPSLForm();
  PasswordForm android_form = CreateAndroidCredential();
  PasswordForm expected_form = form;
  expected_form.origin = GURL(kTestHttpsURL);
  expected_form.signon_realm = expected_form.origin.spec();

  EXPECT_CALL(store(), AddLogin(expected_form));
  EXPECT_CALL(store(), RemoveLogin(form)).Times(is_hsts);
  EXPECT_CALL(consumer(), ProcessForms(ElementsAre(Pointee(expected_form))));
  std::vector<std::unique_ptr<autofill::PasswordForm>> results;
  results.push_back(base::MakeUnique<PasswordForm>(psl_form));
  results.push_back(base::MakeUnique<PasswordForm>(form));
  results.push_back(base::MakeUnique<PasswordForm>(android_form));
  migrator.OnGetPasswordStoreResults(std::move(results));
}

TEST_F(HttpPasswordMigratorTest, EmptyStoreWithHSTS) {
  TestEmptyStore(true);
}

TEST_F(HttpPasswordMigratorTest, EmptyStoreWithoutHSTS) {
  TestEmptyStore(false);
}

TEST_F(HttpPasswordMigratorTest, FullStoreWithHSTS) {
  TestFullStore(true);
}

TEST_F(HttpPasswordMigratorTest, FullStoreWithoutHSTS) {
  TestFullStore(false);
}

}  // namespace password_manager
