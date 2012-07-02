// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/abstract_profile_sync_service_test.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/write_transaction.h"
#include "sync/test/engine/test_id_factory.h"
#include "sync/util/cryptographer.h"

using syncer::TestIdFactory;
using content::BrowserThread;
using syncer::UserShare;
using syncer::syncable::BASE_VERSION;
using syncer::syncable::CREATE;
using syncer::syncable::IS_DEL;
using syncer::syncable::IS_DIR;
using syncer::syncable::IS_UNAPPLIED_UPDATE;
using syncer::syncable::IS_UNSYNCED;
using syncer::syncable::ModelType;
using syncer::syncable::MutableEntry;
using syncer::syncable::SERVER_IS_DIR;
using syncer::syncable::SERVER_VERSION;
using syncer::syncable::SPECIFICS;
using syncer::syncable::UNIQUE_SERVER_TAG;
using syncer::syncable::UNITTEST;
using syncer::syncable::WriteTransaction;

/* static */
const std::string ProfileSyncServiceTestHelper::GetTagForType(
    ModelType model_type) {
  return syncable::ModelTypeToRootTag(model_type);
}

/* static */
bool ProfileSyncServiceTestHelper::CreateRoot(ModelType model_type,
                                              UserShare* user_share,
                                              TestIdFactory* ids) {
  syncer::syncable::Directory* directory = user_share->directory.get();

  std::string tag_name = GetTagForType(model_type);

  WriteTransaction wtrans(FROM_HERE, UNITTEST, directory);
  MutableEntry node(&wtrans,
                    CREATE,
                    wtrans.root_id(),
                    tag_name);
  node.Put(UNIQUE_SERVER_TAG, tag_name);
  node.Put(IS_DIR, true);
  node.Put(SERVER_IS_DIR, false);
  node.Put(IS_UNSYNCED, false);
  node.Put(IS_UNAPPLIED_UPDATE, false);
  node.Put(SERVER_VERSION, 20);
  node.Put(BASE_VERSION, 20);
  node.Put(IS_DEL, false);
  node.Put(syncer::syncable::ID, ids->MakeServer(tag_name));
  sync_pb::EntitySpecifics specifics;
  syncable::AddDefaultFieldValue(model_type, &specifics);
  node.Put(SPECIFICS, specifics);

  return true;
}

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
      io_thread_(BrowserThread::IO) {
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
  ui_loop_.RunAllPending();
  io_thread_.Stop();
  file_thread_.Stop();
  db_thread_.Stop();
  ui_loop_.RunAllPending();
}

bool AbstractProfileSyncServiceTest::CreateRoot(ModelType model_type) {
  return ProfileSyncServiceTestHelper::CreateRoot(
      model_type,
      service_->GetUserShare(),
      service_->id_factory());
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
