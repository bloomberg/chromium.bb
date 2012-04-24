// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/metrics/metrics_log.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/metrics/proto/system_profile.pb.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"
#include "webkit/plugins/webplugininfo.h"

using base::TimeDelta;

namespace {

const char kClientId[] = "bogus client ID";
const int kSessionId = 127;
const int kScreenWidth = 1024;
const int kScreenHeight = 768;
const int kScreenCount = 3;
const base::FieldTrial::NameGroupId kFieldTrialIds[] = {
  {37, 43},
  {13, 47},
  {23, 17}
};

class TestMetricsLog : public MetricsLog {
 public:
  TestMetricsLog(const std::string& client_id, int session_id)
      : MetricsLog(client_id, session_id) {
    browser::RegisterLocalState(&prefs_);

#if defined(OS_CHROMEOS)
    prefs_.SetInteger(prefs::kStabilityChildProcessCrashCount, 10);
    prefs_.SetInteger(prefs::kStabilityOtherUserCrashCount, 11);
    prefs_.SetInteger(prefs::kStabilityKernelCrashCount, 12);
    prefs_.SetInteger(prefs::kStabilitySystemUncleanShutdownCount, 13);
#endif  // OS_CHROMEOS
  }
  virtual ~TestMetricsLog() {}

  virtual PrefService* GetPrefService() OVERRIDE {
    return &prefs_;
  }

  const metrics::SystemProfileProto& system_profile() const {
    return uma_proto()->system_profile();
  }

 private:
  virtual std::string GetCurrentTimeString() OVERRIDE {
    return std::string();
  }

  virtual void GetFieldTrialIds(
      std::vector<base::FieldTrial::NameGroupId>* field_trial_ids) const
      OVERRIDE {
    ASSERT_TRUE(field_trial_ids->empty());

    for (size_t i = 0; i < arraysize(kFieldTrialIds); ++i) {
      field_trial_ids->push_back(kFieldTrialIds[i]);
    }
  }

  virtual gfx::Size GetScreenSize() const OVERRIDE {
    return gfx::Size(kScreenWidth, kScreenHeight);
  }

  virtual int GetScreenCount() const OVERRIDE {
    return kScreenCount;
  }

  TestingPrefService prefs_;

  DISALLOW_COPY_AND_ASSIGN(TestMetricsLog);
};

}  // namespace

class MetricsLogTest : public testing::Test {
 protected:
  void TestRecordEnvironment(bool proto_only) {
    TestMetricsLog log(kClientId, kSessionId);

    std::vector<webkit::WebPluginInfo> plugins;
    if (proto_only)
      log.RecordEnvironmentProto(plugins);
    else
      log.RecordEnvironment(plugins, NULL);

    const metrics::SystemProfileProto& system_profile = log.system_profile();
    ASSERT_EQ(arraysize(kFieldTrialIds),
              static_cast<size_t>(system_profile.field_trial_size()));
    for (size_t i = 0; i < arraysize(kFieldTrialIds); ++i) {
      const metrics::SystemProfileProto::FieldTrial& field_trial =
          system_profile.field_trial(i);
      EXPECT_EQ(kFieldTrialIds[i].name, field_trial.name_id());
      EXPECT_EQ(kFieldTrialIds[i].group, field_trial.group_id());
    }

    // TODO(isherman): Verify other data written into the protobuf as a result
    // of this call.
  }
};

TEST_F(MetricsLogTest, RecordEnvironment) {
  // Test that recording the environment works via both of the public methods
  // RecordEnvironment() and RecordEnvironmentProto().
  TestRecordEnvironment(false);
  TestRecordEnvironment(true);
}

#if defined(OS_CHROMEOS)
TEST_F(MetricsLogTest, ChromeOSStabilityData) {
  TestMetricsLog log(kClientId, kSessionId);

  // Expect 3 warnings about not yet being able to send the
  // Chrome OS stability stats.
  std::vector<webkit::WebPluginInfo> plugins;
  PrefService* prefs = log.GetPrefService();
  log.WriteStabilityElement(plugins, prefs);
  log.CloseLog();

  int size = log.GetEncodedLogSizeXml();
  ASSERT_GT(size, 0);

  EXPECT_EQ(0, prefs->GetInteger(prefs::kStabilityChildProcessCrashCount));
  EXPECT_EQ(0, prefs->GetInteger(prefs::kStabilityOtherUserCrashCount));
  EXPECT_EQ(0, prefs->GetInteger(prefs::kStabilityKernelCrashCount));
  EXPECT_EQ(0, prefs->GetInteger(prefs::kStabilitySystemUncleanShutdownCount));

  std::string encoded;
  // Leave room for the NUL terminator.
  bool encoding_result = log.GetEncodedLogXml(
      WriteInto(&encoded, size + 1), size);
  ASSERT_TRUE(encoding_result);

  // Check that we can find childprocesscrashcount, but not
  // any of the ChromeOS ones that we are not emitting until log
  // servers can handle them.
  EXPECT_NE(std::string::npos,
            encoded.find(" childprocesscrashcount=\"10\""));
  EXPECT_EQ(std::string::npos,
            encoded.find(" otherusercrashcount="));
  EXPECT_EQ(std::string::npos,
            encoded.find(" kernelcrashcount="));
  EXPECT_EQ(std::string::npos,
            encoded.find(" systemuncleanshutdowns="));
}
#endif  // OS_CHROMEOS
