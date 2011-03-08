// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"

namespace {

// The tests that are run by this extension are expected to record the following
// user actions, with the specified counts.  If the tests in test.js are
// modified, this array may need to be updated.
struct RecordedUserAction {
  const char* name;  // base name of metric without extension id.
  int count;  // number of times the metric was recorded.
} g_user_actions[] = {
  {"test.ua.1", 1},
  {"test.ua.2", 2},
};

// The tests that are run by this extension are expected to record the following
// histograms.  If the tests in test.js are modified, this array may need to be
// updated.
struct RecordedHistogram {
  const char* name;  // base name of metric without extension id.
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

// Build the full name of a metrics for the given extension.  Each metric
// is made up of the unique name within the extension followed by the
// extension's id.
std::string BuildFullName(const std::string& name, const Extension* extension) {
  std::string full_name(name);
  full_name += extension->id();
  return full_name;
}

// This class observes and collects user action notifications that are sent
// by the tests, so that they can be examined afterwards for correctness.
class UserActionObserver : public NotificationObserver {
 public:
  UserActionObserver();

  void ValidateUserActions(const Extension* extension,
                           const RecordedUserAction* recorded,
                           int count);

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  typedef std::map<std::string, int> UserActionCountMap;

  int num_metrics() const {
    return count_map_.size();
  }

  int GetMetricCount(const std::string& name) const {
    UserActionCountMap::const_iterator i = count_map_.find(name);
    return i == count_map_.end() ? -1 : i->second;
  }

  NotificationRegistrar registrar_;
  UserActionCountMap count_map_;
};

UserActionObserver::UserActionObserver() {
  registrar_.Add(this, NotificationType::USER_ACTION,
                 NotificationService::AllSources());
}

void UserActionObserver::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  const char* name = *Details<const char*>(details).ptr();
  ++(count_map_[name]);
}

void UserActionObserver::ValidateUserActions(const Extension* extension,
                                             const RecordedUserAction* recorded,
                                             int count) {
  EXPECT_EQ(count, num_metrics());

  for (int i = 0; i < count; ++i) {
    const RecordedUserAction& ua = recorded[i];
    EXPECT_EQ(ua.count, GetMetricCount(BuildFullName(ua.name, extension)));
  }
}

void ValidateHistograms(const Extension* extension,
                        const RecordedHistogram* recorded,
                        int count) {
  base::StatisticsRecorder::Histograms histograms;
  base::StatisticsRecorder::GetHistograms(&histograms);

  // Code other than the tests tun here will record some histogram values, but
  // we will ignore those. This function validates that all the histogram we
  // expect to see are present in the list, and that their basic info is
  // correct.
  for (int i = 0; i < count; ++i) {
    const RecordedHistogram& r = recorded[i];
    std::string name(BuildFullName(r.name, extension));

    size_t j = 0;
    for (j = 0; j < histograms.size(); ++j) {
      scoped_refptr<base::Histogram> histogram(histograms[j]);

      if (name == histogram->histogram_name()) {
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
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  UserActionObserver observer;

  ASSERT_TRUE(RunExtensionTest("metrics")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  observer.ValidateUserActions(extension,
                               g_user_actions,
                               arraysize(g_user_actions));
  ValidateHistograms(extension, g_histograms, arraysize(g_histograms));
}
