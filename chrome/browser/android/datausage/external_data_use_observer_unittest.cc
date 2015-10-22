// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/datausage/external_data_use_observer.h"

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "components/data_usage/core/data_use.h"
#include "components/data_usage/core/data_use_aggregator.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chrome {

namespace android {

TEST(ExternalDataUseObserverTest, SingleRegex) {
  scoped_ptr<data_usage::DataUseAggregator> data_use_aggregator(
      new data_usage::DataUseAggregator());
  scoped_ptr<ExternalDataUseObserver> external_data_use_observer(
      new ExternalDataUseObserver(data_use_aggregator.get()));

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
    external_data_use_observer->RegisterURLRegexes(
        std::vector<std::string>(1, tests[i].regex));
    EXPECT_EQ(tests[i].expect_match,
              external_data_use_observer->Matches(GURL(tests[i].url)))
        << i;
  }
}

TEST(ExternalDataUseObserverTest, TwoRegex) {
  scoped_ptr<data_usage::DataUseAggregator> data_use_aggregator(
      new data_usage::DataUseAggregator());
  scoped_ptr<ExternalDataUseObserver> external_data_use_observer(
      new ExternalDataUseObserver(data_use_aggregator.get()));

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
    std::vector<std::string> url_regexes;
    url_regexes.push_back(tests[i].regex1);
    url_regexes.push_back(tests[i].regex2);
    external_data_use_observer->RegisterURLRegexes(url_regexes);
    EXPECT_EQ(tests[i].expect_match,
              external_data_use_observer->Matches(GURL(tests[i].url)))
        << i;
  }
}

TEST(ExternalDataUseObserverTest, MultipleRegex) {
  scoped_ptr<data_usage::DataUseAggregator> data_use_aggregator(
      new data_usage::DataUseAggregator());
  scoped_ptr<ExternalDataUseObserver> external_data_use_observer(
      new ExternalDataUseObserver(data_use_aggregator.get()));

  std::vector<std::string> url_regexes;
  url_regexes.push_back("http://www[.]google[.]com/#q=.*");
  url_regexes.push_back("https://www[.]google[.]com/#q=.*");
  url_regexes.push_back("http://www[.]google[.]co[.]in/#q=.*");
  url_regexes.push_back("https://www[.]google[.]co[.]in/#q=.*");
  external_data_use_observer->RegisterURLRegexes(url_regexes);

  const struct {
    std::string url;
    bool expect_match;
  } tests[] = {
      {"", false},
      {"http://www.google.com", false},
      {"http://www.googleacom", false},
      {"https://www.google.com", false},
      {"https://www.googleacom", false},
      {"http://www.google.com", false},
      {"quic://www.google.com/q=test", false},
      {"http://www.google.com/q=test", false},
      {"http://www.google.com/.q=test", false},
      {"http://www.google.com/#q=test", true},
      {"https://www.google.com/#q=test", true},
      {"http://www.google.co.in/#q=test", true},
      {"https://www.google.co.in/#q=test", true},
      {"http://www.google.co.br/#q=test", false},
      {"http://google.com/#q=test", false},
      {"https://www.googleacom/#q=test", false},
      {"https://www.google.com/#Q=test", true},  // case in-sensitive
      {"www.google.com/#q=test", false},
      {"www.google.com:80/#q=test", false},
      {"http://www.google.com:80/#q=test", true},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    EXPECT_EQ(tests[i].expect_match,
              external_data_use_observer->Matches(GURL(tests[i].url)))
        << i << tests[i].url;
  }
}

TEST(ExternalDataUseObserverTest, ChangeRegex) {
  scoped_ptr<data_usage::DataUseAggregator> data_use_aggregator(
      new data_usage::DataUseAggregator());
  scoped_ptr<ExternalDataUseObserver> external_data_use_observer(
      new ExternalDataUseObserver(data_use_aggregator.get()));

  // When no regex is specified, the URL match should fail.
  EXPECT_FALSE(external_data_use_observer->Matches(GURL("")));
  EXPECT_FALSE(
      external_data_use_observer->Matches(GURL("http://www.google.com")));
  EXPECT_FALSE(external_data_use_observer->registered_as_observer_);
  EXPECT_FALSE(external_data_use_observer->matching_rules_fetch_pending_);

  std::vector<std::string> url_regexes;
  url_regexes.push_back("http://www[.]google[.]com/#q=.*");
  url_regexes.push_back("https://www[.]google[.]com/#q=.*");
  external_data_use_observer->RegisterURLRegexes(url_regexes);

  EXPECT_FALSE(external_data_use_observer->Matches(GURL("")));
  EXPECT_TRUE(
      external_data_use_observer->Matches(GURL("http://www.google.com#q=abc")));
  EXPECT_FALSE(external_data_use_observer->Matches(
      GURL("http://www.google.co.in#q=abc")));

  // Change the regular expressions to verify that the new regexes replace
  // the ones specified before.
  url_regexes.clear();
  url_regexes.push_back("http://www[.]google[.]co[.]in/#q=.*");
  url_regexes.push_back("https://www[.]google[.]co[.]in/#q=.*");
  external_data_use_observer->RegisterURLRegexes(url_regexes);
  EXPECT_FALSE(external_data_use_observer->Matches(GURL("")));
  EXPECT_FALSE(
      external_data_use_observer->Matches(GURL("http://www.google.com#q=abc")));
  EXPECT_TRUE(external_data_use_observer->Matches(
      GURL("http://www.google.co.in#q=abc")));
}

// Tests that at most one data use request is submitted, and if buffer size
// does not exceed the specified limit.
TEST(ExternalDataUseObserverTest, AtMostOneDataUseSubmitRequest) {
  scoped_ptr<data_usage::DataUseAggregator> data_use_aggregator(
      new data_usage::DataUseAggregator());
  scoped_ptr<ExternalDataUseObserver> external_data_use_observer(
      new ExternalDataUseObserver(data_use_aggregator.get()));

  std::vector<std::string> url_regexes;
  url_regexes.push_back("http://www[.]google[.]com/#q=.*");
  url_regexes.push_back("https://www[.]google[.]com/#q=.*");
  external_data_use_observer->RegisterURLRegexes(url_regexes);
  EXPECT_EQ(0U, external_data_use_observer->buffered_data_reports_.size());
  EXPECT_FALSE(external_data_use_observer->submit_data_report_pending_);

  std::vector<data_usage::DataUse> data_use_sequence;
  data_usage::DataUse data_use(
      GURL("http://www.google.com/#q=abc"), base::Time::Now(), GURL(), 0,
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN, 0, 0);
  data_use_sequence.push_back(data_use);
  data_use_sequence.push_back(data_use);
  external_data_use_observer->OnDataUse(data_use_sequence);

  EXPECT_EQ(1U, external_data_use_observer->buffered_data_reports_.size());
  EXPECT_TRUE(external_data_use_observer->submit_data_report_pending_);

  const size_t max_buffer_size = ExternalDataUseObserver::kMaxBufferSize;

  data_use_sequence.clear();
  for (size_t i = 0; i < max_buffer_size; ++i) {
    data_use_sequence.push_back(data_use);
  }
  external_data_use_observer->OnDataUse(data_use_sequence);
  EXPECT_EQ(max_buffer_size,
            external_data_use_observer->buffered_data_reports_.size());
}

}  // namespace android

}  // namespace chrome
