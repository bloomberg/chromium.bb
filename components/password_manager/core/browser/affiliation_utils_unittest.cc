// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation_utils.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "components/password_manager/core/common/password_manager_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace password_manager {

namespace {
const char kTestFacetURI1[] = "https://alpha.example.com/";
const char kTestFacetURI2[] = "https://beta.example.com/";
const char kTestFacetURI3[] = "https://gamma.example.com/";
}  // namespace

TEST(AffiliationUtilsTest, ValidWebFacetURIs) {
  struct {
    const char* valid_facet_uri;
    const char* expected_canonical_facet_uri;
  } kTestCases[] = {
      {"https://www.example.com", "https://www.example.com"},
      {"HTTPS://www.EXAMPLE.com", "https://www.example.com"},
      {"https://0321.0x86.161.0043", "https://209.134.161.35"},
      {"https://www.%65xample.com", "https://www.example.com"},
      {"https://sz\xc3\xb3t\xc3\xa1r.example.com",
       "https://xn--sztr-7na0i.example.com"},
      {"https://www.example.com/", "https://www.example.com"},
      {"https://www.example.com:", "https://www.example.com"},
      {"https://@www.example.com", "https://www.example.com"},
      {"https://:@www.example.com", "https://www.example.com"},
      {"https://www.example.com:8080", "https://www.example.com:8080"},
      {"https://new-gtld", "https://new-gtld"}};
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message("URI = ") << test_case.valid_facet_uri);
    FacetURI facet_uri =
        FacetURI::FromPotentiallyInvalidSpec(test_case.valid_facet_uri);
    ASSERT_TRUE(facet_uri.IsValidWebFacetURI());
    EXPECT_EQ(std::string(test_case.expected_canonical_facet_uri),
              facet_uri.canonical_spec());
    EXPECT_EQ(url::kHttpsScheme, facet_uri.scheme());
    EXPECT_EQ("", facet_uri.android_package_name());
  }
}

TEST(AffiliationUtilsTest, InvalidWebFacetURIs) {
  const char* kInvalidFacetURIs[]{
      // Invalid URL (actually, will be treated as having only a host part).
      "Does not look like a valid URL.",
      // Path is more than just the root path ('/').
      "https://www.example.com/path",
      // Empty scheme and not HTTPS scheme.
      "://www.example.com",
      "http://www.example.com/",
      // Forbidden non-empty components.
      "https://user@www.example.com/",
      "https://:password@www.example.com/",
      "https://www.example.com/?",
      "https://www.example.com/?query",
      "https://www.example.com/#",
      "https://www.example.com/#ref",
      // Valid Android facet URI.
      "android://hash@com.example.android"};
  for (const char* uri : kInvalidFacetURIs) {
    SCOPED_TRACE(testing::Message("URI = ") << uri);
    FacetURI facet_uri = FacetURI::FromPotentiallyInvalidSpec(uri);
    EXPECT_FALSE(facet_uri.IsValidWebFacetURI());
  }
}

TEST(AffiliationUtilsTest, ValidAndroidFacetURIs) {
  struct {
    const char* valid_facet_uri;
    const char* expected_canonical_facet_uri;
    const char* expected_package_name;
  } kTestCases[] = {
      {"android://"
       "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"
       "@com.example.android",
       "android://"
       "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"
       "@com.example.android",
       "com.example.android"},
      {"ANDROID://"
       "hash@abcdefghijklmnopqrstuvwxyz_0123456789.ABCDEFGHIJKLMNOPQRSTUVWXYZ",
       "android://"
       "hash@abcdefghijklmnopqrstuvwxyz_0123456789.ABCDEFGHIJKLMNOPQRSTUVWXYZ",
       "abcdefghijklmnopqrstuvwxyz_0123456789.ABCDEFGHIJKLMNOPQRSTUVWXYZ"},
      {"android://needpadding@com.example.android",
       "android://needpadding=@com.example.android",
       "com.example.android"},
      {"android://needtounescape%3D%3D@com.%65xample.android",
       "android://needtounescape==@com.example.android",
       "com.example.android"},
      {"ANDROID://hash@com.example.android",
       "android://hash@com.example.android",
       "com.example.android"},
      {"android://hash@com.example.android/",
       "android://hash@com.example.android",
       "com.example.android"},
      {"android://hash:@com.example.android",
       "android://hash@com.example.android",
       "com.example.android"}};
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message("URI = ") << test_case.valid_facet_uri);
    FacetURI facet_uri =
        FacetURI::FromPotentiallyInvalidSpec(test_case.valid_facet_uri);
    ASSERT_TRUE(facet_uri.IsValidAndroidFacetURI());
    EXPECT_EQ(test_case.expected_canonical_facet_uri,
              facet_uri.canonical_spec());
    EXPECT_EQ("android", facet_uri.scheme());
    EXPECT_EQ(test_case.expected_package_name,
              facet_uri.android_package_name());
  }
}

TEST(AffiliationUtilsTest, InvalidAndroidFacetURIs) {
  const char* kInvalidFacetURIs[]{
      // Invalid URL (actually, will be treated as having only a host part).
      "Does not look like a valid URL.",
      // Path is more than just the root path ('/').
      "android://hash@com.example.android/path",
      // Empty scheme or not "android" scheme.
      "://hash@com.example.android",
      "http://hash@com.example.android",
      // Package name with illegal characters.
      "android://hash@com.$example.android",
      "android://hash@com-example-android",
      "android://hash@com%2Dexample.android",             // Escaped '-'.
      "android://hash@com.example.sz\xc3\xb3t\xc3\xa1r",  // UTF-8 o' and a'.
      // Empty, non-existent and malformed hash part.
      "android://@com.example.android",
      "android://com.example.android",
      "android://badpadding=a@com.example.android",
      "android://toolongpaddin===@com.example.android",
      "android://invalid+characters@com.example.android",
      "android://invalid%2Fcharacters@com.example.android",  // Escaped '/'.
      // Forbidden non-empty components.
      "android://hash:password@com.example.android",
      "android://hash@com.example.android:port",
      "android://hash@com.example.android/?",
      "android://hash@com.example.android/?query",
      "android://hash@com.example.android/#",
      "android://hash@com.example.android/#ref",
      // Valid Web facet URI.
      "https://www.example.com/"};
  for (const char* uri : kInvalidFacetURIs) {
    SCOPED_TRACE(testing::Message("URI = ") << uri);
    FacetURI facet_uri = FacetURI::FromPotentiallyInvalidSpec(uri);
    EXPECT_FALSE(facet_uri.IsValidAndroidFacetURI());
    EXPECT_EQ("", facet_uri.android_package_name());
  }
}

TEST(AffiliationUtilsTest, EqualEquivalenceClasses) {
  AffiliatedFacets a;
  a.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI1));
  a.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI2));
  a.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI3));

  AffiliatedFacets b;
  b.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI3));
  b.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI1));
  b.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI2));

  EXPECT_TRUE(AreEquivalenceClassesEqual(a, a));
  EXPECT_TRUE(AreEquivalenceClassesEqual(b, b));
  EXPECT_TRUE(AreEquivalenceClassesEqual(b, a));
  EXPECT_TRUE(AreEquivalenceClassesEqual(a, b));
}

TEST(AffiliationUtilsTest, NotEqualEquivalenceClasses) {
  AffiliatedFacets a;
  a.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI1));
  a.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI2));

  AffiliatedFacets b;
  a.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI2));
  b.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI3));

  AffiliatedFacets c;
  a.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI1));
  a.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI2));
  b.push_back(FacetURI::FromCanonicalSpec(kTestFacetURI3));

  EXPECT_FALSE(AreEquivalenceClassesEqual(a, b));
  EXPECT_FALSE(AreEquivalenceClassesEqual(a, c));
  EXPECT_FALSE(AreEquivalenceClassesEqual(b, a));
  EXPECT_FALSE(AreEquivalenceClassesEqual(b, c));
  EXPECT_FALSE(AreEquivalenceClassesEqual(c, a));
  EXPECT_FALSE(AreEquivalenceClassesEqual(c, b));
}

TEST(AffiliationUtilsTest, IsAffiliationBasedMatchingEnabled) {
  const char kFieldTrialName[] = "AffiliationBasedMatching";

  struct {
    const char* field_trial_group;
    const char* command_line_switch;
    bool expected_enabled;
  } kTestCases[] = {
      {"", "", false},
      {"", switches::kEnableAffiliationBasedMatching, true},
      {"", switches::kDisableAffiliationBasedMatching, false},
      {"garbage value", "", false},
      {"disabled", "", false},
      {"disabled2", "", false},
      {"Disabled", "", false},
      {"Disabled", switches::kDisableAffiliationBasedMatching, false},
      {"Disabled", switches::kEnableAffiliationBasedMatching, true},
      {"enabled", "", true},
      {"enabled2", "", true},
      {"Enabled", "", true},
      {"Enabled", switches::kDisableAffiliationBasedMatching, false},
      {"Enabled", switches::kEnableAffiliationBasedMatching, true}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message("Command line = ")
                 << test_case.command_line_switch);
    SCOPED_TRACE(testing::Message("Group name = ")
                 << test_case.field_trial_group);

    base::FieldTrialList field_trials(nullptr);
    base::FieldTrialList::CreateFieldTrial(kFieldTrialName,
                                           test_case.field_trial_group);

    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch(test_case.command_line_switch);
    EXPECT_EQ(test_case.expected_enabled,
              IsAffiliationBasedMatchingEnabled(command_line));
  }
}

}  // namespace password_manager
