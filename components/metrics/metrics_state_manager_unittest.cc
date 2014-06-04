// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_state_manager.h"

#include <ctype.h>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/testing_pref_service.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_switches.h"
#include "components/variations/caching_permuted_entropy_provider.h"
#include "components/variations/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

class MetricsStateManagerTest : public testing::Test {
 public:
  MetricsStateManagerTest() : is_metrics_reporting_enabled_(false) {
    MetricsStateManager::RegisterPrefs(prefs_.registry());
  }

  scoped_ptr<MetricsStateManager> CreateStateManager() {
    return MetricsStateManager::Create(
        &prefs_,
        base::Bind(&MetricsStateManagerTest::is_metrics_reporting_enabled,
                   base::Unretained(this))).Pass();
  }

  // Sets metrics reporting as enabled for testing.
  void EnableMetricsReporting() {
    is_metrics_reporting_enabled_ = true;
  }

 protected:
  TestingPrefServiceSimple prefs_;

 private:
  bool is_metrics_reporting_enabled() const {
    return is_metrics_reporting_enabled_;
  }

  bool is_metrics_reporting_enabled_;

  DISALLOW_COPY_AND_ASSIGN(MetricsStateManagerTest);
};

// Ensure the ClientId is formatted as expected.
TEST_F(MetricsStateManagerTest, ClientIdCorrectlyFormatted) {
  scoped_ptr<MetricsStateManager> state_manager(CreateStateManager());
  state_manager->ForceClientIdCreation();

  const std::string client_id = state_manager->client_id();
  EXPECT_EQ(36U, client_id.length());

  for (size_t i = 0; i < client_id.length(); ++i) {
    char current = client_id[i];
    if (i == 8 || i == 13 || i == 18 || i == 23)
      EXPECT_EQ('-', current);
    else
      EXPECT_TRUE(isxdigit(current));
  }
}

TEST_F(MetricsStateManagerTest, EntropySourceUsed_Low) {
  scoped_ptr<MetricsStateManager> state_manager(CreateStateManager());
  state_manager->CreateEntropyProvider();
  EXPECT_EQ(MetricsStateManager::ENTROPY_SOURCE_LOW,
            state_manager->entropy_source_returned());
}

TEST_F(MetricsStateManagerTest, EntropySourceUsed_High) {
  EnableMetricsReporting();
  scoped_ptr<MetricsStateManager> state_manager(CreateStateManager());
  state_manager->CreateEntropyProvider();
  EXPECT_EQ(MetricsStateManager::ENTROPY_SOURCE_HIGH,
            state_manager->entropy_source_returned());
}

TEST_F(MetricsStateManagerTest, LowEntropySource0NotReset) {
  scoped_ptr<MetricsStateManager> state_manager(CreateStateManager());

  // Get the low entropy source once, to initialize it.
  state_manager->GetLowEntropySource();

  // Now, set it to 0 and ensure it doesn't get reset.
  state_manager->low_entropy_source_ = 0;
  EXPECT_EQ(0, state_manager->GetLowEntropySource());
  // Call it another time, just to make sure.
  EXPECT_EQ(0, state_manager->GetLowEntropySource());
}

TEST_F(MetricsStateManagerTest,
       PermutedEntropyCacheClearedWhenLowEntropyReset) {
  const PrefService::Preference* low_entropy_pref =
      prefs_.FindPreference(prefs::kMetricsLowEntropySource);
  const char* kCachePrefName = prefs::kVariationsPermutedEntropyCache;
  int low_entropy_value = -1;

  // First, generate an initial low entropy source value.
  {
    EXPECT_TRUE(low_entropy_pref->IsDefaultValue());

    scoped_ptr<MetricsStateManager> state_manager(CreateStateManager());
    state_manager->GetLowEntropySource();

    EXPECT_FALSE(low_entropy_pref->IsDefaultValue());
    EXPECT_TRUE(low_entropy_pref->GetValue()->GetAsInteger(&low_entropy_value));
  }

  // Now, set a dummy value in the permuted entropy cache pref and verify that
  // another call to GetLowEntropySource() doesn't clobber it when
  // --reset-variation-state wasn't specified.
  {
    prefs_.SetString(kCachePrefName, "test");

    scoped_ptr<MetricsStateManager> state_manager(CreateStateManager());
    state_manager->GetLowEntropySource();

    EXPECT_EQ("test", prefs_.GetString(kCachePrefName));
    EXPECT_EQ(low_entropy_value,
              prefs_.GetInteger(prefs::kMetricsLowEntropySource));
  }

  // Verify that the cache does get reset if --reset-variations-state is passed.
  {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kResetVariationState);

    scoped_ptr<MetricsStateManager> state_manager(CreateStateManager());
    state_manager->GetLowEntropySource();

    EXPECT_TRUE(prefs_.GetString(kCachePrefName).empty());
  }
}

// Check that setting the kMetricsResetIds pref to true causes the client id to
// be reset. We do not check that the low entropy source is reset because we
// cannot ensure that metrics state manager won't generate the same id again.
TEST_F(MetricsStateManagerTest, ResetMetricsIDs) {
  // Set an initial client id in prefs. It should not be possible for the
  // metrics state manager to generate this id randomly.
  const std::string kInitialClientId = "initial client id";
  prefs_.SetString(prefs::kMetricsClientID, kInitialClientId);

  // Make sure the initial client id isn't reset by the metrics state manager.
  {
    scoped_ptr<MetricsStateManager> state_manager(CreateStateManager());
    state_manager->ForceClientIdCreation();
    EXPECT_EQ(kInitialClientId, state_manager->client_id());
  }

  // Set the reset pref to cause the IDs to be reset.
  prefs_.SetBoolean(prefs::kMetricsResetIds, true);

  // Cause the actual reset to happen.
  {
    scoped_ptr<MetricsStateManager> state_manager(CreateStateManager());
    state_manager->ForceClientIdCreation();
    EXPECT_NE(kInitialClientId, state_manager->client_id());

    state_manager->GetLowEntropySource();

    EXPECT_FALSE(prefs_.GetBoolean(prefs::kMetricsResetIds));
  }

  EXPECT_NE(kInitialClientId, prefs_.GetString(prefs::kMetricsClientID));
}

}  // namespace metrics
