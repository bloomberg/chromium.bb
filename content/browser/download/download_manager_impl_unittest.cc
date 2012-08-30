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
#include "content/browser/download/download_file_factory.h"
#include "content/browser/download/download_item_factory.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_item_impl_delegate.h"
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
#include "net/base/net_log.h"
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
using content::DownloadInterruptReason;
using content::DownloadItem;
using content::DownloadManager;
using content::WebContents;

namespace content {
class ByteStreamReader;
}

namespace {

class MockDownloadItemImpl : public DownloadItemImpl {
 public:
  // Use history constructor for minimal base object.
  MockDownloadItemImpl(DownloadItemImplDelegate* delegate)
      : DownloadItemImpl(delegate, content::DownloadId(),
                         content::DownloadPersistentStoreInfo(),
                         net::BoundNetLog()) {}
  virtual ~MockDownloadItemImpl() {}

  MOCK_METHOD4(OnDownloadTargetDetermined,
               void(const FilePath&, TargetDisposition,
                    content::DownloadDangerType, const FilePath&));
  MOCK_METHOD1(AddObserver, void(content::DownloadItem::Observer*));
  MOCK_METHOD1(RemoveObserver, void(content::DownloadItem::Observer*));
  MOCK_METHOD0(UpdateObservers, void());
  MOCK_METHOD0(CanShowInFolder, bool());
  MOCK_METHOD0(CanOpenDownload, bool());
  MOCK_METHOD0(ShouldOpenFileBasedOnExtension, bool());
  MOCK_METHOD0(OpenDownload, void());
  MOCK_METHOD0(ShowDownloadInShell, void());
  MOCK_METHOD0(DangerousDownloadValidated, void());
  MOCK_METHOD3(UpdateProgress, void(int64, int64, const std::string&));
  MOCK_METHOD1(Cancel, void(bool));
  MOCK_METHOD0(MarkAsComplete, void());
  MOCK_METHOD1(DelayedDownloadOpened, void(bool));
  MOCK_METHOD1(OnAllDataSaved, void(const std::string&));
  MOCK_METHOD0(OnDownloadedFileRemoved, void());
  MOCK_METHOD0(MaybeCompleteDownload, void());
  virtual void Start(
      scoped_ptr<content::DownloadFile> download_file) OVERRIDE {
    MockStart(download_file.get());
  }

  MOCK_METHOD1(MockStart, void(content::DownloadFile*));

  MOCK_METHOD1(Interrupt, void(DownloadInterruptReason));
  MOCK_METHOD1(Delete, void(DeleteReason));
  MOCK_METHOD0(Remove, void());
  MOCK_CONST_METHOD1(TimeRemaining, bool(base::TimeDelta*));
  MOCK_CONST_METHOD0(CurrentSpeed, int64());
  MOCK_CONST_METHOD0(PercentComplete, int());
  MOCK_CONST_METHOD0(AllDataSaved, bool());
  MOCK_METHOD0(TogglePause, void());
  MOCK_METHOD0(OnDownloadCompleting, void());
  MOCK_CONST_METHOD1(MatchesQuery, bool(const string16& query));
  MOCK_CONST_METHOD0(IsPartialDownload, bool());
  MOCK_CONST_METHOD0(IsInProgress, bool());
  MOCK_CONST_METHOD0(IsCancelled, bool());
  MOCK_CONST_METHOD0(IsInterrupted, bool());
  MOCK_CONST_METHOD0(IsComplete, bool());
  MOCK_CONST_METHOD0(GetFullPath, const FilePath&());
  MOCK_CONST_METHOD0(GetTargetFilePath, const FilePath&());
  MOCK_CONST_METHOD0(GetTargetDisposition, TargetDisposition());
  MOCK_METHOD1(OnContentCheckCompleted, void(content::DownloadDangerType));
  MOCK_CONST_METHOD0(GetState, DownloadState());
  MOCK_CONST_METHOD0(GetUrlChain, const std::vector<GURL>&());
  MOCK_METHOD1(SetTotalBytes, void(int64));
  MOCK_CONST_METHOD0(GetURL, const GURL&());
  MOCK_CONST_METHOD0(GetOriginalUrl, const GURL&());
  MOCK_CONST_METHOD0(GetReferrerUrl, const GURL&());
  MOCK_CONST_METHOD0(GetSuggestedFilename, std::string());
  MOCK_CONST_METHOD0(GetContentDisposition, std::string());
  MOCK_CONST_METHOD0(GetMimeType, std::string());
  MOCK_CONST_METHOD0(GetOriginalMimeType, std::string());
  MOCK_CONST_METHOD0(GetReferrerCharset, std::string());
  MOCK_CONST_METHOD0(GetRemoteAddress, std::string());
  MOCK_CONST_METHOD0(GetTotalBytes, int64());
  MOCK_CONST_METHOD0(GetReceivedBytes, int64());
  MOCK_CONST_METHOD0(GetHashState, const std::string&());
  MOCK_CONST_METHOD0(GetHash, const std::string&());
  MOCK_CONST_METHOD0(GetId, int32());
  MOCK_CONST_METHOD0(GetGlobalId, content::DownloadId());
  MOCK_CONST_METHOD0(GetStartTime, base::Time());
  MOCK_CONST_METHOD0(GetEndTime, base::Time());
  MOCK_METHOD0(SetIsPersisted, void());
  MOCK_CONST_METHOD0(IsPersisted, bool());
  MOCK_METHOD1(SetDbHandle, void(int64));
  MOCK_CONST_METHOD0(GetDbHandle, int64());
  MOCK_METHOD0(GetDownloadManager, content::DownloadManager*());
  MOCK_CONST_METHOD0(IsPaused, bool());
  MOCK_CONST_METHOD0(GetOpenWhenComplete, bool());
  MOCK_METHOD1(SetOpenWhenComplete, void(bool));
  MOCK_CONST_METHOD0(GetFileExternallyRemoved, bool());
  MOCK_CONST_METHOD0(GetSafetyState, SafetyState());
  MOCK_CONST_METHOD0(GetDangerType, content::DownloadDangerType());
  MOCK_CONST_METHOD0(IsDangerous, bool());
  MOCK_METHOD0(GetAutoOpened, bool());
  MOCK_CONST_METHOD0(GetTargetName, FilePath());
  MOCK_CONST_METHOD0(GetForcedFilePath, const FilePath&());
  MOCK_CONST_METHOD0(HasUserGesture, bool());
  MOCK_CONST_METHOD0(GetTransitionType, content::PageTransition());
  MOCK_CONST_METHOD0(IsTemporary, bool());
  MOCK_METHOD1(SetIsTemporary, void(bool));
  MOCK_METHOD1(SetOpened, void(bool));
  MOCK_CONST_METHOD0(GetOpened, bool());
  MOCK_CONST_METHOD0(GetLastModifiedTime, const std::string&());
  MOCK_CONST_METHOD0(GetETag, const std::string&());
  MOCK_CONST_METHOD0(GetLastReason, DownloadInterruptReason());
  MOCK_CONST_METHOD0(GetPersistentStoreInfo,
                     content::DownloadPersistentStoreInfo());
  MOCK_CONST_METHOD0(GetBrowserContext, content::BrowserContext*());
  MOCK_CONST_METHOD0(GetWebContents, content::WebContents*());
  MOCK_CONST_METHOD0(GetFileNameToReportUser, FilePath());
  MOCK_METHOD1(SetDisplayName, void(const FilePath&));
  MOCK_CONST_METHOD0(GetUserVerifiedFilePath, FilePath());
  MOCK_CONST_METHOD1(DebugString, std::string(bool));
  MOCK_METHOD0(MockDownloadOpenForTesting, void());
};

class MockDownloadManagerDelegate : public content::DownloadManagerDelegate {
 public:
  MockDownloadManagerDelegate();
  virtual ~MockDownloadManagerDelegate();

  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD0(GetNextId, content::DownloadId());
  MOCK_METHOD2(DetermineDownloadTarget,
               bool(DownloadItem* item,
                    const content::DownloadTargetCallback&));
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

MockDownloadManagerDelegate::MockDownloadManagerDelegate() {}

MockDownloadManagerDelegate::~MockDownloadManagerDelegate() {}

class NullDownloadItemImplDelegate : public DownloadItemImplDelegate {
 public:
  // Safe to null this out even if it doesn't do anything because none
  // of these functions will ever be called; this class just exists
  // to have something to pass to the DownloadItemImpl base class
  // of MockDownloadItemImpl.
  virtual void DelegateStart(DownloadItemImpl* download) OVERRIDE {
    NOTREACHED();
  }
};

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
  MockDownloadItemImpl* GetItem(int id);

  // Remove and return an item made by the factory.
  // Generally used during teardown.
  MockDownloadItemImpl* PopItem();

  // Should be called when the item of this id is removed so that
  // we don't keep dangling pointers.
  void RemoveItem(int id);

  // Overridden methods from DownloadItemFactory.
  virtual DownloadItemImpl* CreatePersistedItem(
      DownloadItemImplDelegate* delegate,
      content::DownloadId download_id,
      const content::DownloadPersistentStoreInfo& info,
      const net::BoundNetLog& bound_net_log) OVERRIDE;
  virtual DownloadItemImpl* CreateActiveItem(
      DownloadItemImplDelegate* delegate,
      const DownloadCreateInfo& info,
      scoped_ptr<DownloadRequestHandleInterface> request_handle,
      const net::BoundNetLog& bound_net_log) OVERRIDE;
  virtual DownloadItemImpl* CreateSavePageItem(
      DownloadItemImplDelegate* delegate,
      const FilePath& path,
      const GURL& url,
      content::DownloadId download_id,
      const std::string& mime_type,
      const net::BoundNetLog& bound_net_log) OVERRIDE;

 private:
  std::map<int32, MockDownloadItemImpl*> items_;
  NullDownloadItemImplDelegate item_delegate_;

  DISALLOW_COPY_AND_ASSIGN(MockDownloadItemFactory);
};

MockDownloadItemFactory::MockDownloadItemFactory() {}

MockDownloadItemFactory::~MockDownloadItemFactory() {}

MockDownloadItemImpl* MockDownloadItemFactory::GetItem(int id) {
  if (items_.find(id) == items_.end())
    return NULL;
  return items_[id];
}

MockDownloadItemImpl* MockDownloadItemFactory::PopItem() {
  if (items_.empty())
    return NULL;

  std::map<int32, MockDownloadItemImpl*>::iterator first_item
      = items_.begin();
  MockDownloadItemImpl* result = first_item->second;
  items_.erase(first_item);
  return result;
}

void MockDownloadItemFactory::RemoveItem(int id) {
  DCHECK(items_.find(id) != items_.end());
  items_.erase(id);
}

DownloadItemImpl* MockDownloadItemFactory::CreatePersistedItem(
    DownloadItemImplDelegate* delegate,
    content::DownloadId download_id,
    const content::DownloadPersistentStoreInfo& info,
    const net::BoundNetLog& bound_net_log) {
  int local_id = download_id.local();
  DCHECK(items_.find(local_id) == items_.end());

  MockDownloadItemImpl* result =
      new StrictMock<MockDownloadItemImpl>(&item_delegate_);
  EXPECT_CALL(*result, GetId())
      .WillRepeatedly(Return(local_id));
  items_[local_id] = result;

  return result;
}

DownloadItemImpl* MockDownloadItemFactory::CreateActiveItem(
    DownloadItemImplDelegate* delegate,
    const DownloadCreateInfo& info,
    scoped_ptr<DownloadRequestHandleInterface> request_handle,
    const net::BoundNetLog& bound_net_log) {
  int local_id = info.download_id.local();
  DCHECK(items_.find(local_id) == items_.end());

  MockDownloadItemImpl* result =
      new StrictMock<MockDownloadItemImpl>(&item_delegate_);
  EXPECT_CALL(*result, GetId())
      .WillRepeatedly(Return(local_id));
  EXPECT_CALL(*result, GetGlobalId())
      .WillRepeatedly(Return(content::DownloadId(delegate, local_id)));
  items_[local_id] = result;

  // Active items are created and then immediately are called to start
  // the download.
  EXPECT_CALL(*result, MockStart(_));

  return result;
}

DownloadItemImpl* MockDownloadItemFactory::CreateSavePageItem(
    DownloadItemImplDelegate* delegate,
    const FilePath& path,
    const GURL& url,
    content::DownloadId download_id,
    const std::string& mime_type,
    const net::BoundNetLog& bound_net_log) {
  int local_id = download_id.local();
  DCHECK(items_.find(local_id) == items_.end());

  MockDownloadItemImpl* result =
      new StrictMock<MockDownloadItemImpl>(&item_delegate_);
  EXPECT_CALL(*result, GetId())
      .WillRepeatedly(Return(local_id));
  items_[local_id] = result;

  return result;
}

class MockDownloadFileFactory
    : public content::DownloadFileFactory,
      public base::SupportsWeakPtr<MockDownloadFileFactory> {
 public:
  MockDownloadFileFactory() {}
  virtual ~MockDownloadFileFactory() {}

  // Overridden method from DownloadFileFactory
  MOCK_METHOD8(MockCreateFile, content::DownloadFile*(
    const content::DownloadSaveInfo&,
    GURL, GURL, int64, bool,
    content::ByteStreamReader*,
    const net::BoundNetLog&,
    base::WeakPtr<content::DownloadDestinationObserver>));

  virtual content::DownloadFile* CreateFile(
    const content::DownloadSaveInfo& save_info,
    GURL url,
    GURL referrer_url,
    int64 received_bytes,
    bool calculate_hash,
    scoped_ptr<content::ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    base::WeakPtr<content::DownloadDestinationObserver> observer) {
    return MockCreateFile(save_info, url, referrer_url, received_bytes,
                          calculate_hash, stream.get(), bound_net_log,
                          observer);
  }
};

class MockBrowserContext : public content::BrowserContext {
 public:
  MockBrowserContext() {}
  ~MockBrowserContext() {}

  MOCK_METHOD0(GetPath, FilePath());
  MOCK_CONST_METHOD0(IsOffTheRecord, bool());
  MOCK_METHOD0(GetRequestContext, net::URLRequestContextGetter*());
  MOCK_METHOD1(GetRequestContextForRenderProcess,
               net::URLRequestContextGetter*(int renderer_child_id));
  MOCK_METHOD0(GetMediaRequestContext,
               net::URLRequestContextGetter*());
  MOCK_METHOD1(GetMediaRequestContextForRenderProcess,
               net::URLRequestContextGetter*(int renderer_child_id));
  MOCK_METHOD0(GetResourceContext, content::ResourceContext*());
  MOCK_METHOD0(GetDownloadManagerDelegate, content::DownloadManagerDelegate*());
  MOCK_METHOD0(GetGeolocationPermissionContext,
               content::GeolocationPermissionContext* ());
  MOCK_METHOD0(GetSpeechRecognitionPreferences,
               content::SpeechRecognitionPreferences* ());
  MOCK_METHOD0(DidLastSessionExitCleanly, bool());
  MOCK_METHOD0(GetSpecialStoragePolicy, quota::SpecialStoragePolicy*());
};

class MockDownloadManagerObserver : public content::DownloadManager::Observer {
 public:
  MockDownloadManagerObserver() {}
  ~MockDownloadManagerObserver() {}
  MOCK_METHOD2(OnDownloadCreated, void(
        content::DownloadManager*, content::DownloadItem*));
  MOCK_METHOD1(ModelChanged, void(content::DownloadManager*));
  MOCK_METHOD1(ManagerGoingDown, void(content::DownloadManager*));
  MOCK_METHOD2(SelectFileDialogDisplayed, void(
        content::DownloadManager*, int32));
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

  // Create a MockDownloadItemFactory and MockDownloadManagerDelegate,
  // then create a DownloadManager that points
  // at all of those.
  virtual void SetUp() {
    DCHECK(!download_manager_.get());

    mock_download_item_factory_ = (new MockDownloadItemFactory())->AsWeakPtr();
    mock_download_file_factory_ = (new MockDownloadFileFactory())->AsWeakPtr();
    mock_download_manager_delegate_.reset(
        new StrictMock<MockDownloadManagerDelegate>);
    EXPECT_CALL(*mock_download_manager_delegate_.get(), Shutdown())
        .WillOnce(Return());
    mock_browser_context_.reset(new StrictMock<MockBrowserContext>);
    EXPECT_CALL(*mock_browser_context_.get(), IsOffTheRecord())
        .WillRepeatedly(Return(false));

    download_manager_ = new DownloadManagerImpl(
        scoped_ptr<content::DownloadItemFactory>(
            mock_download_item_factory_.get()).Pass(),
        scoped_ptr<content::DownloadFileFactory>(
            mock_download_file_factory_.get()).Pass(), NULL);
    observer_.reset(new MockDownloadManagerObserver());
    EXPECT_CALL(GetMockObserver(), ModelChanged(download_manager_.get()))
        .WillOnce(Return());
    download_manager_->AddObserver(observer_.get());
    download_manager_->SetDelegate(mock_download_manager_delegate_.get());
    download_manager_->Init(mock_browser_context_.get());
  }

  virtual void TearDown() {
    while (MockDownloadItemImpl*
           item = mock_download_item_factory_->PopItem()) {
      EXPECT_CALL(*item, GetSafetyState())
          .WillOnce(Return(content::DownloadItem::SAFE));
      EXPECT_CALL(*item, IsPartialDownload())
          .WillOnce(Return(false));
    }
    EXPECT_CALL(GetMockObserver(), ManagerGoingDown(download_manager_.get()))
        .WillOnce(Return());

    download_manager_->Shutdown();
    download_manager_ = NULL;
    message_loop_.RunAllPending();
    ASSERT_EQ(NULL, mock_download_item_factory_.get());
    ASSERT_EQ(NULL, mock_download_file_factory_.get());
    message_loop_.RunAllPending();
    mock_download_manager_delegate_.reset();
    mock_browser_context_.reset();
  }

  // Returns download id.
  MockDownloadItemImpl& AddItemToManager() {
    DownloadCreateInfo info;

    static const char* kDownloadIdDomain = "Test download id domain";

    // Args are ignored except for download id, so everything else can be
    // null.
    int id = next_download_id_;
    ++next_download_id_;
    info.download_id = content::DownloadId(kDownloadIdDomain, id);
    info.request_handle = DownloadRequestHandle();
    EXPECT_CALL(GetMockObserver(),
                OnDownloadCreated(download_manager_.get(), _));
    download_manager_->CreateDownloadItem(&info, net::BoundNetLog());

    DCHECK(mock_download_item_factory_->GetItem(id));
    MockDownloadItemImpl& item(*mock_download_item_factory_->GetItem(id));
    // Satisfy expectation.  If the item is created in StartDownload(),
    // we call Start on it immediately, so we need to set that expectation
    // in the factory.
    item.Start(scoped_ptr<content::DownloadFile>());

    return item;
  }

  MockDownloadItemImpl& GetMockDownloadItem(int id) {
    MockDownloadItemImpl* itemp = mock_download_item_factory_->GetItem(id);

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

  // Probe at private internals.
  void DownloadStopped(DownloadItemImpl* item) {
    download_manager_->DownloadStopped(item);
  }

  void DelegateStart(DownloadItemImpl* item) {
    download_manager_->DelegateStart(item);
  }

  void AddItemToHistory(MockDownloadItemImpl& item, int64 db_handle) {
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
  base::WeakPtr<MockDownloadFileFactory> mock_download_file_factory_;

 private:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  base::WeakPtr<MockDownloadItemFactory> mock_download_item_factory_;
  scoped_ptr<MockDownloadManagerDelegate> mock_download_manager_delegate_;
  scoped_ptr<MockBrowserContext> mock_browser_context_;
  scoped_ptr<MockDownloadManagerObserver> observer_;
  int next_download_id_;

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerTest);
};

// Confirm the appropriate invocations occur when you start a download.
TEST_F(DownloadManagerTest, StartDownload) {
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
  scoped_ptr<content::ByteStreamReader> stream;
  int32 local_id(5);                    // Random value

  EXPECT_FALSE(download_manager_->GetActiveDownloadItem(local_id));

  EXPECT_CALL(GetMockObserver(), OnDownloadCreated(download_manager_.get(), _))
      .WillOnce(Return());
  EXPECT_CALL(GetMockDownloadManagerDelegate(), GetNextId())
      .WillOnce(Return(content::DownloadId(this, local_id)));
  EXPECT_CALL(GetMockDownloadManagerDelegate(), GenerateFileHash())
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_download_file_factory_.get(),
              MockCreateFile(Ref(info->save_info), _, _, 0, true, stream.get(),
                             _, _));

  download_manager_->StartDownload(info.Pass(), stream.Pass());
  EXPECT_TRUE(download_manager_->GetActiveDownloadItem(local_id));
}

// Confirm that calling DelegateStart behaves properly if the delegate
// blocks starting.
TEST_F(DownloadManagerTest, DelegateStart_True) {
  // Put a mock we have a handle to on the download manager.
  MockDownloadItemImpl& item(AddItemToManager());

  EXPECT_CALL(GetMockDownloadManagerDelegate(),
              DetermineDownloadTarget(&item, _))
      .WillOnce(Return(true));
  DelegateStart(&item);
}

// Confirm that calling DelegateStart behaves properly if the delegate
// allows starting.  This also tests OnDownloadTargetDetermined.
TEST_F(DownloadManagerTest, DelegateStart_False) {
  // Put a mock we have a handle to on the download manager.
  MockDownloadItemImpl& item(AddItemToManager());

  EXPECT_CALL(GetMockDownloadManagerDelegate(),
              DetermineDownloadTarget(&item, _))
      .WillOnce(Return(false));
  EXPECT_CALL(item, OnDownloadTargetDetermined(
      _, content::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, _));
  FilePath null_path;
  EXPECT_CALL(item, GetForcedFilePath())
      .WillOnce(ReturnRef(null_path));
  DelegateStart(&item);
}

// Does DownloadStopped remove Download from appropriate queues?
// This test tests non-persisted downloads.
TEST_F(DownloadManagerTest, OnDownloadStopped_NonPersisted) {
  // Put a mock we have a handle to on the download manager.
  MockDownloadItemImpl& item(AddItemToManager());

  EXPECT_CALL(item, IsPersisted())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(item, GetState())
      .WillRepeatedly(Return(DownloadItem::CANCELLED));
  EXPECT_CALL(item, GetDbHandle())
      .WillRepeatedly(Return(DownloadItem::kUninitializedHandle));

  DownloadStopped(&item);
  // TODO(rdsmith): Confirm that the download item is no longer on the
  // active list by calling download_manager_->GetActiveDownloadItem(id).
  // Currently, the item is left on the active list for rendez-vous with
  // the history system :-{.
}

// Does DownloadStopped remove Download from appropriate queues?
// This test tests persisted downloads.
TEST_F(DownloadManagerTest, OnDownloadStopped_Persisted) {
  // Put a mock we have a handle to on the download manager.
  MockDownloadItemImpl& item(AddItemToManager());
  int download_id = item.GetId();
  int64 db_handle = 0x7;
  EXPECT_CALL(GetMockObserver(), ModelChanged(download_manager_.get()))
      .WillOnce(Return());
  AddItemToHistory(item, db_handle);

  EXPECT_CALL(item, IsPersisted())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(GetMockDownloadManagerDelegate(),
              UpdateItemInPersistentStore(&item));
  EXPECT_CALL(item, GetState())
      .WillRepeatedly(Return(DownloadItem::CANCELLED));
  EXPECT_CALL(item, GetDbHandle())
      .WillRepeatedly(Return(db_handle));

  DownloadStopped(&item);
  EXPECT_EQ(NULL, download_manager_->GetActiveDownloadItem(download_id));
}
