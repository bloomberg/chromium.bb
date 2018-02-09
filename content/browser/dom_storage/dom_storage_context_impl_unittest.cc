// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/dom_storage_namespace.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::ASCIIToUTF16;

namespace content {
namespace {
// An empty namespace is the local storage namespace.
constexpr const char kLocalStorageNamespaceId[] = "";
}  // namespace

class DOMStorageContextImplTest : public testing::Test {
 public:
  DOMStorageContextImplTest()
      : kOrigin(GURL("http://dom_storage/")),
        kKey(ASCIIToUTF16("key")),
        kValue(ASCIIToUTF16("value")),
        kDontIncludeFileInfo(false),
        kDoIncludeFileInfo(true) {}

  const GURL kOrigin;
  const base::string16 kKey;
  const base::string16 kValue;
  const bool kDontIncludeFileInfo;
  const bool kDoIncludeFileInfo;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    storage_policy_ = new MockSpecialStoragePolicy;
    task_runner_ =
        new MockDOMStorageTaskRunner(base::ThreadTaskRunnerHandle::Get().get());
    context_ =
        new DOMStorageContextImpl(temp_dir_.GetPath(), base::FilePath(),
                                  storage_policy_.get(), task_runner_.get());
  }

  void TearDown() override {
    if (context_)
      context_->Shutdown();
    base::RunLoop().RunUntilIdle();
  }

  void VerifySingleOriginRemains(const GURL& origin) {
    // Use a new instance to examine the contexts of temp_dir_.
    scoped_refptr<DOMStorageContextImpl> context = new DOMStorageContextImpl(
        temp_dir_.GetPath(), base::FilePath(), nullptr, nullptr);
    std::vector<LocalStorageUsageInfo> infos;
    context->GetLocalStorageUsage(&infos, kDontIncludeFileInfo);
    ASSERT_EQ(1u, infos.size());
    EXPECT_EQ(origin, infos[0].origin);
    context->Shutdown();
  }

 protected:
  base::MessageLoop message_loop_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<MockSpecialStoragePolicy> storage_policy_;
  scoped_refptr<MockDOMStorageTaskRunner> task_runner_;
  scoped_refptr<DOMStorageContextImpl> context_;
  DISALLOW_COPY_AND_ASSIGN(DOMStorageContextImplTest);
};

TEST_F(DOMStorageContextImplTest, Basics) {
  // This test doesn't do much, checks that the constructor
  // initializes members properly and that invoking methods
  // on a newly created object w/o any data on disk do no harm.
  EXPECT_EQ(temp_dir_.GetPath(), context_->localstorage_directory());
  EXPECT_EQ(base::FilePath(), context_->sessionstorage_directory());
  EXPECT_EQ(storage_policy_.get(), context_->special_storage_policy_.get());
  context_->DeleteLocalStorage(GURL("http://chromium.org/"));
  EXPECT_TRUE(context_->GetStorageNamespace(kLocalStorageNamespaceId));
  std::vector<LocalStorageUsageInfo> infos;
  context_->GetLocalStorageUsage(&infos, kDontIncludeFileInfo);
  EXPECT_TRUE(infos.empty());
  context_->Shutdown();
}

TEST_F(DOMStorageContextImplTest, UsageInfo) {
  // Should be empty initially
  std::vector<LocalStorageUsageInfo> infos;
  context_->GetLocalStorageUsage(&infos, kDontIncludeFileInfo);
  EXPECT_TRUE(infos.empty());
  context_->GetLocalStorageUsage(&infos, kDoIncludeFileInfo);
  EXPECT_TRUE(infos.empty());

  // Put some data into local storage and shutdown the context
  // to ensure data is written to disk.
  base::NullableString16 old_value;
  EXPECT_TRUE(context_->GetStorageNamespace(kLocalStorageNamespaceId)
                  ->OpenStorageArea(kOrigin)
                  ->SetItem(kKey, kValue, old_value, &old_value));
  context_->Shutdown();
  context_ = nullptr;
  base::RunLoop().RunUntilIdle();

  // Create a new context that points to the same directory, see that
  // it knows about the origin that we stored data for.
  context_ = new DOMStorageContextImpl(temp_dir_.GetPath(), base::FilePath(),
                                       nullptr, nullptr);
  context_->GetLocalStorageUsage(&infos, kDontIncludeFileInfo);
  EXPECT_EQ(1u, infos.size());
  EXPECT_EQ(kOrigin, infos[0].origin);
  EXPECT_EQ(0u, infos[0].data_size);
  EXPECT_EQ(base::Time(), infos[0].last_modified);
  infos.clear();
  context_->GetLocalStorageUsage(&infos, kDoIncludeFileInfo);
  EXPECT_EQ(1u, infos.size());
  EXPECT_EQ(kOrigin, infos[0].origin);
  EXPECT_NE(0u, infos[0].data_size);
  EXPECT_NE(base::Time(), infos[0].last_modified);
}

TEST_F(DOMStorageContextImplTest, SessionOnly) {
  const GURL kSessionOnlyOrigin("http://www.sessiononly.com/");
  storage_policy_->AddSessionOnly(kSessionOnlyOrigin);

  // Store data for a normal and a session-only origin and then
  // invoke Shutdown() which should delete data for session-only
  // origins.
  base::NullableString16 old_value;
  EXPECT_TRUE(context_->GetStorageNamespace(kLocalStorageNamespaceId)
                  ->OpenStorageArea(kOrigin)
                  ->SetItem(kKey, kValue, old_value, &old_value));
  EXPECT_TRUE(context_->GetStorageNamespace(kLocalStorageNamespaceId)
                  ->OpenStorageArea(kSessionOnlyOrigin)
                  ->SetItem(kKey, kValue, old_value, &old_value));
  context_->Shutdown();
  context_ = nullptr;
  base::RunLoop().RunUntilIdle();

  // Verify that the session-only origin data is gone.
  VerifySingleOriginRemains(kOrigin);
}

TEST_F(DOMStorageContextImplTest, SetForceKeepSessionState) {
  const GURL kSessionOnlyOrigin("http://www.sessiononly.com/");
  storage_policy_->AddSessionOnly(kSessionOnlyOrigin);

  // Store data for a session-only origin, setup to save session data, then
  // shutdown.
  base::NullableString16 old_value;
  EXPECT_TRUE(context_->GetStorageNamespace(kLocalStorageNamespaceId)
                  ->OpenStorageArea(kSessionOnlyOrigin)
                  ->SetItem(kKey, kValue, old_value, &old_value));
  context_->SetForceKeepSessionState();  // Should override clear behavior.
  context_->Shutdown();
  context_ = nullptr;
  base::RunLoop().RunUntilIdle();

  VerifySingleOriginRemains(kSessionOnlyOrigin);
}

TEST_F(DOMStorageContextImplTest, PersistentIds) {
  const std::string kFirstNamespaceId = "persistent";
  context_->CreateSessionNamespace(kFirstNamespaceId);
  DOMStorageNamespace* dom_namespace =
      context_->GetStorageNamespace(kFirstNamespaceId);
  ASSERT_TRUE(dom_namespace);
  EXPECT_EQ(kFirstNamespaceId, dom_namespace->namespace_id());
  // Verify that the areas inherit the persistent ID.
  DOMStorageArea* area = dom_namespace->OpenStorageArea(kOrigin);
  EXPECT_EQ(kFirstNamespaceId, area->namespace_id_);

  // Verify that the persistent IDs are handled correctly when cloning.
  const std::string kSecondNamespaceId = "cloned";
  context_->CloneSessionNamespace(kFirstNamespaceId, kSecondNamespaceId);
  DOMStorageNamespace* cloned_dom_namespace =
      context_->GetStorageNamespace(kSecondNamespaceId);
  ASSERT_TRUE(dom_namespace);
  EXPECT_EQ(kSecondNamespaceId, cloned_dom_namespace->namespace_id());
  // Verify that the areas inherit the persistent ID.
  DOMStorageArea* cloned_area = cloned_dom_namespace->OpenStorageArea(kOrigin);
  EXPECT_EQ(kSecondNamespaceId, cloned_area->namespace_id_);
}

// Disable this test on Android as on Android we always delete our old session
// storage on startup. This is because we don't do any session restoration for
// the android system. See crbug.com/770307.
#if defined(OS_ANDROID)
TEST_F(DOMStorageContextImplTest, DISABLED_DeleteSessionStorage) {
#else
TEST_F(DOMStorageContextImplTest, DeleteSessionStorage) {
#endif
  // Create a DOMStorageContextImpl which will save sessionStorage on disk.
  context_->Shutdown();
  context_ =
      new DOMStorageContextImpl(temp_dir_.GetPath(), temp_dir_.GetPath(),
                                storage_policy_.get(), task_runner_.get());
  context_->SetSaveSessionStorageOnDisk();
  ASSERT_EQ(temp_dir_.GetPath(), context_->sessionstorage_directory());

  // Write data.
  const std::string kFirstNamespaceId = "persistent";
  context_->CreateSessionNamespace(kFirstNamespaceId);
  DOMStorageNamespace* dom_namespace =
      context_->GetStorageNamespace(kFirstNamespaceId);
  DOMStorageArea* area = dom_namespace->OpenStorageArea(kOrigin);
  const base::string16 kKey(ASCIIToUTF16("foo"));
  const base::string16 kValue(ASCIIToUTF16("bar"));
  base::NullableString16 old_nullable_value;
  area->SetItem(kKey, kValue, old_nullable_value, &old_nullable_value);
  dom_namespace->CloseStorageArea(area);

  // Destroy and recreate the DOMStorageContextImpl.
  context_->Shutdown();
  context_ = nullptr;
  base::RunLoop().RunUntilIdle();
  context_ =
      new DOMStorageContextImpl(temp_dir_.GetPath(), temp_dir_.GetPath(),
                                storage_policy_.get(), task_runner_.get());
  context_->SetSaveSessionStorageOnDisk();

  // Read the data back.
  context_->CreateSessionNamespace(kFirstNamespaceId);
  dom_namespace = context_->GetStorageNamespace(kFirstNamespaceId);
  area = dom_namespace->OpenStorageArea(kOrigin);
  base::NullableString16 read_value;
  EXPECT_EQ(kKey, area->Key(0).string());
  dom_namespace->CloseStorageArea(area);

  SessionStorageUsageInfo info;
  info.origin = kOrigin;
  info.namespace_id = kFirstNamespaceId;
  context_->DeleteSessionStorage(info);

  // Destroy and recreate again.
  context_->Shutdown();
  context_ = nullptr;
  base::RunLoop().RunUntilIdle();
  context_ =
      new DOMStorageContextImpl(temp_dir_.GetPath(), temp_dir_.GetPath(),
                                storage_policy_.get(), task_runner_.get());
  context_->SetSaveSessionStorageOnDisk();

  // Now there should be no data.
  context_->CreateSessionNamespace(kFirstNamespaceId);
  dom_namespace = context_->GetStorageNamespace(kFirstNamespaceId);
  area = dom_namespace->OpenStorageArea(kOrigin);

  EXPECT_EQ(0u, area->Length());
  dom_namespace->CloseStorageArea(area);
  context_->Shutdown();
  context_ = nullptr;
  base::RunLoop().RunUntilIdle();
}

TEST_F(DOMStorageContextImplTest, PurgeMemory) {
  auto* dom_namespace = context_->GetStorageNamespace(kLocalStorageNamespaceId);
  auto* area1 = dom_namespace->OpenStorageArea(kOrigin);
  base::NullableString16 old_value;
  area1->SetItem(kKey, kValue, old_value, &old_value);

  // PURGE_UNOPENED does not delete the open area.
  context_->PurgeMemory(DOMStorageContextImpl::PURGE_UNOPENED);
  EXPECT_EQ(1u, dom_namespace->GetUsageStatistics().total_area_count);
  EXPECT_EQ(0u, dom_namespace->GetUsageStatistics().inactive_area_count);

  // PURGE_UNOPENED deletes the unopened area.
  dom_namespace->CloseStorageArea(area1);
  EXPECT_EQ(1u, dom_namespace->GetUsageStatistics().inactive_area_count);
  area1->CommitChanges(area1->GetCurrentCommitBatch()->batch.get());
  area1->commit_batches_.clear();
  context_->PurgeMemory(DOMStorageContextImpl::PURGE_UNOPENED);
  EXPECT_EQ(0u, dom_namespace->GetUsageStatistics().total_area_count);

  // Add an item to the database and commit changes, and keep it open. So, cache
  // is kept alive.
  auto* area2 = dom_namespace->OpenStorageArea(kOrigin);
  area2->SetItem(kKey, kValue, old_value, &old_value);
  area2->CommitChanges(area2->GetCurrentCommitBatch()->batch.get());
  area2->commit_batches_.clear();

  // PURGE_AGGRESSIVE clears the cache in the open area.
  EXPECT_NE(0u, dom_namespace->GetUsageStatistics().total_cache_size);
  context_->PurgeMemory(DOMStorageContextImpl::PURGE_AGGRESSIVE);
  EXPECT_EQ(0u, dom_namespace->GetUsageStatistics().total_cache_size);
}

}  // namespace content
