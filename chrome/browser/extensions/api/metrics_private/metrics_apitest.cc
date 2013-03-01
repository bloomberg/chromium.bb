// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/browser/user_metrics.h"

namespace {

// The tests that are run by this extension are expected to record the following
// user actions, with the specified counts.  If the tests in test.js are
// modified, this array may need to be updated.
struct RecordedUserAction {
  const char* name;
  int count;  // number of times the metric was recorded.
} g_user_actions[] = {
  {"test.ua.1", 1},
  {"test.ua.2", 2},
};

// The tests that are run by this extension are expected to record the following
// histograms.  If the tests in test.js are modified, this array may need to be
// updated.
struct RecordedHistogram {
  const char* name;
  base::HistogramType type;
  int min;
  int max;
  size_t buckets;
} g_histograms[] = {
  {"test.h.1", base::HISTOGRAM, 1, 100, 50},  // custom
  {"test.h.2", base::LINEAR_HISTOGRAM, 1, 200, 50},  // custom
  {"test.h.3", base::LINEAR_HISTOGRAM, 1, 101, 102},  // percentage
  {"test.time", base::HISTOGRAM, 1, 10000, 50},
  {"test.medium.time", base::HISTOGRAM, 1, 180000, 50},
  {"test.long.time", base::HISTOGRAM, 1, 3600000, 50},
  {"test.count", base::HISTOGRAM, 1, 1000000, 50},
  {"test.medium.count", base::HISTOGRAM, 1, 10000, 50},
  {"test.small.count", base::HISTOGRAM, 1, 100, 50},
};

// This class observes and collects user action notifications that are sent
// by the tests, so that they can be examined afterwards for correctness.
class UserActionObserver {
 public:
  UserActionObserver();
  ~UserActionObserver();

  void ValidateUserActions(const RecordedUserAction* recorded, int count);

 private:
  typedef std::map<std::string, int> UserActionCountMap;

  void OnUserAction(const std::string& action);

  int num_metrics() const {
    return count_map_.size();
  }

  int GetMetricCount(const std::string& name) const {
    UserActionCountMap::const_iterator i = count_map_.find(name);
    return i == count_map_.end() ? -1 : i->second;
  }

  UserActionCountMap count_map_;

  content::ActionCallback action_callback_;

  DISALLOW_COPY_AND_ASSIGN(UserActionObserver);
};

UserActionObserver::UserActionObserver()
    : action_callback_(base::Bind(&UserActionObserver::OnUserAction,
                                  base::Unretained(this))) {
  content::AddActionCallback(action_callback_);
}

UserActionObserver::~UserActionObserver() {
  content::RemoveActionCallback(action_callback_);
}

void UserActionObserver::OnUserAction(const std::string& action) {
  ++(count_map_[action]);
}

void UserActionObserver::ValidateUserActions(const RecordedUserAction* recorded,
                                             int count) {
  EXPECT_EQ(count, num_metrics());

  for (int i = 0; i < count; ++i) {
    const RecordedUserAction& ua = recorded[i];
    EXPECT_EQ(ua.count, GetMetricCount(ua.name));
  }
}

void ValidateHistograms(const RecordedHistogram* recorded,
                        int count) {
  base::StatisticsRecorder::Histograms histograms;
  base::StatisticsRecorder::GetHistograms(&histograms);

  // Code other than the tests tun here will record some histogram values, but
  // we will ignore those. This function validates that all the histogram we
  // expect to see are present in the list, and that their basic info is
  // correct.
  for (int i = 0; i < count; ++i) {
    const RecordedHistogram& r = recorded[i];
    size_t j = 0;
    for (j = 0; j < histograms.size(); ++j) {
      base::HistogramBase* histogram(histograms[j]);

      if (r.name == histogram->histogram_name()) {
        EXPECT_EQ(r.type, histogram->GetHistogramType());
        EXPECT_TRUE(
            histogram->HasConstructionArguments(r.min, r.max, r.buckets));
        break;
      }
    }
    EXPECT_LT(j, histograms.size());
  }
}

}  // anonymous namespace

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Metrics) {
  UserActionObserver observer;

  ASSERT_TRUE(RunComponentExtensionTest("metrics")) << message_;

  observer.ValidateUserActions(g_user_actions, arraysize(g_user_actions));
  ValidateHistograms(g_histograms, arraysize(g_histograms));
}
