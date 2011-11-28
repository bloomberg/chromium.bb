// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/glue/data_type_manager_impl.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/engine/test_id_factory.h"
#include "chrome/test/base/profile_mock.h"
#include "testing/gmock/include/gmock/gmock.h"

class Profile;
class Task;
class TestProfileSyncService;

ACTION(ReturnNewDataTypeManager) {
  return new browser_sync::DataTypeManagerImpl(arg0, arg1);
}

namespace browser_sync {

class SyncBackendHostForProfileSyncTest : public SyncBackendHost {
 public:
  // |synchronous_init| causes initialization to block until the syncapi has
  //     completed setting itself up and called us back.
  SyncBackendHostForProfileSyncTest(
      Profile* profile,
      const base::WeakPtr<SyncPrefs>& sync_prefs,
      bool set_initial_sync_ended_on_init,
      bool synchronous_init,
      bool fail_initial_download);
  virtual ~SyncBackendHostForProfileSyncTest();

  MOCK_METHOD1(RequestNudge, void(const tracked_objects::Location&));

  // Called when a nudge comes in.
  void SimulateSyncCycleCompletedInitialSyncEnded(
      const tracked_objects::Location&);

  virtual sync_api::HttpPostProviderFactory* MakeHttpBridgeFactory(
      const scoped_refptr<net::URLRequestContextGetter>& getter) OVERRIDE;

  virtual void StartConfiguration(const base::Closure& callback) OVERRIDE;

  static void SetDefaultExpectationsForWorkerCreation(ProfileMock* profile);

  static void SetHistoryServiceExpectations(ProfileMock* profile);

 protected:
  virtual void InitCore(const Core::DoInitializeOptions& options) OVERRIDE;

 private:
  bool synchronous_init_;
  bool fail_initial_download_;
};

}  // namespace browser_sync

class TestProfileSyncService : public ProfileSyncService {
 public:
  // |callback| can be used to populate nodes before the OnBackendInitialized
  // callback fires.
  TestProfileSyncService(ProfileSyncComponentsFactory* factory,
                         Profile* profile,
                         const std::string& test_user,
                         bool synchronous_backend_initialization,
                         const base::Closure& callback);

  virtual ~TestProfileSyncService();

  void SetInitialSyncEndedForAllTypes();

  virtual void OnBackendInitialized(
      const browser_sync::WeakHandle<browser_sync::JsBackend>& backend,
      bool success) OVERRIDE;

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // If this is called, configuring data types will require a syncer
  // nudge.
  void dont_set_initial_sync_ended_on_init();
  void set_synchronous_sync_configuration();

  void fail_initial_download();

  browser_sync::TestIdFactory* id_factory();

  // Override of ProfileSyncService::GetBackendForTest() with a more
  // specific return type (since C++ supports covariant return types)
  // that is made public.
  virtual browser_sync::SyncBackendHostForProfileSyncTest*
      GetBackendForTest() OVERRIDE;

 protected:
  virtual void CreateBackend() OVERRIDE;

 private:
  browser_sync::TestIdFactory id_factory_;

  bool synchronous_backend_initialization_;

  // Set to true when a mock data type manager is being used and the configure
  // step is performed synchronously.
  bool synchronous_sync_configuration_;
  bool set_expect_resume_expectations_;

  base::Closure callback_;
  bool set_initial_sync_ended_on_init_;

  bool fail_initial_download_;
};



#endif  // CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
