// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/abstract_profile_sync_service_test.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/protocol/sync.pb.h"
#include "sync/util/cryptographer.h"

using content::BrowserThread;
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
    : ui_thread_(BrowserThread::UI, &ui_loop_),
      db_thread_(BrowserThread::DB),
      file_thread_(BrowserThread::FILE),
      io_thread_(BrowserThread::IO),
      token_service_(NULL),
      sync_service_(NULL) {
}

AbstractProfileSyncServiceTest::~AbstractProfileSyncServiceTest() {}

void AbstractProfileSyncServiceTest::SetUp() {
  db_thread_.Start();
  file_thread_.Start();
  io_thread_.StartIOThread();
}

void AbstractProfileSyncServiceTest::TearDown() {
  // Pump messages posted by the sync core thread (which may end up
  // posting on the IO thread).
  ui_loop_.RunUntilIdle();
  io_thread_.Stop();
  file_thread_.Stop();
  db_thread_.Stop();
  ui_loop_.RunUntilIdle();
}

bool AbstractProfileSyncServiceTest::CreateRoot(ModelType model_type) {
  return syncer::TestUserShare::CreateRoot(model_type,
                                           sync_service_->GetUserShare());
}

// static
ProfileKeyedService* AbstractProfileSyncServiceTest::BuildTokenService(
    Profile* profile) {
  return new TokenService;
}

CreateRootHelper::CreateRootHelper(AbstractProfileSyncServiceTest* test,
                                   ModelType model_type)
    : ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
          base::Bind(&CreateRootHelper::CreateRootCallback,
                     base::Unretained(this)))),
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
