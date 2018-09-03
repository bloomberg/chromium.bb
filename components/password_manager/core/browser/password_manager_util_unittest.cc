// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;

namespace password_manager_util {
namespace {

constexpr char kTestAndroidRealm[] = "android://hash@com.example.beta.android";
constexpr char kTestFederationURL[] = "https://google.com/";
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

#if !defined(OS_IOS)
TEST(PasswordManagerUtil, ReportHttpMigrationMetrics) {
  enum class HttpCredentialType { kNoMatching, kEquivalent, kConflicting };

  struct TestCase {
    bool is_hsts_enabled;
    autofill::PasswordForm::Scheme http_form_scheme;
    bool same_signon_realm;
    bool same_scheme;
    bool same_username;
    bool same_password;
    HttpCredentialType expected;
  };

  struct Histogram {
    bool test_hsts_enabled;
    HttpCredentialType test_type;
    std::string histogram_name;
  };

  constexpr static TestCase cases[] = {

      {true, autofill::PasswordForm::Scheme::SCHEME_HTML, false, true, true,
       true, HttpCredentialType::kNoMatching},
      {true, autofill::PasswordForm::Scheme::SCHEME_HTML, true, false, true,
       true, HttpCredentialType::kNoMatching},
      {true, autofill::PasswordForm::Scheme::SCHEME_HTML, true, true, false,
       true, HttpCredentialType::kNoMatching},
      {true, autofill::PasswordForm::Scheme::SCHEME_HTML, true, true, true,
       false, HttpCredentialType::kConflicting},
      {true, autofill::PasswordForm::Scheme::SCHEME_HTML, true, true, true,
       true, HttpCredentialType::kEquivalent},

      {false, autofill::PasswordForm::Scheme::SCHEME_HTML, false, true, true,
       true, HttpCredentialType::kNoMatching},
      {false, autofill::PasswordForm::Scheme::SCHEME_HTML, true, false, true,
       true, HttpCredentialType::kNoMatching},
      {false, autofill::PasswordForm::Scheme::SCHEME_HTML, true, true, false,
       true, HttpCredentialType::kNoMatching},
      {false, autofill::PasswordForm::Scheme::SCHEME_HTML, true, true, true,
       false, HttpCredentialType::kConflicting},
      {false, autofill::PasswordForm::Scheme::SCHEME_HTML, true, true, true,
       true, HttpCredentialType::kEquivalent},

      {true, autofill::PasswordForm::Scheme::SCHEME_BASIC, false, true, true,
       true, HttpCredentialType::kNoMatching},
      {true, autofill::PasswordForm::Scheme::SCHEME_BASIC, true, false, true,
       true, HttpCredentialType::kNoMatching},
      {true, autofill::PasswordForm::Scheme::SCHEME_BASIC, true, true, false,
       true, HttpCredentialType::kNoMatching},
      {true, autofill::PasswordForm::Scheme::SCHEME_BASIC, true, true, true,
       false, HttpCredentialType::kConflicting},
      {true, autofill::PasswordForm::Scheme::SCHEME_BASIC, true, true, true,
       true, HttpCredentialType::kEquivalent},

      {false, autofill::PasswordForm::Scheme::SCHEME_BASIC, false, true, true,
       true, HttpCredentialType::kNoMatching},
      {false, autofill::PasswordForm::Scheme::SCHEME_BASIC, true, false, true,
       true, HttpCredentialType::kNoMatching},
      {false, autofill::PasswordForm::Scheme::SCHEME_BASIC, true, true, false,
       true, HttpCredentialType::kNoMatching},
      {false, autofill::PasswordForm::Scheme::SCHEME_BASIC, true, true, true,
       false, HttpCredentialType::kConflicting},
      {false, autofill::PasswordForm::Scheme::SCHEME_BASIC, true, true, true,
       true, HttpCredentialType::kEquivalent}

  };

  const base::string16 username[2] = {base::ASCIIToUTF16("user0"),
                                      base::ASCIIToUTF16("user1")};
  const base::string16 password[2] = {base::ASCIIToUTF16("pass0"),
                                      base::ASCIIToUTF16("pass1")};

  std::vector<Histogram> histograms_to_test;
  for (bool test_hsts_enabled : {true, false}) {
    std::string suffix =
        (test_hsts_enabled ? "WithHSTSEnabled" : "HSTSNotEnabled");
    histograms_to_test.push_back(
        {test_hsts_enabled, HttpCredentialType::kNoMatching,
         "PasswordManager.HttpCredentialsWithoutMatchingHttpsCredential." +
             suffix});
    histograms_to_test.push_back(
        {test_hsts_enabled, HttpCredentialType::kEquivalent,
         "PasswordManager.HttpCredentialsWithEquivalentHttpsCredential." +
             suffix});
    histograms_to_test.push_back(
        {test_hsts_enabled, HttpCredentialType::kConflicting,
         "PasswordManager.HttpCredentialsWithConflictingHttpsCredential." +
             suffix});
  }
  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "is_hsts_enabled=" << test.is_hsts_enabled
                 << ", http_form_scheme="
                 << static_cast<int>(test.http_form_scheme)
                 << ", same_signon_realm=" << test.same_signon_realm
                 << ", same_scheme=" << test.same_scheme
                 << ", same_username=" << test.same_username
                 << ", same_password=" << test.same_password);

    base::test::ScopedTaskEnvironment scoped_task_environment;
    auto request_context =
        base::MakeRefCounted<net::TestURLRequestContextGetter>(
            base::ThreadTaskRunnerHandle::Get());
    network::mojom::NetworkContextPtr network_context_pipe;
    auto network_context = std::make_unique<network::NetworkContext>(
        nullptr, mojo::MakeRequest(&network_context_pipe),
        request_context->GetURLRequestContext());

    auto password_store =
        base::MakeRefCounted<password_manager::TestPasswordStore>();
    ASSERT_TRUE(password_store->Init(syncer::SyncableService::StartSyncFlare(),
                                     nullptr));

    autofill::PasswordForm http_form;
    http_form.origin = GURL("http://example.org/");
    http_form.signon_realm = http_form.origin.GetOrigin().spec();
    http_form.scheme = test.http_form_scheme;
    http_form.username_value = username[1];
    http_form.password_value = password[1];
    password_store->AddLogin(http_form);

    autofill::PasswordForm https_form;
    https_form.origin = GURL("https://example.org/");
    https_form.scheme = test.http_form_scheme;
    if (!test.same_scheme) {
      if (https_form.scheme == autofill::PasswordForm::Scheme::SCHEME_BASIC)
        https_form.scheme = autofill::PasswordForm::Scheme::SCHEME_HTML;
      else
        https_form.scheme = autofill::PasswordForm::Scheme::SCHEME_BASIC;
    }

    https_form.signon_realm = https_form.origin.GetOrigin().spec();
    if (!test.same_signon_realm)
      https_form.signon_realm += "different/";

    https_form.username_value = username[test.same_username];
    https_form.password_value = password[test.same_password];
    password_store->AddLogin(https_form);

    if (test.is_hsts_enabled) {
      network_context->AddHSTSForTesting(
          http_form.origin.host(), base::Time::Max(),
          false /*include_subdomains*/, base::DoNothing());
    }
    scoped_task_environment.RunUntilIdle();

    base::HistogramTester histogram_tester;
    ReportHttpMigrationMetrics(
        password_store,
        base::BindLambdaForTesting([&]() -> network::mojom::NetworkContext* {
          // This needs to be network_context_pipe.get() and
          // not network_context.get() to make HSTS queries asynchronous, which
          // is what the progress tracking logic in HttpMetricsMigrationReporter
          // assumes.  This also matches reality, since
          // StoragePartition::GetNetworkContext will return a mojo pipe
          // even in the in-process case.
          return network_context_pipe.get();
        }));
    scoped_task_environment.RunUntilIdle();

    for (const auto& histogram : histograms_to_test) {
      int sample =
          static_cast<int>(histogram.test_type == test.expected &&
                           histogram.test_hsts_enabled == test.is_hsts_enabled);
      histogram_tester.ExpectUniqueSample(histogram.histogram_name, sample, 1);
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

        base::Erase(matches, form);
      }
      // Expect that all non-best matches were found in |matches| and only best
      // matches left.
      EXPECT_EQ(best_matches.size(), matches.size());
    }
  }
}

}  // namespace password_manager_util
