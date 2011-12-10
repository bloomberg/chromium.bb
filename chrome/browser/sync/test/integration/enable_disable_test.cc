// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test.h"

#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/internal_api/read_node.h"
#include "chrome/browser/sync/internal_api/read_transaction.h"
#include "chrome/browser/sync/syncable/model_type.h"

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

bool DoesTopLevelNodeExist(sync_api::UserShare* user_share,
                           syncable::ModelType type) {
    sync_api::ReadTransaction trans(FROM_HERE, user_share);
    sync_api::ReadNode node(&trans);
    return node.InitByTagLookup(syncable::ModelTypeToRootTag(type));
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, EnableOneAtATime) {
  ASSERT_TRUE(SetupClients());

  DisableNotifications();

  // Setup sync with no enabled types.
  ASSERT_TRUE(GetClient(0)->SetupSync(syncable::ModelEnumSet()));

  const syncable::ModelEnumSet registered_types =
      GetClient(0)->service()->GetRegisteredDataTypes();
  sync_api::UserShare* user_share = GetClient(0)->service()->GetUserShare();
  for (syncable::ModelEnumSet::Iterator it = registered_types.First();
       it.Good(); it.Inc()) {
    ASSERT_TRUE(GetClient(0)->EnableSyncForDatatype(it.Get()));

    // AUTOFILL_PROFILE is lumped together with AUTOFILL.
    if (it.Get() == syncable::AUTOFILL_PROFILE) {
      continue;
    }

    ASSERT_TRUE(DoesTopLevelNodeExist(user_share, it.Get()))
        << syncable::ModelTypeToString(it.Get());

    // AUTOFILL_PROFILE is lumped together with AUTOFILL.
    if (it.Get() == syncable::AUTOFILL) {
      ASSERT_TRUE(DoesTopLevelNodeExist(user_share,
                                        syncable::AUTOFILL_PROFILE));
    }
  }

  EnableNotifications();
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, DisableOneAtATime) {
  ASSERT_TRUE(SetupClients());

  DisableNotifications();

  // Setup sync with no disabled types.
  ASSERT_TRUE(GetClient(0)->SetupSync());

  const syncable::ModelEnumSet registered_types =
      GetClient(0)->service()->GetRegisteredDataTypes();

  sync_api::UserShare* user_share = GetClient(0)->service()->GetUserShare();

  // Make sure all top-level nodes exist first.
  for (syncable::ModelEnumSet::Iterator it = registered_types.First();
       it.Good(); it.Inc()) {
    ASSERT_TRUE(DoesTopLevelNodeExist(user_share, it.Get()));
  }

  for (syncable::ModelEnumSet::Iterator it = registered_types.First();
       it.Good(); it.Inc()) {
    ASSERT_TRUE(GetClient(0)->DisableSyncForDatatype(it.Get()));

    // AUTOFILL_PROFILE is lumped together with AUTOFILL.
    if (it.Get() == syncable::AUTOFILL_PROFILE) {
      continue;
    }

    sync_api::UserShare* user_share =
        GetClient(0)->service()->GetUserShare();

    ASSERT_FALSE(DoesTopLevelNodeExist(user_share, it.Get()))
        << syncable::ModelTypeToString(it.Get());

    // AUTOFILL_PROFILE is lumped together with AUTOFILL.
    if (it.Get() == syncable::AUTOFILL) {
      ASSERT_FALSE(DoesTopLevelNodeExist(user_share,
                                         syncable::AUTOFILL_PROFILE));
    }
  }

  EnableNotifications();
}

}  // namespace
