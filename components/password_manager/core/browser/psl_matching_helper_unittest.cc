// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/psl_matching_helper.h"

#include <stddef.h>

#include "base/macros.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

TEST(PSLMatchingUtilsTest, GetMatchResult) {
  struct TestData {
    const char* form_signon_realm;
    const char* form_federation_origin;
    autofill::PasswordForm::Scheme digest_scheme;
    const char* digest_signon_realm;
    const char* digest_origin;
    MatchResult match_result;
  };

  const TestData cases[] = {
      // Test Exact Matches.
      {"http://facebook.com/", "", autofill::PasswordForm::SCHEME_HTML,
       "http://facebook.com/", "http://facebook.com", MatchResult::EXACT_MATCH},

      {"http://m.facebook.com/", "", autofill::PasswordForm::SCHEME_HTML,
       "http://m.facebook.com/", "http://m.facebook.com",
       MatchResult::EXACT_MATCH},

      // Scheme mismatch.
      {"http://www.example.com/", "", autofill::PasswordForm::SCHEME_HTML,
       "https://www.example.com/", "https://www.example.com",
       MatchResult::NO_MATCH},

      // Host mismatch.
      {"http://www.example.com/", "", autofill::PasswordForm::SCHEME_HTML,
       "http://wwwexample.com/", "http://wwwexample.com",
       MatchResult::NO_MATCH},

      {"http://www.example1.com/", "", autofill::PasswordForm::SCHEME_HTML,
       "http://www.example2.com/", "http://www.example2.com",
       MatchResult::NO_MATCH},

      // Port mismatch.
      {"http://www.example.com:123/", "", autofill::PasswordForm::SCHEME_HTML,
       "http://www.example.com/", "http://www.example.com",
       MatchResult::NO_MATCH},

      // TLD mismatch.
      {"http://www.example.org/", "", autofill::PasswordForm::SCHEME_HTML,
       "http://www.example.com/", "http://www.example.com",
       MatchResult::NO_MATCH},

      // URLs without registry controlled domains should not match.
      {"http://localhost/", "", autofill::PasswordForm::SCHEME_HTML,
       "http://127.0.0.1/", "http://127.0.0.1", MatchResult::NO_MATCH},

      // Invalid URLs don't match.
      {"http://www.example.com/", "", autofill::PasswordForm::SCHEME_HTML,
       "http://", "", MatchResult::NO_MATCH},

      {"", "", autofill::PasswordForm::SCHEME_HTML, "http://www.example.com/",
       "http://www.example.com", MatchResult::NO_MATCH},

      {"http://www.example.com", "", autofill::PasswordForm::SCHEME_HTML,
       "bad url", "", MatchResult::NO_MATCH},

      // Test PSL Matches.
      {"http://facebook.com/", "", autofill::PasswordForm::SCHEME_HTML,
       "http://m.facebook.com/", "http://m.facebook.com",
       MatchResult::PSL_MATCH},

      {"https://facebook.com/", "", autofill::PasswordForm::SCHEME_HTML,
       "https://m.facebook.com/", "https://m.facebook.com",
       MatchResult::PSL_MATCH},

      {"https://www.facebook.com/", "", autofill::PasswordForm::SCHEME_HTML,
       "https://m.facebook.com/", "https://m.facebook.com",
       MatchResult::PSL_MATCH},

      // Don't apply PSL matching to Google domains.
      {"https://google.com/", "", autofill::PasswordForm::SCHEME_HTML,
       "https://maps.google.com/", "https://maps.google.com",
       MatchResult::NO_MATCH},

      {"https://google.com/", "", autofill::PasswordForm::SCHEME_HTML,
       "https://maps.google.com/", "https://maps.google.com",
       MatchResult::NO_MATCH},

      // Test Federated Matches.
      {"federation://example.com/google.com", "https://google.com",
       autofill::PasswordForm::SCHEME_HTML, "https://example.com/",
       "https://example.com", MatchResult::FEDERATED_MATCH},

      // Empty federation providers don't match.
      {"federation://example.com/", "", autofill::PasswordForm::SCHEME_HTML,
       "https://example.com/", "https://example.com", MatchResult::NO_MATCH},

      // Invalid origins don't match.
      {"federation://example.com/google.com", "https://google.com",
       autofill::PasswordForm::SCHEME_HTML, "https://example.com",
       "example.com", MatchResult::NO_MATCH},

      {"federation://example.com/google.com", "https://google.com",
       autofill::PasswordForm::SCHEME_HTML, "https://example.com", "example",
       MatchResult::NO_MATCH},

      // Test Federated PSL Matches.
      {"federation://sub.example.com/google.com", "https://google.com",
       autofill::PasswordForm::SCHEME_HTML, "https://sub.example.com/",
       "https://sub.example.com", MatchResult::FEDERATED_MATCH},

      {"federation://sub1.example.com/google.com", "https://google.com",
       autofill::PasswordForm::SCHEME_HTML, "https://sub2.example.com/",
       "https://sub2.example.com", MatchResult::FEDERATED_PSL_MATCH},

      {"federation://example.com/google.com", "https://google.com",
       autofill::PasswordForm::SCHEME_HTML, "https://sub.example.com/",
       "https://sub.example.com", MatchResult::FEDERATED_PSL_MATCH},

      // Federated PSL matches do not apply to HTTP.
      {"federation://sub1.example.com/google.com", "https://google.com",
       autofill::PasswordForm::SCHEME_HTML, "http://sub2.example.com/",
       "http://sub2.example.com", MatchResult::NO_MATCH},

      {"federation://example.com/google.com", "https://google.com",
       autofill::PasswordForm::SCHEME_HTML, "http://sub.example.com/",
       "http://sub.example.com", MatchResult::NO_MATCH},

      // Federated PSL matches do not apply to Google on HTTP or HTTPS.
      {"federation://accounts.google.com/facebook.com", "https://facebook.com",
       autofill::PasswordForm::SCHEME_HTML, "https://maps.google.com/",
       "https://maps.google.com", MatchResult::NO_MATCH},

      {"federation://accounts.google.com/facebook.com", "https://facebook.com",
       autofill::PasswordForm::SCHEME_HTML, "http://maps.google.com/",
       "http://maps.google.com", MatchResult::NO_MATCH},

      // TLD Mismatch.
      {"federation://sub.example.com/google.com", "https://google.com",
       autofill::PasswordForm::SCHEME_HTML, "https://sub.example.org/",
       "https://sub.example.org", MatchResult::NO_MATCH},
  };

  for (const TestData& data : cases) {
    autofill::PasswordForm form;
    form.signon_realm = data.form_signon_realm;
    form.federation_origin = url::Origin(GURL(data.form_federation_origin));
    PasswordStore::FormDigest digest(
        data.digest_scheme, data.digest_signon_realm, GURL(data.digest_origin));

    EXPECT_EQ(data.match_result, GetMatchResult(form, digest))
        << "signon_realm = " << data.form_signon_realm
        << ", federation_origin = " << data.form_federation_origin
        << ", digest = " << digest;
  }
}

TEST(PSLMatchingUtilsTest, IsPublicSuffixDomainMatch) {
  struct TestPair {
    const char* url1;
    const char* url2;
    bool should_match;
  };

  const TestPair pairs[] = {
      {"http://facebook.com", "http://facebook.com", true},
      {"http://facebook.com/path", "http://facebook.com/path", true},
      {"http://facebook.com/path1", "http://facebook.com/path2", true},
      {"http://facebook.com", "http://m.facebook.com", true},
      {"http://www.facebook.com", "http://m.facebook.com", true},
      {"http://facebook.com/path", "http://m.facebook.com/path", true},
      {"http://facebook.com/path1", "http://m.facebook.com/path2", true},
      {"http://example.com/has space", "http://example.com/has space", true},
      {"http://www.example.com", "http://wwwexample.com", false},
      {"http://www.example.com", "https://www.example.com", false},
      {"http://www.example.com:123", "http://www.example.com", false},
      {"http://www.example.org", "http://www.example.com", false},
      // URLs without registry controlled domains should not match.
      {"http://localhost", "http://127.0.0.1", false},
      // Invalid URLs should not match anything.
      {"http://", "http://", false},
      {"", "", false},
      {"bad url", "bad url", false},
      {"http://www.example.com", "http://", false},
      {"", "http://www.example.com", false},
      {"http://www.example.com", "bad url", false},
      {"http://www.example.com/%00", "http://www.example.com/%00", false},
      {"federation://example.com/google.com", "https://example.com/", false},
  };

  for (const TestPair& pair : pairs) {
    autofill::PasswordForm form1;
    form1.signon_realm = pair.url1;
    autofill::PasswordForm form2;
    form2.signon_realm = pair.url2;
    EXPECT_EQ(pair.should_match,
              IsPublicSuffixDomainMatch(form1.signon_realm, form2.signon_realm))
        << "First URL = " << pair.url1 << ", second URL = " << pair.url2;
  }
}

TEST(PSLMatchingUtilsTest, IsFederatedMatch) {
  struct TestPair {
    const char* signon_realm;
    const char* origin;
    bool should_match;
  };

  const TestPair pairs[] = {
      {"https://facebook.com", "https://facebook.com", false},
      {"", "", false},
      {"", "https://facebook.com/", false},
      {"https://facebook.com/", "", false},
      {"federation://example.com/google.com", "https://example.com/", true},
      {"federation://example.com/google.com", "http://example.com/", false},
      {"federation://example.com/google.com", "example.com", false},
      {"federation://example.com/", "http://example.com/", false},
      {"federation://example.com/google.com", "example", false},
  };

  for (const TestPair& pair : pairs) {
    std::string signon_realm = pair.signon_realm;
    GURL origin(pair.origin);
    EXPECT_EQ(pair.should_match, IsFederatedMatch(signon_realm, origin))
        << "signon_realm = " << pair.signon_realm
        << ", origin = " << pair.origin;
  }
}

TEST(PSLMatchingUtilsTest, IsFederatedPSLMatch) {
  struct TestPair {
    const char* signon_realm;
    const char* origin;
    bool should_match;
  };

  const TestPair pairs[] = {
      {"https://facebook.com", "https://facebook.com", false},
      {"", "", false},
      {"", "https://facebook.com/", false},
      {"https://facebook.com/", "", false},

      {"federation://example.com/google.com", "https://example.com/", true},
      {"federation://example.com/google.com", "http://example.com/", false},
      {"federation://example.com/google.com", "example.com", false},
      {"federation://example.com/", "http://example.com/", false},
      {"federation://example.com/google.com", "example", false},

      {"federation://sub.example.com/google.com", "https://sub.example.com/",
       true},
      {"federation://sub1.example.com/google.com", "https://sub2.example.com/",
       true},
      {"federation://example.com/google.com", "https://sub.example.com/", true},
      {"federation://example.com/google.com", "http://sub.example.com/", false},
      {"federation://sub.example.com/", "https://sub.example.com/", false},
  };

  for (const TestPair& pair : pairs) {
    std::string signon_realm = pair.signon_realm;
    GURL origin(pair.origin);
    EXPECT_EQ(pair.should_match, IsFederatedPSLMatch(signon_realm, origin))
        << "signon_realm = " << pair.signon_realm
        << ", origin = " << pair.origin;
  }
}

}  // namespace

}  // namespace password_manager
