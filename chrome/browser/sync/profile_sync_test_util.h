// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_

#include "chrome/browser/webdata/web_database.h"
#include "chrome/browser/sync/glue/bookmark_change_processor.h"
#include "chrome/browser/sync/glue/bookmark_data_type_controller.h"
#include "chrome/browser/sync/glue/bookmark_model_associator.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/data_type_manager_impl.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/unrecoverable_error_handler.h"
#include "chrome/test/sync/test_http_bridge_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

// This action is used to mock out the ProfileSyncFactory
// CreateDataTypeManager method.
ACTION(MakeDataTypeManager) {
  return new browser_sync::DataTypeManagerImpl(arg0);
}

template <class ModelAssociatorImpl>
class TestModelAssociator : public ModelAssociatorImpl {
 public:
  explicit TestModelAssociator(
      ProfileSyncService* service,
      browser_sync::UnrecoverableErrorHandler* error_handler)
      : ModelAssociatorImpl(service, error_handler) {
  }

  virtual bool GetSyncIdForTaggedNode(const std::string& tag, int64* sync_id) {
    std::wstring tag_wide;
    if (!UTF8ToWide(tag.c_str(), tag.length(), &tag_wide)) {
      NOTREACHED() << "Unable to convert UTF8 to wide for string: " << tag;
      return false;
    }

    sync_api::WriteTransaction trans(
        ModelAssociatorImpl::sync_service()->backend()->GetUserShareHandle());
    sync_api::ReadNode root(&trans);
    root.InitByRootLookup();

    // First, try to find a node with the title among the root's children.
    // This will be the case if we are testing model persistence, and
    // are reloading a sync repository created earlier in the test.
    int64 last_child_id = sync_api::kInvalidId;
    for (int64 id = root.GetFirstChildId(); id != sync_api::kInvalidId; /***/) {
      sync_api::ReadNode child(&trans);
      child.InitByIdLookup(id);
      last_child_id = id;
      if (tag_wide == child.GetTitle()) {
        *sync_id = id;
        return true;
      }
      id = child.GetSuccessorId();
    }

    sync_api::ReadNode predecessor_node(&trans);
    sync_api::ReadNode* predecessor = NULL;
    if (last_child_id != sync_api::kInvalidId) {
      predecessor_node.InitByIdLookup(last_child_id);
      predecessor = &predecessor_node;
    }
    sync_api::WriteNode node(&trans);
    // Create new fake tagged nodes at the end of the ordering.
    node.InitByCreation(ModelAssociatorImpl::model_type(), root, predecessor);
    node.SetIsFolder(true);
    node.SetTitle(tag_wide);
    node.SetExternalId(0);
    *sync_id = node.GetId();
    return true;
  }

  ~TestModelAssociator() {}
};

class ProfileSyncServiceObserverMock : public ProfileSyncServiceObserver {
 public:
  MOCK_METHOD0(OnStateChanged, void());
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_
