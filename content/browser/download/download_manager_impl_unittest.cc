// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_manager_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_file_factory.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_item_factory.h"
#include "components/download/public/common/download_item_impl.h"
#include "components/download/public/common/download_item_impl_delegate.h"
#include "components/download/public/common/mock_download_file.h"
#include "components/download/public/common/mock_download_item_impl.h"
#include "components/download/public/common/mock_input_stream.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Ref;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::ReturnRefOfCopy;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

namespace content {

namespace {

const char kGuid[] = "8DF158E8-C980-4618-BB03-EBA3242EB48B";

bool URLAlwaysSafe(int render_process_id, const GURL& url) {
  return true;
}

class MockDownloadManagerDelegate : public DownloadManagerDelegate {
 public:
  MockDownloadManagerDelegate();
  ~MockDownloadManagerDelegate() override;

  MOCK_METHOD0(Shutdown, void());
  void GetNextId(DownloadIdCallback cb) override { GetNextId_(cb); }
  MOCK_METHOD1(GetNextId_, void(DownloadIdCallback&));
  MOCK_METHOD2(DetermineDownloadTarget,
               bool(download::DownloadItem*, DownloadTargetCallback*));
  MOCK_METHOD1(ShouldOpenFileBasedOnExtension, bool(const base::FilePath&));
  bool ShouldCompleteDownload(download::DownloadItem* item,
                              base::OnceClosure cb) override {
    return ShouldCompleteDownload_(item, cb);
  }
  MOCK_METHOD2(ShouldCompleteDownload_,
               bool(download::DownloadItem*, base::OnceClosure&));
  bool ShouldOpenDownload(download::DownloadItem* item,
                          DownloadOpenDelayedCallback cb) override {
    return ShouldOpenDownload_(item, cb);
  }
  MOCK_METHOD2(ShouldOpenDownload_,
               bool(download::DownloadItem*, DownloadOpenDelayedCallback&));
  MOCK_METHOD3(GetSaveDir,
               void(BrowserContext*, base::FilePath*, base::FilePath*));
  MOCK_METHOD5(ChooseSavePath,
               void(WebContents*,
                    const base::FilePath&,
                    const base::FilePath::StringType&,
                    bool,
                    SavePackagePathPickedCallback));
  MOCK_METHOD0(ApplicationClientIdForFileScanning, std::string());
};

MockDownloadManagerDelegate::MockDownloadManagerDelegate() {}

MockDownloadManagerDelegate::~MockDownloadManagerDelegate() {}

class MockDownloadItemFactory
    : public download::DownloadItemFactory,
      public base::SupportsWeakPtr<MockDownloadItemFactory> {
 public:
  MockDownloadItemFactory();
  ~MockDownloadItemFactory() override;

  // Access to map of created items.
  // TODO(rdsmith): Could add type (save page, persisted, etc.)
  // functionality if it's ever needed by consumers.

  // Returns NULL if no item of that id is present.
  download::MockDownloadItemImpl* GetItem(int id);

  // Remove and return an item made by the factory.
  // Generally used during teardown.
  download::MockDownloadItemImpl* PopItem();

  // Should be called when the item of this id is removed so that
  // we don't keep dangling pointers.
  void RemoveItem(int id);

  // Overridden methods from DownloadItemFactory.
  download::DownloadItemImpl* CreatePersistedItem(
      download::DownloadItemImplDelegate* delegate,
      const std::string& guid,
      uint32_t download_id,
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer_url,
      const GURL& site_url,
      const GURL& tab_url,
      const GURL& tab_referrer_url,
      const base::Optional<url::Origin>& request_initiator,
      const std::string& mime_type,
      const std::string& original_mime_type,
      base::Time start_time,
      base::Time end_time,
      const std::string& etag,
      const std::string& last_modofied,
      int64_t received_bytes,
      int64_t total_bytes,
      const std::string& hash,
      download::DownloadItem::DownloadState state,
      download::DownloadDangerType danger_type,
      download::DownloadInterruptReason interrupt_reason,
      bool opened,
      base::Time last_access_time,
      bool transient,
      const std::vector<download::DownloadItem::ReceivedSlice>& received_slices)
      override;
  download::DownloadItemImpl* CreateActiveItem(
      download::DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const download::DownloadCreateInfo& info) override;
  download::DownloadItemImpl* CreateSavePageItem(
      download::DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const base::FilePath& path,
      const GURL& url,
      const std::string& mime_type,
      download::DownloadJob::CancelRequestCallback cancel_request_callback)
      override;

  void set_is_download_persistent(bool is_download_persistent) {
    is_download_persistent_ = is_download_persistent;
  }

 private:
  std::map<uint32_t, download::MockDownloadItemImpl*> items_;
  download::DownloadItemImplDelegate item_delegate_;
  bool is_download_persistent_;

  DISALLOW_COPY_AND_ASSIGN(MockDownloadItemFactory);
};

MockDownloadItemFactory::MockDownloadItemFactory()
    : is_download_persistent_(false) {}

MockDownloadItemFactory::~MockDownloadItemFactory() {}

download::MockDownloadItemImpl* MockDownloadItemFactory::GetItem(int id) {
  if (items_.find(id) == items_.end())
    return nullptr;
  return items_[id];
}

download::MockDownloadItemImpl* MockDownloadItemFactory::PopItem() {
  if (items_.empty())
    return nullptr;

  auto first_item = items_.begin();
  download::MockDownloadItemImpl* result = first_item->second;
  items_.erase(first_item);
  return result;
}

void MockDownloadItemFactory::RemoveItem(int id) {
  DCHECK(items_.find(id) != items_.end());
  items_.erase(id);
}

download::DownloadItemImpl* MockDownloadItemFactory::CreatePersistedItem(
    download::DownloadItemImplDelegate* delegate,
    const std::string& guid,
    uint32_t download_id,
    const base::FilePath& current_path,
    const base::FilePath& target_path,
    const std::vector<GURL>& url_chain,
    const GURL& referrer_url,
    const GURL& site_url,
    const GURL& tab_url,
    const GURL& tab_referrer_url,
    const base::Optional<url::Origin>& request_initiator,
    const std::string& mime_type,
    const std::string& original_mime_type,
    base::Time start_time,
    base::Time end_time,
    const std::string& etag,
    const std::string& last_modified,
    int64_t received_bytes,
    int64_t total_bytes,
    const std::string& hash,
    download::DownloadItem::DownloadState state,
    download::DownloadDangerType danger_type,
    download::DownloadInterruptReason interrupt_reason,
    bool opened,
    base::Time last_access_time,
    bool transient,
    const std::vector<download::DownloadItem::ReceivedSlice>& received_slices) {
  DCHECK(items_.find(download_id) == items_.end());
  download::MockDownloadItemImpl* result =
      new StrictMock<download::MockDownloadItemImpl>(&item_delegate_);
  EXPECT_CALL(*result, GetId())
      .WillRepeatedly(Return(download_id));
  EXPECT_CALL(*result, GetGuid()).WillRepeatedly(ReturnRefOfCopy(guid));
  items_[download_id] = result;
  return result;
}

download::DownloadItemImpl* MockDownloadItemFactory::CreateActiveItem(
    download::DownloadItemImplDelegate* delegate,
    uint32_t download_id,
    const download::DownloadCreateInfo& info) {
  DCHECK(items_.find(download_id) == items_.end());

  download::MockDownloadItemImpl* result =
      new StrictMock<download::MockDownloadItemImpl>(&item_delegate_);
  EXPECT_CALL(*result, GetId())
      .WillRepeatedly(Return(download_id));
  EXPECT_CALL(*result, GetGuid())
      .WillRepeatedly(ReturnRefOfCopy(base::GenerateGUID()));
  EXPECT_CALL(*result, GetUrlChain())
      .WillRepeatedly(ReturnRefOfCopy(std::vector<GURL>()));
  EXPECT_CALL(*result, GetReferrerUrl())
      .WillRepeatedly(ReturnRefOfCopy(GURL()));
  EXPECT_CALL(*result, GetTabUrl()).WillRepeatedly(ReturnRefOfCopy(GURL()));
  EXPECT_CALL(*result, GetTabReferrerUrl())
      .WillRepeatedly(ReturnRefOfCopy(GURL()));
  EXPECT_CALL(*result, GetETag())
      .WillRepeatedly(ReturnRefOfCopy(std::string()));
  EXPECT_CALL(*result, GetLastModifiedTime())
      .WillRepeatedly(ReturnRefOfCopy(std::string()));
  EXPECT_CALL(*result, GetMimeType()).WillRepeatedly(Return(std::string()));
  EXPECT_CALL(*result, GetOriginalMimeType())
      .WillRepeatedly(Return(std::string()));
  EXPECT_CALL(*result, GetTotalBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(*result, GetFullPath())
      .WillRepeatedly(
          ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("foo"))));
  EXPECT_CALL(*result, GetTargetFilePath())
      .WillRepeatedly(
          ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("foo"))));
  EXPECT_CALL(*result, GetReceivedBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(*result, GetStartTime()).WillRepeatedly(Return(base::Time()));
  EXPECT_CALL(*result, GetEndTime()).WillRepeatedly(Return(base::Time()));
  EXPECT_CALL(*result, GetReceivedSlices())
      .WillRepeatedly(ReturnRefOfCopy(
          std::vector<download::DownloadItem::ReceivedSlice>()));
  EXPECT_CALL(*result, GetHash())
      .WillRepeatedly(ReturnRefOfCopy(std::string()));
  EXPECT_CALL(*result, GetState())
      .WillRepeatedly(Return(download::DownloadItem::IN_PROGRESS));
  EXPECT_CALL(*result, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  EXPECT_CALL(*result, GetLastReason())
      .WillRepeatedly(Return(download::DOWNLOAD_INTERRUPT_REASON_NONE));
  EXPECT_CALL(*result, IsPaused()).WillRepeatedly(Return(false));
  EXPECT_CALL(*result, IsTemporary()).WillRepeatedly(Return(false));

  if (is_download_persistent_) {
    EXPECT_CALL(*result, RemoveObserver(_));
    EXPECT_CALL(*result, AddObserver(_));
  }
  items_[download_id] = result;

  // Active items are created and then immediately are called to start
  // the download.
  EXPECT_CALL(*result, MockStart(_));

  return result;
}

download::DownloadItemImpl* MockDownloadItemFactory::CreateSavePageItem(
    download::DownloadItemImplDelegate* delegate,
    uint32_t download_id,
    const base::FilePath& path,
    const GURL& url,
    const std::string& mime_type,
    download::DownloadJob::CancelRequestCallback cancel_request_callback) {
  DCHECK(items_.find(download_id) == items_.end());

  download::MockDownloadItemImpl* result =
      new StrictMock<download::MockDownloadItemImpl>(&item_delegate_);
  EXPECT_CALL(*result, GetId())
      .WillRepeatedly(Return(download_id));
  items_[download_id] = result;

  return result;
}

class MockDownloadFileFactory
    : public download::DownloadFileFactory,
      public base::SupportsWeakPtr<MockDownloadFileFactory> {
 public:
  MockDownloadFileFactory() {}
  ~MockDownloadFileFactory() override {}

  // Overridden method from DownloadFileFactory
  MOCK_METHOD2(MockCreateFile,
               download::MockDownloadFile*(const download::DownloadSaveInfo&,
                                           download::InputStream*));

  download::DownloadFile* CreateFile(
      std::unique_ptr<download::DownloadSaveInfo> save_info,
      const base::FilePath& default_download_directory,
      std::unique_ptr<download::InputStream> stream,
      uint32_t download_id,
      base::WeakPtr<download::DownloadDestinationObserver> observer) override {
    return MockCreateFile(*save_info, stream.get());
  }
};

class MockDownloadManagerObserver : public DownloadManager::Observer {
 public:
  MockDownloadManagerObserver() {}
  ~MockDownloadManagerObserver() override {}
  MOCK_METHOD2(OnDownloadCreated,
               void(DownloadManager*, download::DownloadItem*));
  MOCK_METHOD1(ManagerGoingDown, void(DownloadManager*));
  MOCK_METHOD2(SelectFileDialogDisplayed, void(DownloadManager*, int32_t));
};

class TestInProgressManager : public download::InProgressDownloadManager {
 public:
  TestInProgressManager();
  ~TestInProgressManager() override = default;

  std::vector<std::unique_ptr<download::DownloadItemImpl>>
  TakeInProgressDownloads() override;

  void AddDownloadItem(std::unique_ptr<download::DownloadItemImpl> item);

 private:
  std::vector<std::unique_ptr<download::DownloadItemImpl>> download_items_;
};

TestInProgressManager::TestInProgressManager()
    : download::InProgressDownloadManager(
          nullptr,
          base::FilePath(),
          nullptr,
          download::InProgressDownloadManager::IsOriginSecureCallback(),
          base::BindRepeating(&URLAlwaysSafe),
          /*wake_lock_provider_binder*/ base::NullCallback()) {}

void TestInProgressManager::AddDownloadItem(
    std::unique_ptr<download::DownloadItemImpl> item) {
  download_items_.emplace_back(std::move(item));
}

std::vector<std::unique_ptr<download::DownloadItemImpl>>
TestInProgressManager::TakeInProgressDownloads() {
  return std::move(download_items_);
}

}  // namespace

class DownloadManagerTest : public testing::Test {
 public:
  static const char* kTestData;
  static const size_t kTestDataLen;

  DownloadManagerTest()
      : callback_called_(false),
        target_disposition_(
            download::DownloadItem::TARGET_DISPOSITION_OVERWRITE),
        danger_type_(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS),
        interrupt_reason_(download::DOWNLOAD_INTERRUPT_REASON_NONE),
        next_download_id_(0) {}

  // We tear down everything in TearDown().
  ~DownloadManagerTest() override {}

  // Create a MockDownloadItemFactory and MockDownloadManagerDelegate,
  // then create a DownloadManager that points
  // at all of those.
  void SetUp() override {
    DCHECK(!download_manager_);

    mock_download_item_factory_ = (new MockDownloadItemFactory())->AsWeakPtr();
    mock_download_file_factory_ = (new MockDownloadFileFactory())->AsWeakPtr();
    mock_download_manager_delegate_.reset(
        new StrictMock<MockDownloadManagerDelegate>);
    EXPECT_CALL(*mock_download_manager_delegate_.get(), Shutdown())
        .WillOnce(Return());
    browser_context_ = std::make_unique<TestBrowserContext>();
    download_manager_.reset(new DownloadManagerImpl(browser_context_.get()));
    download_manager_->SetDownloadItemFactoryForTesting(
        std::unique_ptr<download::DownloadItemFactory>(
            mock_download_item_factory_.get()));
    download_manager_->SetDownloadFileFactoryForTesting(
        std::unique_ptr<download::DownloadFileFactory>(
            mock_download_file_factory_.get()));
    observer_.reset(new MockDownloadManagerObserver());
    download_manager_->AddObserver(observer_.get());
    download_manager_->SetDelegate(mock_download_manager_delegate_.get());
    download_urls_.push_back(GURL("http://www.url1.com"));
    download_urls_.push_back(GURL("http://www.url2.com"));
    download_urls_.push_back(GURL("http://www.url3.com"));
    download_urls_.push_back(GURL("http://www.url4.com"));
  }

  void TearDown() override {
    while (download::MockDownloadItemImpl* item =
               mock_download_item_factory_->PopItem()) {
      EXPECT_CALL(*item, GetState())
          .WillOnce(Return(download::DownloadItem::CANCELLED));
    }
    EXPECT_CALL(GetMockObserver(), ManagerGoingDown(download_manager_.get()))
        .WillOnce(Return());

    download_manager_->Shutdown();
    download_manager_.reset();
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(mock_download_item_factory_);
    mock_download_manager_delegate_.reset();
    download_urls_.clear();
  }

  // Returns download id.
  download::MockDownloadItemImpl& AddItemToManager() {
    download::DownloadCreateInfo info;

    // Args are ignored except for download id, so everything else can be
    // null.
    uint32_t id = next_download_id_;
    ++next_download_id_;
    download_manager_->CreateActiveItem(id, info);
    DCHECK(mock_download_item_factory_->GetItem(id));
    download::MockDownloadItemImpl& item(
        *mock_download_item_factory_->GetItem(id));
    // Satisfy expectation.  If the item is created in StartDownload(),
    // we call Start on it immediately, so we need to set that expectation
    // in the factory.
    item.Start(std::unique_ptr<download::DownloadFile>(), base::DoNothing(),
               info, download::URLLoaderFactoryProvider::GetNullPtr());
    DCHECK(id < download_urls_.size());
    EXPECT_CALL(item, GetURL()).WillRepeatedly(ReturnRef(download_urls_[id]));

    return item;
  }

  // Helper function to create a download item.
  download::DownloadItem* CreateDownloadItem(
      const base::Time& start_time,
      const std::vector<GURL>& url_chain,
      download::DownloadItem::DownloadState download_state) {
    download::DownloadItem* download_item =
        download_manager_->CreateDownloadItem(
            kGuid, 10, base::FilePath(), base::FilePath(), url_chain,
            GURL("http://example.com/a"), GURL("http://example.com/a"),
            GURL("http://example.com/a"), GURL("http://example.com/a"),
            url::Origin::Create(GURL("http://example.com/")),
            "application/octet-stream", "application/octet-stream", start_time,
            base::Time::Now(), std::string(), std::string(), 10, 10,
            std::string(), download_state,
            download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
            download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED, false,
            base::Time::Now(), true,
            std::vector<download::DownloadItem::ReceivedSlice>());
    return download_item;
  }

  download::MockDownloadItemImpl& GetMockDownloadItem(int id) {
    download::MockDownloadItemImpl* itemp =
        mock_download_item_factory_->GetItem(id);

    DCHECK(itemp);
    return *itemp;
  }

  void RemoveMockDownloadItem(int id) {
    // Owned by DownloadManager; should be deleted there.
    mock_download_item_factory_->RemoveItem(id);
  }

  MockDownloadManagerDelegate& GetMockDownloadManagerDelegate() {
    return *mock_download_manager_delegate_;
  }

  MockDownloadManagerObserver& GetMockObserver() {
    return *observer_;
  }

  void DownloadTargetDeterminedCallback(
      const base::FilePath& target_path,
      download::DownloadItem::TargetDisposition disposition,
      download::DownloadDangerType danger_type,
      download::DownloadItem::MixedContentStatus mixed_content_status,
      const base::FilePath& intermediate_path,
      download::DownloadInterruptReason interrupt_reason) {
    callback_called_ = true;
    target_path_ = target_path;
    target_disposition_ = disposition;
    danger_type_ = danger_type;
    intermediate_path_ = intermediate_path;
    interrupt_reason_ = interrupt_reason;
  }

  void DetermineDownloadTarget(download::DownloadItemImpl* item) {
    download_manager_->DetermineDownloadTarget(
        item,
        base::BindOnce(&DownloadManagerTest::DownloadTargetDeterminedCallback,
                       base::Unretained(this)));
  }

  void OnInProgressDownloadManagerInitialized() {
    download_manager_->OnDownloadsInitialized();
  }

  void OnHistoryDBInitialized() {
    download_manager_->PostInitialization(
        DownloadManager::DOWNLOAD_INITIALIZATION_DEPENDENCY_HISTORY_DB);
  }

  void SetInProgressDownloadManager(
      std::unique_ptr<download::InProgressDownloadManager> manager) {
    download_manager_->in_progress_manager_ = std::move(manager);
  }

 protected:
  // Key test variable; we'll keep it available to sub-classes.
  std::unique_ptr<DownloadManagerImpl> download_manager_;
  base::WeakPtr<MockDownloadFileFactory> mock_download_file_factory_;
  base::WeakPtr<MockDownloadItemFactory> mock_download_item_factory_;

  // Target detetermined callback.
  bool callback_called_;
  base::FilePath target_path_;
  download::DownloadItem::TargetDisposition target_disposition_;
  download::DownloadDangerType danger_type_;
  base::FilePath intermediate_path_;
  download::DownloadInterruptReason interrupt_reason_;

  std::vector<GURL> download_urls_;

 private:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<MockDownloadManagerDelegate> mock_download_manager_delegate_;
  std::unique_ptr<MockDownloadManagerObserver> observer_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  uint32_t next_download_id_;

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerTest);
};

// Confirm the appropriate invocations occur when you start a download.
TEST_F(DownloadManagerTest, StartDownload) {
  std::unique_ptr<download::DownloadCreateInfo> info(
      new download::DownloadCreateInfo);
  // Random value, a non 0 value means history db is properly loaded, and new
  // downloads should be persisted to the in-progress db.
  uint32_t local_id(5);
  base::FilePath download_path(FILE_PATH_LITERAL("download/path"));
  OnInProgressDownloadManagerInitialized();

  EXPECT_FALSE(download_manager_->GetDownload(local_id));

  EXPECT_CALL(GetMockObserver(), OnDownloadCreated(download_manager_.get(), _))
      .WillOnce(Return());
  EXPECT_CALL(GetMockDownloadManagerDelegate(), GetNextId_(_))
      .WillOnce(RunOnceCallback<0>(local_id));

#if !defined(USE_X11)
  // Doing nothing will set the default download directory to null.
  EXPECT_CALL(GetMockDownloadManagerDelegate(), GetSaveDir(_, _, _));
#endif
  EXPECT_CALL(GetMockDownloadManagerDelegate(),
              ApplicationClientIdForFileScanning())
      .WillRepeatedly(Return("client-id"));
  download::MockDownloadFile* mock_file = new download::MockDownloadFile;
  auto input_stream = std::make_unique<download::MockInputStream>();
  EXPECT_CALL(*input_stream, IsEmpty()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_download_file_factory_.get(),
              MockCreateFile(Ref(*info->save_info.get()), input_stream.get()))
      .WillOnce(Return(mock_file));

  mock_download_item_factory_->set_is_download_persistent(true);
  download_manager_->StartDownload(
      std::move(info), std::move(input_stream),
      download::DownloadUrlParameters::OnStartedCallback());
  EXPECT_TRUE(download_manager_->GetDownload(local_id));
}

// Test the case that a new download is started when history db failed to
// initialize.
TEST_F(DownloadManagerTest, StartDownloadWithoutHistoryDB) {
  std::unique_ptr<download::DownloadCreateInfo> info(
      new download::DownloadCreateInfo);
  base::FilePath download_path(FILE_PATH_LITERAL("download/path"));
  OnInProgressDownloadManagerInitialized();
  EXPECT_FALSE(download_manager_->GetDownload(1));

  EXPECT_CALL(GetMockObserver(), OnDownloadCreated(download_manager_.get(), _))
      .WillOnce(Return());
  // Returning kInvalidId to indicate that the history db failed.
  EXPECT_CALL(GetMockDownloadManagerDelegate(), GetNextId_(_))
      .WillOnce(RunOnceCallback<0>(download::DownloadItem::kInvalidId));

#if !defined(USE_X11)
  // Doing nothing will set the default download directory to null.
  EXPECT_CALL(GetMockDownloadManagerDelegate(), GetSaveDir(_, _, _));
#endif
  EXPECT_CALL(GetMockDownloadManagerDelegate(),
              ApplicationClientIdForFileScanning())
      .WillRepeatedly(Return("client-id"));
  download::MockDownloadFile* mock_file = new download::MockDownloadFile;
  auto input_stream = std::make_unique<download::MockInputStream>();
  EXPECT_CALL(*input_stream, IsEmpty()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_download_file_factory_.get(),
              MockCreateFile(Ref(*info->save_info.get()), input_stream.get()))
      .WillOnce(Return(mock_file));

  download_manager_->StartDownload(
      std::move(info), std::move(input_stream),
      download::DownloadUrlParameters::OnStartedCallback());
  EXPECT_TRUE(download_manager_->GetDownload(1));
}

// Confirm that calling DetermineDownloadTarget behaves properly if the delegate
// blocks starting.
TEST_F(DownloadManagerTest, DetermineDownloadTarget_True) {
  // Put a mock we have a handle to on the download manager.
  download::MockDownloadItemImpl& item(AddItemToManager());
  EXPECT_CALL(item, GetState())
      .WillRepeatedly(Return(download::DownloadItem::IN_PROGRESS));

  EXPECT_CALL(GetMockDownloadManagerDelegate(),
              DetermineDownloadTarget(&item, _))
      .WillOnce(Return(true));
  DetermineDownloadTarget(&item);
}

// Confirm that calling DetermineDownloadTarget behaves properly if the delegate
// allows starting.  This also tests OnDownloadTargetDetermined.
TEST_F(DownloadManagerTest, DetermineDownloadTarget_False) {
  // Put a mock we have a handle to on the download manager.
  download::MockDownloadItemImpl& item(AddItemToManager());

  base::FilePath path(FILE_PATH_LITERAL("random_filepath.txt"));
  EXPECT_CALL(GetMockDownloadManagerDelegate(),
              DetermineDownloadTarget(&item, _))
      .WillOnce(Return(false));
  EXPECT_CALL(item, GetForcedFilePath())
      .WillOnce(ReturnRef(path));

  // Confirm that the callback was called with the right values in this case.
  DetermineDownloadTarget(&item);
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(path, target_path_);
  EXPECT_EQ(download::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
            target_disposition_);
  EXPECT_EQ(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, danger_type_);
  EXPECT_EQ(path, intermediate_path_);
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE, interrupt_reason_);
}

TEST_F(DownloadManagerTest, GetDownloadByGuid) {
  for (uint32_t i = 0; i < 4; ++i)
    AddItemToManager();

  download::MockDownloadItemImpl& item = GetMockDownloadItem(0);
  download::DownloadItem* result =
      download_manager_->GetDownloadByGuid(item.GetGuid());
  ASSERT_TRUE(result);
  ASSERT_EQ(static_cast<download::DownloadItem*>(&item), result);

  ASSERT_FALSE(download_manager_->GetDownloadByGuid(""));

  std::vector<GURL> url_chain;
  url_chain.emplace_back("http://example.com/1.zip");
  EXPECT_CALL(GetMockObserver(), OnDownloadCreated(download_manager_.get(), _));
  download::DownloadItem* persisted_item = CreateDownloadItem(
      base::Time::Now(), url_chain, download::DownloadItem::INTERRUPTED);
  ASSERT_TRUE(persisted_item);
  ASSERT_EQ(persisted_item, download_manager_->GetDownloadByGuid(kGuid));
}

namespace {

base::RepeatingCallback<bool(const GURL&)> GetSingleURLFilter(const GURL& url) {
  return base::BindRepeating(
      [](const GURL& a, const GURL& b) { return a == b; }, url);
}

}  // namespace

// Confirm that only downloads with the specified URL are removed.
TEST_F(DownloadManagerTest, RemoveDownloadsByURL) {
  base::Time now(base::Time::Now());
  for (uint32_t i = 0; i < 2; ++i) {
    download::MockDownloadItemImpl& item(AddItemToManager());
    EXPECT_CALL(item, GetStartTime()).WillRepeatedly(Return(now));
    EXPECT_CALL(item, GetState())
        .WillRepeatedly(Return(download::DownloadItem::COMPLETE));
  }

  EXPECT_CALL(GetMockDownloadItem(0), Remove());
  EXPECT_CALL(GetMockDownloadItem(1), Remove()).Times(0);

  base::RepeatingCallback<bool(const GURL&)> url_filter =
      GetSingleURLFilter(download_urls_[0]);
  int remove_count = download_manager_->RemoveDownloadsByURLAndTime(
      std::move(url_filter), base::Time(), base::Time::Max());
  EXPECT_EQ(remove_count, 1);
}

// Confirm that in-progress downloads will be taken and managed by
// DownloadManager.
TEST_F(DownloadManagerTest, OnInProgressDownloadsLoaded) {
  auto in_progress_manager = std::make_unique<TestInProgressManager>();
  std::vector<GURL> url_chain;
  url_chain.emplace_back("http://example.com/1.zip");
  auto in_progress_item = std::make_unique<download::DownloadItemImpl>(
      in_progress_manager.get(), kGuid, 10, base::FilePath(), base::FilePath(),
      url_chain, GURL("http://example.com/a"), GURL("http://example.com/a"),
      GURL("http://example.com/a"), GURL("http://example.com/a"),
      url::Origin::Create(GURL("http://example.com")),
      "application/octet-stream", "application/octet-stream", base::Time::Now(),
      base::Time::Now(), std::string(), std::string(), 10, 10, 0, std::string(),
      download::DownloadItem::INTERRUPTED,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED, false, false, false,
      base::Time::Now(), true,
      std::vector<download::DownloadItem::ReceivedSlice>(),
      nullptr /* download_entry */);
  in_progress_manager->AddDownloadItem(std::move(in_progress_item));
  SetInProgressDownloadManager(std::move(in_progress_manager));
  EXPECT_CALL(GetMockObserver(), OnDownloadCreated(download_manager_.get(), _))
      .WillOnce(Return());
  OnInProgressDownloadManagerInitialized();
  ASSERT_TRUE(download_manager_->GetDownloadByGuid(kGuid));
  std::vector<download::DownloadItem*> vector;
  download_manager_->GetAllDownloads(&vector);
  ASSERT_EQ(0u, vector.size());

  EXPECT_CALL(GetMockDownloadManagerDelegate(), GetNextId_(_))
      .WillOnce(RunOnceCallback<0>(1));
  OnHistoryDBInitialized();
  ASSERT_TRUE(download_manager_->GetDownloadByGuid(kGuid));
  download::DownloadItem* download =
      download_manager_->GetDownloadByGuid(kGuid);
  download_manager_->GetAllDownloads(&vector);
  ASSERT_EQ(1u, vector.size());
  download->Remove();
  ASSERT_FALSE(download_manager_->GetDownloadByGuid(kGuid));
}

// Verifies that expired canceled or interrupted downloads are deleted
// correctly.
TEST_F(DownloadManagerTest, DeleteExpiredDownload) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::map<std::string, std::string> params = {
      {download::kExpiredDownloadDeleteTimeFinchKey, base::NumberToString(1)}};
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      download::features::kDeleteExpiredDownloads, params);

  std::vector<GURL> url_chain;
  url_chain.emplace_back("http://example.com/1.zip");
  auto expired_start_time = base::Time::Now() - base::TimeDelta::FromDays(10);
  download::DownloadItem* download_item = CreateDownloadItem(
      expired_start_time, url_chain, download::DownloadItem::INTERRUPTED);
  EXPECT_FALSE(download_item)
      << "Expired interrupted download will be deleted.";

  download_item = CreateDownloadItem(expired_start_time, url_chain,
                                     download::DownloadItem::CANCELLED);
  EXPECT_FALSE(download_item) << "Expired canceled download will be deleted.";

  download_item = CreateDownloadItem(expired_start_time, std::vector<GURL>(),
                                     download::DownloadItem::COMPLETE);
  EXPECT_FALSE(download_item) << "Download without URL chain will be deleted.";

  EXPECT_CALL(GetMockObserver(), OnDownloadCreated(download_manager_.get(), _));
  download_item = CreateDownloadItem(expired_start_time, url_chain,
                                     download::DownloadItem::COMPLETE);
  EXPECT_TRUE(download_item)
      << "Expired complete download will not be deleted.";
}

}  // namespace content
