// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_MOCK_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_MOCK_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_driver/change_processor.h"
#include "components/sync_driver/data_type_controller.h"
#include "components/sync_driver/device_info.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync_protocol_error.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Invoke;

class ProfileSyncServiceMock : public ProfileSyncService {
 public:
  explicit ProfileSyncServiceMock(Profile* profile);
  ProfileSyncServiceMock(
      scoped_ptr<ProfileSyncComponentsFactory> factory, Profile* profile);
  virtual ~ProfileSyncServiceMock();

  // A utility used by sync tests to create a TestingProfile with a Google
  // Services username stored in a (Testing)PrefService.
  static TestingProfile* MakeSignedInTestingProfile();

  // Helper routine to be used in conjunction with
  // BrowserContextKeyedServiceFactory::SetTestingFactory().
  static KeyedService* BuildMockProfileSyncService(
      content::BrowserContext* profile);

  MOCK_METHOD0(DisableForUser, void());
  MOCK_METHOD4(OnBackendInitialized,
      void(const syncer::WeakHandle<syncer::JsBackend>&,
           const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&,
           const std::string&,
           bool));
  MOCK_METHOD0(OnSyncCycleCompleted, void());
  MOCK_METHOD0(OnAuthError, void());
  MOCK_METHOD4(OnUserSubmittedAuth,
               void(const std::string& username,
                    const std::string& password,
                    const std::string& captcha,
                    const std::string& access_code));
  MOCK_METHOD0(OnUserCancelledDialog, void());
  MOCK_CONST_METHOD0(GetAuthenticatedUsername, base::string16());
  MOCK_METHOD2(OnUserChoseDatatypes,
               void(bool sync_everything,
                    syncer::ModelTypeSet chosen_types));

  MOCK_METHOD2(OnUnrecoverableError,
               void(const tracked_objects::Location& location,
               const std::string& message));
  MOCK_METHOD1(DisableDatatype, void(const syncer::SyncError&));
  MOCK_CONST_METHOD0(GetUserShare, syncer::UserShare*());
  MOCK_METHOD1(DeactivateDataType, void(syncer::ModelType));
  MOCK_METHOD0(UnsuppressAndStart, void());

  MOCK_METHOD1(AddObserver, void(ProfileSyncServiceBase::Observer*));
  MOCK_METHOD1(RemoveObserver, void(ProfileSyncServiceBase::Observer*));
  MOCK_METHOD0(GetJsController, base::WeakPtr<syncer::JsController>());
  MOCK_CONST_METHOD0(HasSyncSetupCompleted, bool());

  MOCK_CONST_METHOD0(EncryptEverythingEnabled, bool());
  MOCK_METHOD0(EnableEncryptEverything, void());

  MOCK_METHOD1(ChangePreferredDataTypes,
               void(syncer::ModelTypeSet preferred_types));
  MOCK_CONST_METHOD0(GetActiveDataTypes, syncer::ModelTypeSet());
  MOCK_CONST_METHOD0(GetPreferredDataTypes, syncer::ModelTypeSet());
  MOCK_CONST_METHOD0(GetRegisteredDataTypes, syncer::ModelTypeSet());
  MOCK_CONST_METHOD0(GetLastSessionSnapshot,
                     syncer::sessions::SyncSessionSnapshot());

  MOCK_METHOD1(QueryDetailedSyncStatus,
               bool(browser_sync::SyncBackendHost::Status* result));
  MOCK_CONST_METHOD0(GetAuthError, const GoogleServiceAuthError&());
  MOCK_CONST_METHOD0(FirstSetupInProgress, bool());
  MOCK_CONST_METHOD0(GetLastSyncedTimeString, base::string16());
  MOCK_CONST_METHOD0(HasUnrecoverableError, bool());
  MOCK_CONST_METHOD0(sync_initialized, bool());
  MOCK_CONST_METHOD0(IsStartSuppressed, bool());
  MOCK_CONST_METHOD0(waiting_for_auth, bool());
  MOCK_METHOD1(OnActionableError, void(
      const syncer::SyncProtocolError&));
  MOCK_METHOD1(SetSetupInProgress, void(bool));
  MOCK_CONST_METHOD1(IsDataTypeControllerRunning, bool(syncer::ModelType));

  // DataTypeManagerObserver mocks.
  MOCK_METHOD0(OnConfigureBlocked, void());
  MOCK_METHOD1(OnConfigureDone,
               void(const sync_driver::DataTypeManager::ConfigureResult&));
  MOCK_METHOD0(OnConfigureRetry, void());
  MOCK_METHOD0(OnConfigureStart, void());

  MOCK_METHOD0(IsSyncEnabledAndLoggedIn, bool());
  MOCK_CONST_METHOD0(IsManaged, bool());
  MOCK_METHOD0(IsOAuthRefreshTokenAvailable, bool());

  MOCK_CONST_METHOD0(IsPassphraseRequired, bool());
  MOCK_CONST_METHOD0(IsPassphraseRequiredForDecryption, bool());
  MOCK_CONST_METHOD0(IsUsingSecondaryPassphrase, bool());
  MOCK_CONST_METHOD0(GetPassphraseType, syncer::PassphraseType());
  MOCK_CONST_METHOD0(GetPassphraseTime, base::Time());
  MOCK_CONST_METHOD0(GetExplicitPassphraseTime, base::Time());

  MOCK_METHOD1(SetDecryptionPassphrase, bool(const std::string& passphrase));
  MOCK_METHOD2(SetEncryptionPassphrase, void(const std::string& passphrase,
                                             PassphraseType type));

  MOCK_METHOD1(StartUpSlowBackendComponents, void(BackendMode));
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_MOCK_H_
