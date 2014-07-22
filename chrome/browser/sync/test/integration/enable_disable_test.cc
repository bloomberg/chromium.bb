// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"

// This file contains tests that exercise enabling and disabling data
// types.

namespace {

class EnableDisableSingleClientTest : public SyncTest {
 public:
  EnableDisableSingleClientTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~EnableDisableSingleClientTest() {}

  // Don't use self-notifications as they can trigger additional sync cycles.
  virtual bool TestUsesSelfNotifications() OVERRIDE { return false; }
 private:
  DISALLOW_COPY_AND_ASSIGN(EnableDisableSingleClientTest);
};

bool DoesTopLevelNodeExist(syncer::UserShare* user_share,
                           syncer::ModelType type) {
    syncer::ReadTransaction trans(FROM_HERE, user_share);
    syncer::ReadNode node(&trans);
    return node.InitTypeRoot(type) == syncer::BaseNode::INIT_OK;
}

bool IsUnready(const sync_driver::FailedDataTypesHandler& failed_handler,
               syncer::ModelType type) {
  return failed_handler.GetUnreadyErrorTypes().Has(type);
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, EnableOneAtATime) {
  ASSERT_TRUE(SetupClients());

  // Setup sync with no enabled types.
  ASSERT_TRUE(GetClient(0)->SetupSync(syncer::ModelTypeSet()));

  const syncer::ModelTypeSet registered_types =
      GetSyncService(0)->GetRegisteredDataTypes();
  syncer::UserShare* user_share = GetSyncService(0)->GetUserShare();
  const sync_driver::FailedDataTypesHandler& failed_handler =
      GetSyncService(0)->failed_data_types_handler();
  for (syncer::ModelTypeSet::Iterator it = registered_types.First();
       it.Good(); it.Inc()) {
    ASSERT_TRUE(GetClient(0)->EnableSyncForDatatype(it.Get()));

    // AUTOFILL_PROFILE is lumped together with AUTOFILL.
    // SESSIONS is lumped together with PROXY_TABS and
    // HISTORY_DELETE_DIRECTIVES.
    // Favicons are lumped together with PROXY_TABS and
    // HISTORY_DELETE_DIRECTIVES.
    if (it.Get() == syncer::AUTOFILL_PROFILE || it.Get() == syncer::SESSIONS) {
      continue;
    }

    if (!syncer::ProxyTypes().Has(it.Get())) {
      ASSERT_TRUE(DoesTopLevelNodeExist(user_share, it.Get()) ||
                  IsUnready(failed_handler, it.Get()))
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
}

IN_PROC_BROWSER_TEST_F(EnableDisableSingleClientTest, DisableOneAtATime) {
  ASSERT_TRUE(SetupClients());

  // Setup sync with no disabled types.
  ASSERT_TRUE(GetClient(0)->SetupSync());

  const syncer::ModelTypeSet registered_types =
      GetSyncService(0)->GetRegisteredDataTypes();

  syncer::UserShare* user_share = GetSyncService(0)->GetUserShare();

  const sync_driver::FailedDataTypesHandler& failed_handler =
      GetSyncService(0)->failed_data_types_handler();

  // Make sure all top-level nodes exist first.
  for (syncer::ModelTypeSet::Iterator it = registered_types.First();
       it.Good(); it.Inc()) {
    if (!syncer::ProxyTypes().Has(it.Get())) {
      ASSERT_TRUE(DoesTopLevelNodeExist(user_share, it.Get()) ||
                  IsUnready(failed_handler, it.Get()));
    }
  }

  for (syncer::ModelTypeSet::Iterator it = registered_types.First();
       it.Good(); it.Inc()) {
    // SUPERVISED_USERS and SUPERVISED_USER_SHARED_SETTINGS are always synced.
    if (it.Get() == syncer::SUPERVISED_USERS ||
        it.Get() == syncer::SUPERVISED_USER_SHARED_SETTINGS ||
        it.Get() == syncer::SYNCED_NOTIFICATIONS ||
        it.Get() == syncer::SYNCED_NOTIFICATION_APP_INFO)
      continue;

    ASSERT_TRUE(GetClient(0)->DisableSyncForDatatype(it.Get()));

    // AUTOFILL_PROFILE is lumped together with AUTOFILL.
    // SESSIONS is lumped together with PROXY_TABS and TYPED_URLS.
    // HISTORY_DELETE_DIRECTIVES is lumped together with TYPED_URLS.
    // PRIORITY_PREFERENCES is lumped together with PREFERENCES.
    // Favicons are lumped together with PROXY_TABS and
    // HISTORY_DELETE_DIRECTIVES.
    if (it.Get() == syncer::AUTOFILL_PROFILE ||
        it.Get() == syncer::SESSIONS ||
        it.Get() == syncer::HISTORY_DELETE_DIRECTIVES ||
        it.Get() == syncer::PRIORITY_PREFERENCES ||
        it.Get() == syncer::FAVICON_IMAGES ||
        it.Get() == syncer::FAVICON_TRACKING) {
      continue;
    }

    syncer::UserShare* user_share =
        GetSyncService(0)->GetUserShare();

    ASSERT_FALSE(DoesTopLevelNodeExist(user_share, it.Get()))
        << syncer::ModelTypeToString(it.Get());

    if (it.Get() == syncer::AUTOFILL) {
      // AUTOFILL_PROFILE is lumped together with AUTOFILL.
      ASSERT_FALSE(DoesTopLevelNodeExist(user_share, syncer::AUTOFILL_PROFILE));
    } else if (it.Get() == syncer::TYPED_URLS) {
      ASSERT_FALSE(DoesTopLevelNodeExist(user_share,
                                         syncer::HISTORY_DELETE_DIRECTIVES));
      // SESSIONS should be enabled only if PROXY_TABS is.
      ASSERT_EQ(GetClient(0)->IsTypePreferred(syncer::PROXY_TABS),
                DoesTopLevelNodeExist(user_share, syncer::SESSIONS));
    } else if (it.Get() == syncer::PROXY_TABS) {
      // SESSIONS should be enabled only if TYPED_URLS is.
      ASSERT_EQ(GetClient(0)->IsTypePreferred(syncer::TYPED_URLS),
                DoesTopLevelNodeExist(user_share, syncer::SESSIONS));
    } else if (it.Get() == syncer::PREFERENCES) {
      ASSERT_FALSE(DoesTopLevelNodeExist(user_share,
                                         syncer::PRIORITY_PREFERENCES));
    }
  }
}

}  // namespace
