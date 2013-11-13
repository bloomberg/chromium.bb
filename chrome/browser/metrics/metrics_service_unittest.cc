// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctype.h>
#include <string>

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/common/process_type.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MetricsServiceTest : public testing::Test {
 public:
  MetricsServiceTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        testing_local_state_(TestingBrowserProcess::GetGlobal()) {
  }

  PrefService* GetLocalState() {
    return testing_local_state_.Get();
  }

 private:
  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  ScopedTestingLocalState testing_local_state_;

  DISALLOW_COPY_AND_ASSIGN(MetricsServiceTest);
};

}  // namespace

// Ensure the ClientId is formatted as expected.
TEST_F(MetricsServiceTest, ClientIdCorrectlyFormatted) {
  std::string clientid = MetricsService::GenerateClientID();
  EXPECT_EQ(36U, clientid.length());

  for (size_t i = 0; i < clientid.length(); ++i) {
    char current = clientid[i];
    if (i == 8 || i == 13 || i == 18 || i == 23)
      EXPECT_EQ('-', current);
    else
      EXPECT_TRUE(isxdigit(current));
  }
}

TEST_F(MetricsServiceTest, IsPluginProcess) {
  EXPECT_TRUE(
      MetricsService::IsPluginProcess(content::PROCESS_TYPE_PLUGIN));
  EXPECT_TRUE(
      MetricsService::IsPluginProcess(content::PROCESS_TYPE_PPAPI_PLUGIN));
  EXPECT_FALSE(
      MetricsService::IsPluginProcess(content::PROCESS_TYPE_GPU));
}

TEST_F(MetricsServiceTest, LowEntropySource0NotReset) {
  MetricsService service;

  // Get the low entropy source once, to initialize it.
  service.GetLowEntropySource();

  // Now, set it to 0 and ensure it doesn't get reset.
  service.low_entropy_source_ = 0;
  EXPECT_EQ(0, service.GetLowEntropySource());
  // Call it another time, just to make sure.
  EXPECT_EQ(0, service.GetLowEntropySource());
}

TEST_F(MetricsServiceTest, PermutedEntropyCacheClearedWhenLowEntropyReset) {
  const PrefService::Preference* low_entropy_pref =
      GetLocalState()->FindPreference(prefs::kMetricsLowEntropySource);
  const char* kCachePrefName = prefs::kMetricsPermutedEntropyCache;
  int low_entropy_value = -1;

  // First, generate an initial low entropy source value.
  {
    EXPECT_TRUE(low_entropy_pref->IsDefaultValue());

    MetricsService service;
    service.GetLowEntropySource();

    EXPECT_FALSE(low_entropy_pref->IsDefaultValue());
    EXPECT_TRUE(low_entropy_pref->GetValue()->GetAsInteger(&low_entropy_value));
  }

  // Now, set a dummy value in the permuted entropy cache pref and verify that
  // another call to GetLowEntropySource() doesn't clobber it when
  // --reset-variation-state wasn't specified.
  {
    GetLocalState()->SetString(kCachePrefName, "test");

    MetricsService service;
    service.GetLowEntropySource();

    EXPECT_EQ("test", GetLocalState()->GetString(kCachePrefName));
    EXPECT_EQ(low_entropy_value,
              GetLocalState()->GetInteger(prefs::kMetricsLowEntropySource));
  }

  // Verify that the cache does get reset if --reset-variations-state is passed.
  {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kResetVariationState);

    MetricsService service;
    service.GetLowEntropySource();

    EXPECT_TRUE(GetLocalState()->GetString(kCachePrefName).empty());
  }
}
