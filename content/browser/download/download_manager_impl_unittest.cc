// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/download/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/download_item_factory.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/mock_download_file.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/net_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::DoAll;
using ::testing::Ref;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::_;
using content::DownloadItem;
using content::DownloadManager;
using content::WebContents;

namespace content {
class ByteStreamReader;
}

namespace {

class MockDownloadManagerDelegate : public content::DownloadManagerDelegate {
 public:
  MockDownloadManagerDelegate();
  virtual ~MockDownloadManagerDelegate();

  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD0(GetNextId, content::DownloadId());
  MOCK_METHOD1(ShouldStartDownload, bool(int32));
  MOCK_METHOD1(ChooseDownloadPath, void(DownloadItem*));
  MOCK_METHOD2(GetIntermediatePath, FilePath(const DownloadItem&, bool*));
  MOCK_METHOD0(GetAlternativeWebContentsToNotifyForDownload, WebContents*());
  MOCK_METHOD1(ShouldOpenFileBasedOnExtension, bool(const FilePath&));
  MOCK_METHOD2(ShouldCompleteDownload, bool(
      DownloadItem*, const base::Closure&));
  MOCK_METHOD1(ShouldOpenDownload, bool(DownloadItem*));
  MOCK_METHOD0(GenerateFileHash, bool());
  MOCK_METHOD1(AddItemToPersistentStore, void(DownloadItem*));
  MOCK_METHOD1(UpdateItemInPersistentStore, void(DownloadItem*));
  MOCK_METHOD2(UpdatePathForItemInPersistentStore,
               void(DownloadItem*, const FilePath&));
  MOCK_METHOD1(RemoveItemFromPersistentStore, void(DownloadItem*));
  MOCK_METHOD2(RemoveItemsFromPersistentStoreBetween, void(
      base::Time remove_begin, base::Time remove_end));
  MOCK_METHOD4(GetSaveDir, void(WebContents*, FilePath*, FilePath*, bool*));
  MOCK_METHOD5(ChooseSavePath, void(
      WebContents*, const FilePath&, const FilePath::StringType&,
      bool, const content::SavePackagePathPickedCallback&));
};

MockDownloadManagerDelegate::MockDownloadManagerDelegate() { }

MockDownloadManagerDelegate::~MockDownloadManagerDelegate() { }

class MockDownloadFileManager : public DownloadFileManager {
 public:
  MockDownloadFileManager();

  void CreateDownloadFile(
      scoped_ptr<DownloadCreateInfo> info,
      scoped_ptr<content::ByteStreamReader> stream,
      scoped_refptr<content::DownloadManager> download_manager,
      bool hash_needed,
      const net::BoundNetLog& bound_net_log,
      const CreateDownloadFileCallback& callback) OVERRIDE {
    // Note that scoped_refptr<> on download manager is also stripped
    // to make mock comparisons easier.  Comparing the scoped_refptr<>
    // works, but holds a reference to the DownloadManager until
    // MockDownloadFileManager destruction, which messes up destruction
    // testing.
    MockCreateDownloadFile(info.get(), stream.get(), download_manager.get(),
                           hash_needed, bound_net_log, callback);
  }

  MOCK_METHOD6(MockCreateDownloadFile, void(
      DownloadCreateInfo* info,
      content::ByteStreamReader* stream,
      content::DownloadManager* download_manager,
      bool hash_needed,
      const net::BoundNetLog& bound_net_log,
      const CreateDownloadFileCallback& callback));
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD1(CancelDownload, void(content::DownloadId));
  MOCK_METHOD2(CompleteDownload, void(content::DownloadId,
                                      const base::Closure&));
  MOCK_METHOD1(OnDownloadManagerShutdown, void(content::DownloadManager*));
  MOCK_METHOD4(RenameDownloadFile, void(content::DownloadId,
                                        const FilePath&,
                                        bool,
                                        const RenameCompletionCallback&));
  MOCK_CONST_METHOD0(NumberOfActiveDownloads, int());
 protected:
  virtual ~MockDownloadFileManager();
};

MockDownloadFileManager::MockDownloadFileManager()
    : DownloadFileManager(NULL) { }

MockDownloadFileManager::~MockDownloadFileManager() { }

class MockDownloadItemFactory
    : public content::DownloadItemFactory,
      public base::SupportsWeakPtr<MockDownloadItemFactory> {
 public:
  MockDownloadItemFactory();
  virtual ~MockDownloadItemFactory();

  // Access to map of created items.
  // TODO(rdsmith): Could add type (save page, persisted, etc.)
  // functionality if it's ever needed by consumers.

  // Returns NULL if no item of that id is present.
  content::MockDownloadItem* GetItem(int id);

  // Remove and return an item made by the factory.
  // Generally used during teardown.
  content::MockDownloadItem* PopItem();

  // Should be called when the item of this id is removed so that
  // we don't keep dangling pointers.
  void RemoveItem(int id);

  // Overridden methods from DownloadItemFactory.
  virtual content::DownloadItem* CreatePersistedItem(
      DownloadItemImpl::Delegate* delegate,
      content::DownloadId download_id,
      const content::DownloadPersistentStoreInfo& info,
      const net::BoundNetLog& bound_net_log) OVERRIDE;
  virtual content::DownloadItem* CreateActiveItem(
      DownloadItemImpl::Delegate* delegate,
      const DownloadCreateInfo& info,
      scoped_ptr<DownloadRequestHandleInterface> request_handle,
      bool is_otr,
      const net::BoundNetLog& bound_net_log) OVERRIDE;
  virtual content::DownloadItem* CreateSavePageItem(
      DownloadItemImpl::Delegate* delegate,
      const FilePath& path,
      const GURL& url,
      bool is_otr,
      content::DownloadId download_id,
      const std::string& mime_type,
      const net::BoundNetLog& bound_net_log) OVERRIDE;

 private:
  std::map<int32, content::MockDownloadItem*> items_;

  DISALLOW_COPY_AND_ASSIGN(MockDownloadItemFactory);
};

MockDownloadItemFactory::MockDownloadItemFactory() {}

MockDownloadItemFactory::~MockDownloadItemFactory() {}

content::MockDownloadItem* MockDownloadItemFactory::GetItem(int id) {
  if (items_.find(id) == items_.end())
    return NULL;
  return items_[id];
}

content::MockDownloadItem* MockDownloadItemFactory::PopItem() {
  if (items_.empty())
    return NULL;

  std::map<int32, content::MockDownloadItem*>::iterator first_item
      = items_.begin();
  content::MockDownloadItem* result = first_item->second;
  items_.erase(first_item);
  return result;
}

void MockDownloadItemFactory::RemoveItem(int id) {
  DCHECK(items_.find(id) != items_.end());
  items_.erase(id);
}

content::DownloadItem* MockDownloadItemFactory::CreatePersistedItem(
    DownloadItemImpl::Delegate* delegate,
    content::DownloadId download_id,
    const content::DownloadPersistentStoreInfo& info,
    const net::BoundNetLog& bound_net_log) {
  int local_id = download_id.local();
  DCHECK(items_.find(local_id) == items_.end());

  content::MockDownloadItem* result =
      new StrictMock<content::MockDownloadItem>;
  EXPECT_CALL(*result, GetId())
      .WillRepeatedly(Return(local_id));
  items_[local_id] = result;

  return result;
}

content::DownloadItem* MockDownloadItemFactory::CreateActiveItem(
    DownloadItemImpl::Delegate* delegate,
    const DownloadCreateInfo& info,
    scoped_ptr<DownloadRequestHandleInterface> request_handle,
    bool is_otr,
    const net::BoundNetLog& bound_net_log) {
  int local_id = info.download_id.local();
  DCHECK(items_.find(local_id) == items_.end());

  content::MockDownloadItem* result =
      new StrictMock<content::MockDownloadItem>;
  EXPECT_CALL(*result, GetId())
      .WillRepeatedly(Return(local_id));
  items_[local_id] = result;

  return result;
}

content::DownloadItem* MockDownloadItemFactory::CreateSavePageItem(
    DownloadItemImpl::Delegate* delegate,
    const FilePath& path,
    const GURL& url,
    bool is_otr,
    content::DownloadId download_id,
    const std::string& mime_type,
    const net::BoundNetLog& bound_net_log) {
  int local_id = download_id.local();
  DCHECK(items_.find(local_id) == items_.end());

  content::MockDownloadItem* result =
      new StrictMock<content::MockDownloadItem>;
  EXPECT_CALL(*result, GetId())
      .WillRepeatedly(Return(local_id));
  items_[local_id] = result;

  return result;
}

class MockBrowserContext : public content::BrowserContext {
 public:
  MockBrowserContext() { }
  ~MockBrowserContext() { }

  MOCK_METHOD0(GetPath, FilePath());
  MOCK_CONST_METHOD0(IsOffTheRecord, bool());
  MOCK_METHOD0(GetRequestContext, net::URLRequestContextGetter*());
  MOCK_METHOD1(GetRequestContextForRenderProcess,
               net::URLRequestContextGetter*(int renderer_child_id));
  MOCK_METHOD0(GetRequestContextForMedia, net::URLRequestContextGetter*());
  MOCK_METHOD0(GetResourceContext, content::ResourceContext*());
  MOCK_METHOD0(GetDownloadManagerDelegate, content::DownloadManagerDelegate*());
  MOCK_METHOD0(GetGeolocationPermissionContext,
               content::GeolocationPermissionContext* ());
  MOCK_METHOD0(GetSpeechRecognitionPreferences,
               content::SpeechRecognitionPreferences* ());
  MOCK_METHOD0(DidLastSessionExitCleanly, bool());
  MOCK_METHOD0(GetSpecialStoragePolicy, quota::SpecialStoragePolicy*());
};

} // namespace

class DownloadManagerTest : public testing::Test {
 public:
  static const char* kTestData;
  static const size_t kTestDataLen;

  DownloadManagerTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        next_download_id_(0) {
  }

  // We tear down everything in TearDown().
  ~DownloadManagerTest() {}

  // Create a MockDownloadItemFactory, MockDownloadManagerDelegate,
  // and MockDownloadFileManager, then create a DownloadManager that points
  // at all of those.
  virtual void SetUp() {
    DCHECK(!download_manager_.get());

    mock_download_item_factory_ = (new MockDownloadItemFactory())->AsWeakPtr();
    mock_download_manager_delegate_.reset(
        new StrictMock<MockDownloadManagerDelegate>);
    EXPECT_CALL(*mock_download_manager_delegate_.get(), Shutdown())
        .WillOnce(Return());
    mock_download_file_manager_ = new StrictMock<MockDownloadFileManager>;
    EXPECT_CALL(*mock_download_file_manager_.get(),
                OnDownloadManagerShutdown(_));
    mock_browser_context_.reset(new StrictMock<MockBrowserContext>);
    EXPECT_CALL(*mock_browser_context_.get(), IsOffTheRecord())
        .WillRepeatedly(Return(false));

    download_manager_ = new DownloadManagerImpl(
        mock_download_file_manager_.get(),
        scoped_ptr<content::DownloadItemFactory>(
            mock_download_item_factory_.get()).Pass(), NULL);
    download_manager_->SetDelegate(mock_download_manager_delegate_.get());
    download_manager_->Init(mock_browser_context_.get());
  }

  virtual void TearDown() {
    while (content::MockDownloadItem*
           item = mock_download_item_factory_->PopItem()) {
      EXPECT_CALL(*item, GetSafetyState())
          .WillOnce(Return(content::DownloadItem::SAFE));
      EXPECT_CALL(*item, IsPartialDownload())
          .WillOnce(Return(false));
    }

    download_manager_->Shutdown();
    download_manager_ = NULL;
    message_loop_.RunAllPending();
    ASSERT_EQ(NULL, mock_download_item_factory_.get());
    message_loop_.RunAllPending();
    mock_download_manager_delegate_.reset();
    mock_download_file_manager_ = NULL;
    mock_browser_context_.reset();
  }

  // Returns download id.
  content::MockDownloadItem& AddItemToManager() {
    DownloadCreateInfo info;

    static const char* kDownloadIdDomain = "Test download id domain";

    // Args are ignored except for download id, so everything else can be
    // null.
    int id = next_download_id_;
    ++next_download_id_;
    info.download_id = content::DownloadId(kDownloadIdDomain, id);
    info.request_handle = DownloadRequestHandle();
    download_manager_->CreateDownloadItem(&info);

    DCHECK(mock_download_item_factory_->GetItem(id));
    content::MockDownloadItem& item(*mock_download_item_factory_->GetItem(id));
    ON_CALL(item, GetId())
        .WillByDefault(Return(id));

    return item;
  }

  content::MockDownloadItem& GetMockDownloadItem(int id) {
    content::MockDownloadItem* itemp = mock_download_item_factory_->GetItem(id);

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

  MockDownloadFileManager& GetMockDownloadFileManager() {
    return *mock_download_file_manager_;
  }

  // Probe at private internals.
  DownloadItem* GetActiveDownloadItem(int32 id) {
    return download_manager_->GetActiveDownload(id);
  }

  void AddItemToHistory(content::MockDownloadItem& item, int64 db_handle) {
    // For DCHECK in AddDownloadItemToHistory.  Don't want to use
    // WillRepeatedly as it may have to return true after this.
    if (DCHECK_IS_ON())
      EXPECT_CALL(item, IsPersisted())
          .WillRepeatedly(Return(false));

    EXPECT_CALL(item, SetDbHandle(db_handle));
    EXPECT_CALL(item, SetIsPersisted());
    EXPECT_CALL(item, GetDbHandle())
        .WillRepeatedly(Return(db_handle));

    // Null out ShowDownloadInBrowser
    EXPECT_CALL(item, GetWebContents())
        .WillOnce(Return(static_cast<WebContents*>(NULL)));
    EXPECT_CALL(GetMockDownloadManagerDelegate(),
                GetAlternativeWebContentsToNotifyForDownload())
        .WillOnce(Return(static_cast<WebContents*>(NULL)));

    EXPECT_CALL(item, IsInProgress())
        .WillOnce(Return(true));

    // Null out MaybeCompleteDownload
    EXPECT_CALL(item, AllDataSaved())
        .WillOnce(Return(false));

    download_manager_->OnItemAddedToPersistentStore(item.GetId(), db_handle);
  }

 protected:
  // Key test variable; we'll keep it available to sub-classes.
  scoped_refptr<DownloadManagerImpl> download_manager_;

 private:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  base::WeakPtr<MockDownloadItemFactory> mock_download_item_factory_;
  scoped_ptr<MockDownloadManagerDelegate> mock_download_manager_delegate_;
  scoped_refptr<MockDownloadFileManager> mock_download_file_manager_;
  scoped_ptr<MockBrowserContext> mock_browser_context_;
  int next_download_id_;

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerTest);
};

// Confirm the appropriate invocations occur when you start a download.
TEST_F(DownloadManagerTest, StartDownload) {
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
  scoped_ptr<content::ByteStreamReader> stream;
  int32 local_id(5);                    // Random value

  EXPECT_FALSE(GetActiveDownloadItem(local_id));

  EXPECT_CALL(GetMockDownloadManagerDelegate(), GetNextId())
      .WillOnce(Return(content::DownloadId(this, local_id)));
  EXPECT_CALL(GetMockDownloadManagerDelegate(), GenerateFileHash())
      .WillOnce(Return(true));
  EXPECT_CALL(GetMockDownloadFileManager(), MockCreateDownloadFile(
      info.get(), static_cast<content::ByteStreamReader*>(NULL),
      download_manager_.get(), true, _, _));

  download_manager_->StartDownload(info.Pass(), stream.Pass());
  EXPECT_TRUE(GetActiveDownloadItem(local_id));
}

// Does the DownloadManager prompt when requested?
TEST_F(DownloadManagerTest, RestartDownload) {
  // Put a mock we have a handle to on the download manager.
  content::MockDownloadItem& item(AddItemToManager());
  int download_id = item.GetId();

  // Confirm we're internally consistent.
  EXPECT_EQ(&item, GetActiveDownloadItem(download_id));

  ScopedTempDir download_dir;
  ASSERT_TRUE(download_dir.CreateUniqueTempDir());
  FilePath expected_path(download_dir.path().Append(
      FILE_PATH_LITERAL("location")));

  EXPECT_CALL(item, GetTargetDisposition())
      .WillOnce(Return(DownloadItem::TARGET_DISPOSITION_PROMPT));
  EXPECT_CALL(GetMockDownloadManagerDelegate(), ChooseDownloadPath(&item));
  download_manager_->RestartDownload(download_id);

  // The alternative pathway goes straight to OnTargetPathAvailable,
  // so it more naturally belongs below.
}

// Do the results of GetIntermediatePath get passed through to the
// download?  Note that this path is tested from RestartDownload
// to test the non-prompting path in RestartDownload as well.
TEST_F(DownloadManagerTest, OnTargetPathAvailable) {
  // Put a mock we have a handle to on the download manager.
  content::MockDownloadItem& item(AddItemToManager());

  ScopedTempDir download_dir;
  ASSERT_TRUE(download_dir.CreateUniqueTempDir());
  FilePath target_path(download_dir.path().Append(
      FILE_PATH_LITERAL("location")));
  FilePath intermediate_path(download_dir.path().Append(
      FILE_PATH_LITERAL("location.crdownload")));

  EXPECT_CALL(item, GetTargetDisposition())
      .WillOnce(Return(DownloadItem::TARGET_DISPOSITION_UNIQUIFY));
  EXPECT_CALL(GetMockDownloadManagerDelegate(),
              GetIntermediatePath(Ref(item), _))
      .WillOnce(DoAll(SetArgPointee<1>(true), Return(intermediate_path)));
  // Finesse DCHECK with WillRepeatedly.
  EXPECT_CALL(item, GetTargetFilePath())
      .WillRepeatedly(ReturnRef(target_path));
  EXPECT_CALL(item, OnIntermediatePathDetermined(
      &GetMockDownloadFileManager(), intermediate_path, true));
  download_manager_->RestartDownload(item.GetId());
}

// Do the results of an OnDownloadInterrupted get passed through properly
// to the DownloadItem?
TEST_F(DownloadManagerTest, OnDownloadInterrupted) {
  // Put a mock we have a handle to on the download manager.
  content::MockDownloadItem& item(AddItemToManager());
  int download_id = item.GetId();

  int64 size = 0xdeadbeef;
  const std::string hash_state("Undead beef");
  content::DownloadInterruptReason reason(
      content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);

  EXPECT_CALL(item, UpdateProgress(size, 0, hash_state));
  EXPECT_CALL(item, Interrupt(reason));
  download_manager_->OnDownloadInterrupted(
      download_id, size, hash_state, reason);
  EXPECT_EQ(&item, GetActiveDownloadItem(download_id));
}

// Does DownloadStopped remove Download from appropriate queues?
// This test tests non-persisted downloads.
TEST_F(DownloadManagerTest, OnDownloadStopped_NonPersisted) {
  // Put a mock we have a handle to on the download manager.
  content::MockDownloadItem& item(AddItemToManager());

  EXPECT_CALL(item, IsPersisted())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(item, GetState())
      .WillRepeatedly(Return(DownloadItem::CANCELLED));
  EXPECT_CALL(item, GetDbHandle())
      .WillRepeatedly(Return(DownloadItem::kUninitializedHandle));

  EXPECT_CALL(item, OffThreadCancel(&GetMockDownloadFileManager()));
  download_manager_->DownloadStopped(&item);
  // TODO(rdsmith): Confirm that the download item is no longer on the
  // active list by calling GetActiveDownloadItem(id).  Currently, the
  // item is left on the active list for rendez-vous with the history
  // system :-{.
}

// Does DownloadStopped remove Download from appropriate queues?
// This test tests persisted downloads.
TEST_F(DownloadManagerTest, OnDownloadStopped_Persisted) {
  // Put a mock we have a handle to on the download manager.
  content::MockDownloadItem& item(AddItemToManager());
  int download_id = item.GetId();
  int64 db_handle = 0x7;
  AddItemToHistory(item, db_handle);

  EXPECT_CALL(item, IsPersisted())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(GetMockDownloadManagerDelegate(),
              UpdateItemInPersistentStore(&item));
  EXPECT_CALL(item, GetState())
      .WillRepeatedly(Return(DownloadItem::CANCELLED));
  EXPECT_CALL(item, GetDbHandle())
      .WillRepeatedly(Return(db_handle));

  EXPECT_CALL(item, OffThreadCancel(&GetMockDownloadFileManager()));
  download_manager_->DownloadStopped(&item);
  EXPECT_EQ(NULL, GetActiveDownloadItem(download_id));
}
