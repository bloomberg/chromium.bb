// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/sync/abstract_profile_sync_service_test.h"
#include "chrome/browser/sync/glue/session_change_processor.h"
#include "chrome/browser/sync/glue/session_data_type_controller.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/internal_api/change_record.h"
#include "chrome/browser/sync/internal_api/read_node.h"
#include "chrome/browser/sync/internal_api/read_transaction.h"
#include "chrome/browser/sync/internal_api/write_transaction.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/protocol/session_specifics.pb.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/test/engine/test_id_factory.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/profile_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_types.h"

using browser_sync::SessionChangeProcessor;
using browser_sync::SessionDataTypeController;
using browser_sync::SessionModelAssociator;
using browser_sync::SyncBackendHost;
using content::BrowserThread;
using sync_api::ChangeRecord;
using testing::_;
using testing::Return;
using browser_sync::TestIdFactory;

namespace browser_sync {

namespace {

void BuildSessionSpecifics(const std::string& tag,
                           sync_pb::SessionSpecifics* meta) {
  meta->set_session_tag(tag);
  sync_pb::SessionHeader* header = meta->mutable_header();
  header->set_device_type(sync_pb::SessionHeader_DeviceType_TYPE_LINUX);
  header->set_client_name("name");
}

void AddWindowSpecifics(int window_id,
                        const std::vector<int>& tab_list,
                        sync_pb::SessionSpecifics* meta) {
  sync_pb::SessionHeader* header = meta->mutable_header();
  sync_pb::SessionWindow* window = header->add_window();
  window->set_window_id(window_id);
  window->set_selected_tab_index(0);
  window->set_browser_type(sync_pb::SessionWindow_BrowserType_TYPE_TABBED);
  for (std::vector<int>::const_iterator iter = tab_list.begin();
       iter != tab_list.end(); ++iter) {
    window->add_tab(*iter);
  }
}

void BuildTabSpecifics(const std::string& tag, int window_id, int tab_id,
                     sync_pb::SessionSpecifics* tab_base) {
  tab_base->set_session_tag(tag);
  sync_pb::SessionTab* tab = tab_base->mutable_tab();
  tab->set_tab_id(tab_id);
  tab->set_tab_visual_index(1);
  tab->set_current_navigation_index(0);
  tab->set_pinned(true);
  tab->set_extension_app_id("app_id");
  sync_pb::TabNavigation* navigation = tab->add_navigation();
  navigation->set_index(12);
  navigation->set_virtual_url("http://foo/1");
  navigation->set_referrer("referrer");
  navigation->set_title("title");
  navigation->set_page_transition(sync_pb::TabNavigation_PageTransition_TYPED);
}

// Verifies number of windows, number of tabs, and basic fields.
void VerifySyncedSession(
    const std::string& tag,
    const std::vector<std::vector<SessionID::id_type> >& windows,
    const SyncedSession& session) {
  ASSERT_EQ(tag, session.session_tag);
  ASSERT_EQ(SyncedSession::TYPE_LINUX, session.device_type);
  ASSERT_EQ("name", session.session_name);
  ASSERT_EQ(windows.size(), session.windows.size());

  // We assume the window id's are in increasing order.
  int i = 0;
  for (std::vector<std::vector<int> >::const_iterator win_iter =
           windows.begin();
       win_iter != windows.end(); ++win_iter, ++i) {
    SessionWindow* win_ptr;
    SyncedSession::SyncedWindowMap::const_iterator map_iter =
        session.windows.find(i);
    if (map_iter != session.windows.end())
      win_ptr = map_iter->second;
    else
      FAIL();
    ASSERT_EQ(win_iter->size(), win_ptr->tabs.size());
    ASSERT_EQ(0, win_ptr->selected_tab_index);
    ASSERT_EQ(1, win_ptr->type);
    int j = 0;
    for (std::vector<int>::const_iterator tab_iter = (*win_iter).begin();
         tab_iter != (*win_iter).end(); ++tab_iter, ++j) {
      SessionTab* tab = win_ptr->tabs[j];
      ASSERT_EQ(*tab_iter, tab->tab_id.id());
      ASSERT_EQ(1U, tab->navigations.size());
      ASSERT_EQ(1, tab->tab_visual_index);
      ASSERT_EQ(0, tab->current_navigation_index);
      ASSERT_TRUE(tab->pinned);
      ASSERT_EQ("app_id", tab->extension_app_id);
      ASSERT_EQ(1U, tab->navigations.size());
      ASSERT_EQ(12, tab->navigations[0].index());
      ASSERT_EQ(tab->navigations[0].virtual_url(), GURL("http://foo/1"));
      ASSERT_EQ(tab->navigations[0].referrer(), GURL("referrer"));
      ASSERT_EQ(tab->navigations[0].title(), string16(ASCIIToUTF16("title")));
      ASSERT_EQ(tab->navigations[0].transition(),
                content::PAGE_TRANSITION_TYPED);
    }
  }
}

}  // namespace

class ProfileSyncServiceSessionTest
    : public BrowserWithTestWindowTest,
      public content::NotificationObserver {
 public:
  ProfileSyncServiceSessionTest()
      : io_thread_(BrowserThread::IO),
        window_bounds_(0, 1, 2, 3),
        notified_of_update_(false) {}
  ProfileSyncService* sync_service() { return sync_service_.get(); }

  TestIdFactory* ids() { return sync_service_->id_factory(); }

 protected:
  SessionService* service() { return helper_.service(); }

  virtual void SetUp() {
    // BrowserWithTestWindowTest implementation.
    BrowserWithTestWindowTest::SetUp();
    io_thread_.StartIOThread();
    profile()->CreateRequestContext();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    SessionService* session_service = new SessionService(temp_dir_.path());
    helper_.set_service(session_service);
    service()->SetWindowType(window_id_, Browser::TYPE_TABBED);
    service()->SetWindowBounds(window_id_,
                               window_bounds_,
                               ui::SHOW_STATE_NORMAL);
    registrar_.Add(this, chrome::NOTIFICATION_FOREIGN_SESSION_UPDATED,
        content::NotificationService::AllSources());
  }

  void Observe(int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) {
    switch (type) {
      case chrome::NOTIFICATION_FOREIGN_SESSION_UPDATED:
        notified_of_update_ = true;
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void TearDown() {
    if (SessionServiceFactory::GetForProfileIfExisting(profile()) == service())
      helper_.ReleaseService(); // we transferred ownership to profile
    else
      helper_.set_service(NULL);
    SessionServiceFactory::SetForTestProfile(profile(), NULL);
    sync_service_.reset();
    profile()->ResetRequestContext();

    // We need to destroy the profile before shutting down the threads, because
    // some of the ref counted objects in the profile depend on their
    // destruction on the io thread.
    DestroyBrowser();
    set_profile(NULL);

    // Pump messages posted by the sync core thread (which may end up
    // posting on the IO thread).
    MessageLoop::current()->RunAllPending();
    io_thread_.Stop();
    MessageLoop::current()->RunAllPending();
    BrowserWithTestWindowTest::TearDown();
  }

  bool StartSyncService(Task* task, bool will_fail_association) {
    if (sync_service_.get())
      return false;
    sync_service_.reset(new TestProfileSyncService(
        &factory_, profile(), "test user", false, task));
    SessionServiceFactory::SetForTestProfile(profile(), helper_.service());

    // Register the session data type.
    model_associator_ =
        new SessionModelAssociator(sync_service_.get(),
                                   true /* setup_for_test */);
    change_processor_ = new SessionChangeProcessor(
        sync_service_.get(), model_associator_,
        true /* setup_for_test */);
    EXPECT_CALL(factory_, CreateSessionSyncComponents(_, _)).
        WillOnce(Return(ProfileSyncComponentsFactory::SyncComponents(
            model_associator_, change_processor_)));
    EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).
        WillOnce(ReturnNewDataTypeManager());
    sync_service_->RegisterDataTypeController(
        new SessionDataTypeController(&factory_,
                                      profile(),
                                      sync_service_.get()));
    profile()->GetTokenService()->IssueAuthTokenForTest(
        GaiaConstants::kSyncService, "token");
    sync_service_->Initialize();
    MessageLoop::current()->Run();
    return true;
  }

  content::TestBrowserThread io_thread_;
  // Path used in testing.
  ScopedTempDir temp_dir_;
  SessionServiceTestHelper helper_;
  SessionModelAssociator* model_associator_;
  SessionChangeProcessor* change_processor_;
  SessionID window_id_;
  ProfileSyncComponentsFactoryMock factory_;
  scoped_ptr<TestProfileSyncService> sync_service_;
  const gfx::Rect window_bounds_;
  bool notified_of_update_;
  content::NotificationRegistrar registrar_;
};

class CreateRootTask : public Task {
 public:
  explicit CreateRootTask(ProfileSyncServiceSessionTest* test)
      : test_(test), success_(false) {
  }

  virtual ~CreateRootTask() {}
  virtual void Run() {
    success_ = ProfileSyncServiceTestHelper::CreateRoot(
        syncable::SESSIONS,
        test_->sync_service()->GetUserShare(),
        test_->ids());
  }

  bool success() { return success_; }

 private:
  ProfileSyncServiceSessionTest* test_;
  bool success_;
};

// Test that we can write this machine's session to a node and retrieve it.
TEST_F(ProfileSyncServiceSessionTest, WriteSessionToNode) {
  CreateRootTask task(this);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());
  ASSERT_EQ(model_associator_->GetSessionService(), helper_.service());

  // Check that the DataTypeController associated the models.
  bool has_nodes;
  ASSERT_TRUE(model_associator_->SyncModelHasUserCreatedNodes(&has_nodes));
  ASSERT_TRUE(has_nodes);
  std::string machine_tag = model_associator_->GetCurrentMachineTag();
  int64 sync_id = model_associator_->GetSyncIdFromSessionTag(machine_tag);
  ASSERT_NE(sync_api::kInvalidId, sync_id);

  // Check that we can get the correct session specifics back from the node.
  sync_api::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  sync_api::ReadNode node(&trans);
  ASSERT_TRUE(node.InitByClientTagLookup(syncable::SESSIONS, machine_tag));
  const sync_pb::SessionSpecifics& specifics(node.GetSessionSpecifics());
  ASSERT_EQ(machine_tag, specifics.session_tag());
  ASSERT_TRUE(specifics.has_header());
  const sync_pb::SessionHeader& header_s = specifics.header();
  ASSERT_TRUE(header_s.has_device_type());
  ASSERT_EQ("TestSessionName", header_s.client_name());
  ASSERT_EQ(0, header_s.window_size());
}

// Test that we can fill this machine's session, write it to a node,
// and then retrieve it.
TEST_F(ProfileSyncServiceSessionTest, WriteFilledSessionToNode) {
  CreateRootTask task(this);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  // Check that the DataTypeController associated the models.
  bool has_nodes;
  ASSERT_TRUE(model_associator_->SyncModelHasUserCreatedNodes(&has_nodes));
  ASSERT_TRUE(has_nodes);
  AddTab(browser(), GURL("http://foo/1"));
  NavigateAndCommitActiveTab(GURL("http://foo/2"));
  AddTab(browser(), GURL("http://bar/1"));
  NavigateAndCommitActiveTab(GURL("http://bar/2"));

  ASSERT_TRUE(model_associator_->SyncModelHasUserCreatedNodes(&has_nodes));
  ASSERT_TRUE(has_nodes);
  std::string machine_tag = model_associator_->GetCurrentMachineTag();
  int64 sync_id = model_associator_->GetSyncIdFromSessionTag(machine_tag);
  ASSERT_NE(sync_api::kInvalidId, sync_id);

  // Check that this machine's data is not included in the foreign windows.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_FALSE(model_associator_->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(foreign_sessions.size(), 0U);

  // Get the tabs for this machine from the node and check that they were
  // filled.
  SessionModelAssociator::TabLinksMap tab_map = model_associator_->tab_map_;
  ASSERT_EQ(2U, tab_map.size());
  // Tabs are ordered by sessionid in tab_map, so should be able to traverse
  // the tree based on order of tabs created
  SessionModelAssociator::TabLinksMap::iterator iter = tab_map.begin();
  ASSERT_EQ(2, iter->second.tab()->GetEntryCount());
  ASSERT_EQ(GURL("http://foo/1"), iter->second.tab()->
          GetEntryAtIndex(0)->virtual_url());
  ASSERT_EQ(GURL("http://foo/2"), iter->second.tab()->
          GetEntryAtIndex(1)->virtual_url());
  iter++;
  ASSERT_EQ(2, iter->second.tab()->GetEntryCount());
  ASSERT_EQ(GURL("http://bar/1"), iter->second.tab()->
      GetEntryAtIndex(0)->virtual_url());
  ASSERT_EQ(GURL("http://bar/2"), iter->second.tab()->
      GetEntryAtIndex(1)->virtual_url());
}

// Test that we fail on a failed model association.
TEST_F(ProfileSyncServiceSessionTest, FailModelAssociation) {
  ASSERT_TRUE(StartSyncService(NULL, true));
  ASSERT_TRUE(sync_service_->unrecoverable_error_detected());
}

// Write a foreign session to a node, and then retrieve it.
TEST_F(ProfileSyncServiceSessionTest, WriteForeignSessionToNode) {
  CreateRootTask task(this);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  // Check that the DataTypeController associated the models.
  bool has_nodes;
  ASSERT_TRUE(model_associator_->SyncModelHasUserCreatedNodes(&has_nodes));
  ASSERT_TRUE(has_nodes);

  // Fill an instance of session specifics with a foreign session's data.
  std::string tag = "tag1";
  sync_pb::SessionSpecifics meta;
  BuildSessionSpecifics(tag, &meta);
  SessionID::id_type tab_nums1[] = {5, 10, 13, 17};
  std::vector<SessionID::id_type> tab_list1(
      tab_nums1, tab_nums1 + arraysize(tab_nums1));
  AddWindowSpecifics(0, tab_list1, &meta);
  std::vector<sync_pb::SessionSpecifics> tabs1;
  tabs1.resize(tab_list1.size());
  for (size_t i = 0; i < tab_list1.size(); ++i) {
    BuildTabSpecifics(tag, 0, tab_list1[i], &tabs1[i]);
  }

  // Update associator with the session's meta node containing one window.
  model_associator_->AssociateForeignSpecifics(meta, base::Time());
  // Add tabs for the window.
  for (std::vector<sync_pb::SessionSpecifics>::iterator iter = tabs1.begin();
       iter != tabs1.end(); ++iter) {
    model_associator_->AssociateForeignSpecifics(*iter, base::Time());
  }

  // Check that the foreign session was associated and retrieve the data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(model_associator_->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID::id_type> > session_reference;
  session_reference.push_back(tab_list1);
  VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));
}

// Write a foreign session with one window to a node. Sync, then add a window.
// Sync, then add a third window. Close the two windows.
TEST_F(ProfileSyncServiceSessionTest, WriteForeignSessionToNodeThreeWindows) {
  CreateRootTask task(this);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  // Build a foreign session with one window and four tabs.
  std::string tag = "tag1";
  sync_pb::SessionSpecifics meta;
  BuildSessionSpecifics(tag, &meta);
  SessionID::id_type tab_nums1[] = {5, 10, 13, 17};
  std::vector<SessionID::id_type> tab_list1(
      tab_nums1, tab_nums1 + arraysize(tab_nums1));
  AddWindowSpecifics(0, tab_list1, &meta);
  std::vector<sync_pb::SessionSpecifics> tabs1;
  tabs1.resize(tab_list1.size());
  for (size_t i = 0; i < tab_list1.size(); ++i) {
    BuildTabSpecifics(tag, 0, tab_list1[i], &tabs1[i]);
  }
  // Update associator with the session's meta node containing one window.
  model_associator_->AssociateForeignSpecifics(meta, base::Time());
  // Add tabs for first window.
  for (std::vector<sync_pb::SessionSpecifics>::iterator iter = tabs1.begin();
       iter != tabs1.end(); ++iter) {
    model_associator_->AssociateForeignSpecifics(*iter, base::Time());
  }

  // Verify first window
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(model_associator_->GetAllForeignSessions(&foreign_sessions));
  std::vector<std::vector<SessionID::id_type> > session_reference;
  session_reference.push_back(tab_list1);
  VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));

  // Add a second window.
  SessionID::id_type tab_nums2[] = {7, 15, 18, 20};
  std::vector<SessionID::id_type> tab_list2(
      tab_nums2, tab_nums2 + arraysize(tab_nums2));
  AddWindowSpecifics(1, tab_list2, &meta);
  std::vector<sync_pb::SessionSpecifics> tabs2;
  tabs2.resize(tab_list2.size());
  for (size_t i = 0; i < tab_list2.size(); ++i) {
    BuildTabSpecifics(tag, 0, tab_list2[i], &tabs2[i]);
  }
  // Update associator with the session's meta node containing two windows.
  model_associator_->AssociateForeignSpecifics(meta, base::Time());
  // Add tabs for second window.
  for (std::vector<sync_pb::SessionSpecifics>::iterator iter = tabs2.begin();
       iter != tabs2.end(); ++iter) {
    model_associator_->AssociateForeignSpecifics(*iter, base::Time());
  }

  // Verify the two windows.
  foreign_sessions.clear();
  ASSERT_TRUE(model_associator_->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  session_reference.push_back(tab_list2);
  VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));

  // Add a third window.
  SessionID::id_type tab_nums3[] = {8, 16, 19, 21};
  std::vector<SessionID::id_type> tab_list3(
      tab_nums3, tab_nums3 + arraysize(tab_nums3));
  AddWindowSpecifics(2, tab_list3, &meta);
  std::vector<sync_pb::SessionSpecifics> tabs3;
  tabs3.resize(tab_list3.size());
  for (size_t i = 0; i < tab_list3.size(); ++i) {
    BuildTabSpecifics(tag, 0, tab_list3[i], &tabs3[i]);
  }
  // Update associator with the session's meta node containing three windows.
  model_associator_->AssociateForeignSpecifics(meta, base::Time());
  // Add tabs for third window.
  for (std::vector<sync_pb::SessionSpecifics>::iterator iter = tabs3.begin();
       iter != tabs3.end(); ++iter) {
    model_associator_->AssociateForeignSpecifics(*iter, base::Time());
  }

  // Verify the three windows
  foreign_sessions.clear();
  ASSERT_TRUE(model_associator_->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  session_reference.push_back(tab_list3);
  VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));

  // Close third window (by clearing and then not adding it back).
  meta.mutable_header()->clear_window();
  AddWindowSpecifics(0, tab_list1, &meta);
  AddWindowSpecifics(1, tab_list2, &meta);
  // Update associator with just the meta node, now containing only two windows.
  model_associator_->AssociateForeignSpecifics(meta, base::Time());

  // Verify first two windows are still there.
  foreign_sessions.clear();
  ASSERT_TRUE(model_associator_->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  session_reference.pop_back();  // Pop off the data for the third window.
  VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));

  // Close second window (by clearing and then not adding it back).
  meta.mutable_header()->clear_window();
  AddWindowSpecifics(0, tab_list1, &meta);
  // Update associator with just the meta node, now containing only one windows.
  model_associator_->AssociateForeignSpecifics(meta, base::Time());

  // Verify first window is still there.
  foreign_sessions.clear();
  ASSERT_TRUE(model_associator_->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  session_reference.pop_back();  // Pop off the data for the second window.
  VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));
}

// Write a foreign session to a node, with the tabs arriving first, and then
// retrieve it.
TEST_F(ProfileSyncServiceSessionTest, WriteForeignSessionToNodeTabsFirst) {
  CreateRootTask task(this);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  // Fill an instance of session specifics with a foreign session's data.
  std::string tag = "tag1";
  sync_pb::SessionSpecifics meta;
  BuildSessionSpecifics(tag, &meta);
  SessionID::id_type tab_nums1[] = {5, 10, 13, 17};
  std::vector<SessionID::id_type> tab_list1(
      tab_nums1, tab_nums1 + arraysize(tab_nums1));
  AddWindowSpecifics(0, tab_list1, &meta);
  std::vector<sync_pb::SessionSpecifics> tabs1;
  tabs1.resize(tab_list1.size());
  for (size_t i = 0; i < tab_list1.size(); ++i) {
    BuildTabSpecifics(tag, 0, tab_list1[i], &tabs1[i]);
  }

  // Add tabs for first window.
  for (std::vector<sync_pb::SessionSpecifics>::iterator iter = tabs1.begin();
       iter != tabs1.end(); ++iter) {
    model_associator_->AssociateForeignSpecifics(*iter, base::Time());
  }
  // Update associator with the session's meta node containing one window.
  model_associator_->AssociateForeignSpecifics(meta, base::Time());

  // Check that the foreign session was associated and retrieve the data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(model_associator_->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID::id_type> > session_reference;
  session_reference.push_back(tab_list1);
  VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));
}

// Write a foreign session to a node with some tabs that never arrive.
TEST_F(ProfileSyncServiceSessionTest, WriteForeignSessionToNodeMissingTabs) {
  CreateRootTask task(this);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  // Fill an instance of session specifics with a foreign session's data.
  std::string tag = "tag1";
  sync_pb::SessionSpecifics meta;
  BuildSessionSpecifics(tag, &meta);
  SessionID::id_type tab_nums1[] = {5, 10, 13, 17};
  std::vector<SessionID::id_type> tab_list1(
      tab_nums1, tab_nums1 + arraysize(tab_nums1));
  AddWindowSpecifics(0, tab_list1, &meta);
  std::vector<sync_pb::SessionSpecifics> tabs1;
  tabs1.resize(tab_list1.size());  // First window has all the tabs
  for (size_t i = 0; i < tab_list1.size(); ++i) {
    BuildTabSpecifics(tag, 0, tab_list1[i], &tabs1[i]);
  }
  // Add a second window, but this time only create two tab nodes, despite the
  // window expecting four tabs.
  SessionID::id_type tab_nums2[] = {7, 15, 18, 20};
  std::vector<SessionID::id_type> tab_list2(
      tab_nums2, tab_nums2 + arraysize(tab_nums2));
  AddWindowSpecifics(1, tab_list2, &meta);
  std::vector<sync_pb::SessionSpecifics> tabs2;
  tabs2.resize(2);
  for (size_t i = 0; i < 2; ++i) {
    BuildTabSpecifics(tag, 0, tab_list2[i], &tabs2[i]);
  }

  // Update associator with the session's meta node containing two windows.
  model_associator_->AssociateForeignSpecifics(meta, base::Time());
  // Add tabs for first window.
  for (std::vector<sync_pb::SessionSpecifics>::iterator iter = tabs1.begin();
       iter != tabs1.end(); ++iter) {
    model_associator_->AssociateForeignSpecifics(*iter, base::Time());
  }
  // Add tabs for second window.
  for (std::vector<sync_pb::SessionSpecifics>::iterator iter = tabs2.begin();
       iter != tabs2.end(); ++iter) {
    model_associator_->AssociateForeignSpecifics(*iter, base::Time());
  }

  // Check that the foreign session was associated and retrieve the data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(model_associator_->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  ASSERT_EQ(2U, foreign_sessions[0]->windows.size());
  ASSERT_EQ(4U, foreign_sessions[0]->windows.find(0)->second->tabs.size());
  ASSERT_EQ(4U, foreign_sessions[0]->windows.find(1)->second->tabs.size());

  // Close the second window.
  meta.mutable_header()->clear_window();
  AddWindowSpecifics(0, tab_list1, &meta);

  // Update associator with the session's meta node containing one window.
  model_associator_->AssociateForeignSpecifics(meta, base::Time());

  // Check that the foreign session was associated and retrieve the data.
  foreign_sessions.clear();
  ASSERT_TRUE(model_associator_->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  ASSERT_EQ(1U, foreign_sessions[0]->windows.size());
  std::vector<std::vector<SessionID::id_type> > session_reference;
  session_reference.push_back(tab_list1);
  VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));
}

// Test the DataTypeController on update.
TEST_F(ProfileSyncServiceSessionTest, UpdatedSyncNodeActionUpdate) {
  CreateRootTask task(this);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());
  int64 node_id = model_associator_->GetSyncIdFromSessionTag(
      model_associator_->GetCurrentMachineTag());
  ASSERT_FALSE(notified_of_update_);
  {
    sync_api::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(
        &trans,
        ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
            node_id, ChangeRecord::ACTION_UPDATE));
  }
  ASSERT_TRUE(notified_of_update_);
}

// Test the DataTypeController on add.
TEST_F(ProfileSyncServiceSessionTest, UpdatedSyncNodeActionAdd) {
  CreateRootTask task(this);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  int64 node_id = model_associator_->GetSyncIdFromSessionTag(
      model_associator_->GetCurrentMachineTag());
  ASSERT_FALSE(notified_of_update_);
  {
    sync_api::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(
        &trans,
        ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
            node_id, ChangeRecord::ACTION_ADD));
  }
  ASSERT_TRUE(notified_of_update_);
}

// Test the DataTypeController on delete.
TEST_F(ProfileSyncServiceSessionTest, UpdatedSyncNodeActionDelete) {
  CreateRootTask task(this);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  int64 node_id = model_associator_->GetSyncIdFromSessionTag(
      model_associator_->GetCurrentMachineTag());
  sync_pb::EntitySpecifics deleted_specifics;
  deleted_specifics.MutableExtension(sync_pb::session)->set_session_tag("tag");
  ASSERT_FALSE(notified_of_update_);
  {
    sync_api::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(
        &trans,
        ProfileSyncServiceTestHelper::MakeSingletonDeletionChangeRecordList(
            node_id, deleted_specifics));
  }
  ASSERT_TRUE(notified_of_update_);
}
// Test the TabNodePool when it starts off empty.
TEST_F(ProfileSyncServiceSessionTest, TabNodePoolEmpty) {
  CreateRootTask task(this);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  std::vector<int64> node_ids;
  ASSERT_EQ(0U, model_associator_->tab_pool_.capacity());
  ASSERT_TRUE(model_associator_->tab_pool_.empty());
  ASSERT_TRUE(model_associator_->tab_pool_.full());
  const size_t num_ids = 10;
  for (size_t i = 0; i < num_ids; ++i) {
    int64 id = model_associator_->tab_pool_.GetFreeTabNode();
    ASSERT_GT(id, -1);
    node_ids.push_back(id);
  }
  ASSERT_EQ(num_ids, model_associator_->tab_pool_.capacity());
  ASSERT_TRUE(model_associator_->tab_pool_.empty());
  ASSERT_FALSE(model_associator_->tab_pool_.full());
  for (size_t i = 0; i < num_ids; ++i) {
    model_associator_->tab_pool_.FreeTabNode(node_ids[i]);
  }
  ASSERT_EQ(num_ids, model_associator_->tab_pool_.capacity());
  ASSERT_FALSE(model_associator_->tab_pool_.empty());
  ASSERT_TRUE(model_associator_->tab_pool_.full());
}

// Test the TabNodePool when it starts off with nodes
TEST_F(ProfileSyncServiceSessionTest, TabNodePoolNonEmpty) {
  CreateRootTask task(this);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  const size_t num_starting_nodes = 3;
  for (size_t i = 0; i < num_starting_nodes; ++i) {
    model_associator_->tab_pool_.AddTabNode(i);
  }

  std::vector<int64> node_ids;
  ASSERT_EQ(num_starting_nodes, model_associator_->tab_pool_.capacity());
  ASSERT_FALSE(model_associator_->tab_pool_.empty());
  ASSERT_TRUE(model_associator_->tab_pool_.full());
  const size_t num_ids = 10;
  for (size_t i = 0; i < num_ids; ++i) {
    int64 id = model_associator_->tab_pool_.GetFreeTabNode();
    ASSERT_GT(id, -1);
    node_ids.push_back(id);
  }
  ASSERT_EQ(num_ids, model_associator_->tab_pool_.capacity());
  ASSERT_TRUE(model_associator_->tab_pool_.empty());
  ASSERT_FALSE(model_associator_->tab_pool_.full());
  for (size_t i = 0; i < num_ids; ++i) {
    model_associator_->tab_pool_.FreeTabNode(node_ids[i]);
  }
  ASSERT_EQ(num_ids, model_associator_->tab_pool_.capacity());
  ASSERT_FALSE(model_associator_->tab_pool_.empty());
  ASSERT_TRUE(model_associator_->tab_pool_.full());
}

// Write a foreign session to a node, and then delete it.
TEST_F(ProfileSyncServiceSessionTest, DeleteForeignSession) {
  CreateRootTask task(this);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  // Check that the DataTypeController associated the models.
  bool has_nodes;
  ASSERT_TRUE(model_associator_->SyncModelHasUserCreatedNodes(&has_nodes));
  ASSERT_TRUE(has_nodes);

  // A foreign session's tag.
  std::string tag = "tag1";

  // Should do nothing if the foreign session doesn't exist.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_FALSE(model_associator_->GetAllForeignSessions(&foreign_sessions));
  model_associator_->DeleteForeignSession(tag);
  ASSERT_FALSE(model_associator_->GetAllForeignSessions(&foreign_sessions));

  // Fill an instance of session specifics with a foreign session's data.
  sync_pb::SessionSpecifics meta;
  BuildSessionSpecifics(tag, &meta);
  SessionID::id_type tab_nums1[] = {5, 10, 13, 17};
  std::vector<SessionID::id_type> tab_list1(
      tab_nums1, tab_nums1 + arraysize(tab_nums1));
  AddWindowSpecifics(0, tab_list1, &meta);
  std::vector<sync_pb::SessionSpecifics> tabs1;
  tabs1.resize(tab_list1.size());
  for (size_t i = 0; i < tab_list1.size(); ++i) {
    BuildTabSpecifics(tag, 0, tab_list1[i], &tabs1[i]);
  }

  // Update associator with the session's meta node containing one window.
  model_associator_->AssociateForeignSpecifics(meta, base::Time());
  // Add tabs for the window.
  for (std::vector<sync_pb::SessionSpecifics>::iterator iter = tabs1.begin();
       iter != tabs1.end(); ++iter) {
    model_associator_->AssociateForeignSpecifics(*iter, base::Time());
  }

  // Check that the foreign session was associated and retrieve the data.
  ASSERT_TRUE(model_associator_->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID::id_type> > session_reference;
  session_reference.push_back(tab_list1);
  VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));

  // Now delete the foreign session.
  model_associator_->DeleteForeignSession(tag);
  ASSERT_FALSE(model_associator_->GetAllForeignSessions(&foreign_sessions));
}

// Associate both a non-stale foreign session and a stale foreign session.
// Ensure only the stale session gets deleted.
TEST_F(ProfileSyncServiceSessionTest, DeleteStaleSessions) {
  CreateRootTask task(this);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  // Fill two instances of session specifics with a foreign session's data.
  std::string tag = "tag1";
  sync_pb::SessionSpecifics meta;
  BuildSessionSpecifics(tag, &meta);
  SessionID::id_type tab_nums1[] = {5, 10, 13, 17};
  std::vector<SessionID::id_type> tab_list1(
      tab_nums1, tab_nums1 + arraysize(tab_nums1));
  AddWindowSpecifics(0, tab_list1, &meta);
  std::vector<sync_pb::SessionSpecifics> tabs1;
  tabs1.resize(tab_list1.size());
  for (size_t i = 0; i < tab_list1.size(); ++i) {
    BuildTabSpecifics(tag, 0, tab_list1[i], &tabs1[i]);
  }
  std::string tag2 = "tag2";
  sync_pb::SessionSpecifics meta2;
  BuildSessionSpecifics(tag2, &meta2);
  SessionID::id_type tab_nums2[] = {8, 15, 18, 20};
  std::vector<SessionID::id_type> tab_list2(
      tab_nums2, tab_nums2 + arraysize(tab_nums2));
  AddWindowSpecifics(0, tab_list2, &meta2);
  std::vector<sync_pb::SessionSpecifics> tabs2;
  tabs2.resize(tab_list2.size());
  for (size_t i = 0; i < tab_list2.size(); ++i) {
    BuildTabSpecifics(tag2, 0, tab_list2[i], &tabs2[i]);
  }

  // Set the modification time for tag1 to be 21 days ago, tag2 to 5 days ago.
  base::Time tag1_time = base::Time::Now() - base::TimeDelta::FromDays(21);
  base::Time tag2_time = base::Time::Now() - base::TimeDelta::FromDays(5);

  // Associate specifics.
  model_associator_->AssociateForeignSpecifics(meta, tag1_time);
  for (std::vector<sync_pb::SessionSpecifics>::iterator iter = tabs1.begin();
       iter != tabs1.end(); ++iter) {
    model_associator_->AssociateForeignSpecifics(*iter, tag1_time);
  }
  model_associator_->AssociateForeignSpecifics(meta2, tag2_time);
  for (std::vector<sync_pb::SessionSpecifics>::iterator iter = tabs2.begin();
       iter != tabs2.end(); ++iter) {
    model_associator_->AssociateForeignSpecifics(*iter, tag2_time);
  }

  // Check that the foreign session was associated and retrieve the data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(model_associator_->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(2U, foreign_sessions.size());

  // Now delete the stale session and verify the non-stale one is still there.
  model_associator_->DeleteStaleSessions();
  ASSERT_TRUE(model_associator_->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID::id_type> > session_reference;
  session_reference.push_back(tab_list2);
  VerifySyncedSession(tag2, session_reference, *(foreign_sessions[0]));
}

// Write a stale foreign session to a node. Then update one of it's tabs so
// the session is no longer stale. Ensure it doesn't get deleted.
TEST_F(ProfileSyncServiceSessionTest, StaleSessionRefresh) {
  CreateRootTask task(this);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  std::string tag = "tag1";
  sync_pb::SessionSpecifics meta;
  BuildSessionSpecifics(tag, &meta);
  SessionID::id_type tab_nums1[] = {5, 10, 13, 17};
  std::vector<SessionID::id_type> tab_list1(
      tab_nums1, tab_nums1 + arraysize(tab_nums1));
  AddWindowSpecifics(0, tab_list1, &meta);
  std::vector<sync_pb::SessionSpecifics> tabs1;
  tabs1.resize(tab_list1.size());
  for (size_t i = 0; i < tab_list1.size(); ++i) {
    BuildTabSpecifics(tag, 0, tab_list1[i], &tabs1[i]);
  }

  // Associate.
  base::Time stale_time = base::Time::Now() - base::TimeDelta::FromDays(21);
  model_associator_->AssociateForeignSpecifics(meta, stale_time);
  for (std::vector<sync_pb::SessionSpecifics>::iterator iter = tabs1.begin();
       iter != tabs1.end(); ++iter) {
    model_associator_->AssociateForeignSpecifics(*iter, stale_time);
  }

  // Associate one of the tabs with a non-stale time.
  model_associator_->AssociateForeignSpecifics(tabs1[0], base::Time::Now());

  // Check that the foreign session was associated and retrieve the data.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_TRUE(model_associator_->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());

  // Verify the now non-stale session does not get deleted.
  model_associator_->DeleteStaleSessions();
  ASSERT_TRUE(model_associator_->GetAllForeignSessions(&foreign_sessions));
  ASSERT_EQ(1U, foreign_sessions.size());
  std::vector<std::vector<SessionID::id_type> > session_reference;
  session_reference.push_back(tab_list1);
  VerifySyncedSession(tag, session_reference, *(foreign_sessions[0]));
}

}  // namespace browser_sync
