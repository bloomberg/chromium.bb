// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/abstract_profile_sync_service_test.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "content/public/test/test_utils.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/protocol/sync.pb.h"
#include "sync/util/cryptographer.h"

using syncer::ModelType;
using syncer::UserShare;

/* static */
syncer::ImmutableChangeRecordList
    ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
        int64 node_id, syncer::ChangeRecord::Action action) {
  syncer::ChangeRecord record;
  record.action = action;
  record.id = node_id;
  syncer::ChangeRecordList records(1, record);
  return syncer::ImmutableChangeRecordList(&records);
}

/* static */
syncer::ImmutableChangeRecordList
    ProfileSyncServiceTestHelper::MakeSingletonDeletionChangeRecordList(
        int64 node_id, const sync_pb::EntitySpecifics& specifics) {
  syncer::ChangeRecord record;
  record.action = syncer::ChangeRecord::ACTION_DELETE;
  record.id = node_id;
  record.specifics = specifics;
  syncer::ChangeRecordList records(1, record);
  return syncer::ImmutableChangeRecordList(&records);
}

AbstractProfileSyncServiceTest::AbstractProfileSyncServiceTest()
    : thread_bundle_(content::TestBrowserThreadBundle::REAL_DB_THREAD |
                     content::TestBrowserThreadBundle::REAL_FILE_THREAD |
                     content::TestBrowserThreadBundle::REAL_IO_THREAD),
      sync_service_(NULL) {
}

AbstractProfileSyncServiceTest::~AbstractProfileSyncServiceTest() {}

void AbstractProfileSyncServiceTest::SetUp() {
}

void AbstractProfileSyncServiceTest::TearDown() {
  // Pump messages posted by the sync core thread (which may end up
  // posting on the IO thread).
  base::RunLoop().RunUntilIdle();
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  base::RunLoop().RunUntilIdle();
}

bool AbstractProfileSyncServiceTest::CreateRoot(ModelType model_type) {
  return syncer::TestUserShare::CreateRoot(model_type,
                                           sync_service_->GetUserShare());
}

CreateRootHelper::CreateRootHelper(AbstractProfileSyncServiceTest* test,
                                   ModelType model_type)
    : callback_(base::Bind(&CreateRootHelper::CreateRootCallback,
                           base::Unretained(this))),
      test_(test),
      model_type_(model_type),
      success_(false) {
}

CreateRootHelper::~CreateRootHelper() {
}

const base::Closure& CreateRootHelper::callback() const {
  return callback_;
}

bool CreateRootHelper::success() {
  return success_;
}

void CreateRootHelper::CreateRootCallback() {
  success_ = test_->CreateRoot(model_type_);
}
