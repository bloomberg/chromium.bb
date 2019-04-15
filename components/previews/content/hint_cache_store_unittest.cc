// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/hint_cache_store.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/previews/content/proto/hint_cache.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using leveldb_proto::test::FakeDB;
using testing::Mock;

namespace previews {

namespace {

constexpr char kDefaultComponentVersion[] = "1.0.0";
constexpr char kUpdateComponentVersion[] = "2.0.0";

std::string GetHostSuffix(size_t id) {
  // Host suffix alternates between two different domain types depending on
  // whether the id is odd or even.
  if (id % 2 == 0) {
    return "domain" + std::to_string(id) + ".org";
  } else {
    return "different.domain" + std::to_string(id) + ".co.in";
  }
}

enum class MetadataSchemaState {
  kMissing,
  kInvalid,
  kValid,
};

}  // namespace

class HintCacheStoreTest : public testing::Test {
 public:
  using StoreEntry = previews::proto::StoreEntry;
  using StoreEntryMap = std::map<HintCacheStore::EntryKey, StoreEntry>;

  HintCacheStoreTest() : db_(nullptr) {}

  void TearDown() override { last_loaded_hint_.reset(); }

  // Initializes the entries contained within the database on startup.
  void SeedInitialData(
      MetadataSchemaState state,
      base::Optional<size_t> component_hint_count = base::Optional<size_t>(),
      base::Optional<base::Time> fetched_hints_update =
          base::Optional<base::Time>()) {
    db_store_.clear();

    // Add a metadata schema entry if its state isn't kMissing. The version
    // entry version is set to the store's current version if the state is
    // kValid; otherwise, it's set to the invalid version of "0".
    if (state == MetadataSchemaState::kValid) {
      db_store_[HintCacheStore::GetMetadataTypeEntryKey(
                    HintCacheStore::MetadataType::kSchema)]
          .set_version(HintCacheStore::kStoreSchemaVersion);
    } else if (state == MetadataSchemaState::kInvalid) {
      db_store_[HintCacheStore::GetMetadataTypeEntryKey(
                    HintCacheStore::MetadataType::kSchema)]
          .set_version("0");
    }

    // If the database is being seeded with component hints, it is indicated
    // with a provided count. Add the component metadata with the default
    // component version and then add the indicated number of component hints.
    // if (component_hint_count && component_hint_count >
    // static_cast<size_t>(0)) {
    if (component_hint_count && component_hint_count > 0u) {
      db_store_[HintCacheStore::GetMetadataTypeEntryKey(
                    HintCacheStore::MetadataType::kComponent)]
          .set_version(kDefaultComponentVersion);
      HintCacheStore::EntryKeyPrefix component_hint_key_prefix =
          HintCacheStore::GetComponentHintEntryKeyPrefix(
              base::Version(kDefaultComponentVersion));
      for (size_t i = 0; i < component_hint_count.value(); ++i) {
        std::string host_suffix = GetHostSuffix(i);
        StoreEntry& entry = db_store_[component_hint_key_prefix + host_suffix];
        optimization_guide::proto::Hint* hint = entry.mutable_hint();
        hint->set_key(host_suffix);
        hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
        optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
        page_hint->set_page_pattern("page pattern " + std::to_string(i));
      }
    }
    if (fetched_hints_update) {
      db_store_[HintCacheStore::GetMetadataTypeEntryKey(
                    HintCacheStore::MetadataType::kFetched)]
          .set_update_time_secs(
              fetched_hints_update->ToDeltaSinceWindowsEpoch().InSeconds());
    }
  }

  // Moves the specified number of component hints into the update data.
  void SeedUpdateData(HintCacheStore::ComponentUpdateData* update_data,
                      size_t component_hint_count) {
    for (size_t i = 0; i < component_hint_count; ++i) {
      std::string host_suffix = GetHostSuffix(i);
      optimization_guide::proto::Hint hint;
      hint.set_key(host_suffix);
      hint.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
      optimization_guide::proto::PageHint* page_hint = hint.add_page_hints();
      page_hint->set_page_pattern("page pattern " + std::to_string(i));
      update_data->MoveHintIntoUpdateData(std::move(hint));
    }
  }

  void CreateDatabase() {
    // Reset everything.
    db_ = nullptr;
    hint_store_.reset();

    // Setup the fake db and the class under test.
    auto db = std::make_unique<FakeDB<StoreEntry>>(&db_store_);
    db_ = db.get();
    hint_store_ =
        std::make_unique<HintCacheStore>(base::FilePath(), std::move(db));
  }

  void InitializeDatabase(bool success, bool purge_existing_data = false) {
    EXPECT_CALL(*this, OnInitialized());
    hint_store()->Initialize(purge_existing_data,
                             base::BindOnce(&HintCacheStoreTest::OnInitialized,
                                            base::Unretained(this)));
    // OnDatabaseInitialized callback
    db()->InitCallback(success);
  }

  void InitializeStore(MetadataSchemaState state,
                       bool purge_existing_data = false) {
    InitializeDatabase(true /*=success*/, purge_existing_data);

    if (purge_existing_data) {
      // OnPurgeDatabase callback
      db()->UpdateCallback(true);
      return;
    }

    // OnLoadMetadata callback
    db()->LoadCallback(true);
    if (state == MetadataSchemaState::kValid) {
      // OnLoadHintEntryKeys callback
      db()->LoadCallback(true);
    } else {
      // OnPurgeDatabase callback
      db()->UpdateCallback(true);
    }
  }

  void UpdateComponentData(
      std::unique_ptr<HintCacheStore::ComponentUpdateData> component_data,
      bool update_success = true,
      bool load_hint_entry_keys_success = true) {
    EXPECT_CALL(*this, OnUpdateComponentData());
    hint_store()->UpdateComponentData(
        std::move(component_data),
        base::BindOnce(&HintCacheStoreTest::OnUpdateComponentData,
                       base::Unretained(this)));
    // OnUpdateComponentData callback
    db()->UpdateCallback(update_success);
    if (update_success) {
      // OnLoadHintEntryKeys callback
      db()->LoadCallback(load_hint_entry_keys_success);
    }
  }

  bool IsMetadataSchemaEntryKeyPresent() const {
    return IsKeyPresent(HintCacheStore::GetMetadataTypeEntryKey(
        HintCacheStore::MetadataType::kSchema));
  }

  // Verifies that the fetched metadata has the expected next update time.
  void ExpectFetchedMetadata(base::Time update_time) const {
    const auto& metadata_entry =
        db_store_.find(HintCacheStore::GetMetadataTypeEntryKey(
            HintCacheStore::MetadataType::kFetched));
    if (metadata_entry != db_store_.end()) {
      // The next update time should have same time up to the second as the
      // metadata entry is stored in seconds.
      EXPECT_TRUE(
          base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromSeconds(
              metadata_entry->second.update_time_secs())) -
              update_time <
          base::TimeDelta::FromSeconds(1));
    } else {
      FAIL() << "No fetched metadata found";
    }
  }
  // Verifies that the component metadata has the expected version and all
  // expected component hints are present.
  void ExpectComponentHintsPresent(const std::string& version,
                                   int count) const {
    const auto& metadata_entry =
        db_store_.find(HintCacheStore::GetMetadataTypeEntryKey(
            HintCacheStore::MetadataType::kComponent));
    if (metadata_entry != db_store_.end()) {
      EXPECT_EQ(metadata_entry->second.version(), version);
    } else {
      FAIL() << "No component metadata found";
    }

    HintCacheStore::EntryKeyPrefix component_hint_entry_key_prefix =
        HintCacheStore::GetComponentHintEntryKeyPrefix(base::Version(version));
    for (int i = 0; i < count; ++i) {
      std::string host_suffix = GetHostSuffix(i);
      HintCacheStore::EntryKey hint_entry_key =
          component_hint_entry_key_prefix + host_suffix;
      const auto& hint_entry = db_store_.find(hint_entry_key);
      if (hint_entry == db_store_.end()) {
        FAIL() << "No entry found for component hint: " << hint_entry_key;
        continue;
      }

      if (!hint_entry->second.has_hint()) {
        FAIL() << "Component hint entry does not have hint: " << hint_entry_key;
        continue;
      }

      EXPECT_EQ(hint_entry->second.hint().key(), host_suffix);
    }
  }

  // Returns true if the data is present for the given key.
  bool IsKeyPresent(const HintCacheStore::EntryKey& entry_key) const {
    return db_store_.find(entry_key) != db_store_.end();
  }

  size_t GetDBStoreEntryCount() const { return db_store_.size(); }
  size_t GetStoreHintEntryKeyCount() const {
    return hint_store_->GetHintEntryKeyCount();
  }

  HintCacheStore* hint_store() { return hint_store_.get(); }
  FakeDB<previews::proto::StoreEntry>* db() { return db_; }

  const HintCacheStore::EntryKey& last_loaded_hint_entry_key() const {
    return last_loaded_hint_entry_key_;
  }

  optimization_guide::proto::Hint* last_loaded_hint() {
    return last_loaded_hint_.get();
  }

  void OnHintLoaded(
      const HintCacheStore::EntryKey& hint_entry_key,
      std::unique_ptr<optimization_guide::proto::Hint> loaded_hint) {
    last_loaded_hint_entry_key_ = hint_entry_key;
    last_loaded_hint_ = std::move(loaded_hint);
  }

  MOCK_METHOD0(OnInitialized, void());
  MOCK_METHOD0(OnUpdateComponentData, void());

 private:
  FakeDB<previews::proto::StoreEntry>* db_;
  StoreEntryMap db_store_;
  std::unique_ptr<HintCacheStore> hint_store_;

  HintCacheStore::EntryKey last_loaded_hint_entry_key_;
  std::unique_ptr<optimization_guide::proto::Hint> last_loaded_hint_;

  DISALLOW_COPY_AND_ASSIGN(HintCacheStoreTest);
};

TEST_F(HintCacheStoreTest, NoInitialization) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kMissing);
  CreateDatabase();

  histogram_tester.ExpectTotalCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult", 0);

  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     0 /* kUninitialized */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     1 /* kInitializing */, 0);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest, InitializeFailedOnInitializeWithNoInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kMissing);
  CreateDatabase();
  InitializeDatabase(false /*=success*/);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectTotalCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult", 0);

  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     0 /* kUninitialized */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     1 /* kInitializing */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     3 /* kFailed */, 1);
}

TEST_F(HintCacheStoreTest, InitializeFailedOnLoadMetadataWithNoInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kMissing);
  CreateDatabase();
  InitializeDatabase(true /*=success*/);

  // OnLoadMetadata callback
  db()->LoadCallback(false);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectBucketCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult",
      1 /* kLoadMetadataFailed */, 1);

  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     0 /* kUninitialized */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     1 /* kInitializing */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     3 /* kFailed */, 1);
}

TEST_F(HintCacheStoreTest, InitializeFailedOnUpdateMetadataNoInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kMissing);
  CreateDatabase();

  InitializeDatabase(true /*=success*/);

  // OnLoadMetadata callback
  db()->LoadCallback(true);
  // OnPurgeDatabase callback
  db()->UpdateCallback(false);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectBucketCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult",
      2 /* kSchemaMetadataMissing */, 1);

  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     0 /* kUninitialized */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     1 /* kInitializing */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     3 /* kFailed */, 1);
}

TEST_F(HintCacheStoreTest, InitializeFailedOnInitializeWithInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kValid, 10);
  CreateDatabase();
  InitializeDatabase(false /*=success*/);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectTotalCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult", 0);

  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     0 /* kUninitialized */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     1 /* kInitializing */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     3 /* kFailed */, 1);
}

TEST_F(HintCacheStoreTest, InitializeFailedOnLoadMetadataWithInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kValid, 10);
  CreateDatabase();
  InitializeDatabase(true /*=success*/);

  // OnLoadMetadata callback
  db()->LoadCallback(false);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectBucketCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult",
      1 /* kLoadMetadataFailed */, 1);

  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     0 /* kUninitialized */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     1 /* kInitializing */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     3 /* kFailed */, 1);
}

TEST_F(HintCacheStoreTest,
       InitializeFailedOnUpdateMetadataWithInvalidSchemaEntry) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kInvalid, 10);
  CreateDatabase();
  InitializeDatabase(true /*=success*/);

  // OnLoadMetadata callback
  db()->LoadCallback(true);
  // OnPurgeDatabase callback
  db()->UpdateCallback(false);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectBucketCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult",
      3 /* kSchemaMetadataWrongVersion */, 1);

  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     0 /* kUninitialized */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     1 /* kInitializing */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     2 /* kAvailable */, 0);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     3 /* kFailed */, 1);
}

TEST_F(HintCacheStoreTest, InitializeFailedOnLoadHintEntryKeysWithInitialData) {
  base::HistogramTester histogram_tester;

  SeedInitialData(MetadataSchemaState::kValid, 10, base::Time().Now());
  CreateDatabase();
  InitializeDatabase(true /*=success*/);

  // OnLoadMetadata callback
  db()->LoadCallback(true);
  // OnLoadHintEntryKeys callback
  db()->LoadCallback(false);

  // In the case where initialization fails, the store should be fully purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  histogram_tester.ExpectBucketCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult", 0 /* kSuccess */, 1);

  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     0 /* kUninitialized */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     1 /* kInitializing */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     3 /* kFailed */, 1);
}

TEST_F(HintCacheStoreTest, InitializeSucceededWithoutSchemaEntry) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kMissing;
  SeedInitialData(schema_state);
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry and nothing else.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(1));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectBucketCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult",
      2 /* kSchemaMetadataMissing */, 1);

  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     0 /* kUninitialized */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     1 /* kInitializing */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest, InitializeSucceededWithInvalidSchemaEntry) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kInvalid;
  SeedInitialData(schema_state);
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry and nothing else.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(1));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectBucketCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult",
      3 /* kSchemaMetadataWrongVersion */, 1);

  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     0 /* kUninitialized */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     1 /* kInitializing */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest, InitializeSucceededWithValidSchemaEntry) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state);
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry and nothing else.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(1));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectBucketCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult",
      4 /* kComponentMetadataMissing*/, 0);
  histogram_tester.ExpectBucketCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult",
      5 /* kFetchedMetadataMissing*/, 0);
  histogram_tester.ExpectBucketCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult",
      6 /* kComponentAndFetchedMetadataMissing*/, 1);

  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     0 /* kUninitialized */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     1 /* kInitializing */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest,
       InitializeSucceededWithInvalidSchemaEntryAndInitialData) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kInvalid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry and nothing else, as
  // the initial component hints are all purged.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(1));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectBucketCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult",
      3 /* kSchemaMetadataWrongVersion */, 1);

  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     0 /* kUninitialized */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     1 /* kInitializing */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest, InitializeSucceededWithPurgeExistingData) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state, true /*=purge_existing_data*/);

  // The store should contain the schema metadata entry and nothing else.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(1));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectTotalCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult", 0);

  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     0 /* kUninitialized */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     1 /* kInitializing */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest,
       InitializeSucceededWithValidSchemaEntryAndInitialData) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t component_hint_count = 10;
  SeedInitialData(schema_state, component_hint_count, base::Time().Now());
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry, the component metadata
  // entry, and all of the initial component hints.
  EXPECT_EQ(GetDBStoreEntryCount(),
            static_cast<size_t>(component_hint_count + 3));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), component_hint_count);

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());
  ExpectComponentHintsPresent(kDefaultComponentVersion, component_hint_count);

  histogram_tester.ExpectBucketCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult", 0 /* kSuccess */, 1);

  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     0 /* kUninitialized */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     1 /* kInitializing */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest,
       InitializeSucceededWithValidSchemaEntryAndComponentDataOnly) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t component_hint_count = 10;
  SeedInitialData(schema_state, component_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry, the component metadata
  // entry, and all of the initial component hints.
  EXPECT_EQ(GetDBStoreEntryCount(),
            static_cast<size_t>(component_hint_count + 2));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), component_hint_count);

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());
  ExpectComponentHintsPresent(kDefaultComponentVersion, component_hint_count);

  histogram_tester.ExpectBucketCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult",
      4 /* kComponentMetadataMissing*/, 0);
  histogram_tester.ExpectBucketCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult",
      5 /* kFetchedMetadataMissing*/, 1);
  histogram_tester.ExpectBucketCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult",
      6 /* kComponentAndFetchedMetadataMissing*/, 0);

  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     0 /* kUninitialized */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     1 /* kInitializing */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest,
       InitializeSucceededWithValidSchemaEntryAndFetchedMetaData) {
  base::HistogramTester histogram_tester;

  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t component_hint_count = 0;
  SeedInitialData(schema_state, component_hint_count, base::Time().Now());
  CreateDatabase();
  InitializeStore(schema_state);

  // The store should contain the schema metadata entry, the component metadata
  // entry, and all of the initial component hints.
  EXPECT_EQ(GetDBStoreEntryCount(),
            static_cast<size_t>(component_hint_count + 2));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), component_hint_count);

  EXPECT_TRUE(IsMetadataSchemaEntryKeyPresent());

  histogram_tester.ExpectBucketCount(
      "Previews.HintCacheLevelDBStore.LoadMetadataResult",
      4 /* kComponentMetadataMissing*/, 1);

  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     0 /* kUninitialized */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     1 /* kInitializing */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     2 /* kAvailable */, 1);
  histogram_tester.ExpectBucketCount("Previews.HintCacheLevelDBStore.Status",
                                     3 /* kFailed */, 0);
}

TEST_F(HintCacheStoreTest,
       CreateComponentUpdateDataFailsForUninitializedStore) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();

  // ComponentUpdateData can't be created when the store isn't initialized.
  EXPECT_FALSE(hint_store()->MaybeCreateComponentUpdateData(
      base::Version(kUpdateComponentVersion)));
}

TEST_F(HintCacheStoreTest, CreateComponentUpdateDataFailsForEarlierVersion) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  // No ComponentUpdateData should be created when the component version of the
  // update is older than the store's component version.
  EXPECT_FALSE(
      hint_store()->MaybeCreateComponentUpdateData(base::Version("0.0.0")));
}

TEST_F(HintCacheStoreTest, CreateComponentUpdateDataFailsForCurrentVersion) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  // No ComponentUpdateData should be created when the component version of the
  // update is the same as the store's component version.
  EXPECT_FALSE(hint_store()->MaybeCreateComponentUpdateData(
      base::Version(kDefaultComponentVersion)));
}

TEST_F(HintCacheStoreTest,
       CreateComponentUpdateDataSucceedsWithNoPreexistingVersion) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state);
  CreateDatabase();
  InitializeStore(schema_state);

  // ComponentUpdateData should be created when there is no pre-existing
  // component.
  EXPECT_TRUE(hint_store()->MaybeCreateComponentUpdateData(
      base::Version(kDefaultComponentVersion)));
}

TEST_F(HintCacheStoreTest, CreateComponentUpdateDataSucceedsForNewerVersion) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  // ComponentUpdateData should be created when the component version of the
  // update is newer than the store's component version.
  EXPECT_TRUE(hint_store()->MaybeCreateComponentUpdateData(
      base::Version(kUpdateComponentVersion)));
}

TEST_F(HintCacheStoreTest, UpdateComponentDataUpdateEntriesFails) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data =
      hint_store()->MaybeCreateComponentUpdateData(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedUpdateData(update_data.get(), 5);

  UpdateComponentData(std::move(update_data), false /*update_success*/);

  // The store should be purged if the component data update fails.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));
}

TEST_F(HintCacheStoreTest, UpdateComponentDataGetKeysFails) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  SeedInitialData(schema_state, 10);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data =
      hint_store()->MaybeCreateComponentUpdateData(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedUpdateData(update_data.get(), 5);

  UpdateComponentData(std::move(update_data), true /*update_success*/,
                      false /*load_hints_keys_success*/);

  // The store should be purged if loading the keys after the component update
  // fails.
  EXPECT_EQ(GetDBStoreEntryCount(), static_cast<size_t>(0));
  EXPECT_EQ(GetStoreHintEntryKeyCount(), static_cast<size_t>(0));
}

TEST_F(HintCacheStoreTest, UpdateComponentData) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count = 5;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data =
      hint_store()->MaybeCreateComponentUpdateData(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedUpdateData(update_data.get(), update_hint_count);
  UpdateComponentData(std::move(update_data));

  // When the component update succeeds, the store should contain the schema
  // metadata entry, the component metadata entry, and all of the update's
  // component hints.
  EXPECT_EQ(GetDBStoreEntryCount(), update_hint_count + 2);
  EXPECT_EQ(GetStoreHintEntryKeyCount(), update_hint_count);
  ExpectComponentHintsPresent(kUpdateComponentVersion, update_hint_count);
}

TEST_F(HintCacheStoreTest, UpdateComponentDataAfterInitializationDataPurge) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count = 5;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state, true /*=purge_existing_data*/);

  std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data =
      hint_store()->MaybeCreateComponentUpdateData(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedUpdateData(update_data.get(), update_hint_count);
  UpdateComponentData(std::move(update_data));

  // When the component update succeeds, the store should contain the schema
  // metadata entry, the component metadata entry, and all of the update's
  // component hints.
  EXPECT_EQ(GetDBStoreEntryCount(), update_hint_count + 2);
  EXPECT_EQ(GetStoreHintEntryKeyCount(), update_hint_count);
  ExpectComponentHintsPresent(kUpdateComponentVersion, update_hint_count);
}

TEST_F(HintCacheStoreTest, CreateComponentDataWithAlreadyUpdatedVersionFails) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count = 5;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data =
      hint_store()->MaybeCreateComponentUpdateData(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedUpdateData(update_data.get(), update_hint_count);
  UpdateComponentData(std::move(update_data));

  // ComponentUpdateData should not be created for a second component update
  // with the same version as the first component update.
  EXPECT_FALSE(hint_store()->MaybeCreateComponentUpdateData(
      base::Version(kUpdateComponentVersion)));
}

TEST_F(HintCacheStoreTest, UpdateComponentDataWithUpdatedVersionFails) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count_1 = 5;
  size_t update_hint_count_2 = 15;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  // Create two updates for the same component version with different counts.
  std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data_1 =
      hint_store()->MaybeCreateComponentUpdateData(
          base::Version(kUpdateComponentVersion));
  std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data_2 =
      hint_store()->MaybeCreateComponentUpdateData(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data_1);
  SeedUpdateData(update_data_1.get(), update_hint_count_1);
  ASSERT_TRUE(update_data_2);
  SeedUpdateData(update_data_2.get(), update_hint_count_2);

  // Update the component data with the same component version twice:
  // first with |update_data_1| and then with |update_data_2|.
  UpdateComponentData(std::move(update_data_1));

  EXPECT_CALL(*this, OnUpdateComponentData());
  hint_store()->UpdateComponentData(
      std::move(update_data_2),
      base::BindOnce(&HintCacheStoreTest::OnUpdateComponentData,
                     base::Unretained(this)));

  // Verify that the store is populated with the component data from
  // |update_data_1| and not |update_data_2|.
  EXPECT_EQ(GetDBStoreEntryCount(), update_hint_count_1 + 2);
  EXPECT_EQ(GetStoreHintEntryKeyCount(), update_hint_count_1);
  ExpectComponentHintsPresent(kUpdateComponentVersion, update_hint_count_1);
}

TEST_F(HintCacheStoreTest, LoadHintOnUnavailableStore) {
  size_t initial_hint_count = 10;
  SeedInitialData(MetadataSchemaState::kValid, initial_hint_count);
  CreateDatabase();

  const HintCacheStore::EntryKey kInvalidEntryKey = "invalid";
  hint_store()->LoadHint(kInvalidEntryKey,
                         base::BindOnce(&HintCacheStoreTest::OnHintLoaded,
                                        base::Unretained(this)));

  // Verify that the OnHintLoaded callback runs when the store is unavailable
  // and that both the key and the hint were correctly set in it.
  EXPECT_EQ(last_loaded_hint_entry_key(), kInvalidEntryKey);
  EXPECT_FALSE(last_loaded_hint());
}

TEST_F(HintCacheStoreTest, LoadHintFailure) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t hint_count = 10;
  SeedInitialData(schema_state, hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  const HintCacheStore::EntryKey kInvalidEntryKey = "invalid";
  hint_store()->LoadHint(kInvalidEntryKey,
                         base::BindOnce(&HintCacheStoreTest::OnHintLoaded,
                                        base::Unretained(this)));

  // OnLoadHint callback
  db()->GetCallback(false);

  // Verify that the OnHintLoaded callback runs when the store is unavailable
  // and that both the key and the hint were correctly set in it.
  EXPECT_EQ(last_loaded_hint_entry_key(), kInvalidEntryKey);
  EXPECT_FALSE(last_loaded_hint());
}

TEST_F(HintCacheStoreTest, LoadHintSuccessInitialData) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t hint_count = 10;
  SeedInitialData(schema_state, hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  // Verify that all component hints in the initial data can successfully be
  // loaded from the store.
  for (size_t i = 0; i < hint_count; ++i) {
    std::string host_suffix = GetHostSuffix(i);
    HintCacheStore::EntryKey hint_entry_key;
    if (!hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key)) {
      FAIL() << "Hint entry not found for host suffix: " << host_suffix;
      continue;
    }

    hint_store()->LoadHint(hint_entry_key,
                           base::BindOnce(&HintCacheStoreTest::OnHintLoaded,
                                          base::Unretained(this)));

    // OnLoadHint callback
    db()->GetCallback(true);

    EXPECT_EQ(last_loaded_hint_entry_key(), hint_entry_key);
    if (!last_loaded_hint()) {
      FAIL() << "Loaded hint NULL for entry key: " << hint_entry_key;
      continue;
    }

    EXPECT_EQ(last_loaded_hint()->key(), host_suffix);
  }
}

TEST_F(HintCacheStoreTest, LoadHintSuccessUpdateData) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count = 5;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data =
      hint_store()->MaybeCreateComponentUpdateData(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedUpdateData(update_data.get(), update_hint_count);
  UpdateComponentData(std::move(update_data));

  // Verify that all component hints within a successful component update can
  // be loaded from the store.
  for (size_t i = 0; i < update_hint_count; ++i) {
    std::string host_suffix = GetHostSuffix(i);
    HintCacheStore::EntryKey hint_entry_key;
    if (!hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key)) {
      FAIL() << "Hint entry not found for host suffix: " << host_suffix;
      continue;
    }

    hint_store()->LoadHint(hint_entry_key,
                           base::BindOnce(&HintCacheStoreTest::OnHintLoaded,
                                          base::Unretained(this)));

    // OnLoadHint callback
    db()->GetCallback(true);

    EXPECT_EQ(last_loaded_hint_entry_key(), hint_entry_key);
    if (!last_loaded_hint()) {
      FAIL() << "Loaded hint NULL for entry key: " << hint_entry_key;
      continue;
    }

    EXPECT_EQ(last_loaded_hint()->key(), host_suffix);
  }
}

TEST_F(HintCacheStoreTest, FindHintEntryKeyOnUnavailableStore) {
  size_t initial_hint_count = 10;
  SeedInitialData(MetadataSchemaState::kValid, initial_hint_count);
  CreateDatabase();

  std::string host_suffix = GetHostSuffix(0);
  HintCacheStore::EntryKey hint_entry_key;

  // Verify that hint entry keys can't be found when the store is unavailable.
  EXPECT_FALSE(hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key));
}

TEST_F(HintCacheStoreTest, FindHintEntryKeyInitialData) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t hint_count = 10;
  SeedInitialData(schema_state, hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  // Verify that all hints contained within the initial store data are reported
  // as being found and hints that are not containd within the initial data are
  // properly reported as not being found.
  for (size_t i = 0; i < hint_count * 2; ++i) {
    std::string host_suffix = GetHostSuffix(i);
    HintCacheStore::EntryKey hint_entry_key;
    bool success = hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key);
    EXPECT_EQ(success, i < hint_count);
  }
}

TEST_F(HintCacheStoreTest, FindHintEntryKeyUpdateData) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  size_t initial_hint_count = 10;
  size_t update_hint_count = 5;
  SeedInitialData(schema_state, initial_hint_count);
  CreateDatabase();
  InitializeStore(schema_state);

  std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data =
      hint_store()->MaybeCreateComponentUpdateData(
          base::Version(kUpdateComponentVersion));
  ASSERT_TRUE(update_data);
  SeedUpdateData(update_data.get(), update_hint_count);
  UpdateComponentData(std::move(update_data));

  // Verify that all hints contained within the component update are reported
  // by the store as being found and hints that are not containd within the
  // component update are properly reported as not being found.
  for (size_t i = 0; i < update_hint_count * 2; ++i) {
    std::string host_suffix = GetHostSuffix(i);
    HintCacheStore::EntryKey hint_entry_key;
    bool success = hint_store()->FindHintEntryKey(host_suffix, &hint_entry_key);
    EXPECT_EQ(success, i < update_hint_count);
  }
}

TEST_F(HintCacheStoreTest, FetchedHintsMetadataStored) {
  MetadataSchemaState schema_state = MetadataSchemaState::kValid;
  base::Time update_time = base::Time().Now();
  SeedInitialData(schema_state, 10, update_time);
  CreateDatabase();
  InitializeStore(schema_state);

  ExpectFetchedMetadata(update_time);
}
}  // namespace previews
