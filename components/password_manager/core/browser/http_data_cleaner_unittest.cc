// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/http_data_cleaner.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using testing::Invoke;
using testing::Message;
using testing::Mock;
using testing::NiceMock;
using testing::_;

namespace password_manager {

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

class HTTPDataCleanerTest : public testing::Test {
 public:
  HTTPDataCleanerTest()
      : store_(new NiceMock<MockPasswordStore>),
        prefs_(std::make_unique<TestingPrefServiceSimple>()),
        request_context_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())) {
    prefs()->registry()->RegisterBooleanPref(prefs::kWasObsoleteHttpDataCleaned,
                                             false);
    store_->Init(syncer::SyncableService::StartSyncFlare(), nullptr);
  }

  ~HTTPDataCleanerTest() override { store_->ShutdownOnUIThread(); }

  MockPasswordStore* store() { return store_.get(); }

  TestingPrefServiceSimple* prefs() { return prefs_.get(); }

  const scoped_refptr<net::TestURLRequestContextGetter>& request_context() {
    return request_context_;
  }

  void WaitUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<MockPasswordStore> store_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;

  DISALLOW_COPY_AND_ASSIGN(HTTPDataCleanerTest);
};

TEST_F(HTTPDataCleanerTest, TestBlacklistDeletion) {
  for (bool is_http : {false, true}) {
    for (bool is_hsts : {false, true}) {
      SCOPED_TRACE(Message() << std::boolalpha << "(is_http, is_hsts): ("
                             << is_http << ", " << is_hsts << ")");

      prefs()->SetBoolean(prefs::kWasObsoleteHttpDataCleaned, false);

      const bool should_be_deleted = is_http && is_hsts;

      PasswordForm form =
          is_http ? CreateTestHTTPForm() : CreateTestHTTPSForm();
      form.blacklisted_by_user = true;
      form.username_value.clear();
      form.password_value.clear();

      HSTSStateManager manager(
          request_context()->GetURLRequestContext()->transport_security_state(),
          is_hsts, form.origin.host());

      EXPECT_CALL(*store(), FillBlacklistLogins(_))
          .WillOnce(Invoke(
              [&form](
                  std::vector<std::unique_ptr<autofill::PasswordForm>>* forms) {
                *forms = WrapForms({form});
                return true;
              }));

      EXPECT_CALL(*store(), RemoveLogin(form)).Times(should_be_deleted);

      // Initiate clean up and make sure all aync tasks are run until
      // completion.
      CleanObsoleteHttpDataForPasswordStoreAndPrefsForTesting(
          store(), prefs(), request_context());
      WaitUntilIdle();

      // Verify and clear all expectations as well as the preference.
      Mock::VerifyAndClearExpectations(store());
      EXPECT_TRUE(prefs()->GetBoolean(prefs::kWasObsoleteHttpDataCleaned));
    }
  }
}

TEST_F(HTTPDataCleanerTest, TestAutofillableDeletion) {
  for (bool is_hsts : {false, true}) {
    for (bool same_host : {false, true}) {
      for (bool same_user : {false, true}) {
        for (bool same_pass : {false, true}) {
          SCOPED_TRACE(Message()
                       << std::boolalpha
                       << "(is_hsts, same_host, same_user, same_pass): ("
                       << is_hsts << ", " << same_host << ", " << same_user
                       << ", " << same_pass);

          prefs()->SetBoolean(prefs::kWasObsoleteHttpDataCleaned, false);

          const bool should_be_deleted =
              is_hsts && same_host && same_user && same_pass;

          PasswordForm http_form = CreateTestHTTPForm();
          PasswordForm https_form = CreateTestHTTPSForm();

          if (!same_host) {
            GURL::Replacements rep;
            rep.SetHostStr("a-totally-different-host");
            http_form.origin = http_form.origin.ReplaceComponents(rep);
          }

          if (!same_user)
            http_form.username_value = base::ASCIIToUTF16("different-user");

          if (!same_pass)
            http_form.password_value = base::ASCIIToUTF16("different-pass");

          HSTSStateManager manager(request_context()
                                       ->GetURLRequestContext()
                                       ->transport_security_state(),
                                   is_hsts, https_form.origin.host());

          EXPECT_CALL(*store(), FillAutofillableLogins(_))
              .WillOnce(Invoke(
                  [&http_form, &https_form](
                      std::vector<std::unique_ptr<autofill::PasswordForm>>*
                          forms) {
                    *forms = WrapForms({http_form, https_form});
                    return true;
                  }));

          EXPECT_CALL(*store(), RemoveLogin(http_form))
              .Times(should_be_deleted);

          // Initiate clean up and make sure all aync tasks are run until
          // completion.
          CleanObsoleteHttpDataForPasswordStoreAndPrefsForTesting(
              store(), prefs(), request_context());
          WaitUntilIdle();

          // Verify and clear all expectations as well as the preference.
          Mock::VerifyAndClearExpectations(store());
          EXPECT_TRUE(prefs()->GetBoolean(prefs::kWasObsoleteHttpDataCleaned));
        }
      }
    }
  }
}

TEST_F(HTTPDataCleanerTest, TestSiteStatsDeletion) {
  for (bool is_http : {false, true}) {
    for (bool is_hsts : {false, true}) {
      SCOPED_TRACE(Message() << std::boolalpha << "(is_http, is_hsts): ("
                             << is_http << ", " << is_hsts);

      prefs()->SetBoolean(prefs::kWasObsoleteHttpDataCleaned, false);

      const bool should_be_deleted = is_http && is_hsts;

      InteractionsStats stats =
          is_http ? CreateTestHTTPStats() : CreateTestHTTPSStats();

      HSTSStateManager manager(
          request_context()->GetURLRequestContext()->transport_security_state(),
          is_hsts, stats.origin_domain.host());

      EXPECT_CALL(*store(), GetAllSiteStatsImpl()).WillOnce(Invoke([&stats]() {
        return std::vector<InteractionsStats>({stats});
      }));
      EXPECT_CALL(*store(), RemoveSiteStatsImpl(stats.origin_domain))
          .Times(should_be_deleted);

      // Initiate clean up and make sure all async tasks are run until
      // completion.
      CleanObsoleteHttpDataForPasswordStoreAndPrefsForTesting(
          store(), prefs(), request_context());
      WaitUntilIdle();

      // Verify and clear all expectations as well as the preference.
      Mock::VerifyAndClearExpectations(store());
      EXPECT_TRUE(prefs()->GetBoolean(prefs::kWasObsoleteHttpDataCleaned));
    }
  }
}

}  // namespace password_manager
