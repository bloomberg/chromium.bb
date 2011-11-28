// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/abstract_profile_sync_service_test.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "chrome/browser/sync/internal_api/write_transaction.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/browser/sync/test/engine/test_id_factory.h"
#include "chrome/browser/sync/util/cryptographer.h"

using browser_sync::TestIdFactory;
using content::BrowserThread;
using sync_api::UserShare;
using syncable::BASE_VERSION;
using syncable::CREATE;
using syncable::DirectoryManager;
using syncable::IS_DEL;
using syncable::IS_DIR;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::IS_UNSYNCED;
using syncable::ModelType;
using syncable::MutableEntry;
using syncable::SERVER_IS_DIR;
using syncable::SERVER_VERSION;
using syncable::SPECIFICS;
using syncable::ScopedDirLookup;
using syncable::UNIQUE_SERVER_TAG;
using syncable::UNITTEST;
using syncable::WriteTransaction;

/* static */
const std::string ProfileSyncServiceTestHelper::GetTagForType(
    ModelType model_type) {
  return syncable::ModelTypeToRootTag(model_type);
}

/* static */
bool ProfileSyncServiceTestHelper::CreateRoot(ModelType model_type,
                                              UserShare* user_share,
                                              TestIdFactory* ids) {
  DirectoryManager* dir_manager = user_share->dir_manager.get();

  ScopedDirLookup dir(dir_manager, user_share->name);
  if (!dir.good())
    return false;

  std::string tag_name = GetTagForType(model_type);

  WriteTransaction wtrans(FROM_HERE, UNITTEST, dir);
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
  node.Put(syncable::ID, ids->MakeServer(tag_name));
  sync_pb::EntitySpecifics specifics;
  syncable::AddDefaultExtensionValue(model_type, &specifics);
  node.Put(SPECIFICS, specifics);

  return true;
}

/* static */
sync_api::ImmutableChangeRecordList
    ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
        int64 node_id, sync_api::ChangeRecord::Action action) {
  sync_api::ChangeRecord record;
  record.action = action;
  record.id = node_id;
  sync_api::ChangeRecordList records(1, record);
  return sync_api::ImmutableChangeRecordList(&records);
}

/* static */
sync_api::ImmutableChangeRecordList
    ProfileSyncServiceTestHelper::MakeSingletonDeletionChangeRecordList(
        int64 node_id, const sync_pb::EntitySpecifics& specifics) {
  sync_api::ChangeRecord record;
  record.action = sync_api::ChangeRecord::ACTION_DELETE;
  record.id = node_id;
  record.specifics = specifics;
  sync_api::ChangeRecordList records(1, record);
  return sync_api::ImmutableChangeRecordList(&records);
}

AbstractProfileSyncServiceTest::AbstractProfileSyncServiceTest()
    : ui_thread_(BrowserThread::UI, &ui_loop_),
      db_thread_(BrowserThread::DB),
      io_thread_(BrowserThread::IO),
      token_service_(new TokenService) {}

AbstractProfileSyncServiceTest::~AbstractProfileSyncServiceTest() {}

void AbstractProfileSyncServiceTest::SetUp() {
  db_thread_.Start();
  io_thread_.StartIOThread();
}

void AbstractProfileSyncServiceTest::TearDown() {
  // Pump messages posted by the sync core thread (which may end up
  // posting on the IO thread).
  ui_loop_.RunAllPending();
  // We need to destroy the |token_service_| here before we stop the
  // |io_thread_| because it holds references to a ref-counted
  // URLRequestContext. The deletion is passed to the |io_thread_|
  // by scoped_refptr. If |token_service_| is destroyed after stopping
  // the |io_thread_|, the deletion never happens.
  token_service_.reset(NULL);
  io_thread_.Stop();
  db_thread_.Stop();
  ui_loop_.RunAllPending();
}

bool AbstractProfileSyncServiceTest::CreateRoot(ModelType model_type) {
  return ProfileSyncServiceTestHelper::CreateRoot(
      model_type,
      service_->GetUserShare(),
      service_->id_factory());
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
