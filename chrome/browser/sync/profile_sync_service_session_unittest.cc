// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/sessions/session_types_test_helper.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/abstract_profile_sync_service_test.h"
#include "chrome/browser/sync/glue/device_info.h"
#include "chrome/browser/sync/glue/session_change_processor.h"
#include "chrome/browser/sync/glue/session_data_type_controller.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/synced_device_tracker.h"
#include "chrome/browser/sync/glue/synced_tab_delegate.h"
#include "chrome/browser/sync/glue/tab_node_pool.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "googleurl/src/gurl.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/change_record.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/protocol/session_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_types.h"

using browser_sync::SessionChangeProcessor;
using browser_sync::SessionDataTypeController;
using browser_sync::SessionModelAssociator;
using browser_sync::SyncBackendHost;
using content::BrowserThread;
using syncer::ChangeRecord;
using testing::_;
using testing::Return;

namespace browser_sync {

namespace {

class FakeProfileSyncService : public TestProfileSyncService {
 public:
  FakeProfileSyncService(
      ProfileSyncComponentsFactory* factory,
      Profile* profile,
      SigninManager* signin,
      ProfileSyncService::StartBehavior behavior,
      bool synchronous_backend_initialization)
      : TestProfileSyncService(factory,
                               profile,
                               signin,
                               behavior,
                               synchronous_backend_initialization) {}
  virtual ~FakeProfileSyncService() {}

  virtual scoped_ptr<DeviceInfo> GetLocalDeviceInfo() const OVERRIDE {
    return scoped_ptr<DeviceInfo>(
        new DeviceInfo("client_name", "", "", sync_pb::SyncEnums::TYPE_WIN));
  }
};

void BuildSessionSpecifics(const std::string& tag,
                           sync_pb::SessionSpecifics* meta) {
  meta->set_session_tag(tag);
  sync_pb::SessionHeader* header = meta->mutable_header();
  header->set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_LINUX);
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
  navigation->set_virtual_url("http://foo/1");
  navigation->set_referrer("referrer");
  navigation->set_title("title");
  navigation->set_page_transition(sync_pb::SyncEnums_PageTransition_TYPED);
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
      ASSERT_EQ(tab->navigations[0].virtual_url(), GURL("http://foo/1"));
      ASSERT_EQ(SessionTypesTestHelper::GetReferrer(tab->navigations[0]).url,
                GURL("referrer"));
      ASSERT_EQ(tab->navigations[0].title(), string16(ASCIIToUTF16("title")));
      ASSERT_EQ(SessionTypesTestHelper::GetTransitionType(tab->navigations[0]),
                content::PAGE_TRANSITION_TYPED);
    }
  }
}

bool CompareMemoryToString(
    const std::string& str,
    const scoped_refptr<base::RefCountedMemory>& mem) {
  if (mem->size() != str.size())
    return false;
  for (size_t i = 0; i <mem->size(); ++i) {
    if (str[i] != *(mem->front() + i))
      return false;
  }
  return true;
}

}  // namespace

class ProfileSyncServiceSessionTest
    : public BrowserWithTestWindowTest,
      public content::NotificationObserver {
 public:
  ProfileSyncServiceSessionTest()
      : io_thread_(BrowserThread::IO),
        window_bounds_(0, 1, 2, 3),
        notified_of_update_(false),
        notified_of_refresh_(false) {}
  ProfileSyncService* sync_service() { return sync_service_.get(); }

 protected:
  virtual TestingProfile* CreateProfile() OVERRIDE {
    TestingProfile* profile = new TestingProfile();
    // Don't want the profile to create a real ProfileSyncService.
    ProfileSyncServiceFactory::GetInstance()->SetTestingFactory(profile,
                                                                NULL);
    return profile;
  }

  virtual void SetUp() {
    // BrowserWithTestWindowTest implementation.
    BrowserWithTestWindowTest::SetUp();
    io_thread_.StartIOThread();
    profile()->CreateRequestContext();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    registrar_.Add(this, chrome::NOTIFICATION_FOREIGN_SESSION_UPDATED,
        content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
        content::NotificationService::AllSources());
  }

  virtual void Observe(int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_FOREIGN_SESSION_UPDATED:
        notified_of_update_ = true;
        break;
      case chrome::NOTIFICATION_SYNC_REFRESH_LOCAL:
        notified_of_refresh_ = true;
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void TearDown() {
    sync_service_->Shutdown();
    sync_service_.reset();
    profile()->ResetRequestContext();

    // We need to destroy the profile before shutting down the threads, because
    // some of the ref counted objects in the profile depend on their
    // destruction on the io thread.
    DestroyBrowserAndProfile();
    set_profile(NULL);

    // Pump messages posted by the sync core thread (which may end up
    // posting on the IO thread).
    MessageLoop::current()->RunUntilIdle();
    io_thread_.Stop();
    MessageLoop::current()->RunUntilIdle();
    BrowserWithTestWindowTest::TearDown();
  }

  bool StartSyncService(const base::Closure& callback,
                        bool will_fail_association) {
    if (sync_service_.get())
      return false;
    SigninManager* signin = SigninManagerFactory::GetForProfile(profile());
    signin->SetAuthenticatedUsername("test_user");
    ProfileSyncComponentsFactoryMock* factory =
        new ProfileSyncComponentsFactoryMock();
    sync_service_.reset(new FakeProfileSyncService(
        factory,
        profile(),
        signin,
        ProfileSyncService::AUTO_START,
        false));
    sync_service_->set_backend_init_callback(callback);

    // Register the session data type.
    SessionDataTypeController *dtc = new SessionDataTypeController(factory,
                                         profile(),
                                         sync_service_.get());
    sync_service_->RegisterDataTypeController(dtc);

    model_associator_ =
        new SessionModelAssociator(sync_service_.get(),
                                   true /* setup_for_test */);
    change_processor_ = new SessionChangeProcessor(
        dtc, model_associator_,
        true /* setup_for_test */);
    EXPECT_CALL(*factory, CreateSessionSyncComponents(_, _)).
        WillOnce(Return(ProfileSyncComponentsFactory::SyncComponents(
            model_associator_, change_processor_)));
    EXPECT_CALL(*factory, CreateDataTypeManager(_, _, _, _, _)).
        WillOnce(ReturnNewDataTypeManager());

    TokenServiceFactory::GetForProfile(profile())->IssueAuthTokenForTest(
        GaiaConstants::kSyncService, "token");
    sync_service_->Initialize();
    MessageLoop::current()->Run();
    return true;
  }

  content::TestBrowserThread io_thread_;
  // Path used in testing.
  base::ScopedTempDir temp_dir_;
  SessionModelAssociator* model_associator_;
  SessionChangeProcessor* change_processor_;
  SessionID window_id_;
  scoped_ptr<TestProfileSyncService> sync_service_;
  const gfx::Rect window_bounds_;
  bool notified_of_update_;
  bool notified_of_refresh_;
  content::NotificationRegistrar registrar_;
};

class CreateRootHelper {
 public:
  explicit CreateRootHelper(ProfileSyncServiceSessionTest* test)
      : ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
            base::Bind(&CreateRootHelper::CreateRootCallback,
                       base::Unretained(this), test))),
        success_(false) {
  }

  virtual ~CreateRootHelper() {}

  const base::Closure& callback() const { return callback_; }
  bool success() { return success_; }

 private:
  void CreateRootCallback(ProfileSyncServiceSessionTest* test) {
    success_ = syncer::TestUserShare::CreateRoot(
        syncer::SESSIONS, test->sync_service()->GetUserShare());
  }

  base::Closure callback_;
  bool success_;
};

// Test that we can write this machine's session to a node and retrieve it.
TEST_F(ProfileSyncServiceSessionTest, WriteSessionToNode) {
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  // Check that the DataTypeController associated the models.
  bool has_nodes;
  ASSERT_TRUE(model_associator_->SyncModelHasUserCreatedNodes(&has_nodes));
  ASSERT_TRUE(has_nodes);
  std::string machine_tag = model_associator_->GetCurrentMachineTag();
  int64 sync_id = model_associator_->GetSyncIdFromSessionTag(machine_tag);
  ASSERT_NE(syncer::kInvalidId, sync_id);

  // Check that we can get the correct session specifics back from the node.
  syncer::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  syncer::ReadNode node(&trans);
  ASSERT_EQ(syncer::BaseNode::INIT_OK,
            node.InitByClientTagLookup(syncer::SESSIONS, machine_tag));
  const sync_pb::SessionSpecifics& specifics(node.GetSessionSpecifics());
  ASSERT_EQ(machine_tag, specifics.session_tag());
  ASSERT_TRUE(specifics.has_header());
  const sync_pb::SessionHeader& header_s = specifics.header();
  ASSERT_TRUE(header_s.has_device_type());
  ASSERT_EQ("client_name", header_s.client_name());
  ASSERT_EQ(0, header_s.window_size());
}

// Crashes sometimes on Windows, particularly XP.
// See http://crbug.com/174951
#if defined(OS_WIN)
#define MAYBE_WriteFilledSessionToNode DISABLED_WriteFilledSessionToNode
#else
#define MAYBE_WriteFilledSessionToNode WriteFilledSessionToNode
#endif  // defined(OS_WIN)

// Test that we can fill this machine's session, write it to a node,
// and then retrieve it.
TEST_F(ProfileSyncServiceSessionTest, MAYBE_WriteFilledSessionToNode) {
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

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
  ASSERT_NE(syncer::kInvalidId, sync_id);

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
  ASSERT_EQ(2, iter->second->tab()->GetEntryCount());
  ASSERT_EQ(GURL("http://foo/1"), iter->second->tab()->
          GetEntryAtIndex(0)->GetVirtualURL());
  ASSERT_EQ(GURL("http://foo/2"), iter->second->tab()->
          GetEntryAtIndex(1)->GetVirtualURL());
  iter++;
  ASSERT_EQ(2, iter->second->tab()->GetEntryCount());
  ASSERT_EQ(GURL("http://bar/1"), iter->second->tab()->
      GetEntryAtIndex(0)->GetVirtualURL());
  ASSERT_EQ(GURL("http://bar/2"), iter->second->tab()->
      GetEntryAtIndex(1)->GetVirtualURL());
}

// Test that we fail on a failed model association.
TEST_F(ProfileSyncServiceSessionTest, FailModelAssociation) {
  ASSERT_TRUE(StartSyncService(base::Closure(), true));
  ASSERT_TRUE(sync_service_->HasUnrecoverableError());
}

// Write a foreign session to a node, and then retrieve it.
TEST_F(ProfileSyncServiceSessionTest, WriteForeignSessionToNode) {
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

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
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

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
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

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
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

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
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());
  int64 node_id = model_associator_->GetSyncIdFromSessionTag(
      model_associator_->GetCurrentMachineTag());
  ASSERT_FALSE(notified_of_update_);
  {
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(
        &trans, 0,
        ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
            node_id, ChangeRecord::ACTION_UPDATE));
  }
  ASSERT_TRUE(notified_of_update_);
}

// Test the DataTypeController on add.
TEST_F(ProfileSyncServiceSessionTest, UpdatedSyncNodeActionAdd) {
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  int64 node_id = model_associator_->GetSyncIdFromSessionTag(
      model_associator_->GetCurrentMachineTag());
  ASSERT_FALSE(notified_of_update_);
  {
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(
        &trans, 0,
        ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
            node_id, ChangeRecord::ACTION_ADD));
  }
  ASSERT_TRUE(notified_of_update_);
}

// Test the DataTypeController on delete.
TEST_F(ProfileSyncServiceSessionTest, UpdatedSyncNodeActionDelete) {
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  int64 node_id = model_associator_->GetSyncIdFromSessionTag(
      model_associator_->GetCurrentMachineTag());
  sync_pb::EntitySpecifics deleted_specifics;
  deleted_specifics.mutable_session()->set_session_tag("tag");
  ASSERT_FALSE(notified_of_update_);
  {
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    change_processor_->ApplyChangesFromSyncModel(
        &trans, 0,
        ProfileSyncServiceTestHelper::MakeSingletonDeletionChangeRecordList(
            node_id, deleted_specifics));
  }
  ASSERT_TRUE(notified_of_update_);
}
// Test the TabNodePool when it starts off empty.
TEST_F(ProfileSyncServiceSessionTest, TabNodePoolEmpty) {
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

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

// TODO(jhorwich): Re-enable when crbug.com/121487 addressed
TEST_F(ProfileSyncServiceSessionTest, TabNodePoolNonEmpty) {
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

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
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

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
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

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

// Write a stale foreign session to a node. Then update one of its tabs so
// the session is no longer stale. Ensure it doesn't get deleted.
TEST_F(ProfileSyncServiceSessionTest, StaleSessionRefresh) {
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

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

// Crashes sometimes on Windows, particularly XP.
// See http://crbug.com/174951
#if defined(OS_WIN)
#define MAYBE_ValidTabs DISABLED_ValidTabs
#else
#define MAYBE_ValidTabs ValidTabs
#endif  // defined(OS_WIN)

// Test that tabs with nothing but "chrome://*" and "file://*" navigations are
// not be synced.
TEST_F(ProfileSyncServiceSessionTest, MAYBE_ValidTabs) {
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  AddTab(browser(), GURL("chrome://bla1/"));
  NavigateAndCommitActiveTab(GURL("chrome://bla2"));
  AddTab(browser(), GURL("file://bla3/"));
  AddTab(browser(), GURL("bla://bla"));
  // Note: chrome://newtab has special handling which crashes in unit tests.

  // Get the tabs for this machine. Only the bla:// url should be synced.
  SessionModelAssociator::TabLinksMap tab_map = model_associator_->tab_map_;
  ASSERT_EQ(1U, tab_map.size());
  SessionModelAssociator::TabLinksMap::iterator iter = tab_map.begin();
  ASSERT_EQ(1, iter->second->tab()->GetEntryCount());
  ASSERT_EQ(GURL("bla://bla"), iter->second->tab()->
      GetEntryAtIndex(0)->GetVirtualURL());
}

// Verify that AttemptSessionsDataRefresh triggers the
// NOTIFICATION_SYNC_REFRESH_LOCAL notification.
// TODO(zea): Once we can have unit tests that are able to open to the NTP,
// test that the NTP/#opentabs URL triggers a refresh as well (but only when
// it is the active tab).
TEST_F(ProfileSyncServiceSessionTest, SessionsRefresh) {
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  // Empty, so returns false.
  std::vector<const SyncedSession*> foreign_sessions;
  ASSERT_FALSE(model_associator_->GetAllForeignSessions(&foreign_sessions));
  ASSERT_FALSE(notified_of_refresh_);
  model_associator_->AttemptSessionsDataRefresh();
  ASSERT_TRUE(notified_of_refresh_);

  // Nothing should have changed since we don't have unapplied data.
  ASSERT_FALSE(model_associator_->GetAllForeignSessions(&foreign_sessions));
}

// Ensure model association associates the pre-existing tabs.
TEST_F(ProfileSyncServiceSessionTest, ExistingTabs) {
  AddTab(browser(), GURL("http://foo1"));
  NavigateAndCommitActiveTab(GURL("http://foo2"));
  AddTab(browser(), GURL("http://bar1"));
  NavigateAndCommitActiveTab(GURL("http://bar2"));

  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());
  bool has_nodes;
  ASSERT_TRUE(model_associator_->SyncModelHasUserCreatedNodes(&has_nodes));
  ASSERT_TRUE(has_nodes);

  std::string machine_tag = model_associator_->GetCurrentMachineTag();
  int64 sync_id = model_associator_->GetSyncIdFromSessionTag(machine_tag);
  ASSERT_NE(syncer::kInvalidId, sync_id);

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
  ASSERT_EQ(2, iter->second->tab()->GetEntryCount());
  ASSERT_EQ(GURL("http://foo1"), iter->second->tab()->
          GetEntryAtIndex(0)->GetVirtualURL());
  ASSERT_EQ(GURL("http://foo2"), iter->second->tab()->
          GetEntryAtIndex(1)->GetVirtualURL());
  iter++;
  ASSERT_EQ(2, iter->second->tab()->GetEntryCount());
  ASSERT_EQ(GURL("http://bar1"), iter->second->tab()->
      GetEntryAtIndex(0)->GetVirtualURL());
  ASSERT_EQ(GURL("http://bar2"), iter->second->tab()->
      GetEntryAtIndex(1)->GetVirtualURL());
}

TEST_F(ProfileSyncServiceSessionTest, MissingHeaderAndTab) {
  AddTab(browser(), GURL("http://foo1"));
  NavigateAndCommitActiveTab(GURL("http://foo2"));
  AddTab(browser(), GURL("http://bar1"));
  NavigateAndCommitActiveTab(GURL("http://bar2"));
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  syncer::SyncError error;
  std::string local_tag = model_associator_->GetCurrentMachineTag();

  error = model_associator_->DisassociateModels();
  ASSERT_FALSE(error.IsSet());
  {
    // Create a sync node with the local tag but neither header nor tab field.
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    syncer::ReadNode root(&trans);
    root.InitByTagLookup(syncer::ModelTypeToRootTag(syncer::SESSIONS));
    syncer::WriteNode extra_header(&trans);
    syncer::WriteNode::InitUniqueByCreationResult result =
        extra_header.InitUniqueByCreation(syncer::SESSIONS, root, "new_tag");
    ASSERT_EQ(syncer::WriteNode::INIT_SUCCESS, result);
    sync_pb::SessionSpecifics specifics;
    specifics.set_session_tag(local_tag);
    extra_header.SetSessionSpecifics(specifics);
  }

  error = model_associator_->AssociateModels(NULL, NULL);
  ASSERT_FALSE(error.IsSet());
}

TEST_F(ProfileSyncServiceSessionTest, MultipleHeaders) {
  AddTab(browser(), GURL("http://foo1"));
  NavigateAndCommitActiveTab(GURL("http://foo2"));
  AddTab(browser(), GURL("http://bar1"));
  NavigateAndCommitActiveTab(GURL("http://bar2"));
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  syncer::SyncError error;
  std::string local_tag = model_associator_->GetCurrentMachineTag();

  error = model_associator_->DisassociateModels();
  ASSERT_FALSE(error.IsSet());
  {
    // Create another sync node with a header field and the local tag.
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    syncer::ReadNode root(&trans);
    root.InitByTagLookup(syncer::ModelTypeToRootTag(syncer::SESSIONS));
    syncer::WriteNode extra_header(&trans);
    syncer::WriteNode::InitUniqueByCreationResult result =
        extra_header.InitUniqueByCreation(syncer::SESSIONS,
                                          root, local_tag + "_");
    ASSERT_EQ(syncer::WriteNode::INIT_SUCCESS, result);
    sync_pb::SessionSpecifics specifics;
    specifics.set_session_tag(local_tag);
    specifics.mutable_header();
    extra_header.SetSessionSpecifics(specifics);
  }
  error = model_associator_->AssociateModels(NULL, NULL);
  ASSERT_FALSE(error.IsSet());
}

TEST_F(ProfileSyncServiceSessionTest, CorruptedForeign) {
  AddTab(browser(), GURL("http://foo1"));
  NavigateAndCommitActiveTab(GURL("http://foo2"));
  AddTab(browser(), GURL("http://bar1"));
  NavigateAndCommitActiveTab(GURL("http://bar2"));
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  syncer::SyncError error;

  error = model_associator_->DisassociateModels();
  ASSERT_FALSE(error.IsSet());
  {
    // Create another sync node with neither header nor tab field and a foreign
    // tag.
    std::string foreign_tag = "foreign_tag";
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    syncer::ReadNode root(&trans);
    root.InitByTagLookup(syncer::ModelTypeToRootTag(syncer::SESSIONS));
    syncer::WriteNode extra_header(&trans);
    syncer::WriteNode::InitUniqueByCreationResult result =
        extra_header.InitUniqueByCreation(syncer::SESSIONS,
                                          root, foreign_tag);
    ASSERT_EQ(syncer::WriteNode::INIT_SUCCESS, result);
    sync_pb::SessionSpecifics specifics;
    specifics.set_session_tag(foreign_tag);
    extra_header.SetSessionSpecifics(specifics);
  }
  error = model_associator_->AssociateModels(NULL, NULL);
  ASSERT_FALSE(error.IsSet());
}

TEST_F(ProfileSyncServiceSessionTest, MissingLocalTabNode) {
  AddTab(browser(), GURL("http://foo1"));
  NavigateAndCommitActiveTab(GURL("http://foo2"));
  AddTab(browser(), GURL("http://bar1"));
  NavigateAndCommitActiveTab(GURL("http://bar2"));
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  std::string local_tag = model_associator_->GetCurrentMachineTag();
  syncer::SyncError error;

  error = model_associator_->DisassociateModels();
  ASSERT_FALSE(error.IsSet());
  {
    // Delete the first sync tab node.
    std::string tab_tag = TabNodePool::TabIdToTag(local_tag, 0);

    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    syncer::ReadNode root(&trans);
    root.InitByTagLookup(syncer::ModelTypeToRootTag(syncer::SESSIONS));
    syncer::WriteNode tab_node(&trans);
    ASSERT_EQ(syncer::BaseNode::INIT_OK,
              tab_node.InitByClientTagLookup(syncer::SESSIONS, tab_tag));
    tab_node.Tombstone();
  }
  error = model_associator_->AssociateModels(NULL, NULL);
  ASSERT_FALSE(error.IsSet());

  // Add some more tabs to ensure we don't conflict with the pre-existing tab
  // node.
  AddTab(browser(), GURL("http://baz1"));
  AddTab(browser(), GURL("http://baz2"));
}

TEST_F(ProfileSyncServiceSessionTest, Favicons) {
    CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  ASSERT_TRUE(create_root.success());

  // Build a foreign session with one window and one tab.
  std::string tag = "tag1";
  sync_pb::SessionSpecifics meta;
  BuildSessionSpecifics(tag, &meta);
  std::vector<SessionID::id_type> tab_list;
  tab_list.push_back(5);
  AddWindowSpecifics(0, tab_list, &meta);
  sync_pb::SessionSpecifics tab;
  BuildTabSpecifics(tag, 0, tab_list[0], &tab);
  std::string url = tab.tab().navigation(0).virtual_url();
  scoped_refptr<base::RefCountedMemory> favicon;

  // Update associator.
  model_associator_->AssociateForeignSpecifics(meta, base::Time());
  model_associator_->AssociateForeignSpecifics(tab, base::Time());
  MessageLoop::current()->RunUntilIdle();
  ASSERT_FALSE(model_associator_->GetSyncedFaviconForPageURL(url, &favicon));

  // Now add a favicon.
  tab.mutable_tab()->set_favicon_source("http://favicon_source.com/png.ico");
  tab.mutable_tab()->set_favicon_type(sync_pb::SessionTab::TYPE_WEB_FAVICON);
  tab.mutable_tab()->set_favicon("data");
  model_associator_->AssociateForeignSpecifics(tab, base::Time());
  MessageLoop::current()->RunUntilIdle();
  ASSERT_TRUE(model_associator_->GetSyncedFaviconForPageURL(url, &favicon));
  ASSERT_TRUE(CompareMemoryToString("data", favicon));

  // Simulate navigating away. The associator should not delete the favicon.
  tab.mutable_tab()->clear_navigation();
  tab.mutable_tab()->add_navigation()->set_virtual_url("http://new_url.com");
  tab.mutable_tab()->clear_favicon_source();
  tab.mutable_tab()->clear_favicon_type();
  tab.mutable_tab()->clear_favicon();
  model_associator_->AssociateForeignSpecifics(tab, base::Time());
  MessageLoop::current()->RunUntilIdle();
  ASSERT_TRUE(model_associator_->GetSyncedFaviconForPageURL(url, &favicon));
}

TEST_F(ProfileSyncServiceSessionTest, CorruptedLocalHeader) {
  AddTab(browser(), GURL("http://foo1"));
  NavigateAndCommitActiveTab(GURL("http://foo2"));
  AddTab(browser(), GURL("http://bar1"));
  NavigateAndCommitActiveTab(GURL("http://bar2"));
  CreateRootHelper create_root(this);
  ASSERT_TRUE(StartSyncService(create_root.callback(), false));
  std::string local_tag = model_associator_->GetCurrentMachineTag();
  syncer::SyncError error;

  error = model_associator_->DisassociateModels();
  ASSERT_FALSE(error.IsSet());
  {
    // Load the header node and clear it.
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    syncer::WriteNode header(&trans);
    ASSERT_EQ(syncer::BaseNode::INIT_OK,
              header.InitByClientTagLookup(syncer::SESSIONS, local_tag));
    sync_pb::SessionSpecifics specifics;
    header.SetSessionSpecifics(specifics);
  }
  // Ensure we associate properly despite the pre-existing node with our local
  // tag.
  error = model_associator_->AssociateModels(NULL, NULL);
  ASSERT_FALSE(error.IsSet());
}

}  // namespace browser_sync
