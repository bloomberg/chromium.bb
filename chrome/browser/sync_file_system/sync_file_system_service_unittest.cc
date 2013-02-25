// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/sync_file_system/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/mock_remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_event_observer.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/syncable/canned_syncable_file_system.h"
#include "webkit/fileapi/syncable/local_file_sync_context.h"
#include "webkit/fileapi/syncable/mock_sync_status_observer.h"
#include "webkit/fileapi/syncable/sync_callbacks.h"
#include "webkit/fileapi/syncable/sync_file_metadata.h"
#include "webkit/fileapi/syncable/sync_status_code.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

using fileapi::FileSystemURL;
using fileapi::FileSystemURLSet;
using fileapi::MockSyncStatusObserver;
using fileapi::SyncStatusCode;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

namespace sync_file_system {

namespace {

const char kOrigin[] = "http://example.com";
const char kServiceName[] = "test";

template <typename R> struct AssignTrait {
  typedef const R& ArgumentType;
};

template <> struct AssignTrait<SyncFileStatus> {
  typedef SyncFileStatus ArgumentType;
};

template <typename R>
void AssignValueAndQuit(base::RunLoop* run_loop,
                        SyncStatusCode* status_out, R* value_out,
                        SyncStatusCode status,
                        typename AssignTrait<R>::ArgumentType value) {
  DCHECK(status_out);
  DCHECK(value_out);
  DCHECK(run_loop);
  *status_out = status;
  *value_out = value;
  run_loop->Quit();
}

// This is called on IO thread.
void VerifyFileError(base::WaitableEvent* event,
                     base::PlatformFileError error) {
  DCHECK(event);
  EXPECT_EQ(base::PLATFORM_FILE_OK, error);
  event->Signal();
}

}  // namespace

class MockSyncEventObserver : public SyncEventObserver {
 public:
  MockSyncEventObserver() {}
  virtual ~MockSyncEventObserver() {}

  MOCK_METHOD3(OnSyncStateUpdated,
               void(const GURL& app_origin,
                    SyncServiceState state,
                    const std::string& description));
  MOCK_METHOD4(OnFileSynced,
               void(const fileapi::FileSystemURL& url,
                    SyncFileStatus status,
                    SyncAction action,
                    SyncDirection direction));
};

ACTION_P3(NotifyStateAndCallback,
          mock_remote_service, service_state, operation_status) {
  mock_remote_service->NotifyRemoteServiceStateUpdated(
      service_state, "Test event.");
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(arg1, operation_status));
}

ACTION_P(RecordState, states) {
  states->push_back(arg1);
}

ACTION_P(MockStatusCallback, status) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(arg3, status));
}

ACTION_P2(MockSyncFileCallback, status, url) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(arg1, status, url));
}

class SyncFileSystemServiceTest : public testing::Test {
 protected:
  SyncFileSystemServiceTest() {}
  virtual ~SyncFileSystemServiceTest() {}

  virtual void SetUp() OVERRIDE {
    thread_helper_.SetUp();

    file_system_.reset(new fileapi::CannedSyncableFileSystem(
        GURL(kOrigin), kServiceName,
        thread_helper_.io_task_runner(),
        thread_helper_.file_task_runner()));

    local_service_ = new LocalFileSyncService(&profile_);
    remote_service_ = new StrictMock<MockRemoteFileSyncService>;
    sync_service_.reset(new SyncFileSystemService(&profile_));

    EXPECT_CALL(*mock_remote_service(),
                AddServiceObserver(sync_service_.get())).Times(1);
    EXPECT_CALL(*mock_remote_service(),
                AddFileStatusObserver(sync_service_.get())).Times(1);

    sync_service_->Initialize(
        make_scoped_ptr(local_service_),
        scoped_ptr<RemoteFileSyncService>(remote_service_));

    // Disable auto sync by default.
    EXPECT_CALL(*mock_remote_service(), SetSyncEnabled(false)).Times(1);
    sync_service_->SetSyncEnabledForTesting(false);

    file_system_->SetUp();
  }

  virtual void TearDown() OVERRIDE {
    sync_service_->Shutdown();

    file_system_->TearDown();
    fileapi::RevokeSyncableFileSystem(kServiceName);
    thread_helper_.TearDown();
  }

  void InitializeApp() {
    base::RunLoop run_loop;
    SyncStatusCode status = fileapi::SYNC_STATUS_UNKNOWN;

    EXPECT_CALL(*mock_remote_service(),
                RegisterOriginForTrackingChanges(GURL(kOrigin), _)).Times(1);

    sync_service_->InitializeForApp(
        file_system_->file_system_context(),
        kServiceName, GURL(kOrigin),
        AssignAndQuitCallback(&run_loop, &status));
    run_loop.Run();

    EXPECT_EQ(fileapi::SYNC_STATUS_OK, status);
    EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_->OpenFileSystem());
  }

  // Calls InitializeForApp after setting up the mock remote service to
  // perform following when RegisterOriginForTrackingChanges is called:
  //  1. Notify RemoteFileSyncService's observers of |state_to_notify|
  //  2. Run the given callback with |status_to_return|.
  //
  // ..and verifies if following conditions are met:
  //  1. The SyncEventObserver of the service is called with
  //     |expected_states| service state values.
  //  2. InitializeForApp's callback is called with |expected_status|
  //  3. GetCurrentState() is called at least |expected_current_state_calls|
  //     times (which means that the sync service tried to start sync).
  void InitializeAppForObserverTest(
      RemoteServiceState state_to_notify,
      fileapi::SyncStatusCode status_to_return,
      const std::vector<SyncEventObserver::SyncServiceState> expected_states,
      fileapi::SyncStatusCode expected_status,
      int expected_current_state_calls) {
    StrictMock<MockSyncEventObserver> event_observer;
    sync_service_->AddSyncEventObserver(&event_observer);

    EnableSync();

    EXPECT_CALL(*mock_remote_service(),
                RegisterOriginForTrackingChanges(GURL(kOrigin), _))
        .WillOnce(NotifyStateAndCallback(mock_remote_service(),
                                         state_to_notify,
                                         status_to_return));

    if (expected_current_state_calls > 0) {
      EXPECT_CALL(*mock_remote_service(), GetCurrentState())
          .Times(AtLeast(expected_current_state_calls))
          .WillRepeatedly(Return(REMOTE_SERVICE_OK));
    }

    std::vector<SyncEventObserver::SyncServiceState> actual_states;
    EXPECT_CALL(event_observer, OnSyncStateUpdated(GURL(), _, _))
        .WillRepeatedly(RecordState(&actual_states));

    SyncStatusCode actual_status = fileapi::SYNC_STATUS_UNKNOWN;
    base::RunLoop run_loop;
    sync_service_->InitializeForApp(
        file_system_->file_system_context(),
        kServiceName, GURL(kOrigin),
        AssignAndQuitCallback(&run_loop, &actual_status));
    run_loop.Run();

    EXPECT_EQ(expected_status, actual_status);
    ASSERT_EQ(expected_states.size(), actual_states.size());
    for (size_t i = 0; i < actual_states.size(); ++i)
      EXPECT_EQ(expected_states[i], actual_states[i]);
  }

  FileSystemURL URL(const std::string& path) const {
    return file_system_->URL(path);
  }

  StrictMock<MockRemoteFileSyncService>* mock_remote_service() {
    return remote_service_;
  }

  void EnableSync() {
    EXPECT_CALL(*mock_remote_service(), SetSyncEnabled(true)).Times(1);
    sync_service_->SetSyncEnabledForTesting(true);
  }

  MultiThreadTestHelper thread_helper_;
  TestingProfile profile_;
  scoped_ptr<fileapi::CannedSyncableFileSystem> file_system_;

  // Their ownerships are transferred to SyncFileSystemService.
  LocalFileSyncService* local_service_;
  StrictMock<MockRemoteFileSyncService>* remote_service_;

  scoped_ptr<SyncFileSystemService> sync_service_;
};

TEST_F(SyncFileSystemServiceTest, InitializeForApp) {
  InitializeApp();
}

TEST_F(SyncFileSystemServiceTest, InitializeForAppSuccess) {
  std::vector<SyncEventObserver::SyncServiceState> expected_states;
  expected_states.push_back(SyncEventObserver::SYNC_SERVICE_RUNNING);

  InitializeAppForObserverTest(
      REMOTE_SERVICE_OK,
      fileapi::SYNC_STATUS_OK,
      expected_states,
      fileapi::SYNC_STATUS_OK,
      2);
}

TEST_F(SyncFileSystemServiceTest, InitializeForAppWithNetworkFailure) {
  std::vector<SyncEventObserver::SyncServiceState> expected_states;
  expected_states.push_back(
      SyncEventObserver::SYNC_SERVICE_TEMPORARY_UNAVAILABLE);

  // Notify REMOTE_SERVICE_TEMPORARY_UNAVAILABLE and callback with
  // fileapi::SYNC_STATUS_NETWORK_ERROR.  This should let the
  // InitializeApp fail.
  InitializeAppForObserverTest(
      REMOTE_SERVICE_TEMPORARY_UNAVAILABLE,
      fileapi::SYNC_STATUS_NETWORK_ERROR,
      expected_states,
      fileapi::SYNC_STATUS_NETWORK_ERROR,
      0);
}

TEST_F(SyncFileSystemServiceTest, InitializeForAppWithError) {
  std::vector<SyncEventObserver::SyncServiceState> expected_states;
  expected_states.push_back(SyncEventObserver::SYNC_SERVICE_DISABLED);

  // Notify REMOTE_SERVICE_DISABLED and callback with
  // fileapi::SYNC_STATUS_FAILED.  This should let the InitializeApp fail.
  InitializeAppForObserverTest(
      REMOTE_SERVICE_DISABLED,
      fileapi::SYNC_STATUS_FAILED,
      expected_states,
      fileapi::SYNC_STATUS_FAILED,
      0);
}

TEST_F(SyncFileSystemServiceTest, SimpleLocalSyncFlow) {
  InitializeApp();

  StrictMock<MockSyncStatusObserver> status_observer;
  StrictMock<MockLocalChangeProcessor> local_change_processor;

  EnableSync();
  file_system_->file_system_context()->sync_context()->
      set_mock_notify_changes_duration_in_sec(0);
  file_system_->AddSyncStatusObserver(&status_observer);

  // We'll test one local sync for this file.
  const FileSystemURL kFile(file_system_->URL("foo"));

  base::RunLoop run_loop;

  // We should get called OnSyncEnabled and OnWriteEnabled on kFile.
  // (We quit the run loop when OnWriteEnabled is called on kFile)
  EXPECT_CALL(status_observer, OnSyncEnabled(kFile))
      .Times(AtLeast(1));
  EXPECT_CALL(status_observer, OnWriteEnabled(kFile))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  // We expect a set of method calls for starting a local sync.
  EXPECT_CALL(*mock_remote_service(), GetCurrentState())
      .Times(AtLeast(2))
      .WillRepeatedly(Return(REMOTE_SERVICE_OK));
  EXPECT_CALL(*mock_remote_service(), GetLocalChangeProcessor())
      .WillRepeatedly(Return(&local_change_processor));

  // The local_change_processor's ApplyLocalChange should be called once
  // with ADD_OR_UPDATE change for TYPE_FILE.
  const FileChange change(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                          SYNC_FILE_TYPE_FILE);
  EXPECT_CALL(local_change_processor, ApplyLocalChange(change, _, kFile, _))
      .WillOnce(MockStatusCallback(fileapi::SYNC_STATUS_OK));

  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_->CreateFile(kFile));

  run_loop.Run();
}

TEST_F(SyncFileSystemServiceTest, SimpleRemoteSyncFlow) {
  InitializeApp();

  EnableSync();

  base::RunLoop run_loop;

  // We expect a set of method calls for starting a remote sync.
  EXPECT_CALL(*mock_remote_service(), GetCurrentState())
      .Times(AtLeast(2))
      .WillRepeatedly(Return(REMOTE_SERVICE_OK));
  EXPECT_CALL(*mock_remote_service(), ProcessRemoteChange(_, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  // This should trigger a remote sync.
  mock_remote_service()->NotifyRemoteChangeQueueUpdated(1);

  run_loop.Run();
}

TEST_F(SyncFileSystemServiceTest, SimpleSyncFlowWithFileBusy) {
  InitializeApp();

  StrictMock<MockLocalChangeProcessor> local_change_processor;

  EnableSync();
  file_system_->file_system_context()->sync_context()->
      set_mock_notify_changes_duration_in_sec(0);

  const FileSystemURL kFile(file_system_->URL("foo"));

  base::RunLoop run_loop;

  // We expect a set of method calls for starting a remote sync.
  EXPECT_CALL(*mock_remote_service(), GetCurrentState())
      .Times(AtLeast(3))
      .WillRepeatedly(Return(REMOTE_SERVICE_OK));
  EXPECT_CALL(*mock_remote_service(), GetLocalChangeProcessor())
      .WillRepeatedly(Return(&local_change_processor));

  {
    InSequence sequence;

    // Return with SYNC_STATUS_FILE_BUSY once.
    EXPECT_CALL(*mock_remote_service(), ProcessRemoteChange(_, _))
        .WillOnce(MockSyncFileCallback(fileapi::SYNC_STATUS_FILE_BUSY,
                                       kFile));

    // ProcessRemoteChange should be called again when the becomes
    // not busy.
    EXPECT_CALL(*mock_remote_service(), ProcessRemoteChange(_, _))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  }

  // We might also see an activity for local sync as we're going to make
  // a local write operation on kFile.
  EXPECT_CALL(local_change_processor, ApplyLocalChange(_, _, kFile, _))
      .Times(AnyNumber());

  // This should trigger a remote sync.
  mock_remote_service()->NotifyRemoteChangeQueueUpdated(1);

  // Start a local operation on the same file (to make it BUSY).
  base::WaitableEvent event(false, false);
  thread_helper_.io_task_runner()->PostTask(
      FROM_HERE, base::Bind(&fileapi::CannedSyncableFileSystem::DoCreateFile,
                            base::Unretained(file_system_.get()),
                            kFile, base::Bind(&VerifyFileError, &event)));

  run_loop.Run();

  mock_remote_service()->NotifyRemoteChangeQueueUpdated(0);

  event.Wait();
}

TEST_F(SyncFileSystemServiceTest, GetFileSyncStatus) {
  InitializeApp();

  const FileSystemURL kFile(file_system_->URL("foo"));

  fileapi::SyncStatusCode status;
  SyncFileStatus sync_file_status;

  // 1. The file is not in conflicting nor in pending change state.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_remote_service(), IsConflicting(kFile))
        .WillOnce(Return(false));

    status = fileapi::SYNC_STATUS_UNKNOWN;
    sync_file_status = SYNC_FILE_STATUS_UNKNOWN;
    sync_service_->GetFileSyncStatus(
        kFile,
        base::Bind(&AssignValueAndQuit<SyncFileStatus>,
                   &run_loop, &status, &sync_file_status));
    run_loop.Run();

    EXPECT_EQ(fileapi::SYNC_STATUS_OK, status);
    EXPECT_EQ(SYNC_FILE_STATUS_SYNCED, sync_file_status);
  }

  // 2. Conflicting case.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_remote_service(), IsConflicting(kFile))
        .WillOnce(Return(true));

    status = fileapi::SYNC_STATUS_UNKNOWN;
    sync_file_status = SYNC_FILE_STATUS_UNKNOWN;
    sync_service_->GetFileSyncStatus(
        kFile,
        base::Bind(&AssignValueAndQuit<SyncFileStatus>,
                   &run_loop, &status, &sync_file_status));
    run_loop.Run();

    EXPECT_EQ(fileapi::SYNC_STATUS_OK, status);
    EXPECT_EQ(SYNC_FILE_STATUS_CONFLICTING, sync_file_status);
  }

  // 3. The file has pending local changes.
  {
    EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_->CreateFile(kFile));

    base::RunLoop run_loop;
    EXPECT_CALL(*mock_remote_service(), IsConflicting(kFile))
        .WillOnce(Return(false));

    status = fileapi::SYNC_STATUS_UNKNOWN;
    sync_file_status = SYNC_FILE_STATUS_UNKNOWN;
    sync_service_->GetFileSyncStatus(
        kFile,
        base::Bind(&AssignValueAndQuit<SyncFileStatus>,
                   &run_loop, &status, &sync_file_status));
    run_loop.Run();

    EXPECT_EQ(fileapi::SYNC_STATUS_OK, status);
    EXPECT_EQ(SYNC_FILE_STATUS_HAS_PENDING_CHANGES, sync_file_status);
  }

  // 4. The file has a conflict and pending local changes. In this case
  // we return SYNC_FILE_STATUS_CONFLICTING.
  {
    EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_->TruncateFile(kFile, 1U));

    base::RunLoop run_loop;
    EXPECT_CALL(*mock_remote_service(), IsConflicting(kFile))
        .WillOnce(Return(true));

    status = fileapi::SYNC_STATUS_UNKNOWN;
    sync_file_status = SYNC_FILE_STATUS_UNKNOWN;
    sync_service_->GetFileSyncStatus(
        kFile,
        base::Bind(&AssignValueAndQuit<SyncFileStatus>,
                   &run_loop, &status, &sync_file_status));
    run_loop.Run();

    EXPECT_EQ(fileapi::SYNC_STATUS_OK, status);
    EXPECT_EQ(SYNC_FILE_STATUS_CONFLICTING, sync_file_status);
  }
}

}  // namespace sync_file_system
