// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/http_password_store_migrator.h"
#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using autofill::PasswordForm;
using testing::ElementsAre;
using testing::Invoke;
using testing::Pointee;
using testing::SaveArg;
using testing::Unused;
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

class MockConsumer : public HttpPasswordStoreMigrator::Consumer {
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

class HttpPasswordStoreMigratorTest : public testing::Test {
 public:
  HttpPasswordStoreMigratorTest()
      : mock_store_(new testing::StrictMock<MockPasswordStore>),
        client_(mock_store_.get()) {
    mock_store_->Init(syncer::SyncableService::StartSyncFlare(), nullptr);
  }

  ~HttpPasswordStoreMigratorTest() override {
    mock_store_->ShutdownOnUIThread();
  }

  MockConsumer& consumer() { return consumer_; }
  MockPasswordStore& store() { return *mock_store_; }
  MockPasswordManagerClient& client() { return client_; }

  void WaitForPasswordStore() { scoped_task_environment_.RunUntilIdle(); }

 protected:
  void TestEmptyStore(bool is_hsts);
  void TestFullStore(bool is_hsts);
  void TestMigratorDeletionByConsumer(bool is_hsts);

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  MockConsumer consumer_;
  scoped_refptr<MockPasswordStore> mock_store_;
  MockPasswordManagerClient client_;

  DISALLOW_COPY_AND_ASSIGN(HttpPasswordStoreMigratorTest);
};

void HttpPasswordStoreMigratorTest::TestEmptyStore(bool is_hsts) {
  PasswordStore::FormDigest form(autofill::PasswordForm::SCHEME_HTML,
                                 kTestHttpURL, GURL(kTestHttpURL));
  EXPECT_CALL(store(), GetLogins(form, _));
  PasswordManagerClient::HSTSCallback callback;
  EXPECT_CALL(client(), PostHSTSQueryForHost(GURL(kTestHttpsURL), _))
      .WillOnce(SaveArg<1>(&callback));
  HttpPasswordStoreMigrator migrator(GURL(kTestHttpsURL), &client(),
                                     &consumer());
  callback.Run(is_hsts);
  // We expect a potential call to |RemoveSiteStatsImpl| which is a async task
  // posted from |PasswordStore::RemoveSiteStats|. Hence the following lines are
  // necessary to ensure |RemoveSiteStatsImpl| gets called when expected.
  EXPECT_CALL(store(), RemoveSiteStatsImpl(GURL(kTestHttpURL))).Times(is_hsts);
  WaitForPasswordStore();

  EXPECT_CALL(consumer(), ProcessForms(std::vector<autofill::PasswordForm*>()));
  migrator.OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>>());
}

void HttpPasswordStoreMigratorTest::TestFullStore(bool is_hsts) {
  PasswordStore::FormDigest form_digest(autofill::PasswordForm::SCHEME_HTML,
                                        kTestHttpURL, GURL(kTestHttpURL));
  EXPECT_CALL(store(), GetLogins(form_digest, _));
  PasswordManagerClient::HSTSCallback callback;
  EXPECT_CALL(client(), PostHSTSQueryForHost(GURL(kTestHttpsURL), _))
      .WillOnce(SaveArg<1>(&callback));
  HttpPasswordStoreMigrator migrator(GURL(kTestHttpsURL), &client(),
                                     &consumer());
  callback.Run(is_hsts);
  // We expect a potential call to |RemoveSiteStatsImpl| which is a async task
  // posted from |PasswordStore::RemoveSiteStats|. Hence the following lines are
  // necessary to ensure |RemoveSiteStatsImpl| gets called when expected.
  EXPECT_CALL(store(), RemoveSiteStatsImpl(GURL(kTestHttpURL))).Times(is_hsts);
  WaitForPasswordStore();

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
  results.push_back(std::make_unique<PasswordForm>(psl_form));
  results.push_back(std::make_unique<PasswordForm>(form));
  results.push_back(std::make_unique<PasswordForm>(android_form));
  migrator.OnGetPasswordStoreResults(std::move(results));
}

// This test checks whether the migration successfully completes even if the
// migrator gets explicitly deleted by its consumer. This test will crash if
// this is not the case.
void HttpPasswordStoreMigratorTest::TestMigratorDeletionByConsumer(
    bool is_hsts) {
  // Setup expectations on store and client.
  EXPECT_CALL(store(), GetLogins(_, _));
  PasswordManagerClient::HSTSCallback callback;
  EXPECT_CALL(client(), PostHSTSQueryForHost(GURL(kTestHttpsURL), _))
      .WillOnce(SaveArg<1>(&callback));

  // Construct the migrator, call |OnGetPasswordStoreResults| explicitly and
  // manually delete it.
  auto migrator = std::make_unique<HttpPasswordStoreMigrator>(
      GURL(kTestHttpsURL), &client(), &consumer());
  migrator->OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>>());
  EXPECT_CALL(consumer(), ProcessForms(_)).WillOnce(Invoke([&migrator](Unused) {
    migrator.reset();
  }));

  callback.Run(is_hsts);
  // We expect a potential call to |RemoveSiteStatsImpl| which is a async task
  // posted from |PasswordStore::RemoveSiteStats|. Hence the following lines are
  // necessary to ensure |RemoveSiteStatsImpl| gets called when expected.
  EXPECT_CALL(store(), RemoveSiteStatsImpl(GURL(kTestHttpURL))).Times(is_hsts);
  WaitForPasswordStore();
}

TEST_F(HttpPasswordStoreMigratorTest, EmptyStoreWithHSTS) {
  TestEmptyStore(true);
}

TEST_F(HttpPasswordStoreMigratorTest, EmptyStoreWithoutHSTS) {
  TestEmptyStore(false);
}

TEST_F(HttpPasswordStoreMigratorTest, FullStoreWithHSTS) {
  TestFullStore(true);
}

TEST_F(HttpPasswordStoreMigratorTest, FullStoreWithoutHSTS) {
  TestFullStore(false);
}

TEST_F(HttpPasswordStoreMigratorTest, MigratorDeletionByConsumerWithHSTS) {
  TestMigratorDeletionByConsumer(true);
}

TEST_F(HttpPasswordStoreMigratorTest, MigratorDeletionByConsumerWithoutHSTS) {
  TestMigratorDeletionByConsumer(false);
}

}  // namespace password_manager
