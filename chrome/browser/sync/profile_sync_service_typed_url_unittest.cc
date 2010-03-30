// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "base/ref_counted.h"
#include "base/string16.h"
#include "base/thread.h"
#include "base/time.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/sync_backend_host_mock.h"
#include "chrome/browser/sync/glue/typed_url_change_processor.h"
#include "chrome/browser/sync/glue/typed_url_data_type_controller.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/profile_sync_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/profile_mock.h"
#include "chrome/test/sync/engine/test_id_factory.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::Time;
using base::Thread;
using browser_sync::SyncBackendHost;
using browser_sync::SyncBackendHostMock;
using browser_sync::TestIdFactory;
using browser_sync::TypedUrlChangeProcessor;
using browser_sync::TypedUrlDataTypeController;
using browser_sync::TypedUrlModelAssociator;
using browser_sync::UnrecoverableErrorHandler;
using history::HistoryBackend;
using history::URLID;
using history::URLRow;
using sync_api::SyncManager;
using sync_api::UserShare;
using syncable::BASE_VERSION;
using syncable::CREATE;
using syncable::DirectoryManager;
using syncable::ID;
using syncable::IS_DEL;
using syncable::IS_DIR;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::IS_UNSYNCED;
using syncable::MutableEntry;
using syncable::SERVER_IS_DIR;
using syncable::SERVER_VERSION;
using syncable::SPECIFICS;
using syncable::ScopedDirLookup;
using syncable::UNIQUE_SERVER_TAG;
using syncable::UNITTEST;
using syncable::WriteTransaction;
using testing::_;
using testing::DoAll;
using testing::DoDefault;
using testing::Invoke;
using testing::Return;
using testing::SetArgumentPointee;
using testing::WithArgs;

class HistoryBackendMock : public HistoryBackend {
 public:
  HistoryBackendMock() : HistoryBackend(FilePath(), NULL, NULL) {}
  MOCK_METHOD1(GetAllTypedURLs, bool(std::vector<history::URLRow>* entries));
  MOCK_METHOD2(UpdateURL, bool(history::URLID id, history::URLRow& url));
  MOCK_METHOD2(SetPageTitle, void(const GURL& url, const std::wstring& title));
  MOCK_METHOD2(GetURL, bool(const GURL& url_id, history::URLRow* url_row));
  MOCK_METHOD1(DeleteURL, void(const GURL& url));
};

class HistoryServiceMock : public HistoryService {
 public:
  HistoryServiceMock() {}
  MOCK_METHOD2(ScheduleDBTask, Handle(HistoryDBTask*,
                                      CancelableRequestConsumerBase*));
};

class RunOnDBThreadTask : public Task {
 public:
  RunOnDBThreadTask(HistoryBackend* backend, HistoryDBTask* task)
      : backend_(backend), task_(task) {}
  virtual void Run() {
    task_->RunOnDBThread(backend_, NULL);
    task_ = NULL;
  }
 private:
  HistoryBackend* backend_;
  scoped_refptr<HistoryDBTask> task_;
};

ACTION_P2(RunTaskOnDBThread, thread, backend) {
 thread->message_loop()->PostTask(
    FROM_HERE,
    new RunOnDBThreadTask(backend, arg0));
 return 0;
}

ACTION_P3(MakeTypedUrlSyncComponents, service, hb, dtc) {
  TypedUrlModelAssociator* model_associator =
      new TypedUrlModelAssociator(service, hb, dtc);
  TypedUrlChangeProcessor* change_processor =
      new TypedUrlChangeProcessor(model_associator, hb, service);
  return ProfileSyncFactory::SyncComponents(model_associator, change_processor);
}

class ProfileSyncServiceTypedUrlTest : public testing::Test {
 protected:
  ProfileSyncServiceTypedUrlTest()
      : ui_thread_(ChromeThread::UI, &message_loop_),
        history_thread_("history") {
  }

  virtual void SetUp() {
    history_backend_ = new HistoryBackendMock();
    history_service_ = new HistoryServiceMock();
    EXPECT_CALL((*history_service_.get()), ScheduleDBTask(_, _))
        .WillRepeatedly(RunTaskOnDBThread(&history_thread_,
                                          history_backend_.get()));
    history_thread_.Start();

    notification_service_ =
      new ThreadNotificationService(&history_thread_);
    notification_service_->Init();
  }

  virtual void TearDown() {
    history_backend_ = NULL;
    history_service_ = NULL;
    service_.reset();
    notification_service_->TearDown();
    history_thread_.Stop();
    MessageLoop::current()->RunAllPending();
  }

  void StartSyncService(Task* task) {
    if (!service_.get()) {
      service_.reset(
          new TestingProfileSyncService(&factory_, &profile_, false));
      service_->AddObserver(&observer_);
      TypedUrlDataTypeController* data_type_controller =
          new TypedUrlDataTypeController(&factory_,
                                         &profile_,
                                         service_.get());

      EXPECT_CALL(factory_, CreateTypedUrlSyncComponents(_, _, _)).
          WillOnce(MakeTypedUrlSyncComponents(service_.get(),
                                              history_backend_.get(),
                                              data_type_controller));
      EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).
          WillOnce(MakeDataTypeManager(&backend_mock_));

      EXPECT_CALL(profile_, GetHistoryServiceWithoutCreating()).
          WillRepeatedly(Return(history_service_.get()));

      EXPECT_CALL(profile_, GetHistoryService(_)).
          WillRepeatedly(Return(history_service_.get()));

      // State changes once for the backend init and once for startup done.
      EXPECT_CALL(observer_, OnStateChanged()).
          WillOnce(InvokeTask(task)).
          WillOnce(QuitUIMessageLoop());
      service_->RegisterDataTypeController(data_type_controller);
      service_->Initialize();
      MessageLoop::current()->Run();
    }
  }

  void CreateTypedUrlRoot() {
    UserShare* user_share = service_->backend()->GetUserShareHandle();
    DirectoryManager* dir_manager = user_share->dir_manager.get();

    ScopedDirLookup dir(dir_manager, user_share->authenticated_name);
    if (!dir.good())
      return;

    WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
    MutableEntry node(&wtrans,
                      CREATE,
                      wtrans.root_id(),
                      browser_sync::kTypedUrlTag);
    node.Put(UNIQUE_SERVER_TAG, browser_sync::kTypedUrlTag);
    node.Put(IS_DIR, true);
    node.Put(SERVER_IS_DIR, false);
    node.Put(IS_UNSYNCED, false);
    node.Put(IS_UNAPPLIED_UPDATE, false);
    node.Put(SERVER_VERSION, 20);
    node.Put(BASE_VERSION, 20);
    node.Put(IS_DEL, false);
    node.Put(ID, ids_.MakeServer(browser_sync::kTypedUrlTag));
    sync_pb::EntitySpecifics specifics;
    specifics.MutableExtension(sync_pb::typed_url);
    node.Put(SPECIFICS, specifics);
  }

  void AddTypedUrlSyncNode(const history::URLRow& url) {
    sync_api::WriteTransaction trans(
        service_->backend()->GetUserShareHandle());
    sync_api::ReadNode typed_url_root(&trans);
    ASSERT_TRUE(typed_url_root.InitByTagLookup(browser_sync::kTypedUrlTag));

    sync_api::WriteNode node(&trans);
    std::string tag = url.url().spec();
    ASSERT_TRUE(node.InitUniqueByCreation(syncable::TYPED_URLS,
                                          typed_url_root,
                                          tag));
    TypedUrlModelAssociator::WriteToSyncNode(url, &node);
  }

  void GetTypedUrlsFromSyncDB(std::vector<history::URLRow>* urls) {
    sync_api::ReadTransaction trans(service_->backend()->GetUserShareHandle());
    sync_api::ReadNode typed_url_root(&trans);
    if (!typed_url_root.InitByTagLookup(browser_sync::kTypedUrlTag))
      return;

    int64 child_id = typed_url_root.GetFirstChildId();
    while (child_id != sync_api::kInvalidId) {
      sync_api::ReadNode child_node(&trans);
      if (!child_node.InitByIdLookup(child_id))
        return;

      const sync_pb::TypedUrlSpecifics& typed_url(
          child_node.GetTypedUrlSpecifics());
      history::URLRow new_url(GURL(typed_url.url()));

      new_url.set_title(UTF8ToWide(typed_url.title()));
      new_url.set_visit_count(typed_url.visit_count());
      new_url.set_typed_count(typed_url.typed_count());
      new_url.set_last_visit(
          base::Time::FromInternalValue(typed_url.last_visit()));
      new_url.set_hidden(typed_url.hidden());

      urls->push_back(new_url);
      child_id = child_node.GetSuccessorId();
    }
  }

  void SetIdleChangeProcessorExpectations() {
    EXPECT_CALL((*history_backend_.get()), SetPageTitle(_, _)).Times(0);
    EXPECT_CALL((*history_backend_.get()), UpdateURL(_, _)).Times(0);
    EXPECT_CALL((*history_backend_.get()), GetURL(_, _)).Times(0);
    EXPECT_CALL((*history_backend_.get()), DeleteURL(_)).Times(0);
  }

  static bool URLsEqual(history::URLRow& lhs, history::URLRow& rhs) {
    return (lhs.url().spec().compare(rhs.url().spec()) == 0) &&
           (lhs.title().compare(rhs.title()) == 0) &&
           (lhs.visit_count() == rhs.visit_count()) &&
           (lhs.typed_count() == rhs.typed_count()) &&
           (lhs.last_visit() == rhs.last_visit()) &&
           (lhs.hidden() == rhs.hidden());
  }

  static history::URLRow MakeTypedUrlEntry(const char* url,
                                           const char* title,
                                           int visit_count,
                                           int typed_count,
                                           int64 last_visit,
                                           bool hidden) {
    GURL gurl(url);
    URLRow history_url(gurl);
    history_url.set_title(UTF8ToWide(title));
    history_url.set_visit_count(visit_count);
    history_url.set_typed_count(typed_count);
    history_url.set_last_visit(
        base::Time::FromInternalValue(last_visit));
    history_url.set_hidden(hidden);
    return history_url;
  }

  friend class AddTypedUrlEntriesTask;
  friend class CreateTypedUrlRootTask;

  MessageLoopForUI message_loop_;
  ChromeThread ui_thread_;
  Thread history_thread_;
  scoped_refptr<ThreadNotificationService> notification_service_;

  scoped_ptr<TestingProfileSyncService> service_;
  ProfileMock profile_;
  ProfileSyncFactoryMock factory_;
  SyncBackendHostMock backend_mock_;
  ProfileSyncServiceObserverMock observer_;
  scoped_refptr<HistoryBackendMock> history_backend_;
  scoped_refptr<HistoryServiceMock> history_service_;

  TestIdFactory ids_;
};

class CreateTypedUrlRootTask : public Task {
 public:
  explicit CreateTypedUrlRootTask(ProfileSyncServiceTypedUrlTest* test)
      : test_(test) {
  }

  virtual void Run() {
    test_->CreateTypedUrlRoot();
  }

 private:
  ProfileSyncServiceTypedUrlTest* test_;
};

class AddTypedUrlEntriesTask : public Task {
 public:
  AddTypedUrlEntriesTask(ProfileSyncServiceTypedUrlTest* test,
                         const std::vector<history::URLRow>& entries)
      : test_(test), entries_(entries) {
  }

  virtual void Run() {
    test_->CreateTypedUrlRoot();
    for (size_t i = 0; i < entries_.size(); ++i) {
      test_->AddTypedUrlSyncNode(entries_[i]);
    }
  }

 private:
  ProfileSyncServiceTypedUrlTest* test_;
  const std::vector<history::URLRow>& entries_;
};

TEST_F(ProfileSyncServiceTypedUrlTest, EmptyNativeEmptySync) {
  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(Return(true));
  SetIdleChangeProcessorExpectations();
  CreateTypedUrlRootTask task(this);
  StartSyncService(&task);
  std::vector<history::URLRow> sync_entries;
  GetTypedUrlsFromSyncDB(&sync_entries);
  EXPECT_EQ(0U, sync_entries.size());
}

TEST_F(ProfileSyncServiceTypedUrlTest, HasNativeEmptySync) {
  std::vector<history::URLRow> entries;
  entries.push_back(MakeTypedUrlEntry("http://foo.com", "bar",
                                      1, 2, 15, false));
  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(entries), Return(true)));
  SetIdleChangeProcessorExpectations();
  CreateTypedUrlRootTask task(this);
  StartSyncService(&task);
  std::vector<history::URLRow> sync_entries;
  GetTypedUrlsFromSyncDB(&sync_entries);
  ASSERT_EQ(1U, entries.size());
  EXPECT_TRUE(URLsEqual(entries[0], sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, HasNativeHasSyncNoMerge) {
  history::URLRow native_entry(MakeTypedUrlEntry("http://native.com", "entry",
                                                 1, 2, 15, false));
  history::URLRow sync_entry(MakeTypedUrlEntry("http://sync.com", "entry",
                                               2, 3, 16, false));

  std::vector<history::URLRow> native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));

  std::vector<history::URLRow> sync_entries;
  sync_entries.push_back(sync_entry);
  AddTypedUrlEntriesTask task(this, sync_entries);

  EXPECT_CALL((*history_backend_.get()), UpdateURL(_, _)).
      WillRepeatedly(Return(true));
  StartSyncService(&task);

  std::map<std::string, history::URLRow> expected;
  expected[native_entry.url().spec()] = native_entry;
  expected[sync_entry.url().spec()] = sync_entry;

  std::vector<history::URLRow> new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);

  EXPECT_TRUE(new_sync_entries.size() == expected.size());
  for (std::vector<history::URLRow>::iterator entry = new_sync_entries.begin();
       entry != new_sync_entries.end(); ++entry) {
    EXPECT_TRUE(URLsEqual(expected[entry->url().spec()], *entry));
  }
}

TEST_F(ProfileSyncServiceTypedUrlTest, HasNativeHasSyncMerge) {
  history::URLRow native_entry(MakeTypedUrlEntry("http://native.com", "entry",
                                                 1, 2, 15, false));
  history::URLRow sync_entry(MakeTypedUrlEntry("http://native.com", "name",
                                               2, 1, 17, false));
  history::URLRow merged_entry(MakeTypedUrlEntry("http://native.com", "name",
                                                 2, 1, 17, false));

  std::vector<history::URLRow> native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));

  std::vector<history::URLRow> sync_entries;
  sync_entries.push_back(sync_entry);
  AddTypedUrlEntriesTask task(this, sync_entries);

  EXPECT_CALL((*history_backend_.get()), UpdateURL(_, _)).
      WillRepeatedly(Return(true));
  EXPECT_CALL((*history_backend_.get()), SetPageTitle(_, _)).
      WillRepeatedly(Return());
  StartSyncService(&task);

  std::vector<history::URLRow> new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(merged_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeAdd) {
  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(Return(true));
  SetIdleChangeProcessorExpectations();
  CreateTypedUrlRootTask task(this);
  StartSyncService(&task);

  history::URLRow added_entry(MakeTypedUrlEntry("http://added.com", "entry",
                                                1, 2, 15, false));

  history::URLsModifiedDetails details;
  details.changed_urls.push_back(added_entry);
  scoped_refptr<ThreadNotifier> notifier = new ThreadNotifier(&history_thread_);
  notifier->Notify(NotificationType::HISTORY_TYPED_URLS_MODIFIED,
                   Details<history::URLsModifiedDetails>(&details));

  std::vector<history::URLRow> new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(added_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeUpdate) {
  history::URLRow original_entry(MakeTypedUrlEntry("http://mine.com", "entry",
                                                   1, 2, 15, false));
  std::vector<history::URLRow> original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  CreateTypedUrlRootTask task(this);
  StartSyncService(&task);

  history::URLRow updated_entry(MakeTypedUrlEntry("http://mine.com", "entry",
                                                  3, 7, 19, false));

  history::URLsModifiedDetails details;
  details.changed_urls.push_back(updated_entry);
  scoped_refptr<ThreadNotifier> notifier = new ThreadNotifier(&history_thread_);
  notifier->Notify(NotificationType::HISTORY_TYPED_URLS_MODIFIED,
                   Details<history::URLsModifiedDetails>(&details));

  std::vector<history::URLRow> new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(updated_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeRemove) {
  history::URLRow original_entry1(MakeTypedUrlEntry("http://mine.com", "entry",
                                                    1, 2, 15, false));
  history::URLRow original_entry2(MakeTypedUrlEntry("http://mine2.com",
                                                    "entry2",
                                                    2, 3, 17, false));
  std::vector<history::URLRow> original_entries;
  original_entries.push_back(original_entry1);
  original_entries.push_back(original_entry2);

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  CreateTypedUrlRootTask task(this);
  StartSyncService(&task);

  history::URLsDeletedDetails changes;
  changes.all_history = false;
  changes.urls.insert(GURL("http://mine.com"));
  scoped_refptr<ThreadNotifier> notifier = new ThreadNotifier(&history_thread_);
  notifier->Notify(NotificationType::HISTORY_URLS_DELETED,
                   Details<history::URLsDeletedDetails>(&changes));

  std::vector<history::URLRow> new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(original_entry2, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeRemoveAll) {
  history::URLRow original_entry1(MakeTypedUrlEntry("http://mine.com", "entry",
                                                    1, 2, 15, false));
  history::URLRow original_entry2(MakeTypedUrlEntry("http://mine2.com",
                                                    "entry2",
                                                    2, 3, 17, false));
  std::vector<history::URLRow> original_entries;
  original_entries.push_back(original_entry1);
  original_entries.push_back(original_entry2);

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  CreateTypedUrlRootTask task(this);
  StartSyncService(&task);

  history::URLsDeletedDetails changes;
  changes.all_history = true;
  scoped_refptr<ThreadNotifier> notifier = new ThreadNotifier(&history_thread_);
  notifier->Notify(NotificationType::HISTORY_URLS_DELETED,
                   Details<history::URLsDeletedDetails>(&changes));

  std::vector<history::URLRow> new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(0U, new_sync_entries.size());
}
