// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_
#define CHROME_BROWSER_SYNC_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_
#pragma once

#include <string>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/autofill_model_associator.h"
#include "chrome/browser/sync/glue/password_model_associator.h"
#include "chrome/browser/sync/glue/preference_model_associator.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "chrome/browser/sync/profile_sync_factory_mock.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/browser/sync/util/cryptographer.h"
#include "chrome/test/profile_mock.h"
#include "chrome/test/sync/engine/test_id_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

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

class ProfileSyncServiceTestHelper {
 public:
  static const std::string GetTagForType(ModelType type) {
    switch (type) {
      case syncable::AUTOFILL:
        return browser_sync::kAutofillTag;
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
        return std::string();
    }
  }

  static bool CreateRoot(ModelType model_type, ProfileSyncService* service,
                         TestIdFactory* ids) {
    UserShare* user_share = service->backend()->GetUserShareHandle();
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
    EXPECT_TRUE(node.Put(syncable::ID, ids->MakeServer(tag_name)));
    sync_pb::EntitySpecifics specifics;
    syncable::AddDefaultExtensionValue(model_type, &specifics);
    node.Put(SPECIFICS, specifics);

    return true;
  }
};

class AbstractProfileSyncServiceTest : public testing::Test {
 public:
  AbstractProfileSyncServiceTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

  bool CreateRoot(ModelType model_type) {
    return ProfileSyncServiceTestHelper::CreateRoot(model_type,
                                                    service_.get(),
                                                    service_->id_factory());
  }

 protected:

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  ProfileSyncFactoryMock factory_;
  TokenService token_service_;
  scoped_ptr<TestProfileSyncService> service_;
};

class CreateRootTask : public Task {
 public:
  CreateRootTask(AbstractProfileSyncServiceTest* test, ModelType model_type)
      : test_(test), model_type_(model_type), success_(false) {
  }

  virtual ~CreateRootTask() {}
  virtual void Run() {
    success_ = test_->CreateRoot(model_type_);
  }

  bool success() { return success_; }

 private:
  AbstractProfileSyncServiceTest* test_;
  ModelType model_type_;
  bool success_;
};

#endif  // CHROME_BROWSER_SYNC_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_
