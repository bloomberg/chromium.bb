// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_matcher.h"

#include <string>
#include <vector>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chrome {

namespace android {

class DataUseMatcherTest : public testing::Test {
 public:
  DataUseMatcherTest() : data_use_matcher_(base::WeakPtr<DataUseTabModel>()) {}

  DataUseMatcher* data_use_matcher() { return &data_use_matcher_; }

  void RegisterURLRegexes(const std::vector<std::string>& app_package_name,
                          const std::vector<std::string>& domain_path_regex,
                          const std::vector<std::string>& label) {
    data_use_matcher_.RegisterURLRegexes(&app_package_name, &domain_path_regex,
                                         &label);
  }

 private:
  DataUseMatcher data_use_matcher_;
  DISALLOW_COPY_AND_ASSIGN(DataUseMatcherTest);
};

TEST_F(DataUseMatcherTest, SingleRegex) {
  const struct {
    std::string url;
    std::string regex;
    bool expect_match;
  } tests[] = {
      {"http://www.google.com", "http://www.google.com/", true},
      {"http://www.Google.com", "http://www.google.com/", true},
      {"http://www.googleacom", "http://www.google.com/", true},
      {"http://www.googleaacom", "http://www.google.com/", false},
      {"http://www.google.com", "https://www.google.com/", false},
      {"http://www.google.com", "{http|https}://www[.]google[.]com/search.*",
       false},
      {"https://www.google.com/search=test",
       "https://www[.]google[.]com/search.*", true},
      {"https://www.googleacom/search=test",
       "https://www[.]google[.]com/search.*", false},
      {"https://www.google.com/Search=test",
       "https://www[.]google[.]com/search.*", true},
      {"www.google.com", "http://www.google.com", false},
      {"www.google.com:80", "http://www.google.com", false},
      {"http://www.google.com:80", "http://www.google.com", false},
      {"http://www.google.com:80/", "http://www.google.com/", true},
      {"", "http://www.google.com", false},
      {"", "", false},
      {"https://www.google.com", "http://www.google.com", false},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string label("");
    RegisterURLRegexes(
        // App package name not specified in the matching rule.
        std::vector<std::string>(1, std::string()),
        std::vector<std::string>(1, tests[i].regex),
        std::vector<std::string>(1, "label"));
    EXPECT_EQ(tests[i].expect_match,
              data_use_matcher()->MatchesURL(GURL(tests[i].url), &label))
        << i;

    // Verify label matches the expected label.
    std::string expected_label = "";
    if (tests[i].expect_match)
      expected_label = "label";

    EXPECT_EQ(expected_label, label);
    EXPECT_FALSE(data_use_matcher()->MatchesAppPackageName(
        "com.example.helloworld", &label))
        << i;
    // Empty package name should not match against empty package name in the
    // matching rule.
    EXPECT_FALSE(
        data_use_matcher()->MatchesAppPackageName(std::string(), &label))
        << i;
  }
}

TEST_F(DataUseMatcherTest, TwoRegex) {
  const struct {
    std::string url;
    std::string regex1;
    std::string regex2;
    bool expect_match;
  } tests[] = {
      {"http://www.google.com", "http://www.google.com/",
       "https://www.google.com/", true},
      {"http://www.googleacom", "http://www.google.com/",
       "http://www.google.com/", true},
      {"https://www.google.com", "http://www.google.com/",
       "https://www.google.com/", true},
      {"https://www.googleacom", "http://www.google.com/",
       "https://www.google.com/", true},
      {"http://www.google.com", "{http|https}://www[.]google[.]com/search.*",
       "", false},
      {"http://www.google.com/search=test",
       "http://www[.]google[.]com/search.*",
       "https://www[.]google[.]com/search.*", true},
      {"https://www.google.com/search=test",
       "http://www[.]google[.]com/search.*",
       "https://www[.]google[.]com/search.*", true},
      {"http://google.com/search=test", "http://www[.]google[.]com/search.*",
       "https://www[.]google[.]com/search.*", false},
      {"https://www.googleacom/search=test", "",
       "https://www[.]google[.]com/search.*", false},
      {"https://www.google.com/Search=test", "",
       "https://www[.]google[.]com/search.*", true},
      {"www.google.com", "http://www.google.com", "", false},
      {"www.google.com:80", "http://www.google.com", "", false},
      {"http://www.google.com:80", "http://www.google.com", "", false},
      {"", "http://www.google.com", "", false},
      {"https://www.google.com", "http://www.google.com", "", false},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string got_label("");
    std::vector<std::string> url_regexes;
    url_regexes.push_back(tests[i].regex1 + "|" + tests[i].regex2);
    const std::string label("label");
    RegisterURLRegexes(
        std::vector<std::string>(url_regexes.size(), "com.example.helloworld"),
        url_regexes, std::vector<std::string>(url_regexes.size(), label));
    EXPECT_EQ(tests[i].expect_match,
              data_use_matcher()->MatchesURL(GURL(tests[i].url), &got_label))
        << i;
    const std::string expected_label =
        tests[i].expect_match ? label : std::string();
    EXPECT_EQ(expected_label, got_label);

    EXPECT_TRUE(data_use_matcher()->MatchesAppPackageName(
        "com.example.helloworld", &got_label))
        << i;
    EXPECT_EQ(label, got_label);
  }
}

TEST_F(DataUseMatcherTest, MultipleRegex) {
  std::vector<std::string> url_regexes;
  url_regexes.push_back(
      "https?://www[.]google[.]com/#q=.*|https?://www[.]google[.]com[.]ph/"
      "#q=.*|https?://www[.]google[.]com[.]ph/[?]gws_rd=ssl#q=.*");
  RegisterURLRegexes(
      std::vector<std::string>(url_regexes.size(), std::string()), url_regexes,
      std::vector<std::string>(url_regexes.size(), "label"));

  const struct {
    std::string url;
    bool expect_match;
  } tests[] = {
      {"", false},
      {"http://www.google.com", false},
      {"http://www.googleacom", false},
      {"https://www.google.com", false},
      {"https://www.googleacom", false},
      {"https://www.google.com", false},
      {"quic://www.google.com/q=test", false},
      {"http://www.google.com/q=test", false},
      {"http://www.google.com/.q=test", false},
      {"http://www.google.com/#q=test", true},
      {"https://www.google.com/#q=test", true},
      {"https://www.google.com.ph/#q=test+abc", true},
      {"https://www.google.com.ph/?gws_rd=ssl#q=test+abc", true},
      {"http://www.google.com.ph/#q=test", true},
      {"https://www.google.com.ph/#q=test", true},
      {"http://www.google.co.in/#q=test", false},
      {"http://google.com/#q=test", false},
      {"https://www.googleacom/#q=test", false},
      {"https://www.google.com/#Q=test", true},  // case in-sensitive
      {"www.google.com/#q=test", false},
      {"www.google.com:80/#q=test", false},
      {"http://www.google.com:80/#q=test", true},
      {"http://www.google.com:80/search?=test", false},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string label("");
    EXPECT_EQ(tests[i].expect_match,
              data_use_matcher()->MatchesURL(GURL(tests[i].url), &label))
        << i << " " << tests[i].url;
  }
}

TEST_F(DataUseMatcherTest, ChangeRegex) {
  std::string label;
  // When no regex is specified, the URL match should fail.
  EXPECT_FALSE(data_use_matcher()->MatchesURL(GURL(""), &label));
  EXPECT_FALSE(
      data_use_matcher()->MatchesURL(GURL("http://www.google.com"), &label));

  std::vector<std::string> url_regexes;
  url_regexes.push_back("http://www[.]google[.]com/#q=.*");
  url_regexes.push_back("https://www[.]google[.]com/#q=.*");
  RegisterURLRegexes(
      std::vector<std::string>(url_regexes.size(), std::string()), url_regexes,
      std::vector<std::string>(url_regexes.size(), "label"));

  EXPECT_FALSE(data_use_matcher()->MatchesURL(GURL(""), &label));
  EXPECT_TRUE(data_use_matcher()->MatchesURL(
      GURL("http://www.google.com#q=abc"), &label));
  EXPECT_FALSE(data_use_matcher()->MatchesURL(
      GURL("http://www.google.co.in#q=abc"), &label));

  // Change the regular expressions to verify that the new regexes replace
  // the ones specified before.
  url_regexes.clear();
  url_regexes.push_back("http://www[.]google[.]co[.]in/#q=.*");
  url_regexes.push_back("https://www[.]google[.]co[.]in/#q=.*");
  RegisterURLRegexes(
      std::vector<std::string>(url_regexes.size(), std::string()), url_regexes,
      std::vector<std::string>(url_regexes.size(), "label"));
  EXPECT_FALSE(data_use_matcher()->MatchesURL(GURL(""), &label));
  EXPECT_FALSE(data_use_matcher()->MatchesURL(
      GURL("http://www.google.com#q=abc"), &label));
  EXPECT_TRUE(data_use_matcher()->MatchesURL(
      GURL("http://www.google.co.in#q=abc"), &label));
}

TEST_F(DataUseMatcherTest, MultipleAppPackageName) {
  std::vector<std::string> url_regexes;
  url_regexes.push_back(
      "http://www[.]foo[.]com/#q=.*|https://www[.]foo[.]com/#q=.*");
  url_regexes.push_back(
      "http://www[.]bar[.]com/#q=.*|https://www[.]bar[.]com/#q=.*");
  url_regexes.push_back("");

  std::vector<std::string> labels;
  const char kLabelFoo[] = "label_foo";
  const char kLabelBar[] = "label_bar";
  const char kLabelBaz[] = "label_baz";
  labels.push_back(kLabelFoo);
  labels.push_back(kLabelBar);
  labels.push_back(kLabelBaz);

  std::vector<std::string> app_package_names;
  const char kAppFoo[] = "com.example.foo";
  const char kAppBar[] = "com.example.bar";
  const char kAppBaz[] = "com.example.baz";
  app_package_names.push_back(kAppFoo);
  app_package_names.push_back(kAppBar);
  app_package_names.push_back(kAppBaz);

  RegisterURLRegexes(app_package_names, url_regexes, labels);

  // Test if labels are matched properly for app package names.
  std::string got_label;
  EXPECT_TRUE(data_use_matcher()->MatchesAppPackageName(kAppFoo, &got_label));
  EXPECT_EQ(kLabelFoo, got_label);

  EXPECT_TRUE(data_use_matcher()->MatchesAppPackageName(kAppBar, &got_label));
  EXPECT_EQ(kLabelBar, got_label);

  EXPECT_TRUE(data_use_matcher()->MatchesAppPackageName(kAppBaz, &got_label));
  EXPECT_EQ(kLabelBaz, got_label);

  EXPECT_FALSE(data_use_matcher()->MatchesAppPackageName(
      "com.example.unmatched", &got_label));
  EXPECT_EQ(std::string(), got_label);

  EXPECT_FALSE(
      data_use_matcher()->MatchesAppPackageName(std::string(), &got_label));
  EXPECT_EQ(std::string(), got_label);

  EXPECT_FALSE(
      data_use_matcher()->MatchesAppPackageName(std::string(), &got_label));

  // An empty URL pattern should not match with any URL pattern.
  EXPECT_FALSE(data_use_matcher()->MatchesURL(GURL(""), &got_label));
  EXPECT_FALSE(
      data_use_matcher()->MatchesURL(GURL("http://www.baz.com"), &got_label));
}

}  // namespace android

}  // namespace chrome
