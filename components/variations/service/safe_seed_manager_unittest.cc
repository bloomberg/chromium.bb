// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/safe_seed_manager.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/variations_seed_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

const char kTestSeed[] = "compressed, base-64 encoded serialized seed data";
const char kTestSignature[] = "a completely unforged signature, I promise!";
const char kTestLocale[] = "en-US";
const char kTestPermanentConsistencyCountry[] = "US";
const char kTestSessionConsistencyCountry[] = "CA";

// A simple fake data store.
class FakeSeedStore : public VariationsSeedStore {
 public:
  FakeSeedStore() : VariationsSeedStore(nullptr) {}
  ~FakeSeedStore() override = default;

  bool StoreSafeSeed(const std::string& seed_data,
                     const std::string& base64_seed_signature,
                     const ClientFilterableState& client_state) override {
    seed_data_ = seed_data;
    signature_ = base64_seed_signature;
    date_ = client_state.reference_date;
    locale_ = client_state.locale;
    permanent_consistency_country_ = client_state.permanent_consistency_country;
    session_consistency_country_ = client_state.session_consistency_country;
    return true;
  }

  // TODO(isherman): Replace these with a LoadSafeSeed() function once that's
  // implemented on the seed store.
  const std::string& seed_data() const { return seed_data_; }
  const std::string& signature() const { return signature_; }
  const base::Time& date() const { return date_; }
  const std::string& locale() const { return locale_; }
  const std::string& permanent_consistency_country() const {
    return permanent_consistency_country_;
  }
  const std::string& session_consistency_country() const {
    return session_consistency_country_;
  }

 private:
  // The stored data.
  std::string seed_data_;
  std::string signature_;
  base::Time date_;
  std::string locale_;
  std::string permanent_consistency_country_;
  std::string session_consistency_country_;

  DISALLOW_COPY_AND_ASSIGN(FakeSeedStore);
};

// Passes the default test values as the active state into the
// |safe_seed_manager|.
void SetDefaultActiveState(SafeSeedManager* safe_seed_manager) {
  std::unique_ptr<ClientFilterableState> client_state =
      std::make_unique<ClientFilterableState>();
  client_state->locale = kTestLocale;
  client_state->permanent_consistency_country =
      kTestPermanentConsistencyCountry;
  client_state->session_consistency_country = kTestSessionConsistencyCountry;
  client_state->reference_date = base::Time::UnixEpoch();

  safe_seed_manager->SetActiveSeedState(kTestSeed, kTestSignature,
                                        std::move(client_state));
}

// Verifies that the default test values were written to the seed store.
void ExpectDefaultActiveState(const FakeSeedStore& seed_store) {
  EXPECT_EQ(kTestSeed, seed_store.seed_data());
  EXPECT_EQ(kTestSignature, seed_store.signature());
  EXPECT_EQ(kTestLocale, seed_store.locale());
  EXPECT_EQ(kTestPermanentConsistencyCountry,
            seed_store.permanent_consistency_country());
  EXPECT_EQ(kTestSessionConsistencyCountry,
            seed_store.session_consistency_country());
  EXPECT_EQ(base::Time::UnixEpoch(), seed_store.date());
}

}  // namespace

class SafeSeedManagerTest : public testing::Test {
 public:
  SafeSeedManagerTest() { SafeSeedManager::RegisterPrefs(prefs_.registry()); }
  ~SafeSeedManagerTest() override = default;

 protected:
  TestingPrefServiceSimple prefs_;
};

TEST_F(SafeSeedManagerTest, RecordSuccessfulFetch_FirstCallSavesSafeSeed) {
  SafeSeedManager safe_seed_manager(true, &prefs_);
  SetDefaultActiveState(&safe_seed_manager);

  FakeSeedStore seed_store;
  safe_seed_manager.RecordSuccessfulFetch(&seed_store);

  ExpectDefaultActiveState(seed_store);
}

TEST_F(SafeSeedManagerTest, RecordSuccessfulFetch_RepeatedCallsRetainSafeSeed) {
  SafeSeedManager safe_seed_manager(true, &prefs_);
  SetDefaultActiveState(&safe_seed_manager);

  FakeSeedStore seed_store;
  safe_seed_manager.RecordSuccessfulFetch(&seed_store);
  safe_seed_manager.RecordSuccessfulFetch(&seed_store);
  safe_seed_manager.RecordSuccessfulFetch(&seed_store);

  ExpectDefaultActiveState(seed_store);
}

TEST_F(SafeSeedManagerTest,
       RecordSuccessfulFetch_NoActiveState_DoesntSaveSafeSeed) {
  SafeSeedManager safe_seed_manager(true, &prefs_);
  // Omit setting any active state.

  FakeSeedStore seed_store;
  safe_seed_manager.RecordSuccessfulFetch(&seed_store);

  EXPECT_EQ(std::string(), seed_store.seed_data());
  EXPECT_EQ(std::string(), seed_store.signature());
  EXPECT_EQ(std::string(), seed_store.locale());
  EXPECT_EQ(std::string(), seed_store.permanent_consistency_country());
  EXPECT_EQ(std::string(), seed_store.session_consistency_country());
  EXPECT_EQ(base::Time(), seed_store.date());
}

}  // namespace variations
