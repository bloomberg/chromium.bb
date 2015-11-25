// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/external_data_use_observer.h"

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/android/data_usage/data_use_tab_model_test_utils.h"
#include "components/data_usage/core/data_use.h"
#include "components/data_usage/core/data_use_aggregator.h"
#include "components/data_usage/core/data_use_amortizer.h"
#include "components/data_usage/core/data_use_annotator.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chrome {

namespace android {

class ExternalDataUseObserverTest : public testing::Test {
 public:
  void SetUp() override {
    thread_bundle_.reset(new content::TestBrowserThreadBundle(
        content::TestBrowserThreadBundle::REAL_IO_THREAD));
    io_task_runner_ = content::BrowserThread::GetMessageLoopProxyForThread(
        content::BrowserThread::IO);
    ui_task_runner_ = content::BrowserThread::GetMessageLoopProxyForThread(
        content::BrowserThread::UI);
    data_use_aggregator_.reset(
        new data_usage::DataUseAggregator(nullptr, nullptr));
    external_data_use_observer_.reset(new ExternalDataUseObserver(
        data_use_aggregator_.get(), io_task_runner_.get(),
        ui_task_runner_.get()));
    test_data_use_tab_model_ = new TestDataUseTabModel(
        external_data_use_observer_.get(), ui_task_runner_.get());
    external_data_use_observer_->data_use_tab_model_.reset(
        test_data_use_tab_model_);
  }

  scoped_ptr<ExternalDataUseObserver> Create() const {
    return scoped_ptr<ExternalDataUseObserver>(new ExternalDataUseObserver(
        data_use_aggregator_.get(), io_task_runner_.get(),
        ui_task_runner_.get()));
  }

  ExternalDataUseObserver* external_data_use_observer() const {
    return external_data_use_observer_.get();
  }

  TestDataUseTabModel* test_data_use_tab_model() const {
    return test_data_use_tab_model_;
  }

  const ExternalDataUseObserver::DataUseReports& buffered_data_reports() const {
    return external_data_use_observer_->buffered_data_reports_;
  }

 private:
  // Required for creating multiple threads for unit testing.
  scoped_ptr<content::TestBrowserThreadBundle> thread_bundle_;
  scoped_ptr<data_usage::DataUseAggregator> data_use_aggregator_;
  scoped_ptr<ExternalDataUseObserver> external_data_use_observer_;
  TestDataUseTabModel* test_data_use_tab_model_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
};

TEST_F(ExternalDataUseObserverTest, SingleRegex) {
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

  std::string label("test");
  for (size_t i = 0; i < arraysize(tests); ++i) {
    external_data_use_observer()->RegisterURLRegexes(
        // App package name not specified in the matching rule.
        std::vector<std::string>(1, std::string()),
        std::vector<std::string>(1, tests[i].regex),
        std::vector<std::string>(1, "label"));
    EXPECT_EQ(tests[i].expect_match,
              external_data_use_observer()->Matches(GURL(tests[i].url), &label))
        << i;

    // Verify label matches the expected label.
    std::string expected_label = "";
    if (tests[i].expect_match)
      expected_label = "label";

    EXPECT_EQ(expected_label, label);
    EXPECT_FALSE(external_data_use_observer()->MatchesAppPackageName(
        "com.example.helloworld", &label))
        << i;
    // Empty package name should not match against empty package name in the
    // matching rule.
    EXPECT_FALSE(external_data_use_observer()->MatchesAppPackageName(
        std::string(), &label))
        << i;
  }
}

TEST_F(ExternalDataUseObserverTest, TwoRegex) {
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

  std::string got_label;
  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::vector<std::string> url_regexes;
    url_regexes.push_back(tests[i].regex1 + "|" + tests[i].regex2);
    const std::string label("label");
    external_data_use_observer()->RegisterURLRegexes(
        std::vector<std::string>(url_regexes.size(), "com.example.helloworld"),
        url_regexes, std::vector<std::string>(url_regexes.size(), label));
    EXPECT_EQ(tests[i].expect_match, external_data_use_observer()->Matches(
                                         GURL(tests[i].url), &got_label))
        << i;
    const std::string expected_label =
        tests[i].expect_match ? label : std::string();
    EXPECT_EQ(expected_label, got_label);

    EXPECT_TRUE(external_data_use_observer()->MatchesAppPackageName(
        "com.example.helloworld", &got_label))
        << i;
    EXPECT_EQ(label, got_label);
  }
}

TEST_F(ExternalDataUseObserverTest, MultipleRegex) {
  std::vector<std::string> url_regexes;
  url_regexes.push_back(
      "https?://www[.]google[.]com/#q=.*|https?://www[.]google[.]com[.]ph/"
      "#q=.*|https?://www[.]google[.]com[.]ph/[?]gws_rd=ssl#q=.*");
  external_data_use_observer()->RegisterURLRegexes(
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

  std::string label;
  for (size_t i = 0; i < arraysize(tests); ++i) {
    EXPECT_EQ(tests[i].expect_match,
              external_data_use_observer()->Matches(GURL(tests[i].url), &label))
        << i << " " << tests[i].url;
  }
}

TEST_F(ExternalDataUseObserverTest, ChangeRegex) {
  std::string label;
  // When no regex is specified, the URL match should fail.
  EXPECT_FALSE(external_data_use_observer()->Matches(GURL(""), &label));
  EXPECT_FALSE(external_data_use_observer()->Matches(
      GURL("http://www.google.com"), &label));

  std::vector<std::string> url_regexes;
  url_regexes.push_back("http://www[.]google[.]com/#q=.*");
  url_regexes.push_back("https://www[.]google[.]com/#q=.*");
  external_data_use_observer()->RegisterURLRegexes(
      std::vector<std::string>(url_regexes.size(), std::string()), url_regexes,
      std::vector<std::string>(url_regexes.size(), "label"));

  EXPECT_FALSE(external_data_use_observer()->Matches(GURL(""), &label));
  EXPECT_TRUE(external_data_use_observer()->Matches(
      GURL("http://www.google.com#q=abc"), &label));
  EXPECT_FALSE(external_data_use_observer()->Matches(
      GURL("http://www.google.co.in#q=abc"), &label));

  // Change the regular expressions to verify that the new regexes replace
  // the ones specified before.
  url_regexes.clear();
  url_regexes.push_back("http://www[.]google[.]co[.]in/#q=.*");
  url_regexes.push_back("https://www[.]google[.]co[.]in/#q=.*");
  external_data_use_observer()->RegisterURLRegexes(
      std::vector<std::string>(url_regexes.size(), std::string()), url_regexes,
      std::vector<std::string>(url_regexes.size(), "label"));
  EXPECT_FALSE(external_data_use_observer()->Matches(GURL(""), &label));
  EXPECT_FALSE(external_data_use_observer()->Matches(
      GURL("http://www.google.com#q=abc"), &label));
  EXPECT_TRUE(external_data_use_observer()->Matches(
      GURL("http://www.google.co.in#q=abc"), &label));
}

// Tests that tab model is notified when tracking labels are removed.
TEST_F(ExternalDataUseObserverTest, LabelRemoved) {
  std::vector<std::string> labels;

  labels.push_back("label_1");
  labels.push_back("label_2");
  labels.push_back("label_3");
  external_data_use_observer()->RegisterURLRegexes(
      std::vector<std::string>(labels.size(), std::string()),
      std::vector<std::string>(labels.size(), "http://foobar.com"), labels);

  EXPECT_CALL(*test_data_use_tab_model(), OnTrackingLabelRemoved("label_3"))
      .Times(1);
  EXPECT_CALL(*test_data_use_tab_model(), OnTrackingLabelRemoved("label_2"))
      .Times(1);

  labels.clear();
  labels.push_back("label_1");
  labels.push_back("label_4");
  labels.push_back("label_5");
  external_data_use_observer()->RegisterURLRegexes(
      std::vector<std::string>(labels.size(), std::string()),
      std::vector<std::string>(labels.size(), "http://foobar.com"), labels);
}

// Verifies that buffer size does not exceed the specified limit.
TEST_F(ExternalDataUseObserverTest, BufferSize) {
  const char kLabel[] = "label";

  std::vector<std::string> url_regexes;
  url_regexes.push_back(
      "http://www[.]google[.]com/#q=.*|https://www[.]google[.]com/#q=.*");

  external_data_use_observer()->FetchMatchingRulesDoneOnIOThread(
      std::vector<std::string>(url_regexes.size(), std::string()), url_regexes,
      std::vector<std::string>(url_regexes.size(), kLabel));

  const int64_t bytes_downloaded =
      external_data_use_observer()->data_use_report_min_bytes_;
  const int64_t bytes_uploaded = 1;

  // Push more entries than the buffer size. Buffer size should not be exceeded.
  for (size_t i = 0; i < ExternalDataUseObserver::kMaxBufferSize * 2; ++i) {
    data_usage::DataUse data_use(
        GURL("http://www.google.com/#q=abc"), base::TimeTicks::Now(), GURL(), 0,
        net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
        "mccmnc" + base::Int64ToString(i), bytes_uploaded, bytes_downloaded);
    external_data_use_observer()->OnDataUse(data_use);
  }

  EXPECT_LE(0, external_data_use_observer()->total_bytes_buffered_);

  // Verify that total buffered bytes is computed correctly.
  EXPECT_EQ(static_cast<int64_t>(ExternalDataUseObserver::kMaxBufferSize *
                                 (bytes_downloaded + bytes_uploaded)),
            external_data_use_observer()->total_bytes_buffered_);
  EXPECT_EQ(ExternalDataUseObserver::kMaxBufferSize,
            buffered_data_reports().size());

  // Verify the label of the data use report.
  for (const auto& it : buffered_data_reports())
    EXPECT_EQ(kLabel, it.first.label);
}

// Tests that buffered data use reports are merged correctly.
TEST_F(ExternalDataUseObserverTest, ReportsMergedCorrectly) {
  const char kLabel[] = "label";

  std::vector<std::string> url_regexes;
  url_regexes.push_back(
      "http://www[.]google[.]com/#q=.*|https://www[.]google[.]com/#q=.*");

  external_data_use_observer()->FetchMatchingRulesDoneOnIOThread(
      std::vector<std::string>(url_regexes.size(), std::string()), url_regexes,
      std::vector<std::string>(url_regexes.size(), kLabel));

  const size_t num_iterations = ExternalDataUseObserver::kMaxBufferSize * 2;

  for (size_t i = 0; i < num_iterations; ++i) {
    data_usage::DataUse data_use_foo(
        GURL("http://www.google.com/#q=abc"), base::TimeTicks::Now(), GURL(), 0,
        net::NetworkChangeNotifier::CONNECTION_UNKNOWN, "mccmnc_foo",
        external_data_use_observer()->data_use_report_min_bytes_, 1);
    external_data_use_observer()->OnDataUse(data_use_foo);

    data_usage::DataUse data_use_bar(
        GURL("http://www.google.com/#q=abc"), base::TimeTicks::Now(), GURL(), 0,
        net::NetworkChangeNotifier::CONNECTION_UNKNOWN, "mccmnc_bar",
        external_data_use_observer()->data_use_report_min_bytes_, 1);
    external_data_use_observer()->OnDataUse(data_use_bar);

    data_usage::DataUse data_use_baz(
        GURL("http://www.google.com/#q=abc"), base::TimeTicks::Now(), GURL(), 0,
        net::NetworkChangeNotifier::CONNECTION_UNKNOWN, "mccmnc_baz",
        external_data_use_observer()->data_use_report_min_bytes_, 1);
    external_data_use_observer()->OnDataUse(data_use_baz);
  }

  EXPECT_EQ(3U, buffered_data_reports().size());

  // One of the foo reports should have been submitted, and all the other foo
  // reports should have been merged together. All of the bar and baz reports
  // should have been merged together respectively.
  const struct {
    std::string mcc_mnc;
    size_t number_of_merged_reports;
  } expected_data_use_reports[] = {{"mccmnc_foo", num_iterations - 1},
                                   {"mccmnc_bar", num_iterations},
                                   {"mccmnc_baz", num_iterations}};

  for (const auto& expected_report : expected_data_use_reports) {
    const ExternalDataUseObserver::DataUseReportKey key(
        kLabel, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
        expected_report.mcc_mnc);

    EXPECT_NE(buffered_data_reports().end(), buffered_data_reports().find(key));
    EXPECT_EQ(static_cast<int64_t>(expected_report.number_of_merged_reports),
              buffered_data_reports().find(key)->second.bytes_downloaded);
    EXPECT_EQ(static_cast<int64_t>(
                  expected_report.number_of_merged_reports *
                  external_data_use_observer()->data_use_report_min_bytes_),
              buffered_data_reports().find(key)->second.bytes_uploaded);
  }
}

// Tests that timestamps of merged reports is correct.
TEST_F(ExternalDataUseObserverTest, TimestampsMergedCorrectly) {
  const char kLabel[] = "label";

  std::vector<std::string> url_regexes;
  url_regexes.push_back(
      "http://www[.]google[.]com/#q=.*|https://www[.]google[.]com/#q=.*");

  external_data_use_observer()->FetchMatchingRulesDoneOnIOThread(
      std::vector<std::string>(url_regexes.size(), std::string()), url_regexes,
      std::vector<std::string>(url_regexes.size(), kLabel));

  const size_t num_iterations = ExternalDataUseObserver::kMaxBufferSize * 2;

  base::Time start_timestamp = base::Time::UnixEpoch();
  base::Time end_timestamp = start_timestamp + base::TimeDelta::FromSeconds(1);
  for (size_t i = 0; i < num_iterations; ++i) {
    data_usage::DataUse data_use_foo(
        GURL("http://www.google.com/#q=abc"), base::TimeTicks::Now(), GURL(), 0,
        net::NetworkChangeNotifier::CONNECTION_UNKNOWN, "mccmnc_foo", 1, 2);
    external_data_use_observer()->BufferDataUseReport(
        data_use_foo, kLabel, start_timestamp, end_timestamp);

    start_timestamp += base::TimeDelta::FromSeconds(1);
    end_timestamp += base::TimeDelta::FromSeconds(1);
  }

  EXPECT_EQ(1U, buffered_data_reports().size());
  EXPECT_EQ(0, buffered_data_reports().begin()->second.start_time.ToJavaTime());
  // Convert from seconds to milliseconds.
  EXPECT_EQ(static_cast<int64_t>(num_iterations * 1000),
            buffered_data_reports().begin()->second.end_time.ToJavaTime());
}

// Tests the behavior when multiple matching rules are available for URL and
// package name matching.
TEST_F(ExternalDataUseObserverTest, MultipleMatchingRules) {
  std::vector<std::string> url_regexes;
  url_regexes.push_back(
      "http://www[.]foo[.]com/#q=.*|https://www[.]foo[.]com/#q=.*");
  url_regexes.push_back(
      "http://www[.]bar[.]com/#q=.*|https://www[.]bar[.]com/#q=.*");

  std::vector<std::string> labels;
  const char kLabelFoo[] = "label_foo";
  const char kLabelBar[] = "label_bar";
  labels.push_back(kLabelFoo);
  labels.push_back(kLabelBar);

  std::vector<std::string> app_package_names;
  const char kAppFoo[] = "com.example.foo";
  const char kAppBar[] = "com.example.bar";
  app_package_names.push_back(kAppFoo);
  app_package_names.push_back(kAppBar);

  external_data_use_observer()->FetchMatchingRulesDoneOnIOThread(
      app_package_names, url_regexes, labels);
  EXPECT_EQ(0U, external_data_use_observer()->buffered_data_reports_.size());
  EXPECT_FALSE(external_data_use_observer()->submit_data_report_pending_);
  EXPECT_FALSE(external_data_use_observer()->matching_rules_fetch_pending_);

  // Check |kLabelFoo| matching rule.
  data_usage::DataUse data_foo_1(
      GURL("http://www.foo.com/#q=abc"), base::TimeTicks::Now(), GURL(), 0,
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN, "mccmnc_1",
      external_data_use_observer()->data_use_report_min_bytes_, 0);
  external_data_use_observer()->OnDataUse(data_foo_1);
  data_usage::DataUse data_foo_2(
      GURL("http://www.foo.com/#q=abc"), base::TimeTicks::Now(), GURL(), 0,
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN, "mccmnc_2",
      external_data_use_observer()->data_use_report_min_bytes_, 0);
  external_data_use_observer()->OnDataUse(data_foo_2);

  // The foo1 report should have been submitted.
  EXPECT_TRUE(external_data_use_observer()->submit_data_report_pending_);

  // Only the foo2 report should be present.
  EXPECT_EQ(1U, buffered_data_reports().size());
  const ExternalDataUseObserver::DataUseReportKey key_foo_2(
      kLabelFoo, net::NetworkChangeNotifier::CONNECTION_UNKNOWN, "mccmnc_2");
  EXPECT_NE(buffered_data_reports().end(),
            buffered_data_reports().find(key_foo_2));

  // Check |kLabelBar| matching rule.
  data_usage::DataUse data_bar(
      GURL("http://www.bar.com/#q=abc"), base::TimeTicks::Now(), GURL(), 0,
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN, "mccmnc", 0, 0);
  external_data_use_observer()->OnDataUse(data_bar);

  // Both the foo2 and bar reports should be present.
  EXPECT_EQ(2U, buffered_data_reports().size());
  EXPECT_NE(buffered_data_reports().end(),
            buffered_data_reports().find(key_foo_2));

  const ExternalDataUseObserver::DataUseReportKey key_bar(
      kLabelBar, net::NetworkChangeNotifier::CONNECTION_UNKNOWN, "mccmnc");
  EXPECT_NE(buffered_data_reports().end(),
            buffered_data_reports().find(key_bar));

  // Test if labels are matched properly for app package names.
  std::string got_label;
  EXPECT_TRUE(
      external_data_use_observer()->MatchesAppPackageName(kAppFoo, &got_label));
  EXPECT_EQ(kLabelFoo, got_label);

  got_label = "";
  EXPECT_TRUE(
      external_data_use_observer()->MatchesAppPackageName(kAppBar, &got_label));
  EXPECT_EQ(kLabelBar, got_label);

  got_label = "";
  EXPECT_FALSE(external_data_use_observer()->MatchesAppPackageName(
      "com.example.unmatched", &got_label));
  EXPECT_EQ(std::string(), got_label);

  EXPECT_FALSE(external_data_use_observer()->MatchesAppPackageName(
      std::string(), &got_label));
  EXPECT_EQ(std::string(), got_label);
}

// Tests that hash function reports distinct values. This test may fail if there
// is a hash collision, however the chances of that happening are very low.
TEST_F(ExternalDataUseObserverTest, HashFunction) {
  ExternalDataUseObserver::DataUseReportKeyHash hash;

  ExternalDataUseObserver::DataUseReportKey foo(
      "foo_label", net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      "foo_mcc_mnc");
  ExternalDataUseObserver::DataUseReportKey bar_label(
      "bar_label", net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      "foo_mcc_mnc");
  ExternalDataUseObserver::DataUseReportKey bar_network_type(
      "foo_label", net::NetworkChangeNotifier::CONNECTION_WIFI, "foo_mcc_mnc");
  ExternalDataUseObserver::DataUseReportKey bar_mcc_mnc(
      "foo_label", net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      "bar_mcc_mnc");

  EXPECT_NE(hash(foo), hash(bar_label));
  EXPECT_NE(hash(foo), hash(bar_label));
  EXPECT_NE(hash(foo), hash(bar_network_type));
  EXPECT_NE(hash(foo), hash(bar_mcc_mnc));
}

// Tests if matching rules are fetched periodically.
TEST_F(ExternalDataUseObserverTest, PeriodicFetchMatchingRules) {
  const char kLabel[] = "label";

  std::vector<std::string> url_regexes;
  url_regexes.push_back(
      "http://www[.]google[.]com/#q=.*|https://www[.]google[.]com/#q=.*");

  external_data_use_observer()->FetchMatchingRulesDoneOnIOThread(
      std::vector<std::string>(url_regexes.size(), std::string()), url_regexes,
      std::vector<std::string>(url_regexes.size(), kLabel));

  EXPECT_FALSE(external_data_use_observer()->matching_rules_fetch_pending_);
  EXPECT_FALSE(
      external_data_use_observer()->last_matching_rules_fetch_time_.is_null());

  // Change the time when the fetching rules were fetched.
  external_data_use_observer()->last_matching_rules_fetch_time_ =
      base::TimeTicks::Now() -
      external_data_use_observer()->fetch_matching_rules_duration_;
  // Matching rules should be expired.
  EXPECT_GE(base::TimeTicks::Now() -
                external_data_use_observer()->last_matching_rules_fetch_time_,
            external_data_use_observer()->fetch_matching_rules_duration_);

  // OnDataUse should trigger fetching of matching rules.
  data_usage::DataUse data_use_foo(
      GURL("http://www.google.com/#q=abc"), base::TimeTicks::Now(), GURL(), 0,
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN, "mccmnc_foo",
      external_data_use_observer()->data_use_report_min_bytes_, 1);
  external_data_use_observer()->OnDataUse(data_use_foo);

  // Matching rules should not be expired.
  EXPECT_LT(base::TimeTicks::Now() -
                external_data_use_observer()->last_matching_rules_fetch_time_,
            external_data_use_observer()->fetch_matching_rules_duration_);
}

// Tests if data use reports are sent only after the total bytes send/received
// across all buffered reports have reached the specified threshold.
TEST_F(ExternalDataUseObserverTest, BufferDataUseReports) {
  const char kLabel[] = "label";

  std::vector<std::string> url_regexes;
  url_regexes.push_back(
      "http://www[.]google[.]com/#q=.*|https://www[.]google[.]com/#q=.*");

  external_data_use_observer()->FetchMatchingRulesDoneOnIOThread(
      std::vector<std::string>(url_regexes.size(), std::string()), url_regexes,
      std::vector<std::string>(url_regexes.size(), kLabel));

  // This tests reports 1024 bytes in each loop iteration. For the test to work
  // properly, |data_use_report_min_bytes_| should be a multiple of 1024.
  ASSERT_EQ(0, external_data_use_observer()->data_use_report_min_bytes_ % 1024);

  const size_t num_iterations =
      external_data_use_observer()->data_use_report_min_bytes_ / 1024;

  for (size_t i = 0; i < num_iterations; ++i) {
    data_usage::DataUse data_use_foo(
        GURL("http://www.google.com/#q=abc"), base::TimeTicks::Now(), GURL(), 0,
        net::NetworkChangeNotifier::CONNECTION_UNKNOWN, "mccmnc_foo", 1024, 0);
    external_data_use_observer()->OnDataUse(data_use_foo);

    if (i != num_iterations - 1) {
      // Total buffered bytes is less than the minimum threshold. Data use
      // report should not be send.
      EXPECT_FALSE(external_data_use_observer()->submit_data_report_pending_);
      EXPECT_EQ(static_cast<int64_t>(i + 1),
                external_data_use_observer()->total_bytes_buffered_ / 1024);

    } else {
      // Total bytes is at least the minimum threshold. This should trigger
      // submitting of the buffered data use report.
      EXPECT_TRUE(external_data_use_observer()->submit_data_report_pending_);
      EXPECT_EQ(0, external_data_use_observer()->total_bytes_buffered_);
    }
  }
  EXPECT_EQ(0, external_data_use_observer()->total_bytes_buffered_);
}

// Tests if the parameters from the field trial are populated correctly.
TEST_F(ExternalDataUseObserverTest, Variations) {
  EXPECT_EQ(base::TimeDelta::FromSeconds(60 * 15),
            external_data_use_observer()->fetch_matching_rules_duration_);
  EXPECT_EQ(100 * 1024,
            external_data_use_observer()->data_use_report_min_bytes_);

  variations::testing::ClearAllVariationParams();
  std::map<std::string, std::string> variation_params;

  const int kFetchMatchingRulesDurationSeconds = 10000;
  const int64_t kDataUseReportMinBytes = 5000;
  variation_params["fetch_matching_rules_duration_seconds"] =
      base::Int64ToString(kFetchMatchingRulesDurationSeconds);
  variation_params["data_use_report_min_bytes"] =
      base::Int64ToString(kDataUseReportMinBytes);

  ASSERT_TRUE(variations::AssociateVariationParams(
      ExternalDataUseObserver::kExternalDataUseObserverFieldTrial, "Enabled",
      variation_params));

  base::FieldTrialList field_trial_list(nullptr);

  base::FieldTrialList::CreateFieldTrial(
      ExternalDataUseObserver::kExternalDataUseObserverFieldTrial, "Enabled");

  // Create another ExternalDataUseObserver object. This would fetch variation
  // params.
  scoped_ptr<ExternalDataUseObserver>
      external_data_use_obsever_with_variations = Create();
  EXPECT_EQ(base::TimeDelta::FromSeconds(kFetchMatchingRulesDurationSeconds),
            external_data_use_obsever_with_variations
                ->fetch_matching_rules_duration_);
  EXPECT_EQ(
      kDataUseReportMinBytes,
      external_data_use_obsever_with_variations->data_use_report_min_bytes_);
}

}  // namespace android

}  // namespace chrome
