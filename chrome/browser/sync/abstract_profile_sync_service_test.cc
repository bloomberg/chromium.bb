// Copyright (c) 20111 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/abstract_profile_sync_service_test.h"

#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/autofill_model_associator.h"
#include "chrome/browser/sync/glue/autofill_profile_model_associator.h"
#include "chrome/browser/sync/glue/password_model_associator.h"
#include "chrome/browser/sync/glue/preference_model_associator.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/browser/sync/util/cryptographer.h"
#include "chrome/test/profile_mock.h"
#include "chrome/test/sync/engine/test_id_factory.h"

using browser_sync::TestIdFactory;
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

const std::string ProfileSyncServiceTestHelper::GetTagForType(
    ModelType model_type) {
  switch (model_type) {
    case syncable::AUTOFILL:
      return browser_sync::kAutofillTag;
    case syncable::AUTOFILL_PROFILE:
      return browser_sync::kAutofillProfileTag;
    case syncable::PREFERENCES:
      return browser_sync::kPreferencesTag;
    case syncable::PASSWORDS:
      return browser_sync::kPasswordTag;
    case syncable::NIGORI:
      return browser_sync::kNigoriTag;
    case syncable::TYPED_URLS:
      return browser_sync::kTypedUrlTag;
    case syncable::SESSIONS:
      return browser_sync::kSessionsTag;
    case syncable::BOOKMARKS:
      return "google_chrome_bookmarks";
    default:
      NOTREACHED();
  }
  return std::string();
}

bool ProfileSyncServiceTestHelper::CreateRoot(
    ModelType model_type, UserShare* user_share,
    TestIdFactory* ids) {
  DirectoryManager* dir_manager = user_share->dir_manager.get();

  ScopedDirLookup dir(dir_manager, user_share->name);
  if (!dir.good())
    return false;

  std::string tag_name = GetTagForType(model_type);

  WriteTransaction wtrans(dir, UNITTEST, __FILE__, __LINE__);
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

AbstractProfileSyncServiceTest::AbstractProfileSyncServiceTest()
    : ui_thread_(BrowserThread::UI, &message_loop_) {}

bool AbstractProfileSyncServiceTest::CreateRoot(ModelType model_type) {
  return ProfileSyncServiceTestHelper::CreateRoot(
      model_type,
      service_->GetUserShare(),
      service_->id_factory());
}

CreateRootTask::CreateRootTask(
    AbstractProfileSyncServiceTest* test, ModelType model_type)
    : test_(test), model_type_(model_type), success_(false) {
}

CreateRootTask::~CreateRootTask() {}
void CreateRootTask::Run() {
  success_ = test_->CreateRoot(model_type_);
}

bool CreateRootTask::success() {
  return success_;
}
