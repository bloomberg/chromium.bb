// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_MOCK_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DRIVER_MOCK_SYNC_SERVICE_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "components/signin/core/browser/account_info.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/driver/sync_user_settings_mock.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

// Mock implementation of SyncService. You probably don't need this; look at
// TestSyncService instead!
// As one special case compared to a regular mock, the GetUserSettings() methods
// are not mocked. Instead, they return a mock version of SyncUserSettings.
class MockSyncService : public SyncService {
 public:
  MockSyncService();
  ~MockSyncService() override;

  syncer::SyncUserSettingsMock* GetMockUserSettings();

  // SyncService implementation.
  syncer::SyncUserSettings* GetUserSettings() override;
  const syncer::SyncUserSettings* GetUserSettings() const override;
  MOCK_CONST_METHOD0(GetDisableReasons, int());
  MOCK_CONST_METHOD0(GetTransportState, TransportState());
  MOCK_CONST_METHOD0(IsLocalSyncEnabled, bool());
  MOCK_CONST_METHOD0(GetAuthenticatedAccountInfo, CoreAccountInfo());
  MOCK_CONST_METHOD0(IsAuthenticatedAccountPrimary, bool());
  MOCK_CONST_METHOD0(GetAuthError, GoogleServiceAuthError());

  MOCK_METHOD0(GetSetupInProgressHandle,
               std::unique_ptr<SyncSetupInProgressHandle>());
  MOCK_CONST_METHOD0(IsSetupInProgress, bool());

  MOCK_CONST_METHOD0(GetRegisteredDataTypes, ModelTypeSet());
  MOCK_CONST_METHOD0(GetForcedDataTypes, ModelTypeSet());
  MOCK_CONST_METHOD0(GetPreferredDataTypes, ModelTypeSet());
  MOCK_CONST_METHOD0(GetActiveDataTypes, ModelTypeSet());

  MOCK_METHOD0(StopAndClear, void());
  MOCK_METHOD1(OnDataTypeRequestsSyncStartup, void(ModelType type));
  MOCK_METHOD1(TriggerRefresh, void(const ModelTypeSet& types));
  MOCK_METHOD1(ReenableDatatype, void(ModelType type));
  MOCK_METHOD1(ReadyForStartChanged, void(syncer::ModelType type));
  MOCK_METHOD1(SetInvalidationsForSessionsEnabled, void(bool enabled));

  MOCK_METHOD1(AddObserver, void(SyncServiceObserver* observer));
  MOCK_METHOD1(RemoveObserver, void(SyncServiceObserver* observer));
  MOCK_CONST_METHOD1(HasObserver, bool(const SyncServiceObserver* observer));

  MOCK_METHOD1(AddPreferenceProvider,
               void(SyncTypePreferenceProvider* provider));
  MOCK_METHOD1(RemovePreferenceProvider,
               void(SyncTypePreferenceProvider* provider));
  MOCK_CONST_METHOD1(HasPreferenceProvider,
                     bool(SyncTypePreferenceProvider* provider));

  MOCK_CONST_METHOD0(GetUserShare, UserShare*());

  MOCK_CONST_METHOD0(GetSyncTokenStatus, SyncTokenStatus());
  MOCK_CONST_METHOD1(QueryDetailedSyncStatus, bool(SyncStatus* result));
  MOCK_CONST_METHOD0(GetLastSyncedTime, base::Time());
  MOCK_CONST_METHOD0(GetLastCycleSnapshot, SyncCycleSnapshot());
  MOCK_METHOD0(GetTypeStatusMap, std::unique_ptr<base::Value>());
  MOCK_CONST_METHOD0(sync_service_url, const GURL&());
  MOCK_CONST_METHOD0(unrecoverable_error_message, std::string());
  MOCK_CONST_METHOD0(unrecoverable_error_location, base::Location());
  MOCK_METHOD1(AddProtocolEventObserver, void(ProtocolEventObserver* observer));
  MOCK_METHOD1(RemoveProtocolEventObserver,
               void(ProtocolEventObserver* observer));
  MOCK_METHOD1(AddTypeDebugInfoObserver, void(TypeDebugInfoObserver* observer));
  MOCK_METHOD1(RemoveTypeDebugInfoObserver,
               void(TypeDebugInfoObserver* observer));
  MOCK_METHOD0(GetJsController, base::WeakPtr<JsController>());
  MOCK_METHOD1(GetAllNodes,
               void(const base::Callback<
                    void(std::unique_ptr<base::ListValue>)>& callback));

  // KeyedService implementation.
  MOCK_METHOD0(Shutdown, void());

 private:
  testing::NiceMock<syncer::SyncUserSettingsMock> user_settings_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_MOCK_SYNC_SERVICE_H_
