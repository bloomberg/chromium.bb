// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

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
  base::Histogram::ClassType type;
  int min;
  int max;
  size_t buckets;
} g_histograms[] = {
  {"test.h.1", base::Histogram::HISTOGRAM, 1, 100, 50},  // custom
  {"test.h.2", base::Histogram::LINEAR_HISTOGRAM, 1, 200, 50},  // custom
  {"test.h.3", base::Histogram::LINEAR_HISTOGRAM, 1, 101, 102},  // percentage
  {"test.time", base::Histogram::HISTOGRAM, 1, 10000, 50},
  {"test.medium.time", base::Histogram::HISTOGRAM, 1, 180000, 50},
  {"test.long.time", base::Histogram::HISTOGRAM, 1, 3600000, 50},
  {"test.count", base::Histogram::HISTOGRAM, 1, 1000000, 50},
  {"test.medium.count", base::Histogram::HISTOGRAM, 1, 10000, 50},
  {"test.small.count", base::Histogram::HISTOGRAM, 1, 100, 50},
};

// This class observes and collects user action notifications that are sent
// by the tests, so that they can be examined afterwards for correctness.
class UserActionObserver : public content::NotificationObserver {
 public:
  UserActionObserver();

  void ValidateUserActions(const RecordedUserAction* recorded, int count);

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  typedef std::map<std::string, int> UserActionCountMap;

  int num_metrics() const {
    return count_map_.size();
  }

  int GetMetricCount(const std::string& name) const {
    UserActionCountMap::const_iterator i = count_map_.find(name);
    return i == count_map_.end() ? -1 : i->second;
  }

  content::NotificationRegistrar registrar_;
  UserActionCountMap count_map_;
};

UserActionObserver::UserActionObserver() {
  registrar_.Add(this, content::NOTIFICATION_USER_ACTION,
                 content::NotificationService::AllSources());
}

void UserActionObserver::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  const char* name = *content::Details<const char*>(details).ptr();
  ++(count_map_[name]);
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
      base::Histogram* histogram(histograms[j]);

      if (r.name == histogram->histogram_name()) {
        EXPECT_EQ(r.type, histogram->histogram_type());
        EXPECT_EQ(r.min, histogram->declared_min());
        EXPECT_EQ(r.max, histogram->declared_max());
        EXPECT_EQ(r.buckets, histogram->bucket_count());
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
