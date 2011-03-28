// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_
#define CHROME_BROWSER_SYNC_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/sync/profile_sync_factory_mock.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "content/browser/browser_thread.h"
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
};

class AbstractProfileSyncServiceTest : public testing::Test {
 public:
  AbstractProfileSyncServiceTest();
  virtual ~AbstractProfileSyncServiceTest();

  bool CreateRoot(syncable::ModelType model_type);

 protected:
  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  ProfileSyncFactoryMock factory_;
  TokenService token_service_;
  scoped_ptr<TestProfileSyncService> service_;
};

class CreateRootTask : public Task {
 public:
  CreateRootTask(AbstractProfileSyncServiceTest* test,
                 syncable::ModelType model_type);

  virtual ~CreateRootTask();
  virtual void Run();

  bool success();

 private:
  AbstractProfileSyncServiceTest* test_;
  syncable::ModelType model_type_;
  bool success_;
};

#endif  // CHROME_BROWSER_SYNC_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_
