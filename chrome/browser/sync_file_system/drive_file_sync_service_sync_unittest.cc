// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_service.h"

#include <algorithm>

#include "base/format_macros.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/sync_file_system/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/fake_drive_file_sync_client.h"
#include "chrome/browser/sync_file_system/fake_remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

namespace sync_file_system {

namespace {

const char kSyncRootResourceId[] = "sync_root_resource_id";
const char kParentResourceId[] = "parent_resource_id";
const char kAppId[] = "app-id";
const char kAppOrigin[] = "chrome-extension://app-id";

void DidInitialize(bool* done, SyncStatusCode status, bool created) {
  EXPECT_EQ(SYNC_STATUS_OK, status);
  EXPECT_TRUE(created);
  *done = true;
}

void DidProcessRemoteChange(bool* done,
                            SyncStatusCode* status_out,
                            SyncStatusCode status,
                            const fileapi::FileSystemURL& url) {
  *status_out = status;
  *done = true;
}

}  // namespace

class DriveFileSyncServiceSyncTest : public testing::Test {
 public:
  DriveFileSyncServiceSyncTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        fake_sync_client_(NULL),
        fake_remote_processor_(NULL),
        metadata_store_(NULL),
        resource_count_(0) {
  }

  virtual ~DriveFileSyncServiceSyncTest() {
  }

  virtual void SetUp() OVERRIDE {
    RegisterSyncableFileSystem(DriveFileSyncService::kServiceName);

    ASSERT_TRUE(base_dir_.CreateUniqueTempDir());

    fake_sync_client_ = new FakeDriveFileSyncClient;
    fake_remote_processor_.reset(new FakeRemoteChangeProcessor);

    metadata_store_ = new DriveMetadataStore(
        base_dir_.path(),
        base::MessageLoopProxy::current());

    bool done = false;
    metadata_store_->Initialize(base::Bind(&DidInitialize, &done));
    message_loop_.RunUntilIdle();
    EXPECT_TRUE(done);

    metadata_store_->SetSyncRootDirectory(kSyncRootResourceId);
    metadata_store_->AddBatchSyncOrigin(GURL(kAppOrigin), kParentResourceId);
    metadata_store_->MoveBatchSyncOriginToIncremental(GURL(kAppOrigin));

    sync_service_ = DriveFileSyncService::CreateForTesting(
        &profile_,
        base_dir_.path(),
        scoped_ptr<DriveFileSyncClientInterface>(fake_sync_client_),
        scoped_ptr<DriveMetadataStore>(metadata_store_)).Pass();
  }

  virtual void TearDown() OVERRIDE {
    RevokeSyncableFileSystem(DriveFileSyncService::kServiceName);
    message_loop_.RunUntilIdle();
  }

 protected:
  struct SyncEvent {
    std::string description;
    base::Closure callback;

    SyncEvent(const std::string& description,
              base::Closure callback)
        : description(description),
          callback(callback) {
    }
  };

  SyncEvent CreateRemoteFileAddOrUpdateEvent(const std::string& title) {
    return SyncEvent(
        "SyncEvent: Add or Update remote file [" + title + "]",
        base::Bind(&DriveFileSyncServiceSyncTest::AddOrUpdateResource,
                   base::Unretained(this), title));
  }

  SyncEvent CreateRemoteFileDeleteEvent(const std::string& title) {
    return SyncEvent(
        "SyncEvent: Delete remote file [" + title + "]",
        base::Bind(&DriveFileSyncServiceSyncTest::DeleteResource,
                   base::Unretained(this), title));
  }

  SyncEvent CreateFetchEvent() {
    return SyncEvent(
        "SyncEvent: Fetch remote changes",
        base::Bind(&DriveFileSyncServiceSyncTest::FetchRemoteChange,
                   base::Unretained(this)));
  }

  SyncEvent CreateProcessRemoteChangeEvent() {
    return SyncEvent(
        "SyncEvent: Process remote change",
        base::Bind(
            base::IgnoreResult(
                &DriveFileSyncServiceSyncTest::ProcessRemoteChange),
            base::Unretained(this)));
  }

  template <size_t array_size>
  std::vector<SyncEvent> CreateTestCase(const SyncEvent (&events)[array_size]) {
    return std::vector<SyncEvent>(events, events + array_size);
  }

  void ShuffleTestCase(std::vector<SyncEvent>* events) {
    std::random_shuffle(events->begin(), events->end(), base::RandGenerator);
  }

  void RunTest(const std::vector<SyncEvent>& events) {
    typedef std::vector<SyncEvent>::const_iterator iterator;
    std::ostringstream out;
    for (iterator itr = events.begin(); itr != events.end(); ++itr)
      out << itr->description << '\n';
    SCOPED_TRACE(out.str());

    for (iterator itr = events.begin(); itr != events.end(); ++itr)
      itr->callback.Run();

    FetchRemoteChange();
    while (ProcessRemoteChange()) {}

    VerifyResult();
  }

 private:
  void AddOrUpdateResource(const std::string& title) {
    typedef ResourceIdByTitle::iterator iterator;
    std::pair<iterator, bool> inserted =
        resources_.insert(std::make_pair(title, std::string()));
    if (inserted.second)
      inserted.first->second = StringPrintf("%" PRId64, ++resource_count_);
    std::string resource_id = inserted.first->second;

    fake_sync_client_->PushRemoteChange(
        kParentResourceId, kAppId, title, resource_id,
        StringPrintf("%" PRIx64, base::RandUint64()),
        false /* deleted */);
    message_loop_.RunUntilIdle();
  }

  void DeleteResource(const std::string& title) {
    ResourceIdByTitle::iterator found = resources_.find(title);
    if (found == resources_.end())
      return;
    std::string resource_id = found->second;
    resources_.erase(found);
    fake_sync_client_->PushRemoteChange(
        kParentResourceId, kAppId,
        title, resource_id, std::string(), true /* deleted */);
    message_loop_.RunUntilIdle();
  }

  void FetchRemoteChange() {
    sync_service_->may_have_unfetched_changes_ = true;
    sync_service_->MaybeStartFetchChanges();
    message_loop_.RunUntilIdle();
  }

  bool ProcessRemoteChange() {
    bool done = false;
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    sync_service_->ProcessRemoteChange(
        fake_remote_processor_.get(),
        base::Bind(&DidProcessRemoteChange, &done, &status));
    message_loop_.RunUntilIdle();
    EXPECT_TRUE(done);
    return status == SYNC_STATUS_OK;
  }


  void VerifyResult() {
    typedef FakeDriveFileSyncClient::RemoteResourceByResourceId
        RemoteResourceMap;
    typedef FakeDriveFileSyncClient::RemoteResource RemoteResource;
    typedef FakeRemoteChangeProcessor::URLToFileChangesMap
        AppliedRemoteChangeMap;

    const RemoteResourceMap& remote_resources =
        fake_sync_client_->remote_resources();
    const AppliedRemoteChangeMap applied_changes =
        fake_remote_processor_->GetAppliedRemoteChanges();

    std::set<std::string> local_resources;
    for (AppliedRemoteChangeMap::const_iterator itr = applied_changes.begin();
         itr != applied_changes.end(); ++itr) {
      const fileapi::FileSystemURL& url = itr->first;
      const FileChange& applied_change = itr->second.back();

      DriveMetadata metadata;
      SyncStatusCode status = metadata_store_->ReadEntry(itr->first, &metadata);
      if (applied_change.IsDelete()) {
        EXPECT_EQ(SYNC_DATABASE_ERROR_NOT_FOUND, status);
        continue;
      }

      EXPECT_EQ(SYNC_STATUS_OK, status);
      EXPECT_FALSE(metadata.resource_id().empty());
      EXPECT_FALSE(metadata.conflicted());
      EXPECT_FALSE(metadata.to_be_fetched());
      EXPECT_FALSE(metadata.md5_checksum().empty());

      RemoteResourceMap::const_iterator found =
          remote_resources.find(metadata.resource_id());
      ASSERT_TRUE(found != remote_resources.end());
      const RemoteResource& remote_resource = found->second;

      EXPECT_EQ(base::FilePath::FromUTF8Unsafe(remote_resource.title),
                url.path());
      EXPECT_EQ(remote_resource.md5_checksum, metadata.md5_checksum());
      EXPECT_FALSE(remote_resource.deleted);

      EXPECT_TRUE(local_resources.insert(metadata.resource_id()).second);
    }

    for (RemoteResourceMap::const_iterator itr = remote_resources.begin();
         itr != remote_resources.end(); ++itr) {
      if (!itr->second.deleted)
        EXPECT_TRUE(ContainsKey(local_resources, itr->first));
      else
        EXPECT_FALSE(ContainsKey(local_resources, itr->first));
    }
  }

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  TestingProfile profile_;
  base::ScopedTempDir base_dir_;

  FakeDriveFileSyncClient* fake_sync_client_;
  scoped_ptr<FakeRemoteChangeProcessor> fake_remote_processor_;
  DriveMetadataStore* metadata_store_;

  scoped_ptr<DriveFileSyncService> sync_service_;

  typedef std::map<std::string, std::string> ResourceIdByTitle;
  ResourceIdByTitle resources_;
  int64 resource_count_;

  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncServiceSyncTest);
};

TEST_F(DriveFileSyncServiceSyncTest, AddFileTest) {
  std::string kFile1 = "file title 1";
  SyncEvent sync_event[] = {
    CreateRemoteFileAddOrUpdateEvent(kFile1),
  };

  RunTest(CreateTestCase(sync_event));
}

TEST_F(DriveFileSyncServiceSyncTest, UpdateFileTest) {
  std::string kFile1 = "file title 1";
  SyncEvent sync_event[] = {
    CreateRemoteFileAddOrUpdateEvent(kFile1),
    CreateRemoteFileAddOrUpdateEvent(kFile1),
  };

  RunTest(CreateTestCase(sync_event));
}


TEST_F(DriveFileSyncServiceSyncTest, DeleteFileTest) {
  std::string kFile1 = "file title 1";
  SyncEvent sync_event[] = {
    CreateRemoteFileAddOrUpdateEvent(kFile1),
    CreateRemoteFileDeleteEvent(kFile1),
  };

  RunTest(CreateTestCase(sync_event));
}

}  // namespace sync_file_system
