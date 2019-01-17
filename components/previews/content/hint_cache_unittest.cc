// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/hint_cache.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "components/previews/content/hint_cache_leveldb_store.h"
#include "components/previews/core/previews_experiments.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {

std::string GetHostDomainOrg(int index) {
  return "host.domain" + std::to_string(index) + ".org";
}

class HintCacheTest : public testing::Test {
 public:
  HintCacheTest() : loaded_hint_(nullptr) {}

  ~HintCacheTest() override {}

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override { DestroyHintCache(); }

 protected:
  // Creates and initializes the hint cache and hint cache store and waits for
  // the callback indicating that initialization is complete.
  void CreateAndInitializeHintCache(int memory_cache_size,
                                    bool purge_existing_data = false) {
    hint_cache_ = std::make_unique<HintCache>(
        std::make_unique<HintCacheLevelDBStore>(
            temp_dir_.GetPath(),
            scoped_task_environment_.GetMainThreadTaskRunner()),
        memory_cache_size);
    is_store_initialized_ = false;
    hint_cache_->Initialize(purge_existing_data,
                            base::BindOnce(&HintCacheTest::OnStoreInitialized,
                                           base::Unretained(this)));
    while (!is_store_initialized_) {
      RunUntilIdle();
    }
  }

  void DestroyHintCache() {
    hint_cache_.reset();
    loaded_hint_ = nullptr;
    is_store_initialized_ = false;
    is_component_data_updated_ = false;
    on_load_hint_callback_called = false;

    RunUntilIdle();
  }

  HintCache* hint_cache() { return hint_cache_.get(); }

  // Updates the cache with |component_data| and waits for callback indicating
  // that the update is complete.
  void UpdateComponentData(
      std::unique_ptr<HintCacheStore::ComponentUpdateData> component_data) {
    is_component_data_updated_ = false;
    hint_cache_->UpdateComponentData(
        std::move(component_data),
        base::BindOnce(&HintCacheTest::OnUpdateComponentData,
                       base::Unretained(this)));
    while (!is_component_data_updated_) {
      RunUntilIdle();
    }
  }

  // Loads hint for the specified host from the cache and waits for callback
  // indicating that loading the hint is complete.
  void LoadHint(const std::string& host) {
    on_load_hint_callback_called = false;
    loaded_hint_ = nullptr;
    hint_cache_->LoadHint(host, base::BindOnce(&HintCacheTest::OnLoadHint,
                                               base::Unretained(this)));
    while (!on_load_hint_callback_called) {
      RunUntilIdle();
    }
  }

  const optimization_guide::proto::Hint* GetLoadedHint() const {
    return loaded_hint_;
  }

 private:
  void RunUntilIdle() {
    scoped_task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  void OnStoreInitialized() { is_store_initialized_ = true; }
  void OnUpdateComponentData() { is_component_data_updated_ = true; }
  void OnLoadHint(const optimization_guide::proto::Hint* hint) {
    on_load_hint_callback_called = true;
    loaded_hint_ = hint;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<HintCache> hint_cache_;
  const optimization_guide::proto::Hint* loaded_hint_;

  bool is_store_initialized_;
  bool is_component_data_updated_;
  bool on_load_hint_callback_called;

  DISALLOW_COPY_AND_ASSIGN(HintCacheTest);
};

TEST_F(HintCacheTest, ComponentUpdate) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version("2.0.0");
  std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data =
      hint_cache()->MaybeCreateComponentUpdateData(version);
  ASSERT_TRUE(update_data);

  optimization_guide::proto::Hint hint1;
  hint1.set_key("subdomain.domain.org");
  hint1.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Hint hint2;
  hint2.set_key("host.domain.org");
  hint2.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Hint hint3;
  hint3.set_key("otherhost.subdomain.domain.org");
  hint3.set_key_representation(optimization_guide::proto::HOST_SUFFIX);

  update_data->MoveHintIntoUpdateData(std::move(hint1));
  update_data->MoveHintIntoUpdateData(std::move(hint2));
  update_data->MoveHintIntoUpdateData(std::move(hint3));

  UpdateComponentData(std::move(update_data));

  // Not matched
  EXPECT_FALSE(hint_cache()->HasHint("domain.org"));
  EXPECT_FALSE(hint_cache()->HasHint("othersubdomain.domain.org"));

  // Matched
  EXPECT_TRUE(hint_cache()->HasHint("otherhost.subdomain.domain.org"));
  EXPECT_TRUE(hint_cache()->HasHint("host.subdomain.domain.org"));
  EXPECT_TRUE(hint_cache()->HasHint("subhost.host.subdomain.domain.org"));
}

TEST_F(HintCacheTest, ComponentUpdateWithSameVersionIgnored) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version("2.0.0");
  std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data =
      hint_cache()->MaybeCreateComponentUpdateData(version);
  ASSERT_TRUE(update_data);

  UpdateComponentData(std::move(update_data));

  EXPECT_FALSE(hint_cache()->MaybeCreateComponentUpdateData(version));
}

TEST_F(HintCacheTest, ComponentUpdateWithEarlierVersionIgnored) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version_1("1.0.0");
  base::Version version_2("2.0.0");

  std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data =
      hint_cache()->MaybeCreateComponentUpdateData(version_2);
  ASSERT_TRUE(update_data);

  UpdateComponentData(std::move(update_data));

  EXPECT_FALSE(hint_cache()->MaybeCreateComponentUpdateData(version_1));
}

TEST_F(HintCacheTest, ComponentUpdateWithLaterVersionProcessed) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version_1("1.0.0");
  base::Version version_2("2.0.0");

  std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data_1 =
      hint_cache()->MaybeCreateComponentUpdateData(version_1);
  ASSERT_TRUE(update_data_1);

  optimization_guide::proto::Hint hint1;
  hint1.set_key("subdomain.domain.org");
  hint1.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Hint hint2;
  hint2.set_key("host.domain.org");
  hint2.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Hint hint3;
  hint3.set_key("otherhost.subdomain.domain.org");
  hint3.set_key_representation(optimization_guide::proto::HOST_SUFFIX);

  update_data_1->MoveHintIntoUpdateData(std::move(hint1));
  update_data_1->MoveHintIntoUpdateData(std::move(hint2));
  update_data_1->MoveHintIntoUpdateData(std::move(hint3));

  UpdateComponentData(std::move(update_data_1));

  // Not matched
  EXPECT_FALSE(hint_cache()->HasHint("domain.org"));
  EXPECT_FALSE(hint_cache()->HasHint("othersubdomain.domain.org"));

  // Matched
  EXPECT_TRUE(hint_cache()->HasHint("otherhost.subdomain.domain.org"));
  EXPECT_TRUE(hint_cache()->HasHint("host.subdomain.domain.org"));
  EXPECT_TRUE(hint_cache()->HasHint("subhost.host.subdomain.domain.org"));

  std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data_2 =
      hint_cache()->MaybeCreateComponentUpdateData(version_2);
  ASSERT_TRUE(update_data_2);

  optimization_guide::proto::Hint hint4;
  hint4.set_key("subdomain.domain2.org");
  hint4.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Hint hint5;
  hint5.set_key("host.domain2.org");
  hint5.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Hint hint6;
  hint6.set_key("otherhost.subdomain.domain2.org");
  hint6.set_key_representation(optimization_guide::proto::HOST_SUFFIX);

  update_data_2->MoveHintIntoUpdateData(std::move(hint4));
  update_data_2->MoveHintIntoUpdateData(std::move(hint5));
  update_data_2->MoveHintIntoUpdateData(std::move(hint6));

  UpdateComponentData(std::move(update_data_2));

  // Not matched
  EXPECT_FALSE(hint_cache()->HasHint("otherhost.subdomain.domain.org"));
  EXPECT_FALSE(hint_cache()->HasHint("host.subdomain.domain.org"));
  EXPECT_FALSE(hint_cache()->HasHint("subhost.host.subdomain.domain.org"));
  EXPECT_FALSE(hint_cache()->HasHint("domain2.org"));
  EXPECT_FALSE(hint_cache()->HasHint("othersubdomain.domain2.org"));

  // Matched
  EXPECT_TRUE(hint_cache()->HasHint("otherhost.subdomain.domain2.org"));
  EXPECT_TRUE(hint_cache()->HasHint("host.subdomain.domain2.org"));
  EXPECT_TRUE(hint_cache()->HasHint("subhost.host.subdomain.domain2.org"));
}

TEST_F(HintCacheTest, ComponentHintsAvailableAfterRestart) {
  for (int i = 0; i < 2; ++i) {
    const int kMemoryCacheSize = 5;
    CreateAndInitializeHintCache(kMemoryCacheSize,
                                 false /*=purge_existing_data*/);

    base::Version version("2.0.0");

    std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data =
        hint_cache()->MaybeCreateComponentUpdateData(version);
    if (i == 0) {
      ASSERT_TRUE(update_data);

      optimization_guide::proto::Hint hint1;
      hint1.set_key("subdomain.domain.org");
      hint1.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
      optimization_guide::proto::Hint hint2;
      hint2.set_key("host.domain.org");
      hint2.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
      optimization_guide::proto::Hint hint3;
      hint3.set_key("otherhost.subdomain.domain.org");
      hint3.set_key_representation(optimization_guide::proto::HOST_SUFFIX);

      update_data->MoveHintIntoUpdateData(std::move(hint1));
      update_data->MoveHintIntoUpdateData(std::move(hint2));
      update_data->MoveHintIntoUpdateData(std::move(hint3));

      UpdateComponentData(std::move(update_data));
    } else {
      EXPECT_FALSE(update_data);
    }

    // Not matched
    EXPECT_FALSE(hint_cache()->HasHint("domain.org"));
    EXPECT_FALSE(hint_cache()->HasHint("othersubdomain.domain.org"));

    // Matched
    EXPECT_TRUE(hint_cache()->HasHint("otherhost.subdomain.domain.org"));
    EXPECT_TRUE(hint_cache()->HasHint("host.subdomain.domain.org"));
    EXPECT_TRUE(hint_cache()->HasHint("subhost.host.subdomain.domain.org"));

    DestroyHintCache();
  }
}

TEST_F(HintCacheTest, ComponentHintsUpdatableAfterRestartWithPurge) {
  for (int i = 0; i < 2; ++i) {
    const int kMemoryCacheSize = 5;
    CreateAndInitializeHintCache(kMemoryCacheSize,
                                 true /*=purge_existing_data*/);

    base::Version version("2.0.0");

    std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data =
        hint_cache()->MaybeCreateComponentUpdateData(version);
    ASSERT_TRUE(update_data);

    optimization_guide::proto::Hint hint1;
    hint1.set_key("subdomain.domain.org");
    hint1.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
    optimization_guide::proto::Hint hint2;
    hint2.set_key("host.domain.org");
    hint2.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
    optimization_guide::proto::Hint hint3;
    hint3.set_key("otherhost.subdomain.domain.org");
    hint3.set_key_representation(optimization_guide::proto::HOST_SUFFIX);

    update_data->MoveHintIntoUpdateData(std::move(hint1));
    update_data->MoveHintIntoUpdateData(std::move(hint2));
    update_data->MoveHintIntoUpdateData(std::move(hint3));

    UpdateComponentData(std::move(update_data));

    // Not matched
    EXPECT_FALSE(hint_cache()->HasHint("domain.org"));
    EXPECT_FALSE(hint_cache()->HasHint("othersubdomain.domain.org"));

    // Matched
    EXPECT_TRUE(hint_cache()->HasHint("otherhost.subdomain.domain.org"));
    EXPECT_TRUE(hint_cache()->HasHint("host.subdomain.domain.org"));
    EXPECT_TRUE(hint_cache()->HasHint("subhost.host.subdomain.domain.org"));

    DestroyHintCache();
  }
}

TEST_F(HintCacheTest, ComponentHintsNotRetainedAfterRestartWithPurge) {
  for (int i = 0; i < 2; ++i) {
    const int kMemoryCacheSize = 5;
    CreateAndInitializeHintCache(kMemoryCacheSize,
                                 true /*=purge_existing_data*/);

    base::Version version("2.0.0");

    std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data =
        hint_cache()->MaybeCreateComponentUpdateData(version);
    if (i == 0) {
      ASSERT_TRUE(update_data);

      optimization_guide::proto::Hint hint1;
      hint1.set_key("subdomain.domain.org");
      hint1.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
      optimization_guide::proto::Hint hint2;
      hint2.set_key("host.domain.org");
      hint2.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
      optimization_guide::proto::Hint hint3;
      hint3.set_key("otherhost.subdomain.domain.org");
      hint3.set_key_representation(optimization_guide::proto::HOST_SUFFIX);

      update_data->MoveHintIntoUpdateData(std::move(hint1));
      update_data->MoveHintIntoUpdateData(std::move(hint2));
      update_data->MoveHintIntoUpdateData(std::move(hint3));

      UpdateComponentData(std::move(update_data));
    } else {
      EXPECT_TRUE(update_data);
    }

    // Not matched
    EXPECT_FALSE(hint_cache()->HasHint("domain.org"));
    EXPECT_FALSE(hint_cache()->HasHint("othersubdomain.domain.org"));

    // Maybe matched
    bool should_match = (i == 0);
    EXPECT_EQ(hint_cache()->HasHint("otherhost.subdomain.domain.org"),
              should_match);
    EXPECT_EQ(hint_cache()->HasHint("host.subdomain.domain.org"), should_match);
    EXPECT_EQ(hint_cache()->HasHint("subhost.host.subdomain.domain.org"),
              should_match);

    DestroyHintCache();
  }
}

TEST_F(HintCacheTest, TestMemoryCacheLeastRecentlyUsedPurge) {
  const int kTestHintCount = 10;
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version("1.0.0");
  std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data =
      hint_cache()->MaybeCreateComponentUpdateData(version);
  ASSERT_TRUE(update_data);

  for (int i = 0; i < kTestHintCount; ++i) {
    optimization_guide::proto::Hint hint;
    hint.set_key(GetHostDomainOrg(i));
    hint.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
    update_data->MoveHintIntoUpdateData(std::move(hint));
  }

  UpdateComponentData(std::move(update_data));

  for (int i = kTestHintCount - 1; i >= 0; --i) {
    std::string host = GetHostDomainOrg(i);
    EXPECT_TRUE(hint_cache()->HasHint(host));
    LoadHint(host);
    ASSERT_TRUE(GetLoadedHint());
    EXPECT_EQ(GetLoadedHint()->key(), host);
  }

  for (int i = 0; i < kTestHintCount; ++i) {
    std::string host = GetHostDomainOrg(i);
    if (i < kMemoryCacheSize) {
      ASSERT_TRUE(hint_cache()->GetHintIfLoaded(host));
      EXPECT_EQ(GetHostDomainOrg(i),
                hint_cache()->GetHintIfLoaded(host)->key());
    } else {
      EXPECT_FALSE(hint_cache()->GetHintIfLoaded(host));
    }
    EXPECT_TRUE(hint_cache()->HasHint(host));
  }
}

TEST_F(HintCacheTest, TestHostNotInCache) {
  const int kTestHintCount = 10;
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version("1.0.0");
  std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data =
      hint_cache()->MaybeCreateComponentUpdateData(version);
  ASSERT_TRUE(update_data);

  for (int i = 0; i < kTestHintCount; ++i) {
    optimization_guide::proto::Hint hint;
    hint.set_key(GetHostDomainOrg(i));
    hint.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
    update_data->MoveHintIntoUpdateData(std::move(hint));
  }

  UpdateComponentData(std::move(update_data));

  EXPECT_FALSE(hint_cache()->HasHint(GetHostDomainOrg(kTestHintCount)));
}

TEST_F(HintCacheTest, TestMemoryCacheLoadCallback) {
  const int kMemoryCacheSize = 5;
  CreateAndInitializeHintCache(kMemoryCacheSize);

  base::Version version("1.0.0");
  std::unique_ptr<HintCacheStore::ComponentUpdateData> update_data =
      hint_cache()->MaybeCreateComponentUpdateData(version);
  ASSERT_TRUE(update_data);

  std::string hint_key = "subdomain.domain.org";
  optimization_guide::proto::Hint hint;
  hint.set_key(hint_key);
  hint.set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  update_data->MoveHintIntoUpdateData(std::move(hint));

  UpdateComponentData(std::move(update_data));

  EXPECT_FALSE(hint_cache()->GetHintIfLoaded("host.subdomain.domain.org"));
  LoadHint("host.subdomain.domain.org");
  EXPECT_TRUE(hint_cache()->GetHintIfLoaded("host.subdomain.domain.org"));

  EXPECT_TRUE(GetLoadedHint());
  EXPECT_EQ(hint_key, GetLoadedHint()->key());
}

}  // namespace

}  // namespace previews
