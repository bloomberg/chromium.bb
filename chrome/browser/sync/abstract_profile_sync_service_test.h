// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_
#define CHROME_BROWSER_SYNC_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/sync/internal_api/change_record.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileSyncService;
class TestProfileSyncService;

namespace browser_sync {
class TestIdFactory;
}  // namespace browser_sync

namespace sync_api {
struct UserShare;
}  //  namespace sync_api

class ProfileSyncServiceTestHelper {
 public:
  static const std::string GetTagForType(syncable::ModelType model_type);

  static bool CreateRoot(syncable::ModelType model_type,
                         sync_api::UserShare* service,
                         browser_sync::TestIdFactory* ids);

  static sync_api::ImmutableChangeRecordList MakeSingletonChangeRecordList(
      int64 node_id, sync_api::ChangeRecord::Action action);

  // Deletions must provide an EntitySpecifics for the deleted data.
  static sync_api::ImmutableChangeRecordList
      MakeSingletonDeletionChangeRecordList(
          int64 node_id,
          const sync_pb::EntitySpecifics& specifics);
};

class AbstractProfileSyncServiceTest : public testing::Test {
 public:
  AbstractProfileSyncServiceTest();
  virtual ~AbstractProfileSyncServiceTest();

  virtual void SetUp() OVERRIDE;

  virtual void TearDown() OVERRIDE;

  bool CreateRoot(syncable::ModelType model_type);

 protected:
  MessageLoopForUI ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  content::TestBrowserThread io_thread_;
  ProfileSyncComponentsFactoryMock factory_;
  scoped_ptr<TokenService> token_service_;
  scoped_ptr<TestProfileSyncService> service_;
};

class CreateRootHelper {
 public:
  CreateRootHelper(AbstractProfileSyncServiceTest* test,
                   syncable::ModelType model_type);
  virtual ~CreateRootHelper();

  const base::Closure& callback() const;
  bool success();

 private:
  void CreateRootCallback();

  base::Closure callback_;
  AbstractProfileSyncServiceTest* test_;
  syncable::ModelType model_type_;
  bool success_;
};

#endif  // CHROME_BROWSER_SYNC_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_
