// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js_sync_manager_observer.h"

#include <cstddef>

#include "base/basictypes.h"
#include "base/values.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/js_arg_list.h"
#include "chrome/browser/sync/js_test_util.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/test/sync/engine/test_directory_setter_upper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace {

using ::testing::InSequence;
using ::testing::StrictMock;

class JsSyncManagerObserverTest : public testing::Test {
 protected:
  JsSyncManagerObserverTest() : sync_manager_observer_(&mock_router_) {}

  StrictMock<MockJsEventRouter> mock_router_;
  JsSyncManagerObserver sync_manager_observer_;
};

TEST_F(JsSyncManagerObserverTest, NoArgNotifiations) {
  InSequence dummy;

  EXPECT_CALL(mock_router_,
              RouteJsEvent("onInitializationComplete",
                           HasArgs(JsArgList()), NULL));
  EXPECT_CALL(mock_router_,
              RouteJsEvent("onPaused",
                           HasArgs(JsArgList()), NULL));
  EXPECT_CALL(mock_router_,
              RouteJsEvent("onResumed",
                           HasArgs(JsArgList()), NULL));
  EXPECT_CALL(mock_router_,
              RouteJsEvent("onStopSyncingPermanently",
                           HasArgs(JsArgList()), NULL));
  EXPECT_CALL(mock_router_,
              RouteJsEvent("onClearServerDataSucceeded",
                           HasArgs(JsArgList()), NULL));
  EXPECT_CALL(mock_router_,
              RouteJsEvent("onClearServerDataFailed",
                           HasArgs(JsArgList()), NULL));

  sync_manager_observer_.OnInitializationComplete();
  sync_manager_observer_.OnPaused();
  sync_manager_observer_.OnResumed();
  sync_manager_observer_.OnStopSyncingPermanently();
  sync_manager_observer_.OnClearServerDataSucceeded();
  sync_manager_observer_.OnClearServerDataFailed();
}

TEST_F(JsSyncManagerObserverTest, OnChangesComplete) {
  InSequence dummy;

  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    const std::string& model_type_str =
        syncable::ModelTypeToString(syncable::ModelTypeFromInt(i));
    ListValue expected_args;
    expected_args.Append(Value::CreateStringValue(model_type_str));
    EXPECT_CALL(mock_router_,
                RouteJsEvent("onChangesComplete",
                             HasArgsAsList(expected_args), NULL));
  }

  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    sync_manager_observer_.OnChangesComplete(syncable::ModelTypeFromInt(i));
  }
}

TEST_F(JsSyncManagerObserverTest, OnSyncCycleCompleted) {
  std::string download_progress_markers[syncable::MODEL_TYPE_COUNT];
  sessions::SyncSessionSnapshot snapshot(sessions::SyncerStatus(),
                                         sessions::ErrorCounters(),
                                         100,
                                         false,
                                         syncable::ModelTypeBitSet(),
                                         download_progress_markers,
                                         false,
                                         true,
                                         100,
                                         5,
                                         false,
                                         sessions::SyncSourceInfo());
  ListValue expected_args;
  expected_args.Append(snapshot.ToValue());

  EXPECT_CALL(mock_router_,
              RouteJsEvent("onSyncCycleCompleted",
                           HasArgsAsList(expected_args), NULL));

  sync_manager_observer_.OnSyncCycleCompleted(&snapshot);
}

TEST_F(JsSyncManagerObserverTest, OnAuthError) {
  GoogleServiceAuthError error(GoogleServiceAuthError::TWO_FACTOR);
  ListValue expected_args;
  expected_args.Append(error.ToValue());

  EXPECT_CALL(mock_router_,
              RouteJsEvent("onAuthError",
                           HasArgsAsList(expected_args), NULL));

  sync_manager_observer_.OnAuthError(error);
}

TEST_F(JsSyncManagerObserverTest, OnPassphraseRequired) {
  InSequence dummy;

  ListValue true_args, false_args;
  true_args.Append(Value::CreateBooleanValue(true));
  false_args.Append(Value::CreateBooleanValue(false));

  EXPECT_CALL(mock_router_,
              RouteJsEvent("onPassphraseRequired",
                           HasArgsAsList(false_args), NULL));
  EXPECT_CALL(mock_router_,
              RouteJsEvent("onPassphraseRequired",
                           HasArgsAsList(true_args), NULL));

  sync_manager_observer_.OnPassphraseRequired(false);
  sync_manager_observer_.OnPassphraseRequired(true);
}

TEST_F(JsSyncManagerObserverTest, SensitiveNotifiations) {
  ListValue redacted_args;
  redacted_args.Append(Value::CreateStringValue("<redacted>"));

  EXPECT_CALL(mock_router_,
              RouteJsEvent("onUpdatedToken",
                           HasArgsAsList(redacted_args), NULL));
  EXPECT_CALL(mock_router_,
              RouteJsEvent("onPassphraseAccepted",
                           HasArgsAsList(redacted_args), NULL));

  sync_manager_observer_.OnUpdatedToken("sensitive_token");
  sync_manager_observer_.OnPassphraseAccepted("sensitive_token");
}

namespace {

// Makes a node of the given model type.  Returns the id of the
// newly-created node.
int64 MakeNode(sync_api::UserShare* share, syncable::ModelType model_type) {
  sync_api::WriteTransaction trans(share);
  sync_api::ReadNode root_node(&trans);
  root_node.InitByRootLookup();
  sync_api::WriteNode node(&trans);
  EXPECT_TRUE(node.InitUniqueByCreation(
      model_type, root_node,
      syncable::ModelTypeToString(model_type)));
  node.SetIsFolder(false);
  return node.GetId();
}

}  // namespace

TEST_F(JsSyncManagerObserverTest, OnChangesApplied) {
  InSequence dummy;

  TestDirectorySetterUpper setter_upper;
  sync_api::UserShare share;
  setter_upper.SetUp();
  share.dir_manager.reset(setter_upper.manager());
  share.name = setter_upper.name();

  // We don't test with passwords as that requires additional setup.

  // Build a list of example ChangeRecords.
  sync_api::SyncManager::ChangeRecord changes[syncable::PASSWORDS];
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::PASSWORDS; ++i) {
    changes[i].id = MakeNode(&share, syncable::ModelTypeFromInt(i));
    switch (i % 3) {
      case 0:
        changes[i].action =
            sync_api::SyncManager::ChangeRecord::ACTION_ADD;
        break;
      case 1:
        changes[i].action =
            sync_api::SyncManager::ChangeRecord::ACTION_UPDATE;
        break;
      default:
        changes[i].action =
            sync_api::SyncManager::ChangeRecord::ACTION_DELETE;
        break;
    }
    {
      sync_api::ReadTransaction trans(&share);
      sync_api::ReadNode node(&trans);
      EXPECT_TRUE(node.InitByIdLookup(changes[i].id));
      changes[i].specifics = node.GetEntry()->Get(syncable::SPECIFICS);
    }
  }

  // Set expectations for each data type.
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::PASSWORDS; ++i) {
    const std::string& model_type_str =
        syncable::ModelTypeToString(syncable::ModelTypeFromInt(i));
    ListValue expected_args;
    expected_args.Append(Value::CreateStringValue(model_type_str));
    ListValue* expected_changes = new ListValue();
    expected_args.Append(expected_changes);
    for (int j = i; j < syncable::PASSWORDS; ++j) {
      sync_api::ReadTransaction trans(&share);
      expected_changes->Append(changes[i].ToValue(&trans));
    }
    EXPECT_CALL(mock_router_,
                RouteJsEvent("onChangesApplied",
                             HasArgsAsList(expected_args), NULL));
  }

  // Fire OnChangesApplied() for each data type.
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::PASSWORDS; ++i) {
    sync_api::ReadTransaction trans(&share);
    sync_manager_observer_.OnChangesApplied(syncable::ModelTypeFromInt(i),
                                            &trans, &changes[i],
                                            syncable::PASSWORDS - i);
  }

  // |share.dir_manager| does not actually own its value.
  ignore_result(share.dir_manager.release());
  setter_upper.TearDown();
}

}  // namespace
}  // namespace browser_sync
