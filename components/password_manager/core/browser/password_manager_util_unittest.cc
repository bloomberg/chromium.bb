// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;

namespace password_manager_util {
namespace {

constexpr char kTestAndroidRealm[] = "android://hash@com.example.beta.android";
constexpr char kTestFederationURL[] = "https://google.com/";
constexpr char kTestURL[] = "https://example.com/";
constexpr char kTestUsername[] = "Username";
constexpr char kTestUsername2[] = "Username2";
constexpr char kTestPassword[] = "12345";

autofill::PasswordForm GetTestAndroidCredentials(const char* signon_realm) {
  autofill::PasswordForm form;
  form.scheme = autofill::PasswordForm::SCHEME_HTML;
  form.signon_realm = signon_realm;
  form.username_value = base::ASCIIToUTF16(kTestUsername);
  form.password_value = base::ASCIIToUTF16(kTestPassword);
  return form;
}

// The argument is std::vector<autofill::PasswordForm*>*. The caller is
// responsible for the lifetime of all the password forms.
ACTION_P(AppendForm, form) {
  arg0->push_back(std::make_unique<autofill::PasswordForm>(form));
}

}  // namespace

using password_manager::UnorderedPasswordFormElementsAre;
using testing::_;
using testing::DoAll;
using testing::Return;

TEST(PasswordManagerUtil, TrimUsernameOnlyCredentials) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> forms;
  std::vector<std::unique_ptr<autofill::PasswordForm>> expected_forms;
  forms.push_back(std::make_unique<autofill::PasswordForm>(
      GetTestAndroidCredentials(kTestAndroidRealm)));
  expected_forms.push_back(std::make_unique<autofill::PasswordForm>(
      GetTestAndroidCredentials(kTestAndroidRealm)));

  autofill::PasswordForm username_only;
  username_only.scheme = autofill::PasswordForm::SCHEME_USERNAME_ONLY;
  username_only.signon_realm = kTestAndroidRealm;
  username_only.username_value = base::ASCIIToUTF16(kTestUsername2);
  forms.push_back(std::make_unique<autofill::PasswordForm>(username_only));

  username_only.federation_origin =
      url::Origin::Create(GURL(kTestFederationURL));
  username_only.skip_zero_click = false;
  forms.push_back(std::make_unique<autofill::PasswordForm>(username_only));
  username_only.skip_zero_click = true;
  expected_forms.push_back(
      std::make_unique<autofill::PasswordForm>(username_only));

  TrimUsernameOnlyCredentials(&forms);

  EXPECT_THAT(forms, UnorderedPasswordFormElementsAre(&expected_forms));
}

TEST(PasswordManagerUtil, RemoveBlacklistedDuplicates) {
  autofill::PasswordForm blacklisted;
  blacklisted.blacklisted_by_user = true;
  blacklisted.signon_realm = kTestURL;
  blacklisted.origin = GURL(kTestURL);

  autofill::PasswordForm blacklisted_first_example = blacklisted;
  blacklisted_first_example.signon_realm += "first_example";

  autofill::PasswordForm blacklisted_second_example = blacklisted;
  blacklisted_second_example.signon_realm += "second_example";

  base::test::ScopedTaskEnvironment scoped_task_environment;
  TestingPrefServiceSimple prefs;

  prefs.registry()->RegisterBooleanPref(
      password_manager::prefs::kDuplicatedBlacklistedCredentialsRemoved, false);

  auto password_store = base::MakeRefCounted<
      testing::StrictMock<password_manager::MockPasswordStore>>();
  ASSERT_TRUE(
      password_store->Init(syncer::SyncableService::StartSyncFlare(), nullptr));

  EXPECT_CALL(*password_store, FillBlacklistLogins(_))
      .WillOnce(DoAll(AppendForm(blacklisted),
                      AppendForm(blacklisted_first_example),
                      AppendForm(blacklisted_second_example),
                      AppendForm(blacklisted_first_example), Return(true)));

  // Duplicated credentials are to be deleted.
  EXPECT_CALL(*password_store, RemoveLogin(blacklisted_first_example));
  DeleteBlacklistedDuplicates(password_store.get(), &prefs, 0);
  scoped_task_environment.RunUntilIdle();
  password_store->ShutdownOnUIThread();
}

#if !defined(OS_IOS)
TEST(PasswordManagerUtil, ReportHttpMigrationMetrics) {
  for (bool is_hsts_enabled : {false, true}) {
    base::test::ScopedTaskEnvironment scoped_task_environment;
    auto request_context =
        base::MakeRefCounted<net::TestURLRequestContextGetter>(
            base::ThreadTaskRunnerHandle::Get());

    net::TransportSecurityState* state =
        request_context->GetURLRequestContext()->transport_security_state();
    const base::Time expiry = base::Time::Max();
    const bool include_subdomains = false;

    auto password_store =
        base::MakeRefCounted<password_manager::TestPasswordStore>();
    ASSERT_TRUE(password_store->Init(syncer::SyncableService::StartSyncFlare(),
                                     nullptr));

    autofill::PasswordForm hsts_host_1;
    hsts_host_1.origin = GURL("http://hsts-host-1/");
    hsts_host_1.username_value = base::ASCIIToUTF16(kTestUsername);
    hsts_host_1.password_value = base::ASCIIToUTF16(kTestPassword);
    password_store->AddLogin(hsts_host_1);

    autofill::PasswordForm hsts_host_1_https;
    hsts_host_1_https.origin = GURL("https://hsts-host-1/");
    hsts_host_1_https.username_value = base::ASCIIToUTF16(kTestUsername);
    hsts_host_1_https.password_value = base::ASCIIToUTF16(kTestPassword);
    password_store->AddLogin(hsts_host_1_https);

    autofill::PasswordForm hsts_host_2;
    hsts_host_2.origin = GURL("http://hsts-host-2/");
    hsts_host_2.username_value = base::ASCIIToUTF16(kTestUsername2);
    hsts_host_2.password_value = base::ASCIIToUTF16(kTestPassword);
    password_store->AddLogin(hsts_host_2);

    autofill::PasswordForm hsts_host_2_https;
    hsts_host_2_https.origin = GURL("https://hsts-host-2/");
    hsts_host_2_https.username_value = base::ASCIIToUTF16(kTestUsername2);
    hsts_host_2_https.password_value = base::ASCIIToUTF16("different_password");
    password_store->AddLogin(hsts_host_2_https);

    // HTTPS version is not in Password Store.
    autofill::PasswordForm hsts_host_3;
    hsts_host_3.origin = GURL("http://hsts-host-3/");
    password_store->AddLogin(hsts_host_3);

    if (is_hsts_enabled) {
      state->AddHSTS(hsts_host_1.origin.host(), expiry, include_subdomains);
      state->AddHSTS(hsts_host_2.origin.host(), expiry, include_subdomains);
      state->AddHSTS(hsts_host_3.origin.host(), expiry, include_subdomains);
    }
    scoped_task_environment.RunUntilIdle();

    base::HistogramTester histogram_tester;
    ReportHttpMigrationMetrics(password_store, std::move(request_context));
    scoped_task_environment.RunUntilIdle();

    for (bool test_hsts_enabled : {false, true}) {
      std::string suffix =
          (test_hsts_enabled ? "WithHSTSEnabled" : "HSTSNotEnabled");

      // hsts_host_1
      histogram_tester.ExpectUniqueSample(
          "PasswordManager.HttpCredentialsWithEquivalentHttpsCredential." +
              suffix,
          (test_hsts_enabled == is_hsts_enabled), 1);

      // hsts_host_2
      histogram_tester.ExpectUniqueSample(
          "PasswordManager.HttpCredentialsWithConflictingHttpsCredential." +
              suffix,
          (test_hsts_enabled == is_hsts_enabled), 1);

      // hsts_host_3
      histogram_tester.ExpectUniqueSample(
          "PasswordManager.HttpCredentialsWithoutMatchingHttpsCredential." +
              suffix,
          (test_hsts_enabled == is_hsts_enabled), 1);
    }
    password_store->ShutdownOnUIThread();
    scoped_task_environment.RunUntilIdle();
  }
}
#endif  // !defined(OS_IOS)

TEST(PasswordManagerUtil, FindBestMatches) {
  const int kNotFound = -1;
  struct TestMatch {
    bool is_psl_match;
    bool preferred;
    std::string username;
  };
  struct TestCase {
    const char* description;
    std::vector<TestMatch> matches;
    int expected_preferred_match_index;
    std::map<std::string, size_t> expected_best_matches_indices;
  } test_cases[] = {
      {"Empty matches", {}, kNotFound, {}},
      {"1 preferred non-psl match",
       {{.is_psl_match = false, .preferred = true, .username = "u"}},
       0,
       {{"u", 0}}},
      {"1 non-preferred psl match",
       {{.is_psl_match = true, .preferred = false, .username = "u"}},
       0,
       {{"u", 0}}},
      {"2 matches with the same username",
       {{.is_psl_match = false, .preferred = false, .username = "u"},
        {.is_psl_match = false, .preferred = true, .username = "u"}},
       1,
       {{"u", 1}}},
      {"2 matches with different usernames, preferred taken",
       {{.is_psl_match = false, .preferred = false, .username = "u1"},
        {.is_psl_match = false, .preferred = true, .username = "u2"}},
       1,
       {{"u1", 0}, {"u2", 1}}},
      {"2 matches with different usernames, non-psl much taken",
       {{.is_psl_match = false, .preferred = false, .username = "u1"},
        {.is_psl_match = true, .preferred = true, .username = "u2"}},
       0,
       {{"u1", 0}, {"u2", 1}}},
      {"8 matches, 3 usernames",
       {{.is_psl_match = false, .preferred = false, .username = "u2"},
        {.is_psl_match = true, .preferred = false, .username = "u3"},
        {.is_psl_match = true, .preferred = false, .username = "u1"},
        {.is_psl_match = false, .preferred = true, .username = "u3"},
        {.is_psl_match = true, .preferred = false, .username = "u1"},
        {.is_psl_match = false, .preferred = false, .username = "u2"},
        {.is_psl_match = true, .preferred = true, .username = "u3"},
        {.is_psl_match = false, .preferred = false, .username = "u1"}},
       3,
       {{"u1", 7}, {"u2", 0}, {"u3", 3}}},

  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(testing::Message("Test description: ")
                 << test_case.description);
    // Convert TestMatch to PasswordForm.
    std::vector<PasswordForm> owning_matches;
    for (const TestMatch match : test_case.matches) {
      PasswordForm form;
      form.is_public_suffix_match = match.is_psl_match;
      form.preferred = match.preferred;
      form.username_value = base::ASCIIToUTF16(match.username);
      owning_matches.push_back(form);
    }
    std::vector<const PasswordForm*> matches;
    for (const PasswordForm& match : owning_matches)
      matches.push_back(&match);

    std::map<base::string16, const PasswordForm*> best_matches;
    std::vector<const PasswordForm*> not_best_matches;
    const PasswordForm* preferred_match = nullptr;

    FindBestMatches(matches, &best_matches, &not_best_matches,
                    &preferred_match);

    if (test_case.expected_preferred_match_index == kNotFound) {
      // Case of empty |matches|.
      EXPECT_FALSE(preferred_match);
      EXPECT_TRUE(best_matches.empty());
      EXPECT_TRUE(not_best_matches.empty());
    } else {
      // Check |preferred_match|.
      EXPECT_EQ(matches[test_case.expected_preferred_match_index],
                preferred_match);
      // Check best matches.
      ASSERT_EQ(test_case.expected_best_matches_indices.size(),
                best_matches.size());

      for (const auto& username_match : best_matches) {
        std::string username = base::UTF16ToUTF8(username_match.first);
        ASSERT_NE(test_case.expected_best_matches_indices.end(),
                  test_case.expected_best_matches_indices.find(username));
        size_t expected_index =
            test_case.expected_best_matches_indices.at(username);
        size_t actual_index = std::distance(
            matches.begin(),
            std::find(matches.begin(), matches.end(), username_match.second));
        EXPECT_EQ(expected_index, actual_index);
      }

      // Check non-best matches.
      ASSERT_EQ(matches.size(), best_matches.size() + not_best_matches.size());
      for (const PasswordForm* form : not_best_matches) {
        // A non-best match form must not be in |best_matches|.
        EXPECT_NE(best_matches[form->username_value], form);

        matches.erase(std::remove(matches.begin(), matches.end(), form),
                      matches.end());
      }
      // Expect that all non-best matches were found in |matches| and only best
      // matches left.
      EXPECT_EQ(best_matches.size(), matches.size());
    }
  }
}

}  // namespace password_manager_util
