// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/obsolete_http_cleaner.h"

#include <ios>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using autofill::PasswordForm;
using testing::Return;

namespace password_manager {

namespace {

constexpr char kTestHttpURL[] = "http://example.org/";
constexpr char kTestHttpsURL[] = "https://example.org/";

PasswordForm CreateTestHTTPForm() {
  PasswordForm form;
  form.origin = GURL(kTestHttpURL);
  form.signon_realm = form.origin.spec();
  form.action = form.origin;
  form.username_value = base::ASCIIToUTF16("user");
  form.password_value = base::ASCIIToUTF16("password");
  return form;
}

PasswordForm CreateTestHTTPSForm() {
  PasswordForm form;
  form.origin = GURL(kTestHttpsURL);
  form.signon_realm = form.origin.spec();
  form.action = form.origin;
  form.username_value = base::ASCIIToUTF16("user");
  form.password_value = base::ASCIIToUTF16("password");
  return form;
}

InteractionsStats CreateTestHTTPStats() {
  InteractionsStats stats;
  stats.origin_domain = GURL(kTestHttpURL);
  stats.username_value = base::ASCIIToUTF16("user");
  return stats;
}

InteractionsStats CreateTestHTTPSStats() {
  InteractionsStats stats;
  stats.origin_domain = GURL(kTestHttpsURL);
  stats.username_value = base::ASCIIToUTF16("user");
  return stats;
}

// Small helper that wraps passed in forms in unique ptrs.
std::vector<std::unique_ptr<PasswordForm>> MakeResults(
    const std::vector<PasswordForm>& forms) {
  std::vector<std::unique_ptr<PasswordForm>> results;
  results.reserve(forms.size());
  for (const auto& form : forms)
    results.push_back(base::MakeUnique<PasswordForm>(form));
  return results;
}

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  explicit MockPasswordManagerClient(PasswordStore* store) : store_(store) {}

  PasswordStore* GetPasswordStore() const override { return store_; }
  MOCK_CONST_METHOD1(IsHSTSActiveForHost, bool(const GURL&));

 private:
  PasswordStore* store_;

  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerClient);
};

}  // namespace

class ObsoleteHttpCleanerTest : public testing::Test {
 public:
  ObsoleteHttpCleanerTest()
      : mock_store_(new testing::StrictMock<MockPasswordStore>),
        client_(mock_store_.get()) {}
  ~ObsoleteHttpCleanerTest() override { mock_store_->ShutdownOnUIThread(); }

  MockPasswordStore& store() { return *mock_store_; }
  MockPasswordManagerClient& client() { return client_; }

 private:
  base::MessageLoop message_loop_;  // Used by mock_store_.
  scoped_refptr<MockPasswordStore> mock_store_;
  MockPasswordManagerClient client_;

  DISALLOW_COPY_AND_ASSIGN(ObsoleteHttpCleanerTest);
};

TEST_F(ObsoleteHttpCleanerTest, TestBlacklistDeletion) {
  struct TestCase {
    bool is_http;
    bool is_blacklisted;
    bool is_hsts;
    bool is_deleted;
  };

  constexpr static TestCase cases[] = {
      {true, true, true, true},    {true, true, false, false},
      {true, false, true, false},  {true, false, false, false},
      {false, true, true, false},  {false, true, false, false},
      {false, false, true, false}, {false, false, false, false},
  };

  ObsoleteHttpCleaner cleaner(&client());
  for (const auto& test_case : cases) {
    SCOPED_TRACE(testing::Message()
                 << std::boolalpha
                 << "(is_http, is_blacklisted, is_hsts, is_deleted): ("
                 << test_case.is_http << ", " << test_case.is_blacklisted
                 << ", " << test_case.is_hsts << ", " << test_case.is_deleted
                 << ")");

    PasswordForm form =
        test_case.is_http ? CreateTestHTTPForm() : CreateTestHTTPSForm();
    form.blacklisted_by_user = test_case.is_blacklisted;
    if (test_case.is_http && test_case.is_blacklisted) {
      EXPECT_CALL(client(), IsHSTSActiveForHost(form.origin))
          .WillOnce(Return(test_case.is_hsts));
    }

    EXPECT_CALL(store(), RemoveLogin(form)).Times(test_case.is_deleted);
    cleaner.OnGetPasswordStoreResults(MakeResults({form}));
  }
}

TEST_F(ObsoleteHttpCleanerTest, TestAutofillableDeletion) {
  struct TestCase {
    bool is_hsts;
    bool same_host;
    bool same_user;
    bool same_pass;
    bool is_deleted;
  };

  constexpr static TestCase cases[] = {
      {true, true, true, true, true},     {true, true, true, false, false},
      {true, true, false, true, false},   {true, true, false, false, false},
      {true, false, true, true, false},   {true, false, true, false, false},
      {true, false, false, true, false},  {true, false, false, false, false},
      {false, true, true, true, false},   {false, true, true, false, false},
      {false, true, false, true, false},  {false, true, false, false, false},
      {false, false, true, true, false},  {false, false, true, false, false},
      {false, false, false, true, false}, {false, false, false, false, false},
  };

  ObsoleteHttpCleaner cleaner(&client());
  for (const auto& test_case : cases) {
    SCOPED_TRACE(testing::Message()
                 << std::boolalpha
                 << "(is_hsts, same_host, same_user, same_pass, is_deleted): ("
                 << test_case.is_hsts << ", " << test_case.same_host << ", "
                 << test_case.same_user << ", " << test_case.same_pass << ", "
                 << test_case.is_deleted << ")");

    PasswordForm http_form = CreateTestHTTPForm();
    PasswordForm https_form = CreateTestHTTPSForm();

    if (!test_case.same_host) {
      GURL::Replacements rep;
      rep.SetHostStr("a-totally-different-host");
      http_form.origin = http_form.origin.ReplaceComponents(rep);
    }

    if (!test_case.same_user) {
      http_form.username_value =
          https_form.username_value + base::ASCIIToUTF16("-different");
    }

    if (!test_case.same_pass) {
      http_form.password_value =
          https_form.password_value + base::ASCIIToUTF16("-different");
    }

    EXPECT_CALL(client(), IsHSTSActiveForHost(https_form.origin))
        .WillOnce(Return(test_case.is_hsts));
    EXPECT_CALL(store(), RemoveLogin(http_form)).Times(test_case.is_deleted);
    cleaner.OnGetPasswordStoreResults(MakeResults({http_form, https_form}));
  }
}

TEST_F(ObsoleteHttpCleanerTest, TestSiteStatsDeletion) {
  struct TestCase {
    bool is_http;
    bool is_hsts;
    bool is_deleted;
  };

  constexpr static TestCase cases[] = {
      {true, true, true},
      {true, false, false},
      {false, true, false},
      {false, false, false},
  };

  ObsoleteHttpCleaner cleaner(&client());
  for (const auto& test_case : cases) {
    SCOPED_TRACE(testing::Message()
                 << std::boolalpha << "(is_http, is_hsts, is_deleted): ("
                 << test_case.is_http << ", " << test_case.is_hsts << ", "
                 << test_case.is_deleted << ")");

    InteractionsStats stats =
        test_case.is_http ? CreateTestHTTPStats() : CreateTestHTTPSStats();
    if (test_case.is_http) {
      EXPECT_CALL(client(), IsHSTSActiveForHost(stats.origin_domain))
          .WillOnce(Return(test_case.is_hsts));
    }

    EXPECT_CALL(store(), RemoveSiteStatsImpl(stats.origin_domain))
        .Times(test_case.is_deleted);
    cleaner.OnGetSiteStatistics({stats});
    base::RunLoop().RunUntilIdle();
  }
}

}  // namespace password_manager
