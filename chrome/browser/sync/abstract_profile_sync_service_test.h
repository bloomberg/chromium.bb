// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_
#define CHROME_BROWSER_SYNC_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "content/public/test/test_browser_thread.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/change_record.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileSyncService;
class TestProfileSyncService;

namespace syncer {
struct UserShare;
}  //  namespace syncer

class ProfileSyncServiceTestHelper {
 public:
  static syncer::ImmutableChangeRecordList MakeSingletonChangeRecordList(
      int64 node_id, syncer::ChangeRecord::Action action);

  // Deletions must provide an EntitySpecifics for the deleted data.
  static syncer::ImmutableChangeRecordList
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

  bool CreateRoot(syncer::ModelType model_type);

  static ProfileKeyedService* BuildTokenService(
      content::BrowserContext* profile);

 protected:
  MessageLoopForUI ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;
  TokenService* token_service_;
  TestProfileSyncService* sync_service_;
};

class CreateRootHelper {
 public:
  CreateRootHelper(AbstractProfileSyncServiceTest* test,
                   syncer::ModelType model_type);
  virtual ~CreateRootHelper();

  const base::Closure& callback() const;
  bool success();

 private:
  void CreateRootCallback();

  base::Closure callback_;
  AbstractProfileSyncServiceTest* test_;
  syncer::ModelType model_type_;
  bool success_;
};

#endif  // CHROME_BROWSER_SYNC_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_
