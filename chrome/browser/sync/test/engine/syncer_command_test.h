// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_ENGINE_SYNCER_COMMAND_TEST_H_
#define CHROME_BROWSER_SYNC_TEST_ENGINE_SYNCER_COMMAND_TEST_H_
#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/engine/model_changing_syncer_command.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/sessions/debug_info_getter.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/sessions/sync_session_context.h"
#include "chrome/browser/sync/test/engine/mock_connection_manager.h"
#include "chrome/browser/sync/test/engine/fake_model_worker.h"
#include "chrome/browser/sync/test/engine/test_directory_setter_upper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::NiceMock;

namespace browser_sync {

class MockDebugInfoGetter : public browser_sync::sessions::DebugInfoGetter {
 public:
  MockDebugInfoGetter();
  virtual ~MockDebugInfoGetter();
  MOCK_METHOD1(GetAndClearDebugInfo, void(sync_pb::DebugInfo* debug_info));
};

// A test fixture that simplifies writing unit tests for individual
// SyncerCommands, providing convenient access to a test directory
// and a syncer session.
template<typename T>
class SyncerCommandTestWithParam : public testing::TestWithParam<T>,
                                   public sessions::SyncSession::Delegate,
                                   public ModelSafeWorkerRegistrar {
 public:
  enum UseMockDirectory {
    USE_MOCK_DIRECTORY
  };

  // SyncSession::Delegate implementation.
  virtual void OnSilencedUntil(
      const base::TimeTicks& silenced_until) OVERRIDE {
    FAIL() << "Should not get silenced.";
  }
  virtual bool IsSyncingCurrentlySilenced() OVERRIDE {
    return false;
  }
  virtual void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE {
    FAIL() << "Should not get poll interval update.";
  }
  virtual void OnReceivedShortPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE {
    FAIL() << "Should not get poll interval update.";
  }
  virtual void OnReceivedSessionsCommitDelay(
      const base::TimeDelta& new_delay) OVERRIDE {
    FAIL() << "Should not get sessions commit delay.";
  }
  virtual void OnShouldStopSyncingPermanently() OVERRIDE {
    FAIL() << "Shouldn't be called.";
  }
  virtual void OnSyncProtocolError(
      const sessions::SyncSessionSnapshot& session) OVERRIDE {
    return;
  }

  // ModelSafeWorkerRegistrar implementation.
  virtual void GetWorkers(std::vector<ModelSafeWorker*>* out) OVERRIDE {
    std::vector<scoped_refptr<ModelSafeWorker> >::iterator it;
    for (it = workers_.begin(); it != workers_.end(); ++it)
      out->push_back(*it);
  }
  virtual void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) OVERRIDE {
    ModelSafeRoutingInfo copy(routing_info_);
    out->swap(copy);
  }

 protected:
  SyncerCommandTestWithParam() : syncdb_(new TestDirectorySetterUpper()) {}

  explicit SyncerCommandTestWithParam(UseMockDirectory use_mock)
      : syncdb_(new MockDirectorySetterUpper()) {}

  virtual ~SyncerCommandTestWithParam() {}
  virtual void SetUp() {
    syncdb_->SetUp();
    ResetContext();
    // The session always expects there to be a passive worker.
    workers()->push_back(
        make_scoped_refptr(new FakeModelWorker(GROUP_PASSIVE)));
  }
  virtual void TearDown() {
    syncdb_->TearDown();
  }

  TestDirectorySetterUpper* syncdb() { return syncdb_.get(); }
  sessions::SyncSessionContext* context() const { return context_.get(); }
  sessions::SyncSession::Delegate* delegate() { return this; }
  ModelSafeWorkerRegistrar* registrar() { return this; }

  // Lazily create a session requesting all datatypes with no payload.
  sessions::SyncSession* session() {
    syncable::ModelTypePayloadMap types =
        syncable::ModelTypePayloadMapFromRoutingInfo(routing_info_,
                                                     std::string());
    return session(sessions::SyncSourceInfo(types));
  }

  // Create a session with the provided source.
  sessions::SyncSession* session(const sessions::SyncSourceInfo& source) {
    if (!session_.get()) {
      std::vector<ModelSafeWorker*> workers;
      GetWorkers(&workers);
      session_.reset(new sessions::SyncSession(context(), delegate(), source,
                     routing_info_, workers));
    }
    return session_.get();
  }

  void ClearSession() {
    session_.reset();
  }

  void ResetContext() {
    context_.reset(new sessions::SyncSessionContext(
        mock_server_.get(), syncdb_->manager(), registrar(),
        std::vector<SyncEngineEventListener*>(), &mock_debug_info_getter_));
    context_->set_account_name(syncdb_->name());
    ClearSession();
  }

  // Install a MockServerConnection.  Resets the context.  By default,
  // the context does not have a MockServerConnection attached.
  void ConfigureMockServerConnection() {
    mock_server_.reset(
        new MockConnectionManager(syncdb_->manager(), syncdb_->name()));
    ResetContext();
  }

  std::vector<scoped_refptr<ModelSafeWorker> >* workers() {
    return &workers_;
  }

  const ModelSafeRoutingInfo& routing_info() { return routing_info_; }
  ModelSafeRoutingInfo* mutable_routing_info() { return &routing_info_; }

  MockConnectionManager* mock_server() {
    return mock_server_.get();
  }

  MockDebugInfoGetter* mock_debug_info_getter() {
    return &mock_debug_info_getter_;
  }

  // Helper functions to check command.GetGroupsToChange().

  void ExpectNoGroupsToChange(const ModelChangingSyncerCommand& command) {
    EXPECT_TRUE(command.GetGroupsToChangeForTest(*session()).empty());
  }

  void ExpectGroupToChange(
      const ModelChangingSyncerCommand& command, ModelSafeGroup group) {
    std::set<ModelSafeGroup> expected_groups_to_change;
    expected_groups_to_change.insert(group);
    EXPECT_EQ(expected_groups_to_change,
              command.GetGroupsToChangeForTest(*session()));
  }

  void ExpectGroupsToChange(
      const ModelChangingSyncerCommand& command,
      ModelSafeGroup group1, ModelSafeGroup group2) {
    std::set<ModelSafeGroup> expected_groups_to_change;
    expected_groups_to_change.insert(group1);
    expected_groups_to_change.insert(group2);
    EXPECT_EQ(expected_groups_to_change,
              command.GetGroupsToChangeForTest(*session()));
  }

  void ExpectGroupsToChange(
      const ModelChangingSyncerCommand& command,
      ModelSafeGroup group1, ModelSafeGroup group2, ModelSafeGroup group3) {
    std::set<ModelSafeGroup> expected_groups_to_change;
    expected_groups_to_change.insert(group1);
    expected_groups_to_change.insert(group2);
    expected_groups_to_change.insert(group3);
    EXPECT_EQ(expected_groups_to_change,
              command.GetGroupsToChangeForTest(*session()));
  }

 private:
  MessageLoop message_loop_;
  scoped_ptr<TestDirectorySetterUpper> syncdb_;
  scoped_ptr<sessions::SyncSessionContext> context_;
  scoped_ptr<MockConnectionManager> mock_server_;
  scoped_ptr<sessions::SyncSession> session_;
  std::vector<scoped_refptr<ModelSafeWorker> > workers_;
  ModelSafeRoutingInfo routing_info_;
  NiceMock<MockDebugInfoGetter> mock_debug_info_getter_;
  DISALLOW_COPY_AND_ASSIGN(SyncerCommandTestWithParam);
};

class SyncerCommandTest : public SyncerCommandTestWithParam<void*> {};

class MockDirectorySyncerCommandTest
    : public SyncerCommandTestWithParam<void*> {
 public:
  MockDirectorySyncerCommandTest()
      : SyncerCommandTestWithParam<void*>(
          SyncerCommandTestWithParam<void*>::USE_MOCK_DIRECTORY) {}
  MockDirectorySetterUpper::MockDirectory* mock_directory() {
    return static_cast<MockDirectorySetterUpper*>(syncdb())->directory();
  }
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_TEST_ENGINE_SYNCER_COMMAND_TEST_H_
