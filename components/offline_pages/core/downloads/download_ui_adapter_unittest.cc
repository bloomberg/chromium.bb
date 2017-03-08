// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/downloads/download_ui_adapter.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/core/background/offliner_stub.h"
#include "components/offline_pages/core/background/request_coordinator_stub_taco.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
// Constants for a test OfflinePageItem.
static const int kTestOfflineId1 = 1;
static const int kTestOfflineId2 = 2;
static const char kTestUrl[] = "http://foo.com/bar.mhtml";
static const char kTestGuid1[] = "cccccccc-cccc-4ccc-0ccc-ccccccccccc1";
static const char kTestGuid2[] = "cccccccc-cccc-4ccc-0ccc-ccccccccccc2";
static const char kTestBadGuid[] = "ccccccc-cccc-0ccc-0ccc-ccccccccccc0";
static const ClientId kTestClientIdOtherNamespace(kLastNNamespace, kTestGuid1);
static const ClientId kTestClientIdOtherGuid(kLastNNamespace, kTestBadGuid);
static const ClientId kTestClientId1(kAsyncNamespace, kTestGuid1);
static const ClientId kTestClientId2(kAsyncNamespace, kTestGuid2);
static const base::FilePath kTestFilePath =
    base::FilePath(FILE_PATH_LITERAL("foo/bar.mhtml"));
static const int kFileSize = 1000;
static const base::Time kTestCreationTime = base::Time::Now();
static const base::string16 kTestTitle = base::ASCIIToUTF16("test title");
}  // namespace

// Mock DownloadUIAdapter::Delegate
class DownloadUIAdapterDelegate : public DownloadUIAdapter::Delegate {
 public:
  DownloadUIAdapterDelegate() {}
  ~DownloadUIAdapterDelegate() override {}

  // DownloadUIAdapter::Delegate
  bool IsVisibleInUI(const ClientId& client_id) override { return is_visible; }
  bool IsTemporarilyHiddenInUI(const ClientId& client_id) override {
    return is_temporarily_hidden;
  }
  void SetUIAdapter(DownloadUIAdapter* ui_adapter) override{};

  bool is_visible = true;
  bool is_temporarily_hidden = false;
};

// Mock OfflinePageModel for testing the SavePage calls.
class MockOfflinePageModel : public StubOfflinePageModel {
 public:
  MockOfflinePageModel(base::TestMockTimeTaskRunner* task_runner)
      : observer_(nullptr),
        task_runner_(task_runner),
        policy_controller_(new ClientPolicyController()) {}

  ~MockOfflinePageModel() override {}

  void AddInitialPage() {
    OfflinePageItem page(GURL(kTestUrl), kTestOfflineId1, kTestClientId1,
                         kTestFilePath, kFileSize, kTestCreationTime);
    page.title = kTestTitle;
    pages[kTestOfflineId1] = page;
  }

  // OfflinePageModel overrides.
  void AddObserver(Observer* observer) override {
    EXPECT_TRUE(observer != nullptr);
    observer_ = observer;
  }

  void RemoveObserver(Observer* observer) override {
    EXPECT_TRUE(observer != nullptr);
    EXPECT_EQ(observer, observer_);
    observer_ = nullptr;
  }

  // PostTask instead of just running callback to simpulate the real class.
  void GetAllPages(const MultipleOfflinePageItemCallback& callback) override {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&MockOfflinePageModel::GetAllPagesImpl,
                                      base::Unretained(this), callback));
  }

  void GetAllPagesImpl(const MultipleOfflinePageItemCallback& callback) {
    std::vector<OfflinePageItem> result;
    for (const auto& page : pages)
      result.push_back(page.second);
    callback.Run(result);
  }

  void DeletePageAndNotifyAdapter(const std::string& guid) {
    for (const auto& page : pages) {
      if (page.second.client_id.id == guid) {
        observer_->OfflinePageDeleted(page.second.offline_id,
                                      page.second.client_id);
        pages.erase(page.first);
        return;
      }
    }
  }

  void AddPageAndNotifyAdapter(const OfflinePageItem& page) {
    EXPECT_EQ(pages.end(), pages.find(page.offline_id));
    pages[page.offline_id] = page;
    observer_->OfflinePageAdded(this, page);
  }

  ClientPolicyController* GetPolicyController() override {
    return policy_controller_.get();
  }

  std::map<int64_t, OfflinePageItem> pages;

 private:
  OfflinePageModel::Observer* observer_;
  base::TestMockTimeTaskRunner* task_runner_;
  // Normally owned by OfflinePageModel.
  std::unique_ptr<ClientPolicyController> policy_controller_;

  DISALLOW_COPY_AND_ASSIGN(MockOfflinePageModel);
};

// Creates mock versions for OfflinePageModel, RequestCoordinator and their
// dependencies, then passes them to DownloadUIAdapter for testing.
// Note that initially the OfflienPageModel is not "loaded". PumpLoop() will
// load it, firing ItemsLoaded callback to the Adapter. Hence some tests
// start from PumpLoop() right away if they don't need to test this.
class DownloadUIAdapterTest : public testing::Test,
                              public DownloadUIAdapter::Observer {
 public:
  DownloadUIAdapterTest();
  ~DownloadUIAdapterTest() override;

  // testing::Test
  void SetUp() override;

  // DownloadUIAdapter::Observer
  void ItemsLoaded() override;
  void ItemAdded(const DownloadUIItem& item) override;
  void ItemUpdated(const DownloadUIItem& item) override;
  void ItemDeleted(const std::string& guid) override;

  // Runs until all of the tasks that are not delayed are gone from the task
  // queue.
  void PumpLoop();

  int64_t AddRequest(const GURL& url, const ClientId& client_id);

  RequestCoordinator* request_coordinator() {
    return request_coordinator_taco_->request_coordinator();
  }

  bool items_loaded;
  std::vector<std::string> added_guids, updated_guids, deleted_guids;
  int64_t download_progress_bytes;
  std::unique_ptr<MockOfflinePageModel> model;
  DownloadUIAdapterDelegate* adapter_delegate;
  std::unique_ptr<DownloadUIAdapter> adapter;
  OfflinerStub* offliner_stub;

 private:
  std::unique_ptr<RequestCoordinatorStubTaco> request_coordinator_taco_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

DownloadUIAdapterTest::DownloadUIAdapterTest()
    : items_loaded(false),
      task_runner_(new base::TestMockTimeTaskRunner),
      task_runner_handle_(task_runner_) {}

DownloadUIAdapterTest::~DownloadUIAdapterTest() {}

void DownloadUIAdapterTest::SetUp() {
  model = base::MakeUnique<MockOfflinePageModel>(task_runner_.get());
  std::unique_ptr<DownloadUIAdapterDelegate> delegate =
      base::MakeUnique<DownloadUIAdapterDelegate>();
  adapter_delegate = delegate.get();
  request_coordinator_taco_ = base::MakeUnique<RequestCoordinatorStubTaco>();

  std::unique_ptr<OfflinerStub> offliner = base::MakeUnique<OfflinerStub>();
  offliner_stub = offliner.get();
  request_coordinator_taco_->SetOffliner(std::move(offliner));

  request_coordinator_taco_->CreateRequestCoordinator();
  adapter = base::MakeUnique<DownloadUIAdapter>(
      model.get(), request_coordinator_taco_->request_coordinator(),
      std::move(delegate));

  adapter->AddObserver(this);
}

void DownloadUIAdapterTest::ItemsLoaded() {
  items_loaded = true;
}

void DownloadUIAdapterTest::ItemAdded(const DownloadUIItem& item) {
  added_guids.push_back(item.guid);
}

void DownloadUIAdapterTest::ItemUpdated(const DownloadUIItem& item) {
  updated_guids.push_back(item.guid);
  download_progress_bytes += item.download_progress_bytes;
}

void DownloadUIAdapterTest::ItemDeleted(const std::string& guid) {
  deleted_guids.push_back(guid);
}

void DownloadUIAdapterTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

int64_t DownloadUIAdapterTest::AddRequest(const GURL& url,
                                          const ClientId& client_id) {
  RequestCoordinator::SavePageLaterParams params;
  params.url = url;
  params.client_id = client_id;
  return request_coordinator()->SavePageLater(params);
}

TEST_F(DownloadUIAdapterTest, InitialLoad) {
  EXPECT_NE(nullptr, adapter.get());
  model->AddInitialPage();
  EXPECT_FALSE(items_loaded);
  PumpLoop();
  EXPECT_TRUE(items_loaded);
  const DownloadUIItem* item = adapter->GetItem(kTestGuid1);
  EXPECT_NE(nullptr, item);
}

TEST_F(DownloadUIAdapterTest, InitialItemConversion) {
  model->AddInitialPage();
  EXPECT_EQ(1UL, model->pages.size());
  EXPECT_EQ(kTestGuid1, model->pages[kTestOfflineId1].client_id.id);
  PumpLoop();
  const DownloadUIItem* item = adapter->GetItem(kTestGuid1);
  EXPECT_EQ(kTestGuid1, item->guid);
  EXPECT_EQ(kTestUrl, item->url.spec());
  EXPECT_EQ(DownloadUIItem::DownloadState::COMPLETE, item->download_state);
  EXPECT_EQ(0, item->download_progress_bytes);
  EXPECT_EQ(kTestFilePath, item->target_path);
  EXPECT_EQ(kTestCreationTime, item->start_time);
  EXPECT_EQ(kFileSize, item->total_bytes);
  EXPECT_EQ(kTestTitle, item->title);
}

TEST_F(DownloadUIAdapterTest, ItemDeletedAdded) {
  model->AddInitialPage();
  PumpLoop();
  // Add page, notify adapter.
  OfflinePageItem page(GURL(kTestUrl), kTestOfflineId2, kTestClientId2,
                       base::FilePath(kTestFilePath), kFileSize,
                       kTestCreationTime);
  model->AddPageAndNotifyAdapter(page);
  PumpLoop();
  EXPECT_EQ(1UL, added_guids.size());
  EXPECT_EQ(kTestGuid2, added_guids[0]);
  // Remove pages, notify adapter.
  model->DeletePageAndNotifyAdapter(kTestGuid1);
  model->DeletePageAndNotifyAdapter(kTestGuid2);
  PumpLoop();
  EXPECT_EQ(2UL, deleted_guids.size());
  EXPECT_EQ(kTestGuid1, deleted_guids[0]);
  EXPECT_EQ(kTestGuid2, deleted_guids[1]);
}

TEST_F(DownloadUIAdapterTest, NotVisibleItem) {
  model->AddInitialPage();
  PumpLoop();
  adapter_delegate->is_visible = false;
  OfflinePageItem page1(
      GURL(kTestUrl), kTestOfflineId2, kTestClientIdOtherNamespace,
      base::FilePath(kTestFilePath), kFileSize, kTestCreationTime);
  model->AddPageAndNotifyAdapter(page1);
  PumpLoop();
  // Should not add the page.
  EXPECT_EQ(0UL, added_guids.size());
}

TEST_F(DownloadUIAdapterTest, TemporarilyNotVisibleItem) {
  adapter_delegate->is_temporarily_hidden = true;
  model->AddInitialPage();
  PumpLoop();
  // Initial Item should be invisible in the collection now.
  EXPECT_EQ(nullptr, adapter->GetItem(kTestGuid1));
  EXPECT_EQ(0UL, adapter->GetAllItems().size());
  EXPECT_EQ(0UL, added_guids.size());
  EXPECT_EQ(0UL, deleted_guids.size());

  adapter_delegate->is_temporarily_hidden = false;
  // Notify adapter about visibility change for the clientId of initial page.
  adapter->TemporaryHiddenStatusChanged(kTestClientId1);
  PumpLoop();

  // There should be OnAdded simulated.
  EXPECT_EQ(1UL, added_guids.size());
  EXPECT_EQ(0UL, deleted_guids.size());
  // Also the item should be visible in the collection of items now.
  EXPECT_NE(nullptr, adapter->GetItem(kTestGuid1));
  EXPECT_EQ(1UL, adapter->GetAllItems().size());

  // Switch visibility back to hidden
  adapter_delegate->is_temporarily_hidden = true;
  adapter->TemporaryHiddenStatusChanged(kTestClientId1);
  // There should be OnDeleted fired.
  EXPECT_EQ(1UL, added_guids.size());
  EXPECT_EQ(1UL, deleted_guids.size());
  // Also the item should be visible in the collection of items now.
  EXPECT_EQ(nullptr, adapter->GetItem(kTestGuid1));
  EXPECT_EQ(0UL, adapter->GetAllItems().size());
}

TEST_F(DownloadUIAdapterTest, ItemAdded) {
  model->AddInitialPage();
  PumpLoop();
  // Clear the initial page and replace it with updated one.
  model->pages.clear();
  // Add a new page which did not exist before.
  OfflinePageItem page2(GURL(kTestUrl), kTestOfflineId2, kTestClientId2,
                        base::FilePath(kTestFilePath), kFileSize,
                        kTestCreationTime);
  model->AddPageAndNotifyAdapter(page2);
  PumpLoop();
  EXPECT_EQ(1UL, added_guids.size());
  EXPECT_EQ(kTestGuid2, added_guids[0]);
  // TODO(dimich): we currently don't report updated items since OPM doesn't
  // have support for that. Add as needed, this will have to be updated when
  // support is added.
  EXPECT_EQ(0UL, updated_guids.size());
}

TEST_F(DownloadUIAdapterTest, NoHangingLoad) {
  model->AddInitialPage();
  EXPECT_NE(nullptr, adapter.get());
  EXPECT_FALSE(items_loaded);
  // Removal of last observer causes cache unload of not-yet-loaded cache.
  adapter->RemoveObserver(this);
  // This will complete async fetch of items, but...
  PumpLoop();
  // items should not be loaded when there is no observers!
  EXPECT_FALSE(items_loaded);
  // This should not crash.
  adapter->AddObserver(this);
}

TEST_F(DownloadUIAdapterTest, LoadExistingRequest) {
  AddRequest(GURL(kTestUrl), kTestClientId1);
  PumpLoop();
  EXPECT_TRUE(items_loaded);
  const DownloadUIItem* item = adapter->GetItem(kTestGuid1);
  EXPECT_NE(nullptr, item);
}

TEST_F(DownloadUIAdapterTest, AddRequest) {
  PumpLoop();
  EXPECT_TRUE(items_loaded);
  EXPECT_EQ(0UL, added_guids.size());
  AddRequest(GURL(kTestUrl), kTestClientId1);
  PumpLoop();
  EXPECT_EQ(1UL, added_guids.size());
  EXPECT_EQ(kTestClientId1.id, added_guids[0]);
  const DownloadUIItem* item = adapter->GetItem(kTestGuid1);
  EXPECT_NE(nullptr, item);
}

TEST_F(DownloadUIAdapterTest, RemoveRequest) {
  int64_t id = AddRequest(GURL(kTestUrl), kTestClientId1);
  PumpLoop();
  // No added requests, the initial one is loaded.
  EXPECT_EQ(0UL, added_guids.size());
  EXPECT_NE(nullptr, adapter->GetItem(kTestGuid1));
  EXPECT_EQ(0UL, deleted_guids.size());

  std::vector<int64_t> requests_to_remove = {id};
  request_coordinator()->RemoveRequests(
      requests_to_remove,
      base::Bind(
          [](int64_t id, const MultipleItemStatuses& statuses) {
            EXPECT_EQ(1UL, statuses.size());
            EXPECT_EQ(id, statuses[0].first);
            EXPECT_EQ(ItemActionStatus::SUCCESS, statuses[0].second);
          },
          id));
  PumpLoop();

  EXPECT_EQ(0UL, added_guids.size());
  EXPECT_EQ(1UL, deleted_guids.size());
  EXPECT_EQ(kTestClientId1.id, deleted_guids[0]);
  EXPECT_EQ(nullptr, adapter->GetItem(kTestGuid1));
}

TEST_F(DownloadUIAdapterTest, RequestBecomesPage) {
  // This will cause requests to be 'offlined' all the way and removed.
  offliner_stub->enable_callback(true);
  AddRequest(GURL(kTestUrl), kTestClientId1);
  PumpLoop();

  const DownloadUIItem* item = adapter->GetItem(kTestGuid1);
  EXPECT_NE(nullptr, item);
  // The item is still IN_PROGRESS, since we did not delete it when
  // request is competed successfully, waiting for the page with the
  // same client_id to come in.
  EXPECT_EQ(DownloadUIItem::DownloadState::IN_PROGRESS, item->download_state);
  // Add a new saved page with the same client id.
  // This simulates what happens when the request is completed.
  // It should not fire and OnAdded or OnDeleted, just OnUpdated.
  OfflinePageItem page(GURL(kTestUrl), kTestOfflineId1, kTestClientId1,
                       base::FilePath(kTestFilePath), kFileSize,
                       kTestCreationTime);
  model->AddPageAndNotifyAdapter(page);
  PumpLoop();

  // No added or deleted items, the one existing item should be 'updated'.
  EXPECT_EQ(0UL, added_guids.size());
  EXPECT_EQ(0UL, deleted_guids.size());

  EXPECT_GE(updated_guids.size(), 1UL);
  std::string last_updated_guid = updated_guids[updated_guids.size() - 1];
  item = adapter->GetItem(last_updated_guid);
  EXPECT_NE(nullptr, item);
  EXPECT_EQ(DownloadUIItem::DownloadState::COMPLETE, item->download_state);
}

TEST_F(DownloadUIAdapterTest, RemoveObserversWhenClearingCache) {
  PumpLoop();
  EXPECT_TRUE(items_loaded);

  // Remove this from the adapter's observer list.  This should cause the cache
  // to be cleared.
  adapter->RemoveObserver(this);
  items_loaded = false;

  PumpLoop();

  adapter->AddObserver(this);
  PumpLoop();
  EXPECT_TRUE(items_loaded);
}

TEST_F(DownloadUIAdapterTest, UpdateProgress) {
  offliner_stub->enable_callback(true);
  AddRequest(GURL(kTestUrl), kTestClientId1);
  PumpLoop();

  const DownloadUIItem* item = adapter->GetItem(kTestGuid1);

  ASSERT_NE(nullptr, item);
  EXPECT_GT(item->download_progress_bytes, 0LL);
  // Updated 2 times - with progress and to 'completed'.
  EXPECT_EQ(2UL, updated_guids.size());
  EXPECT_EQ(kTestGuid1, updated_guids[0]);
  EXPECT_EQ(kTestGuid1, updated_guids[1]);
}

}  // namespace offline_pages
