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
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_pages/core/background/offliner_stub.h"
#include "components/offline_pages/core/background/request_coordinator_stub_taco.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/downloads/offline_item_conversions.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "testing/gtest/include/gtest/gtest.h"

using offline_items_collection::OfflineItemState;

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
static const ContentId kTestContentId1(kOfflinePageNamespace, kTestGuid1);
static const base::FilePath kTestFilePath =
    base::FilePath(FILE_PATH_LITERAL("foo/bar.mhtml"));
static const int kFileSize = 1000;
static const base::Time kTestCreationTime = base::Time::Now();
static const base::string16 kTestTitle = base::ASCIIToUTF16("test title");

void GetItemAndVerify(const base::Optional<OfflineItem>& expected,
                      const base::Optional<OfflineItem>& actual) {
  EXPECT_EQ(expected.has_value(), actual.has_value());
  if (!expected.has_value() || !actual.has_value())
    return;

  EXPECT_EQ(expected.value().id, actual.value().id);
  EXPECT_EQ(expected.value().state, actual.value().state);
}

void GetAllItemsAndVerify(size_t expected_size,
                          const std::vector<OfflineItem>& actual) {
  EXPECT_EQ(expected_size, actual.size());
}

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
  void SetUIAdapter(DownloadUIAdapter* ui_adapter) override {}
  void OpenItem(const OfflineItem& item, int64_t offline_id) override {}

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
        DeletedPageInfo info(page.second.offline_id, page.second.client_id,
                             page.second.request_origin);
        observer_->OfflinePageDeleted(info);
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
                              public OfflineContentProvider::Observer {
 public:
  DownloadUIAdapterTest();
  ~DownloadUIAdapterTest() override;

  // testing::Test
  void SetUp() override;

  // DownloadUIAdapter::Observer
  void OnItemsAvailable(OfflineContentProvider* provider) override;
  void OnItemsAdded(const std::vector<OfflineItem>& items) override;
  void OnItemUpdated(const OfflineItem& item) override;
  void OnItemRemoved(const ContentId& id) override;

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
  model = std::make_unique<MockOfflinePageModel>(task_runner_.get());
  std::unique_ptr<DownloadUIAdapterDelegate> delegate =
      std::make_unique<DownloadUIAdapterDelegate>();
  adapter_delegate = delegate.get();
  request_coordinator_taco_ = std::make_unique<RequestCoordinatorStubTaco>();

  std::unique_ptr<OfflinerStub> offliner = std::make_unique<OfflinerStub>();
  offliner_stub = offliner.get();
  request_coordinator_taco_->SetOffliner(std::move(offliner));

  request_coordinator_taco_->CreateRequestCoordinator();
  adapter = std::make_unique<DownloadUIAdapter>(
      nullptr, model.get(), request_coordinator_taco_->request_coordinator(),
      std::move(delegate));

  adapter->AddObserver(this);
}

void DownloadUIAdapterTest::OnItemsAvailable(OfflineContentProvider* provider) {
  items_loaded = true;
}

void DownloadUIAdapterTest::OnItemsAdded(
    const std::vector<OfflineItem>& items) {
  for (const OfflineItem& item : items) {
    added_guids.push_back(item.id.id);
  }
}

void DownloadUIAdapterTest::OnItemUpdated(const OfflineItem& item) {
  updated_guids.push_back(item.id.id);
  download_progress_bytes += item.received_bytes;
}

void DownloadUIAdapterTest::OnItemRemoved(const ContentId& id) {
  deleted_guids.push_back(id.id);
}

void DownloadUIAdapterTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void SavePageLaterCallback(AddRequestResult ignored) {}

int64_t DownloadUIAdapterTest::AddRequest(const GURL& url,
                                          const ClientId& client_id) {
  RequestCoordinator::SavePageLaterParams params;
  params.url = url;
  params.client_id = client_id;
  return request_coordinator()->SavePageLater(
      params, base::Bind(&SavePageLaterCallback));
}

TEST_F(DownloadUIAdapterTest, InitialLoad) {
  EXPECT_NE(nullptr, adapter.get());
  model->AddInitialPage();
  EXPECT_FALSE(items_loaded);
  PumpLoop();
  EXPECT_TRUE(items_loaded);
  OfflineItem item(kTestContentId1);
  adapter->GetItemById(kTestContentId1,
                       base::BindOnce(&GetItemAndVerify, item));
  PumpLoop();
}

TEST_F(DownloadUIAdapterTest, InitialItemConversion) {
  model->AddInitialPage();
  EXPECT_EQ(1UL, model->pages.size());
  EXPECT_EQ(kTestGuid1, model->pages[kTestOfflineId1].client_id.id);
  PumpLoop();

  auto callback = [](const base::Optional<OfflineItem>& item) {
    EXPECT_EQ(kTestGuid1, item.value().id.id);
    EXPECT_EQ(kTestUrl, item.value().page_url.spec());
    EXPECT_EQ(OfflineItemState::COMPLETE, item.value().state);
    EXPECT_EQ(0, item.value().received_bytes);
    EXPECT_EQ(kTestFilePath, item.value().file_path);
    EXPECT_EQ(kTestCreationTime, item.value().creation_time);
    EXPECT_EQ(kFileSize, item.value().total_size_bytes);
    EXPECT_EQ(kTestTitle, base::ASCIIToUTF16(item.value().title));
  };

  adapter->GetItemById(kTestContentId1, base::BindOnce(callback));
  PumpLoop();
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
  adapter->GetItemById(kTestContentId1,
                       base::BindOnce(&GetItemAndVerify, base::nullopt));
  adapter->GetAllItems(base::BindOnce(&GetAllItemsAndVerify, 0UL));
  PumpLoop();

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
  OfflineItem item(kTestContentId1);
  adapter->GetItemById(kTestContentId1,
                       base::BindOnce(&GetItemAndVerify, item));
  adapter->GetAllItems(base::BindOnce(&GetAllItemsAndVerify, 1UL));
  PumpLoop();

  // Switch visibility back to hidden
  adapter_delegate->is_temporarily_hidden = true;
  adapter->TemporaryHiddenStatusChanged(kTestClientId1);
  // There should be OnDeleted fired.
  EXPECT_EQ(1UL, added_guids.size());
  EXPECT_EQ(1UL, deleted_guids.size());
  // Also the item should be visible in the collection of items now.
  adapter->GetItemById(kTestContentId1,
                       base::BindOnce(&GetItemAndVerify, base::nullopt));
  adapter->GetAllItems(base::BindOnce(&GetAllItemsAndVerify, 0UL));
  PumpLoop();
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
  OfflineItem item(kTestContentId1);
  item.state = OfflineItemState::IN_PROGRESS;
  adapter->GetItemById(kTestContentId1,
                       base::BindOnce(&GetItemAndVerify, item));
  PumpLoop();
}

TEST_F(DownloadUIAdapterTest, AddRequest) {
  PumpLoop();
  EXPECT_TRUE(items_loaded);
  EXPECT_EQ(0UL, added_guids.size());
  AddRequest(GURL(kTestUrl), kTestClientId1);
  PumpLoop();
  EXPECT_EQ(1UL, added_guids.size());
  EXPECT_EQ(kTestClientId1.id, added_guids[0]);
  OfflineItem item(kTestContentId1);
  item.state = OfflineItemState::IN_PROGRESS;
  adapter->GetItemById(kTestContentId1,
                       base::BindOnce(&GetItemAndVerify, item));
  PumpLoop();
}

TEST_F(DownloadUIAdapterTest, RemoveRequest) {
  int64_t id = AddRequest(GURL(kTestUrl), kTestClientId1);
  PumpLoop();
  // No added requests, the initial one is loaded.
  EXPECT_EQ(0UL, added_guids.size());
  OfflineItem item(kTestContentId1);
  item.state = OfflineItemState::IN_PROGRESS;
  adapter->GetItemById(kTestContentId1,
                       base::BindOnce(&GetItemAndVerify, item));
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
  adapter->GetItemById(kTestContentId1,
                       base::BindOnce(&GetItemAndVerify, base::nullopt));
  PumpLoop();
}

TEST_F(DownloadUIAdapterTest, PauseAndResume) {
  PumpLoop();
  EXPECT_TRUE(items_loaded);

  AddRequest(GURL(kTestUrl), kTestClientId1);
  PumpLoop();

  size_t num_updates = updated_guids.size();
  OfflineItem item(kTestContentId1);
  item.state = OfflineItemState::IN_PROGRESS;
  adapter->GetItemById(kTestContentId1,
                       base::BindOnce(&GetItemAndVerify, item));

  // Pause the download. It should fire OnChanged and the item should move to
  // PAUSED.
  adapter->PauseDownload(kTestContentId1);
  PumpLoop();

  EXPECT_GE(updated_guids.size(), num_updates);
  num_updates = updated_guids.size();
  item.state = OfflineItemState::PAUSED;
  adapter->GetItemById(kTestContentId1,
                       base::BindOnce(&GetItemAndVerify, item));

  // Resume the download. It should fire OnChanged again and move the item to
  // IN_PROGRESS.
  adapter->ResumeDownload(kTestContentId1, true);
  PumpLoop();

  EXPECT_GE(updated_guids.size(), num_updates);
  item.state = OfflineItemState::IN_PROGRESS;
  adapter->GetItemById(kTestContentId1,
                       base::BindOnce(&GetItemAndVerify, item));
  PumpLoop();
}

TEST_F(DownloadUIAdapterTest, OnChangedReceivedAfterPageAdded) {
  AddRequest(GURL(kTestUrl), kTestClientId1);
  PumpLoop();
  OfflineItem item(kTestContentId1);
  item.state = OfflineItemState::IN_PROGRESS;
  adapter->GetItemById(kTestContentId1,
                       base::BindOnce(&GetItemAndVerify, item));
  PumpLoop();

  // Add a new saved page with the same client id.
  // This simulates what happens when the request is completed.
  OfflinePageItem page(GURL(kTestUrl), kTestOfflineId1, kTestClientId1,
                       base::FilePath(kTestFilePath), kFileSize,
                       kTestCreationTime);
  model->AddPageAndNotifyAdapter(page);
  PumpLoop();

  item.state = OfflineItemState::COMPLETE;
  adapter->GetItemById(kTestContentId1,
                       base::BindOnce(&GetItemAndVerify, item));

  // Pause the request. It should fire OnChanged, but should not have any effect
  // as the item is already COMPLETE.
  adapter->PauseDownload(kTestContentId1);
  PumpLoop();

  item.state = OfflineItemState::COMPLETE;
  adapter->GetItemById(kTestContentId1,
                       base::BindOnce(&GetItemAndVerify, item));
  PumpLoop();
}

TEST_F(DownloadUIAdapterTest, RequestBecomesPage) {
  // This will cause requests to be 'offlined' all the way and removed.
  offliner_stub->enable_callback(true);
  AddRequest(GURL(kTestUrl), kTestClientId1);
  PumpLoop();

  OfflineItem item(kTestContentId1);

  // The item is still IN_PROGRESS, since we did not delete it when
  // request is competed successfully, waiting for the page with the
  // same client_id to come in.
  item.state = OfflineItemState::IN_PROGRESS;
  adapter->GetItemById(kTestContentId1,
                       base::BindOnce(&GetItemAndVerify, item));
  PumpLoop();

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
  item.id = ContentId(kOfflinePageNamespace, last_updated_guid);
  item.state = OfflineItemState::COMPLETE;
  adapter->GetItemById(kTestContentId1,
                       base::BindOnce(&GetItemAndVerify, item));
  PumpLoop();
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

  auto callback = [](const base::Optional<OfflineItem>& item) {
    ASSERT_TRUE(item.has_value());
    EXPECT_GT(item.value().received_bytes, 0LL);
  };
  adapter->GetItemById(kTestContentId1, base::BindOnce(callback));

  // Updated 2 times - with progress and to 'completed'.
  EXPECT_EQ(2UL, updated_guids.size());
  EXPECT_EQ(kTestGuid1, updated_guids[0]);
  EXPECT_EQ(kTestGuid1, updated_guids[1]);
  PumpLoop();
}

}  // namespace offline_pages
