// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util-inl.h"
#include "base/task.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/sync/abstract_profile_sync_service_test.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/session_change_processor.h"
#include "chrome/browser/sync/glue/session_data_type_controller.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/profile_sync_factory_mock.h"
#include "chrome/browser/sync/protocol/session_specifics.pb.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/file_test_utils.h"
#include "chrome/test/profile_mock.h"
#include "chrome/test/testing_profile.h"
#include "chrome/test/sync/engine/test_id_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::SessionChangeProcessor;
using browser_sync::SessionDataTypeController;
using browser_sync::SessionModelAssociator;
using browser_sync::SyncBackendHost;
using sync_api::SyncManager;
using testing::_;
using testing::Return;
using browser_sync::TestIdFactory;

namespace browser_sync {

class ProfileSyncServiceSessionTest
    : public BrowserWithTestWindowTest,
      public NotificationObserver {
 public:
  ProfileSyncServiceSessionTest()
      : window_bounds_(0, 1, 2, 3),
        notified_of_update_(false),
        notification_sync_id_(0) {}

  ProfileSyncService* sync_service() { return sync_service_.get(); }

  TestIdFactory* ids() { return &ids_; }

 protected:
  SessionService* service() { return helper_.service(); }

  virtual void SetUp() {
    BrowserWithTestWindowTest::SetUp();
    profile()->set_has_history_service(true);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    SessionService* session_service = new SessionService(temp_dir_.path());
    helper_.set_service(session_service);
    service()->SetWindowType(window_id_, Browser::TYPE_NORMAL);
    service()->SetWindowBounds(window_id_, window_bounds_, false);
    registrar_.Add(this, NotificationType::FOREIGN_SESSION_UPDATED,
        NotificationService::AllSources());
    registrar_.Add(this, NotificationType::FOREIGN_SESSION_DELETED,
        NotificationService::AllSources());
  }

  void Observe(NotificationType type,
      const NotificationSource& source,
      const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::FOREIGN_SESSION_UPDATED: {
        notified_of_update_ = true;
        notification_sync_id_ = *Details<int64>(details).ptr();
        break;
      }
      case NotificationType::FOREIGN_SESSION_DELETED: {
        notified_of_update_ = true;
        notification_sync_id_ = -1;
        break;
      }
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void TearDown() {
    helper_.set_service(NULL);
    profile()->set_session_service(NULL);
    sync_service_.reset();
  }

  bool StartSyncService(Task* task, bool will_fail_association) {
    if (sync_service_.get())
      return false;

    sync_service_.reset(new TestProfileSyncService(
        &factory_, profile(), "test user", false, task));
    profile()->set_session_service(helper_.service());

    // Register the session data type.
    model_associator_ =
        new SessionModelAssociator(sync_service_.get());
    change_processor_ = new SessionChangeProcessor(
        sync_service_.get(), model_associator_);
    EXPECT_CALL(factory_, CreateSessionSyncComponents(_, _)).
        WillOnce(Return(ProfileSyncFactory::SyncComponents(
            model_associator_, change_processor_)));
    EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).
        WillOnce(ReturnNewDataTypeManager());
    sync_service_->set_num_expected_resumes(will_fail_association ? 0 : 1);
    sync_service_->RegisterDataTypeController(
        new SessionDataTypeController(&factory_, sync_service_.get()));
    profile()->GetTokenService()->IssueAuthTokenForTest(
        GaiaConstants::kSyncService, "token");
    sync_service_->Initialize();
    MessageLoop::current()->Run();
    return true;
  }

  SyncBackendHost* backend() { return sync_service_->backend(); }

  // Path used in testing.
  ScopedTempDir temp_dir_;
  SessionServiceTestHelper helper_;
  SessionModelAssociator* model_associator_;
  SessionChangeProcessor* change_processor_;
  SessionID window_id_;
  ProfileSyncFactoryMock factory_;
  scoped_ptr<TestProfileSyncService> sync_service_;
  TestIdFactory ids_;
  const gfx::Rect window_bounds_;
  bool notified_of_update_;
  int64 notification_sync_id_;
  NotificationRegistrar registrar_;
};

class CreateRootTask : public Task {
 public:
  explicit CreateRootTask(ProfileSyncServiceSessionTest* test)
      : test_(test), success_(false) {
  }

  virtual ~CreateRootTask() {}
  virtual void Run() {
    success_ = ProfileSyncServiceTestHelper::CreateRoot(syncable::SESSIONS,
        test_->sync_service(), test_->ids());
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
  ASSERT_TRUE(model_associator_->ChromeModelHasUserCreatedNodes(&has_nodes));
  ASSERT_TRUE(has_nodes);
  std::string machine_tag = model_associator_->GetCurrentMachineTag();
  int64 sync_id;
  ASSERT_TRUE(model_associator_->GetSyncIdForTaggedNode(&machine_tag,
      &sync_id));
  ASSERT_EQ(model_associator_->GetSyncIdFromChromeId(machine_tag), sync_id);
  scoped_ptr<const sync_pb::SessionSpecifics> sync_specifics(
      model_associator_->GetChromeNodeFromSyncId(sync_id));
  ASSERT_TRUE(sync_specifics != NULL);

  // Check that we can get the correct session specifics back from the node.
  sync_api::ReadTransaction trans(sync_service_->
      backend()->GetUserShareHandle());
  sync_api::ReadNode node(&trans);
  ASSERT_TRUE(node.InitByClientTagLookup(syncable::SESSIONS,
      machine_tag));
  const sync_pb::SessionSpecifics& specifics(node.GetSessionSpecifics());
  ASSERT_EQ(sync_specifics->session_tag(), specifics.session_tag());
  ASSERT_EQ(machine_tag, specifics.session_tag());
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

  // Report a saved session, thus causing the ChangeProcessor to write to a
  // node.
  NotificationService::current()->Notify(
      NotificationType::SESSION_SERVICE_SAVED,
      Source<Profile>(profile()),
      NotificationService::NoDetails());
  std::string machine_tag = model_associator_->GetCurrentMachineTag();
  int64 sync_id;
  ASSERT_TRUE(model_associator_->GetSyncIdForTaggedNode(&machine_tag,
      &sync_id));
  ASSERT_EQ(model_associator_->GetSyncIdFromChromeId(machine_tag), sync_id);
  scoped_ptr<const sync_pb::SessionSpecifics> sync_specifics(
      model_associator_->GetChromeNodeFromSyncId(sync_id));
  ASSERT_TRUE(sync_specifics != NULL);

  // Check that this machine's data is not included in the foreign windows.
  ScopedVector<ForeignSession> foreign_sessions;
  model_associator_->GetSessionDataFromSyncModel(&foreign_sessions.get());
  ASSERT_EQ(foreign_sessions.size(), 0U);

  // Get the windows for this machine from the node and check that they were
  // filled.
  sync_api::ReadTransaction trans(sync_service_->
      backend()->GetUserShareHandle());
  sync_api::ReadNode node(&trans);
  ASSERT_TRUE(node.InitByClientTagLookup(syncable::SESSIONS,
      machine_tag));
  model_associator_->AppendForeignSessionWithID(sync_id,
      &foreign_sessions.get(), &trans);
  ASSERT_EQ(foreign_sessions.size(), 1U);
  ASSERT_EQ(1U,  foreign_sessions[0]->windows.size());
  ASSERT_EQ(2U, foreign_sessions[0]->windows[0]->tabs.size());
  ASSERT_EQ(2U, foreign_sessions[0]->windows[0]->tabs[0]->navigations.size());
  ASSERT_EQ(GURL("http://bar/1"),
      foreign_sessions[0]->windows[0]->tabs[0]->navigations[0].virtual_url());
  ASSERT_EQ(GURL("http://bar/2"),
      foreign_sessions[0]->windows[0]->tabs[0]->navigations[1].virtual_url());
  ASSERT_EQ(2U, foreign_sessions[0]->windows[0]->tabs[1]->navigations.size());
  ASSERT_EQ(GURL("http://foo/1"),
      foreign_sessions[0]->windows[0]->tabs[1]->navigations[0].virtual_url());
  ASSERT_EQ(GURL("http://foo/2"),
      foreign_sessions[0]->windows[0]->tabs[1]->navigations[1].virtual_url());
  const sync_pb::SessionSpecifics& specifics(node.GetSessionSpecifics());
  ASSERT_EQ(sync_specifics->session_tag(), specifics.session_tag());
  ASSERT_EQ(machine_tag, specifics.session_tag());
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
  sync_pb::SessionSpecifics specifics;
  std::string machine_tag = "session_sync123";
  specifics.set_session_tag(machine_tag);
  sync_pb::SessionWindow* window = specifics.add_session_window();
  window->set_selected_tab_index(1);
  window->set_browser_type(sync_pb::SessionWindow_BrowserType_TYPE_NORMAL);
  sync_pb::SessionTab* tab = window->add_session_tab();
  tab->set_tab_visual_index(13);
  tab->set_current_navigation_index(3);
  tab->set_pinned(true);
  tab->set_extension_app_id("app_id");
  sync_pb::TabNavigation* navigation = tab->add_navigation();
  navigation->set_index(12);
  navigation->set_virtual_url("http://foo/1");
  navigation->set_referrer("referrer");
  navigation->set_title("title");
  navigation->set_page_transition(sync_pb::TabNavigation_PageTransition_TYPED);

  // Update the server with the session specifics.
  {
    sync_api::WriteTransaction trans(sync_service_->
        backend()->GetUserShareHandle());
    sync_api::ReadNode root(&trans);
    ASSERT_TRUE(root.InitByTagLookup(kSessionsTag));
    model_associator_->UpdateSyncModel(&specifics, &trans, &root);
  }

  // Check that the foreign session was written to a node and retrieve the data.
  int64 sync_id;
  ASSERT_TRUE(model_associator_->GetSyncIdForTaggedNode(&machine_tag,
      &sync_id));
  ASSERT_EQ(model_associator_->GetSyncIdFromChromeId(machine_tag), sync_id);
  scoped_ptr<const sync_pb::SessionSpecifics> sync_specifics(
      model_associator_->GetChromeNodeFromSyncId(sync_id));
  ASSERT_TRUE(sync_specifics != NULL);
  ScopedVector<ForeignSession> foreign_sessions;
  model_associator_->GetSessionDataFromSyncModel(&foreign_sessions.get());
  ASSERT_EQ(foreign_sessions.size(), 1U);
  ASSERT_EQ(1U,  foreign_sessions[0]->windows.size());
  ASSERT_EQ(1U, foreign_sessions[0]->windows[0]->tabs.size());
  ASSERT_EQ(1U, foreign_sessions[0]->windows[0]->tabs[0]->navigations.size());
  ASSERT_EQ(foreign_sessions[0]->foreign_tession_tag, machine_tag);
  ASSERT_EQ(1, foreign_sessions[0]->windows[0]->selected_tab_index);
  ASSERT_EQ(1, foreign_sessions[0]->windows[0]->type);
  ASSERT_EQ(13, foreign_sessions[0]->windows[0]->tabs[0]->tab_visual_index);
  ASSERT_EQ(3,
      foreign_sessions[0]->windows[0]->tabs[0]->current_navigation_index);
  ASSERT_TRUE(foreign_sessions[0]->windows[0]->tabs[0]->pinned);
  ASSERT_EQ("app_id",
      foreign_sessions[0]->windows[0]->tabs[0]->extension_app_id);
  ASSERT_EQ(12,
      foreign_sessions[0]->windows[0]->tabs[0]->navigations[0].index());
  ASSERT_EQ(GURL("referrer"),
      foreign_sessions[0]->windows[0]->tabs[0]->navigations[0].referrer());
  ASSERT_EQ(string16(ASCIIToUTF16("title")),
      foreign_sessions[0]->windows[0]->tabs[0]->navigations[0].title());
  ASSERT_EQ(PageTransition::TYPED,
      foreign_sessions[0]->windows[0]->tabs[0]->navigations[0].transition());
  ASSERT_EQ(GURL("http://foo/1"),
      foreign_sessions[0]->windows[0]->tabs[0]->navigations[0].virtual_url());
  sync_api::WriteTransaction trans(sync_service_->
      backend()->GetUserShareHandle());
  sync_api::ReadNode node(&trans);
  ASSERT_TRUE(node.InitByClientTagLookup(syncable::SESSIONS,
      machine_tag));
  const sync_pb::SessionSpecifics& specifics_(node.GetSessionSpecifics());
  ASSERT_EQ(sync_specifics->session_tag(), specifics_.session_tag());
  ASSERT_EQ(machine_tag, specifics_.session_tag());
}

// Test the DataTypeController on update.
TEST_F(ProfileSyncServiceSessionTest, UpdatedSyncNodeActionUpdate) {
  CreateRootTask task(this);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());
  int64 node_id = model_associator_->GetSyncIdFromChromeId(
      model_associator_->GetCurrentMachineTag());
  scoped_ptr<SyncManager::ChangeRecord> record(new SyncManager::ChangeRecord);
  record->action = SyncManager::ChangeRecord::ACTION_UPDATE;
  record->id = node_id;
  ASSERT_EQ(notification_sync_id_, 0);
  ASSERT_FALSE(notified_of_update_);
  {
    sync_api::WriteTransaction trans(backend()->GetUserShareHandle());
    change_processor_->ApplyChangesFromSyncModel(&trans, record.get(), 1);
  }
  ASSERT_EQ(notification_sync_id_, node_id);
  ASSERT_TRUE(notified_of_update_);
}

// Test the DataTypeController on add.
TEST_F(ProfileSyncServiceSessionTest, UpdatedSyncNodeActionAdd) {
  CreateRootTask task(this);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  int64 node_id = model_associator_->GetSyncIdFromChromeId(
      model_associator_->GetCurrentMachineTag());
  scoped_ptr<SyncManager::ChangeRecord> record(new SyncManager::ChangeRecord);
  record->action = SyncManager::ChangeRecord::ACTION_ADD;
  record->id = node_id;
  ASSERT_EQ(notification_sync_id_, 0);
  ASSERT_FALSE(notified_of_update_);
  {
    sync_api::WriteTransaction trans(backend()->GetUserShareHandle());
    change_processor_->ApplyChangesFromSyncModel(&trans, record.get(), 1);
  }
  ASSERT_EQ(notification_sync_id_, node_id);
  ASSERT_TRUE(notified_of_update_);
}

// Test the DataTypeController on delete.
TEST_F(ProfileSyncServiceSessionTest, UpdatedSyncNodeActionDelete) {
  CreateRootTask task(this);
  ASSERT_TRUE(StartSyncService(&task, false));
  ASSERT_TRUE(task.success());

  int64 node_id = model_associator_->GetSyncIdFromChromeId(
      model_associator_->GetCurrentMachineTag());
  scoped_ptr<SyncManager::ChangeRecord> record(new SyncManager::ChangeRecord);
  record->action = SyncManager::ChangeRecord::ACTION_DELETE;
  record->id = node_id;
  ASSERT_EQ(notification_sync_id_, 0);
  ASSERT_FALSE(notified_of_update_);
  {
    sync_api::WriteTransaction trans(backend()->GetUserShareHandle());
    change_processor_->ApplyChangesFromSyncModel(&trans, record.get(), 1);
  }
  ASSERT_EQ(notification_sync_id_, -1);
  ASSERT_TRUE(notified_of_update_);
}

}  // namespace browser_sync

