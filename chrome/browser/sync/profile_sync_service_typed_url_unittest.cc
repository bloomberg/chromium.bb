// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/abstract_profile_sync_service_test.h"
#include "chrome/browser/sync/glue/data_type_error_handler_mock.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/typed_url_change_processor.h"
#include "chrome/browser/sync/glue/typed_url_data_type_controller.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/test/base/profile_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "googleurl/src/gurl.h"
#include "sync/internal_api/read_node.h"
#include "sync/internal_api/read_transaction.h"
#include "sync/internal_api/write_node.h"
#include "sync/internal_api/write_transaction.h"
#include "sync/protocol/typed_url_specifics.pb.h"
#include "sync/syncable/syncable.h"
#include "sync/test/engine/test_id_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::Time;
using base::Thread;
using browser_sync::SyncBackendHost;
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
using syncable::IS_DEL;
using syncable::IS_DIR;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::IS_UNSYNCED;
using syncable::MutableEntry;
using syncable::SERVER_IS_DIR;
using syncable::SERVER_VERSION;
using syncable::SPECIFICS;
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

namespace {
// Visits with this timestamp are treated as expired.
static const int EXPIRED_VISIT = -1;

class HistoryBackendMock : public HistoryBackend {
 public:
  HistoryBackendMock() : HistoryBackend(FilePath(), 0, NULL, NULL) {}
  virtual bool IsExpiredVisitTime(const base::Time& time) OVERRIDE {
    return time.ToInternalValue() == EXPIRED_VISIT;
  }
  MOCK_METHOD1(GetAllTypedURLs, bool(history::URLRows* entries));
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

 private:
  virtual ~HistoryBackendMock() {}
};

class HistoryServiceMock : public HistoryService {
 public:
  HistoryServiceMock() {}
  MOCK_METHOD2(ScheduleDBTask, void(HistoryDBTask*,
                                    CancelableRequestConsumerBase*));

 private:
  virtual ~HistoryServiceMock() {}
};

class TestTypedUrlModelAssociator : public TypedUrlModelAssociator {
 public:
  TestTypedUrlModelAssociator(
      ProfileSyncService* sync_service,
      history::HistoryBackend* history_backend,
      browser_sync::DataTypeErrorHandler* error_handler) :
      TypedUrlModelAssociator(sync_service, history_backend, error_handler) {}

 protected:
  // Don't clear error stats - that way we can verify their values in our
  // tests.
  virtual void ClearErrorStats() {}
};

void RunOnDBThreadCallback(HistoryBackend* backend,
                           HistoryDBTask* task) {
  task->RunOnDBThread(backend, NULL);
}

ACTION_P2(RunTaskOnDBThread, thread, backend) {
  // ScheduleDBTask takes ownership of its task argument, so we
  // should, too.
  scoped_refptr<HistoryDBTask> task(arg0);
  thread->message_loop()->PostTask(
      FROM_HERE, base::Bind(&RunOnDBThreadCallback, base::Unretained(backend),
                            task));
}

ACTION_P6(MakeTypedUrlSyncComponents,
              profile,
              service,
              hb,
              dtc,
              error_handler,
              model_associator) {
  *model_associator =
      new TestTypedUrlModelAssociator(service, hb, error_handler);
  TypedUrlChangeProcessor* change_processor =
      new TypedUrlChangeProcessor(profile, *model_associator, hb, dtc);
  return ProfileSyncComponentsFactory::SyncComponents(*model_associator,
                                                      change_processor);
}

class ProfileSyncServiceTypedUrlTest : public AbstractProfileSyncServiceTest {
 public:
  void AddTypedUrlSyncNode(const history::URLRow& url,
                           const history::VisitVector& visits) {
    sync_api::WriteTransaction trans(FROM_HERE, service_->GetUserShare());
    sync_api::ReadNode typed_url_root(&trans);
    ASSERT_EQ(sync_api::BaseNode::INIT_OK,
              typed_url_root.InitByTagLookup(browser_sync::kTypedUrlTag));

    sync_api::WriteNode node(&trans);
    std::string tag = url.url().spec();
    sync_api::WriteNode::InitUniqueByCreationResult result =
        node.InitUniqueByCreation(syncable::TYPED_URLS, typed_url_root, tag);
    ASSERT_EQ(sync_api::WriteNode::INIT_SUCCESS, result);
    TypedUrlModelAssociator::WriteToSyncNode(url, visits, &node);
  }

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
  }

  virtual void TearDown() {
    history_backend_ = NULL;
    history_service_ = NULL;
    service_.reset();
    history_thread_.Stop();
    profile_.ResetRequestContext();
    AbstractProfileSyncServiceTest::TearDown();
  }

  TypedUrlModelAssociator* StartSyncService(const base::Closure& callback) {
    TypedUrlModelAssociator* model_associator = NULL;
    if (!service_.get()) {
      SigninManager* signin = SigninManagerFactory::GetForProfile(&profile_);
      signin->SetAuthenticatedUsername("test");
      token_service_ = static_cast<TokenService*>(
          TokenServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile_, BuildTokenService));
      ProfileSyncComponentsFactoryMock* factory =
          new ProfileSyncComponentsFactoryMock();
      service_.reset(
          new TestProfileSyncService(factory,
                                     &profile_,
                                     signin,
                                     ProfileSyncService::AUTO_START,
                                     false,
                                     callback));
      TypedUrlDataTypeController* data_type_controller =
          new TypedUrlDataTypeController(factory,
                                         &profile_,
                                         service_.get());

      EXPECT_CALL(*factory, CreateTypedUrlSyncComponents(_, _, _)).
          WillOnce(MakeTypedUrlSyncComponents(&profile_,
                                              service_.get(),
                                              history_backend_.get(),
                                              data_type_controller,
                                              &error_handler_,
                                              &model_associator));
      EXPECT_CALL(*factory, CreateDataTypeManager(_, _)).
          WillOnce(ReturnNewDataTypeManager());

      EXPECT_CALL(profile_, GetHistoryServiceWithoutCreating()).
          WillRepeatedly(Return(history_service_.get()));

      EXPECT_CALL(profile_, GetHistoryService(_)).
          WillRepeatedly(Return(history_service_.get()));

      token_service_->IssueAuthTokenForTest(
          GaiaConstants::kSyncService, "token");

      service_->RegisterDataTypeController(data_type_controller);

      service_->Initialize();
      MessageLoop::current()->Run();
    }
    return model_associator;
  }

  void GetTypedUrlsFromSyncDB(history::URLRows* urls) {
    urls->clear();
    sync_api::ReadTransaction trans(FROM_HERE, service_->GetUserShare());
    sync_api::ReadNode typed_url_root(&trans);
    if (typed_url_root.InitByTagLookup(browser_sync::kTypedUrlTag) !=
            sync_api::BaseNode::INIT_OK)
      return;

    int64 child_id = typed_url_root.GetFirstChildId();
    while (child_id != sync_api::kInvalidId) {
      sync_api::ReadNode child_node(&trans);
      if (child_node.InitByIdLookup(child_id) != sync_api::BaseNode::INIT_OK)
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

  Thread history_thread_;

  ProfileMock profile_;
  scoped_refptr<HistoryBackendMock> history_backend_;
  scoped_refptr<HistoryServiceMock> history_service_;
  browser_sync::DataTypeErrorHandlerMock error_handler_;
};

void AddTypedUrlEntries(ProfileSyncServiceTypedUrlTest* test,
                        const history::URLRows& entries) {
  test->CreateRoot(syncable::TYPED_URLS);
  for (size_t i = 0; i < entries.size(); ++i) {
    history::VisitVector visits;
    visits.push_back(history::VisitRow(
        entries[i].id(), entries[i].last_visit(), 0,
        content::PageTransitionFromInt(0), 0));
    test->AddTypedUrlSyncNode(entries[i], visits);
  }
}

} // namespace

TEST_F(ProfileSyncServiceTypedUrlTest, EmptyNativeEmptySync) {
  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(Return(true));
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncable::TYPED_URLS);
  TypedUrlModelAssociator* associator =
      StartSyncService(create_root.callback());
  history::URLRows sync_entries;
  GetTypedUrlsFromSyncDB(&sync_entries);
  EXPECT_EQ(0U, sync_entries.size());
  ASSERT_EQ(0, associator->GetErrorPercentage());
}

TEST_F(ProfileSyncServiceTypedUrlTest, HasNativeEmptySync) {
  history::URLRows entries;
  history::VisitVector visits;
  entries.push_back(MakeTypedUrlEntry("http://foo.com", "bar",
                                      2, 15, false, &visits));

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(visits), Return(true)));
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncable::TYPED_URLS);
  TypedUrlModelAssociator* associator =
      StartSyncService(create_root.callback());
  history::URLRows sync_entries;
  GetTypedUrlsFromSyncDB(&sync_entries);
  ASSERT_EQ(1U, sync_entries.size());
  EXPECT_TRUE(URLsEqual(entries[0], sync_entries[0]));
  ASSERT_EQ(0, associator->GetErrorPercentage());
}

TEST_F(ProfileSyncServiceTypedUrlTest, HasNativeErrorReadingVisits) {
  history::URLRows entries;
  history::VisitVector visits;
  history::URLRow native_entry1(MakeTypedUrlEntry("http://foo.com", "bar",
                                                  2, 15, false, &visits));
  history::URLRow native_entry2(MakeTypedUrlEntry("http://foo2.com", "bar",
                                                  3, 15, false, &visits));
  entries.push_back(native_entry1);
  entries.push_back(native_entry2);
  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(entries), Return(true)));
  // Return an error from GetMostRecentVisitsForURL() for the second URL.
  EXPECT_CALL((*history_backend_.get()),
              GetMostRecentVisitsForURL(native_entry1.id(), _, _)).
                  WillRepeatedly(Return(true));
  EXPECT_CALL((*history_backend_.get()),
              GetMostRecentVisitsForURL(native_entry2.id(), _, _)).
                  WillRepeatedly(Return(false));
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncable::TYPED_URLS);
  StartSyncService(create_root.callback());
  history::URLRows sync_entries;
  GetTypedUrlsFromSyncDB(&sync_entries);
  ASSERT_EQ(1U, sync_entries.size());
  EXPECT_TRUE(URLsEqual(native_entry1, sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, HasNativeWithBlankEmptySync) {
  std::vector<history::URLRow> entries;
  history::VisitVector visits;
  // Add an empty URL.
  entries.push_back(MakeTypedUrlEntry("", "bar",
                                      2, 15, false, &visits));
  entries.push_back(MakeTypedUrlEntry("http://foo.com", "bar",
                                      2, 15, false, &visits));
  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(visits), Return(true)));
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncable::TYPED_URLS);
  StartSyncService(create_root.callback());
  std::vector<history::URLRow> sync_entries;
  GetTypedUrlsFromSyncDB(&sync_entries);
  // The empty URL should be ignored.
  ASSERT_EQ(1U, sync_entries.size());
  EXPECT_TRUE(URLsEqual(entries[1], sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, HasNativeHasSyncNoMerge) {
  history::VisitVector native_visits;
  history::VisitVector sync_visits;
  history::URLRow native_entry(MakeTypedUrlEntry("http://native.com", "entry",
                                                 2, 15, false, &native_visits));
  history::URLRow sync_entry(MakeTypedUrlEntry("http://sync.com", "entry",
                                               3, 16, false, &sync_visits));

  history::URLRows native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(native_visits), Return(true)));
  EXPECT_CALL((*history_backend_.get()),
      AddVisits(_, _, history::SOURCE_SYNCED)).WillRepeatedly(Return(true));

  history::URLRows sync_entries;
  sync_entries.push_back(sync_entry);

  EXPECT_CALL((*history_backend_.get()), UpdateURL(_, _)).
      WillRepeatedly(Return(true));
  StartSyncService(base::Bind(&AddTypedUrlEntries, this, sync_entries));

  std::map<std::string, history::URLRow> expected;
  expected[native_entry.url().spec()] = native_entry;
  expected[sync_entry.url().spec()] = sync_entry;

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);

  EXPECT_TRUE(new_sync_entries.size() == expected.size());
  for (history::URLRows::iterator entry = new_sync_entries.begin();
       entry != new_sync_entries.end(); ++entry) {
    EXPECT_TRUE(URLsEqual(expected[entry->url().spec()], *entry));
  }
}

TEST_F(ProfileSyncServiceTypedUrlTest, EmptyNativeExpiredSync) {
  history::VisitVector sync_visits;
  history::URLRow sync_entry(MakeTypedUrlEntry("http://sync.com", "entry",
                                               3, EXPIRED_VISIT, false,
                                               &sync_visits));
  history::URLRows sync_entries;
  sync_entries.push_back(sync_entry);

  // Since all our URLs are expired, no backend calls to add new URLs will be
  // made.
  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(Return(true));
  SetIdleChangeProcessorExpectations();

  StartSyncService(base::Bind(&AddTypedUrlEntries, this, sync_entries));
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

  history::URLRows native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(native_visits), Return(true)));
  EXPECT_CALL((*history_backend_.get()),
      AddVisits(_, _, history::SOURCE_SYNCED)). WillRepeatedly(Return(true));

  history::URLRows sync_entries;
  sync_entries.push_back(sync_entry);

  EXPECT_CALL((*history_backend_.get()), UpdateURL(_, _)).
      WillRepeatedly(Return(true));
  EXPECT_CALL((*history_backend_.get()), SetPageTitle(_, _)).
      WillRepeatedly(Return());
  StartSyncService(base::Bind(&AddTypedUrlEntries, this, sync_entries));

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(merged_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, HasNativeWithErrorHasSyncMerge) {
  history::VisitVector native_visits;
  history::URLRow native_entry(MakeTypedUrlEntry("http://native.com", "native",
                                                 2, 15, false, &native_visits));
  history::VisitVector sync_visits;
  history::URLRow sync_entry(MakeTypedUrlEntry("http://native.com", "sync",
                                               1, 17, false, &sync_visits));

  history::URLRows native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));
  // Return an error getting the visits for the native URL.
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(Return(false));
  EXPECT_CALL((*history_backend_.get()), GetURL(_, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<1>(native_entry), Return(true)));
  EXPECT_CALL((*history_backend_.get()),
      AddVisits(_, _, history::SOURCE_SYNCED)). WillRepeatedly(Return(true));

  history::URLRows sync_entries;
  sync_entries.push_back(sync_entry);

  EXPECT_CALL((*history_backend_.get()), UpdateURL(_, _)).
      WillRepeatedly(Return(true));
  EXPECT_CALL((*history_backend_.get()), SetPageTitle(_, _)).
      WillRepeatedly(Return());
  StartSyncService(base::Bind(&AddTypedUrlEntries, this, sync_entries));

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(sync_entry, new_sync_entries[0]));
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
  CreateRootHelper create_root(this, syncable::TYPED_URLS);
  StartSyncService(create_root.callback());

  history::URLsModifiedDetails details;
  details.changed_urls.push_back(added_entry);
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&history_thread_));
  notifier->Notify(chrome::NOTIFICATION_HISTORY_URLS_MODIFIED,
                   content::Source<Profile>(&profile_),
                   content::Details<history::URLsModifiedDetails>(&details));

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(added_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeAddWithBlank) {
  history::VisitVector added_visits;
  history::URLRow empty_entry(MakeTypedUrlEntry("", "entry",
                                                2, 15, false, &added_visits));
  history::URLRow added_entry(MakeTypedUrlEntry("http://added.com", "entry",
                                                2, 15, false, &added_visits));

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(Return(true));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(added_visits), Return(true)));

  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncable::TYPED_URLS);
  StartSyncService(create_root.callback());

  history::URLsModifiedDetails details;
  details.changed_urls.push_back(empty_entry);
  details.changed_urls.push_back(added_entry);
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&history_thread_));
  notifier->Notify(chrome::NOTIFICATION_HISTORY_URLS_MODIFIED,
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
  history::URLRows original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillOnce(DoAll(SetArgumentPointee<2>(original_visits),
                     Return(true)));
  CreateRootHelper create_root(this, syncable::TYPED_URLS);
  StartSyncService(create_root.callback());

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
  notifier->Notify(chrome::NOTIFICATION_HISTORY_URLS_MODIFIED,
                   content::Source<Profile>(&profile_),
                   content::Details<history::URLsModifiedDetails>(&details));

  history::URLRows new_sync_entries;
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
  CreateRootHelper create_root(this, syncable::TYPED_URLS);
  StartSyncService(create_root.callback());

  history::URLVisitedDetails details;
  details.row = added_entry;
  details.transition = content::PAGE_TRANSITION_TYPED;
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&history_thread_));
  notifier->Notify(chrome::NOTIFICATION_HISTORY_URL_VISITED,
                   content::Source<Profile>(&profile_),
                   content::Details<history::URLVisitedDetails>(&details));

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(added_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeUpdateFromVisit) {
  history::VisitVector original_visits;
  history::URLRow original_entry(MakeTypedUrlEntry("http://mine.com", "entry",
                                                   2, 15, false,
                                                   &original_visits));
  history::URLRows original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillOnce(DoAll(SetArgumentPointee<2>(original_visits),
                           Return(true)));
  CreateRootHelper create_root(this, syncable::TYPED_URLS);
  StartSyncService(create_root.callback());

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

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(updated_entry, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserIgnoreChangeUpdateFromVisit) {
  history::VisitVector original_visits;
  history::URLRow original_entry(MakeTypedUrlEntry("http://mine.com", "entry",
                                                   2, 15, false,
                                                   &original_visits));
  history::URLRows original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(original_visits),
                           Return(true)));
  CreateRootHelper create_root(this, syncable::TYPED_URLS);
  StartSyncService(create_root.callback());
  history::URLRows new_sync_entries;
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
  history::URLRows original_entries;
  original_entries.push_back(original_entry1);
  original_entries.push_back(original_entry2);

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(original_visits1),
                           Return(true)));
  CreateRootHelper create_root(this, syncable::TYPED_URLS);
  StartSyncService(create_root.callback());

  history::URLsDeletedDetails changes;
  changes.all_history = false;
  changes.rows.push_back(history::URLRow(GURL("http://mine.com")));
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&history_thread_));
  notifier->Notify(chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                   content::Source<Profile>(&profile_),
                   content::Details<history::URLsDeletedDetails>(&changes));

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(URLsEqual(original_entry2, new_sync_entries[0]));
}

TEST_F(ProfileSyncServiceTypedUrlTest, ProcessUserChangeRemoveArchive) {
  history::VisitVector original_visits1;
  history::URLRow original_entry1(MakeTypedUrlEntry("http://mine.com", "entry",
                                                    2, 15, false,
                                                    &original_visits1));
  history::VisitVector original_visits2;
  history::URLRow original_entry2(MakeTypedUrlEntry("http://mine2.com",
                                                    "entry2",
                                                    3, 15, false,
                                                    &original_visits2));
  history::URLRows original_entries;
  original_entries.push_back(original_entry1);
  original_entries.push_back(original_entry2);

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(original_visits1),
                           Return(true)));
  CreateRootHelper create_root(this, syncable::TYPED_URLS);
  StartSyncService(create_root.callback());

  history::URLsDeletedDetails changes;
  changes.all_history = false;
  // Setting archived=true should cause the sync code to ignore this deletion.
  changes.archived = true;
  changes.rows.push_back(history::URLRow(GURL("http://mine.com")));
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&history_thread_));
  notifier->Notify(chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                   content::Source<Profile>(&profile_),
                   content::Details<history::URLsDeletedDetails>(&changes));

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  // Both URLs should still be there.
  ASSERT_EQ(2U, new_sync_entries.size());
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
  history::URLRows original_entries;
  original_entries.push_back(original_entry1);
  original_entries.push_back(original_entry2);

  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(original_visits1),
                           Return(true)));
  CreateRootHelper create_root(this, syncable::TYPED_URLS);
  StartSyncService(create_root.callback());

  history::URLRows new_sync_entries;
  GetTypedUrlsFromSyncDB(&new_sync_entries);
  ASSERT_EQ(2U, new_sync_entries.size());

  history::URLsDeletedDetails changes;
  changes.all_history = true;
  scoped_refptr<ThreadNotifier> notifier(new ThreadNotifier(&history_thread_));
  notifier->Notify(chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                   content::Source<Profile>(&profile_),
                   content::Details<history::URLsDeletedDetails>(&changes));

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

  history::URLRows native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetURL(_, _)).
      WillOnce(DoAll(SetArgumentPointee<1>(native_entry), Return(true)));
  EXPECT_CALL((*history_backend_.get()), GetMostRecentVisitsForURL(_, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(native_visits), Return(true)));
  EXPECT_CALL((*history_backend_.get()),
      AddVisits(_, _, history::SOURCE_SYNCED)).WillRepeatedly(Return(false));

  history::URLRows sync_entries;
  sync_entries.push_back(sync_entry);

  EXPECT_CALL((*history_backend_.get()), UpdateURL(_, _)).
      WillRepeatedly(Return(false));
  TypedUrlModelAssociator* associator =
      StartSyncService(base::Bind(&AddTypedUrlEntries, this, sync_entries));
  // Errors writing to the DB should be recorded, but should not cause an
  // unrecoverable error.
  ASSERT_FALSE(
      service_->failed_datatypes_handler().GetFailedTypes().Has(
          syncable::TYPED_URLS));
  // Some calls should have succeeded, so the error percentage should be
  // somewhere > 0 and < 100.
  ASSERT_NE(0, associator->GetErrorPercentage());
  ASSERT_NE(100, associator->GetErrorPercentage());
}

TEST_F(ProfileSyncServiceTypedUrlTest, FailToGetTypedURLs) {
  history::VisitVector native_visits;
  history::VisitVector sync_visits;
  history::URLRow native_entry(MakeTypedUrlEntry("http://native.com", "entry",
                                                 2, 15, false, &native_visits));
  history::URLRow sync_entry(MakeTypedUrlEntry("http://sync.com", "entry",
                                               3, 16, false, &sync_visits));

  history::URLRows native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL((*history_backend_.get()), GetAllTypedURLs(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(false)));

  history::URLRows sync_entries;
  sync_entries.push_back(sync_entry);

  EXPECT_CALL(error_handler_, CreateAndUploadError(_, _, _)).
              WillOnce(Return(SyncError(FROM_HERE,
                                        "Unit test",
                                        syncable::TYPED_URLS)));
  StartSyncService(base::Bind(&AddTypedUrlEntries, this, sync_entries));
  // Errors getting typed URLs will cause an unrecoverable error (since we can
  // do *nothing* in that case).
  ASSERT_TRUE(
      service_->failed_datatypes_handler().GetFailedTypes().Has(
          syncable::TYPED_URLS));
  ASSERT_EQ(
      1u, service_->failed_datatypes_handler().GetFailedTypes().Size());
  // Can't check GetErrorPercentage(), because generating an unrecoverable
  // error will free the model associator.
}
