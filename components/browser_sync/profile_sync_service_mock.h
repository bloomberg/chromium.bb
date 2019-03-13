// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_SERVICE_MOCK_H_
#define COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_SERVICE_MOCK_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_user_settings_mock.h"
#include "components/sync/protocol/sync_protocol_error.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {

class ProfileSyncServiceMock : public ProfileSyncService {
 public:
  explicit ProfileSyncServiceMock(InitParams init_params);
  ~ProfileSyncServiceMock() override;

  syncer::SyncUserSettingsMock* GetUserSettingsMock();

  // SyncService overrides.
  syncer::SyncUserSettings* GetUserSettings() override;
  const syncer::SyncUserSettings* GetUserSettings() const override;
  MOCK_CONST_METHOD0(GetDisableReasons, int());
  MOCK_CONST_METHOD0(GetTransportState, TransportState());
  // TODO(crbug.com/871221): Remove this override. This is overridden here to
  // return true by default, as a workaround for tests not setting up an
  // authenticated account and IsSyncFeatureEnabled() therefore returning false.
  bool IsAuthenticatedAccountPrimary() const override;
  MOCK_CONST_METHOD0(GetAuthError, GoogleServiceAuthError());

  MOCK_METHOD0(GetSetupInProgressHandle,
               std::unique_ptr<syncer::SyncSetupInProgressHandle>());
  MOCK_CONST_METHOD0(IsSetupInProgress, bool());

  MOCK_CONST_METHOD0(GetRegisteredDataTypes, syncer::ModelTypeSet());
  syncer::ModelTypeSet GetPreferredDataTypes() const override;
  MOCK_CONST_METHOD0(GetActiveDataTypes, syncer::ModelTypeSet());

  MOCK_METHOD0(StopAndClear, void());

  MOCK_METHOD1(AddObserver, void(syncer::SyncServiceObserver*));
  MOCK_METHOD1(RemoveObserver, void(syncer::SyncServiceObserver*));

  MOCK_CONST_METHOD0(GetUserShare, syncer::UserShare*());

  MOCK_CONST_METHOD1(QueryDetailedSyncStatus, bool(syncer::SyncStatus* result));
  MOCK_CONST_METHOD0(GetLastSyncedTime, base::Time());
  MOCK_CONST_METHOD0(GetLastCycleSnapshot, syncer::SyncCycleSnapshot());

  MOCK_METHOD0(GetJsController, base::WeakPtr<syncer::JsController>());

  // SyncEngineHost overrides.
  MOCK_METHOD8(
      OnEngineInitialized,
      void(syncer::ModelTypeSet initial_types,
           const syncer::WeakHandle<syncer::JsBackend>&,
           const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&,
           const std::string&,
           const std::string&,
           const std::string&,
           const std::string&,
           bool));
  MOCK_METHOD1(OnSyncCycleCompleted, void(const syncer::SyncCycleSnapshot&));
  MOCK_METHOD1(OnActionableError, void(const syncer::SyncProtocolError&));

  // DataTypeManagerObserver overrides.
  MOCK_METHOD1(OnConfigureDone,
               void(const syncer::DataTypeManager::ConfigureResult&));
  MOCK_METHOD0(OnConfigureStart, void());

  // syncer::UnrecoverableErrorHandler overrides.
  MOCK_METHOD2(OnUnrecoverableError,
               void(const base::Location& location,
                    const std::string& message));

  // ProfileSyncService overrides.
  MOCK_CONST_METHOD1(IsDataTypeControllerRunning, bool(syncer::ModelType));

  MOCK_METHOD0(StartUpSlowEngineComponents, void());

  MOCK_METHOD0(OnSetupInProgressHandleDestroyed, void());

  // Gives access to the real implementation of ProfileSyncService methods:
  std::unique_ptr<syncer::SyncSetupInProgressHandle>
  GetSetupInProgressHandleConcrete();

 private:
  testing::NiceMock<syncer::SyncUserSettingsMock> user_settings_;
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_SERVICE_MOCK_H_
