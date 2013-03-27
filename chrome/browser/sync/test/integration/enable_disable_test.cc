// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test.h"

#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"

// This file contains tests that exercise enabling and disabling data
// types.

namespace {

class EnableDisableTest : public SyncTest {
 public:
  explicit EnableDisableTest(TestType test_type) : SyncTest(test_type) {}
  virtual ~EnableDisableTest() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(EnableDisableTest);
};

class EnableDisableSingleClientTest : public EnableDisableTest {
 public:
  EnableDisableSingleClientTest() : EnableDisableTest(SINGLE_CLIENT) {}
  virtual ~EnableDisableSingleClientTest() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(EnableDisableSingleClientTest);
};

bool DoesTopLevelNodeExist(syncer::UserShare* user_share,
                           syncer::ModelType type) {
    syncer::ReadTransaction trans(FROM_HERE, user_share);
    syncer::ReadNode node(&trans);
    return node.InitByTagLookup(syncer::ModelTypeToRootTag(type)) ==
        syncer::BaseNode::INIT_OK;
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, EnableOneAtATime) {
  ASSERT_TRUE(SetupClients());

  // Setup sync with no enabled types.
  ASSERT_TRUE(GetClient(0)->SetupSync(syncer::ModelTypeSet()));

  // TODO(rlarocque, 97780): It should be possible to disable notifications
  // before calling SetupSync().  We should move this line back to the top
  // of this function when this is supported.
  DisableNotifications();

  const syncer::ModelTypeSet registered_types =
      GetClient(0)->service()->GetRegisteredDataTypes();
  syncer::UserShare* user_share = GetClient(0)->service()->GetUserShare();
  for (syncer::ModelTypeSet::Iterator it = registered_types.First();
       it.Good(); it.Inc()) {
    ASSERT_TRUE(GetClient(0)->EnableSyncForDatatype(it.Get()));

    // AUTOFILL_PROFILE is lumped together with AUTOFILL.
    // SESSIONS is lumped together with PROXY_TABS and
    // HISTORY_DELETE_DIRECTIVES.
    if (it.Get() == syncer::AUTOFILL_PROFILE || it.Get() == syncer::SESSIONS) {
      continue;
    }

    if (!syncer::ProxyTypes().Has(it.Get())) {
      ASSERT_TRUE(DoesTopLevelNodeExist(user_share, it.Get()))
          << syncer::ModelTypeToString(it.Get());
    }

    // AUTOFILL_PROFILE is lumped together with AUTOFILL.
    if (it.Get() == syncer::AUTOFILL) {
      ASSERT_TRUE(DoesTopLevelNodeExist(user_share,
                                        syncer::AUTOFILL_PROFILE));
    } else if (it.Get() == syncer::HISTORY_DELETE_DIRECTIVES ||
               it.Get() == syncer::PROXY_TABS) {
      ASSERT_TRUE(DoesTopLevelNodeExist(user_share,
                                        syncer::SESSIONS));
    }
  }

  EnableNotifications();
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, DisableOneAtATime) {
  ASSERT_TRUE(SetupClients());

  // Setup sync with no disabled types.
  ASSERT_TRUE(GetClient(0)->SetupSync());

  // TODO(rlarocque, 97780): It should be possible to disable notifications
  // before calling SetupSync().  We should move this line back to the top
  // of this function when this is supported.
  DisableNotifications();

  const syncer::ModelTypeSet registered_types =
      GetClient(0)->service()->GetRegisteredDataTypes();

  syncer::UserShare* user_share = GetClient(0)->service()->GetUserShare();

  // Make sure all top-level nodes exist first.
  for (syncer::ModelTypeSet::Iterator it = registered_types.First();
       it.Good(); it.Inc()) {
    if (!syncer::ProxyTypes().Has(it.Get())) {
      ASSERT_TRUE(DoesTopLevelNodeExist(user_share, it.Get()));
    }
  }

  for (syncer::ModelTypeSet::Iterator it = registered_types.First();
       it.Good(); it.Inc()) {
    ASSERT_TRUE(GetClient(0)->DisableSyncForDatatype(it.Get()));

    // AUTOFILL_PROFILE is lumped together with AUTOFILL.
    // SESSIONS is lumped together with PROXY_TABS and
    // HISTORY_DELETE_DIRECTIVES.
    // PRIORITY_PREFERENCES is lumped together with PREFERENCES.
    if (it.Get() == syncer::AUTOFILL_PROFILE || it.Get() == syncer::SESSIONS ||
        it.Get() == syncer::PRIORITY_PREFERENCES) {
      continue;
    }

    syncer::UserShare* user_share =
        GetClient(0)->service()->GetUserShare();

    ASSERT_FALSE(DoesTopLevelNodeExist(user_share, it.Get()))
        << syncer::ModelTypeToString(it.Get());

    // AUTOFILL_PROFILE is lumped together with AUTOFILL.
    if (it.Get() == syncer::AUTOFILL) {
      ASSERT_FALSE(DoesTopLevelNodeExist(user_share,
                                         syncer::AUTOFILL_PROFILE));
    } else if (it.Get() == syncer::HISTORY_DELETE_DIRECTIVES ||
               it.Get() == syncer::PROXY_TABS) {
      ASSERT_FALSE(DoesTopLevelNodeExist(user_share,
                                         syncer::SESSIONS));
    } else if (it.Get() == syncer::PREFERENCES) {
      ASSERT_FALSE(DoesTopLevelNodeExist(user_share,
                                         syncer::PRIORITY_PREFERENCES));
    }
  }

  EnableNotifications();
}

}  // namespace
