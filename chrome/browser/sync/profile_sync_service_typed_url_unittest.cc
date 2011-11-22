// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/sync/abstract_profile_sync_service_test.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/sync_backend_host_mock.h"
#include "chrome/browser/sync/glue/typed_url_change_processor.h"
#include "chrome/browser/sync/glue/typed_url_data_type_controller.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "chrome/browser/sync/internal_api/read_node.h"
#include "chrome/browser/sync/internal_api/read_transaction.h"
#include "chrome/browser/sync/internal_api/write_node.h"
#include "chrome/browser/sync/internal_api/write_transaction.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/browser/sync/test/engine/test_id_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/test/base/profile_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
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
  HistoryBackendMock() : HistoryBackend(FilePath(), 0, NULL, NULL) {}
  MOCK_METHOD1(GetAllTypedURLs, bool(std::vector<history::URLRow>* entries));
  MOCK_METHOD3(GetMostRecentVisitsForURL, bool(history::URLID id,
                                               int max_visits,
                                               history::VisitVector* visits));
  MOCK_METHOD2(UpdateURL, bool(history::URLID id, const history::URLRow& url));
  MOCK_METHOD3(AddVisits, bool(const GURL& url,
                               const std::vector<history::VisitInfo>& visits,
                               history::VisitSource visit_source));
  MOCK_METHOD1(RemoveVisits, bool(const history::VisitVector& visits));
  MOCK_METHOD2(GetURL, bool(const GURL& url_id, history::URLRow* url_row));
  MOCK_METHOD2(SetPageTitle, void(const GURL& url, const string16& title));
  MOCK_METHOD1(DeleteURL, void(const GURL& url));
};

class HistoryServiceMock : public HistoryService {
 public:
  HistoryServiceMock() {}
  MOCK_METHOD2(ScheduleDBTask, void(HistoryDBTask*,
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
}

ACTION_P4(MakeTypedUrlSyncComponents, profile, service, hb, dtc) {
  TypedUrlModelAssociator* model_associator =
      new TypedUrlModelAssociator(service, hb);
  TypedUrlChangeProcessor* change_processor =
      new TypedUrlChangeProcessor(profile, model_associator, hb, service);
  return ProfileSyncComponentsFactory::SyncComponents(model_associator,
                                                      change_processor);
}

class ProfileSyncServiceTypedUrlTest : public AbstractProfileSyncServiceTest {
 protected:
  ProfileSyncServiceTypedUrlTest()
      : history_thread_("history") {
  }

  virtual void SetUp() {
    AbstractProfileSyncServiceTest::SetUp();
    profile_.CreateRequestContext();
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
    profile_.ResetRequestContext();
    AbstractProfileSyncServiceTest::TearDown();
  }

  void StartSyncService(Task* task) {
    if (!service_.get()) {
      service_.reset(
          new TestProfileSyncService(&factory_, &profile_, "test", false,
                                     task));
      EXPECT_CALL(profile_, GetProfileSyncService()).WillRepeatedly(
          Return(service_.get()));
      TypedUrlDataTypeController* data_type_controller =
          new TypedUrlDataTypeController(&factory_,
                                         &profile_);

      EXPECT_CALL(factory_, CreateTypedUrlSyncComponents(_, _, _)).
          WillOnce(MakeTypedUrlSyncComponents(&profile_,
                                              service_.get(),
                                              history_backend_.get(),
                                              data_type_controller));
      EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).
          WillOnce(ReturnNewDataTypeManager());

      EXPECT_CALL(profile_, GetHistoryServiceWithoutCreating()).
          WillRepeatedly(Return(history_service_.get()));

      EXPECT_CALL(profile_, GetPasswordStore(_)).
          WillOnce(Return(static_cast<PasswordStore*>(NULL)));

      EXPECT_CALL(profile_, GetHistoryService(_)).
          WillRepeatedly(Return(history_service_.get()));

      token_service_->IssueAuthTokenForTest(
          GaiaConstants::kSyncService, "token");

      EXPECT_CALL(profile_, GetTokenService()).
          WillRepeatedly(Return(token_service_.get()));

      service_->RegisterDataTypeController(data_type_controller);

      service_->Initialize();
      MessageLoop::current()->Run();
    }
  }

  void AddTypedUrlSyncNode(const history::URLRow& url,
                           const history::VisitVector& visits) {
    sync_api::WriteTransaction trans(FROM_HERE, service_->GetUserShare());
    sync_api::ReadNode typed_url_root(&trans);
    ASSERT_TRUE(typed_url_root.InitByTagLookup(browser_sync::kTypedUrlTag));

    sync_api::WriteNode node(&trans);
    std::string tag = url.url().spec();
    ASSERT_TRUE(node.InitUniqueByCreation(syncable::TYPED_URLS,
                                          typed_url_root,
                                          tag));
    TypedUrlModelAssociator::WriteToSyncNode(url, visits, &node);
  }

  void GetTypedUrlsFromSyncDB(std::vector<history::URLRow>* urls) {
    urls->clear();
    sync_api::ReadTransaction trans(FROM_HERE, service_->GetUserShare());
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

      new_url.set_title(UTF8ToUTF16(typed_url.title()));
      DCHECK(typed_url.visits_size());
      DCHECK_EQ(typed_url.visits_size(), typed_url.visit_transitions_size());
      new_url.set_last_visit(base::Time::FromInternalValue(
          typed_url.visits(typed_url.visits_size() - 1)));
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
    // Only verify the fields we explicitly sync (i.e. don't verify typed_count
    // or visit_count because we rely on the history DB to manage those values
    // and they are left unchanged by HistoryBackendMock).
    return (lhs.url().spec().compare(rhs.url().spec()) == 0) &&
           (lhs.title().compare(rhs.title()) == 0) &&
           (lhs.last_visit() == rhs.last_visit()) &&
           (lhs.hidden() == rhs.hidden());
  }

  static history::URLRow MakeTypedUrlEntry(const char* url,
                                           const char* title,
                                           int typed_count,
                                           int64 last_visit,
                                           bool hidden,
                                           history::VisitVector* visits) {
    // Give each URL a unique ID, to mimic the behavior of the real database.
    static int unique_url_id = 0;
    GURL gurl(url);
    URLRow history_url(gurl, ++unique_url_id);
    history_url.set_title(UTF8ToUTF16(title));
    history_url.set_typed_count(typed_count);
    history_url.set_last_visit(
        base::Time::FromInternalValue(last_visit));
    history_url.set_hidden(hidden);
    visits->push_back(history::VisitRow(
        history_url.id(), history_url.last_visit(), 0,
        content::PAGE_TRANSITION_TYPED, 0));
    history_url.set_visit_count(visits->size());
    return history_url;
  }

  friend class AddTypedUrlEntriesTask;
  friend class CreateTypedUrlRootTask;

  Thread history_thread_;
  scoped_refptr<ThreadNotificationService> notification_service_;

  ProfileMock profile_;
  ProfileSyncComponentsFactoryMock factory_;
  scoped_refptr<HistoryBackendMock> history_backend_;
  scoped_refptr<HistoryServiceMock> history_service_;
};

class AddTypedUrlEntriesTask : public Task {
 public:
  AddTypedUrlEntriesTask(ProfileSyncServiceTypedUrlTest* test,
                         const std::vector<history::URLRow>& entries)
      : test_(test), entries_(entries) {
  }

  virtual void Run() {
    test_->CreateRoot(syncable::TYPED_URLS);
    for (size_t i = 0; i < entries_.size(); ++i) {
      history::VisitVector visits;
      visits.push_back(history::VisitRow(
          entries_[i].id(), entries_[i].last_visit(), 0,
          content::PageTransitionFromInt(0), 0));
      test_->AddTypedUrlSyncNode(entries_[i], visits);
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
  CreateRootTask task(this, syncable::TYPED_URLS);
  StartSyncService(&task);
  std::vector<history::URLRow> sync_entries;
  GetTypedUrlsFromSyncDB(&sync_entries);
  EXPECT_EQ(0U, sync_entries.size());
}

TEST_F(ProfileSyncServiceTypedUrlTest, HasNativeEmptySync) {
  std::vector<history::URLRow> entries;
  history::VisitVector visits;
  entries.push_back(MakeTypedUrlEntry("http://foo.com", "bar",
                                      2, 15, false, &visits));

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(visits), Return(true)));
  SetIdleChangeProcessorExpectations();
  CreateRootTask task(this, syncable::TYPED_URLS);
  StartSyncService(&task);
  std::vector<history::URLRow> sync_entries;
  GetTypedUrlsFromSyncDB(&sync_entries);
  ASSERT_EQ(1U, sync_entries.size());
  EXPECT_TRUE(URLsEqual(entries[0], sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, HasNativeHasSyncNoMerge) {
  history::VisitVector native_visits;
  history::VisitVector sync_visits;
  history::URLRow native_entry(MakeTypedUrlEntry("http://native.com", "entry",
                                                 2, 15, false, &native_visits));
  history::URLRow sync_entry(MakeTypedUrlEntry("http://sync.com", "entry",
                                               3, 16, false, &sync_visits));

  std::vector<history::URLRow> native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(native_visits), Return(true)));
  EXPECT_CALL((*history_backend_.get()),
      AddVisits(_, _, history::SOURCE_SYNCED)).WillRepeatedly(Return(true));

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
  history::VisitVector native_visits;
  history::URLRow native_entry(MakeTypedUrlEntry("http://native.com", "entry",
                                                 2, 15, false, &native_visits));
  history::VisitVector sync_visits;
  history::URLRow sync_entry(MakeTypedUrlEntry("http://native.com", "name",
                                               1, 17, false, &sync_visits));
  history::VisitVector merged_visits;
  merged_visits.push_back(history::VisitRow(
      sync_entry.id(), base::Time::FromInternalValue(15), 0,
      content::PageTransitionFromInt(0), 0));

  history::URLRow merged_entry(MakeTypedUrlEntry("http://native.com", "name",
                                                 2, 17, false, &merged_visits));

  std::vector<history::URLRow> native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(native_visits), Return(true)));
  EXPECT_CALL((*history_backend_.get()),
      AddVisits(_, _, history::SOURCE_SYNCED)). WillRepeatedly(Return(true));

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
  history::VisitVector added_visits;
  history::URLRow added_entry(MakeTypedUrlEntry("http://added.com", "entry",
                                                2, 15, false, &added_visits));

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(Return(true));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillOnce(DoAll(SetArgumentPointee<2>(added_visits), Return(true)));

  SetIdleChangeProcessorExpectations();
  CreateRootTask task(this, syncable::TYPED_URLS);
  StartSyncService(&task);

  history::URLsModifiedDetails details;
  details.changed_urls.push_back(added_entry);
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&history_thread_));
  notifier->Notify(chrome::NOTIFICATION_HISTORY_TYPED_URLS_MODIFIED,
                   content::Source<Profile>(&profile_),
                   content::Details<history::URLsModifiedDetails>(&details));

  std::vector<history::URLRow> new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(added_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeUpdate) {
  history::VisitVector original_visits;
  history::URLRow original_entry(MakeTypedUrlEntry("http://mine.com", "entry",
                                                   2, 15, false,
                                                   &original_visits));
  std::vector<history::URLRow> original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillOnce(DoAll(SetArgumentPointee<2>(original_visits),
                     Return(true)));
  CreateRootTask task(this, syncable::TYPED_URLS);
  StartSyncService(&task);

  history::VisitVector updated_visits;
  history::URLRow updated_entry(MakeTypedUrlEntry("http://mine.com", "entry",
                                                  7, 17, false,
                                                  &updated_visits));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillOnce(DoAll(SetArgumentPointee<2>(updated_visits),
                     Return(true)));

  history::URLsModifiedDetails details;
  details.changed_urls.push_back(updated_entry);
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&history_thread_));
  notifier->Notify(chrome::NOTIFICATION_HISTORY_TYPED_URLS_MODIFIED,
                   content::Source<Profile>(&profile_),
                   content::Details<history::URLsModifiedDetails>(&details));

  std::vector<history::URLRow> new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(updated_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeAddFromVisit) {
  history::VisitVector added_visits;
  history::URLRow added_entry(MakeTypedUrlEntry("http://added.com", "entry",
                                                2, 15, false, &added_visits));

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(Return(true));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillOnce(DoAll(SetArgumentPointee<2>(added_visits), Return(true)));

  SetIdleChangeProcessorExpectations();
  CreateRootTask task(this, syncable::TYPED_URLS);
  StartSyncService(&task);

  history::URLVisitedDetails details;
  details.row = added_entry;
  details.transition = content::PAGE_TRANSITION_TYPED;
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&history_thread_));
  notifier->Notify(chrome::NOTIFICATION_HISTORY_URL_VISITED,
                   content::Source<Profile>(&profile_),
                   content::Details<history::URLVisitedDetails>(&details));

  std::vector<history::URLRow> new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(added_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeUpdateFromVisit) {
  history::VisitVector original_visits;
  history::URLRow original_entry(MakeTypedUrlEntry("http://mine.com", "entry",
                                                   2, 15, false,
                                                   &original_visits));
  std::vector<history::URLRow> original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillOnce(DoAll(SetArgumentPointee<2>(original_visits),
                           Return(true)));
  CreateRootTask task(this, syncable::TYPED_URLS);
  StartSyncService(&task);

  history::VisitVector updated_visits;
  history::URLRow updated_entry(MakeTypedUrlEntry("http://mine.com", "entry",
                                                  7, 17, false,
                                                  &updated_visits));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillOnce(DoAll(SetArgumentPointee<2>(updated_visits),
                           Return(true)));

  history::URLVisitedDetails details;
  details.row = updated_entry;
  details.transition = content::PAGE_TRANSITION_TYPED;
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&history_thread_));
  notifier->Notify(chrome::NOTIFICATION_HISTORY_URL_VISITED,
                   content::Source<Profile>(&profile_),
                   content::Details<history::URLVisitedDetails>(&details));

  std::vector<history::URLRow> new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(updated_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserIgnoreChangeUpdateFromVisit) {
  history::VisitVector original_visits;
  history::URLRow original_entry(MakeTypedUrlEntry("http://mine.com", "entry",
                                                   2, 15, false,
                                                   &original_visits));
  std::vector<history::URLRow> original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(original_visits),
                           Return(true)));
  CreateRootTask task(this, syncable::TYPED_URLS);
  StartSyncService(&task);
  std::vector<history::URLRow> new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(original_entry, new_sync_entries[0]));

  history::VisitVector updated_visits;
  history::URLRow updated_entry(MakeTypedUrlEntry("http://mine.com", "entry",
                                                  7, 15, false,
                                                  &updated_visits));
  history::URLVisitedDetails details;
  details.row = updated_entry;

  // Should ignore this change because it's not TYPED.
  details.transition = content::PAGE_TRANSITION_RELOAD;
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&history_thread_));
  notifier->Notify(chrome::NOTIFICATION_HISTORY_URL_VISITED,
                   content::Source<Profile>(&profile_),
                   content::Details<history::URLVisitedDetails>(&details));

  GetTypedUrlsFromSyncDB(&new_sync_entries);

  // Should be no changes to the sync DB from this notification.
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(original_entry, new_sync_entries[0]));

  // Now, try updating it with a large number of visits not divisible by 10
  // (should ignore this visit).
  history::URLRow twelve_visits(MakeTypedUrlEntry("http://mine.com", "entry",
                                                  12, 15, false,
                                                  &updated_visits));
  details.row = twelve_visits;
  details.transition = content::PAGE_TRANSITION_TYPED;
  notifier->Notify(chrome::NOTIFICATION_HISTORY_URL_VISITED,
                   content::Source<Profile>(&profile_),
                   content::Details<history::URLVisitedDetails>(&details));
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  // Should be no changes to the sync DB from this notification.
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(original_entry, new_sync_entries[0]));

  // Now, try updating it with a large number of visits that is divisible by 10
  // (should *not* be ignored).
  history::URLRow twenty_visits(MakeTypedUrlEntry("http://mine.com", "entry",
                                                  20, 15, false,
                                                  &updated_visits));
  details.row = twenty_visits;
  details.transition = content::PAGE_TRANSITION_TYPED;
  notifier->Notify(chrome::NOTIFICATION_HISTORY_URL_VISITED,
                   content::Source<Profile>(&profile_),
                   content::Details<history::URLVisitedDetails>(&details));
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(twenty_visits, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeRemove) {
  history::VisitVector original_visits1;
  history::URLRow original_entry1(MakeTypedUrlEntry("http://mine.com", "entry",
                                                    2, 15, false,
                                                    &original_visits1));
  history::VisitVector original_visits2;
  history::URLRow original_entry2(MakeTypedUrlEntry("http://mine2.com",
                                                    "entry2",
                                                    3, 15, false,
                                                    &original_visits2));
  std::vector<history::URLRow> original_entries;
  original_entries.push_back(original_entry1);
  original_entries.push_back(original_entry2);

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(original_visits1),
                           Return(true)));
  CreateRootTask task(this, syncable::TYPED_URLS);
  StartSyncService(&task);

  history::URLsDeletedDetails changes;
  changes.all_history = false;
  changes.urls.insert(GURL("http://mine.com"));
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&history_thread_));
  notifier->Notify(chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                   content::Source<Profile>(&profile_),
                   content::Details<history::URLsDeletedDetails>(&changes));

  std::vector<history::URLRow> new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(original_entry2, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeRemoveAll) {
  history::VisitVector original_visits1;
  history::URLRow original_entry1(MakeTypedUrlEntry("http://mine.com", "entry",
                                                    2, 15, false,
                                                    &original_visits1));
  history::VisitVector original_visits2;
  history::URLRow original_entry2(MakeTypedUrlEntry("http://mine2.com",
                                                    "entry2",
                                                    3, 15, false,
                                                    &original_visits2));
  std::vector<history::URLRow> original_entries;
  original_entries.push_back(original_entry1);
  original_entries.push_back(original_entry2);

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(original_visits1),
                           Return(true)));
  CreateRootTask task(this, syncable::TYPED_URLS);
  StartSyncService(&task);

  history::URLsDeletedDetails changes;
  changes.all_history = true;
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&history_thread_));
  notifier->Notify(chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                   content::Source<Profile>(&profile_),
                   content::Details<history::URLsDeletedDetails>(&changes));

  std::vector<history::URLRow> new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(0U, new_sync_entries.size());
}

TEST_F(ProfileSyncServiceTypedUrlTest, FailWriteToHistoryBackend) {
  history::VisitVector native_visits;
  history::VisitVector sync_visits;
  history::URLRow native_entry(MakeTypedUrlEntry("http://native.com", "entry",
                                                 2, 15, false, &native_visits));
  history::URLRow sync_entry(MakeTypedUrlEntry("http://sync.com", "entry",
                                               3, 16, false, &sync_visits));

  std::vector<history::URLRow> native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(native_visits), Return(true)));
  EXPECT_CALL((*history_backend_.get()),
      AddVisits(_, _, history::SOURCE_SYNCED)).WillRepeatedly(Return(false));

  std::vector<history::URLRow> sync_entries;
  sync_entries.push_back(sync_entry);
  AddTypedUrlEntriesTask task(this, sync_entries);

  EXPECT_CALL((*history_backend_.get()), UpdateURL(_, _)).
      WillRepeatedly(Return(false));
  StartSyncService(&task);
  ASSERT_TRUE(
      service_->failed_datatypes_handler().GetFailedTypes().count(
          syncable::TYPED_URLS) != 0);
  ASSERT_TRUE(
      service_->failed_datatypes_handler().GetFailedTypes().size() == 1);
}
