// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/core/test/sync_manager_factory_for_profile_sync_test.h"

#include "components/sync/core_impl/test/sync_manager_for_profile_sync_test.h"

namespace syncer {

SyncManagerFactoryForProfileSyncTest::SyncManagerFactoryForProfileSyncTest(
    base::Closure init_callback)
    : SyncManagerFactory(), init_callback_(init_callback) {}

SyncManagerFactoryForProfileSyncTest::~SyncManagerFactoryForProfileSyncTest() {}

std::unique_ptr<syncer::SyncManager>
SyncManagerFactoryForProfileSyncTest::CreateSyncManager(
    const std::string& name) {
  return std::unique_ptr<syncer::SyncManager>(
      new SyncManagerForProfileSyncTest(name, init_callback_));
}

}  // namespace syncer
