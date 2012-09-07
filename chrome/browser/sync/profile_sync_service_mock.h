// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_MOCK_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_MOCK_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/test/base/testing_profile.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync_protocol_error.h"
#include "testing/gmock/include/gmock/gmock.h"

class ProfileSyncServiceMock : public ProfileSyncService {
 public:
  // no-arg constructor provided so TestingProfile can use NiceMock.
  ProfileSyncServiceMock();
  explicit ProfileSyncServiceMock(Profile* profile);
  virtual ~ProfileSyncServiceMock();

  // A utility used by sync tests to create a TestingProfile with a Google
  // Services username stored in a (Testing)PrefService.
  static TestingProfile* MakeSignedInTestingProfile();

  // Helper routine to be used in conjunction with
  // ProfileKeyedServiceFactory::SetTestingFactory().
  static ProfileKeyedService* BuildMockProfileSyncService(Profile* profile);

  MOCK_METHOD0(DisableForUser, void());
  MOCK_METHOD2(OnBackendInitialized,
               void(const syncer::WeakHandle<syncer::JsBackend>&,
                    bool));
  MOCK_METHOD0(OnSyncCycleCompleted, void());
  MOCK_METHOD0(OnAuthError, void());
  MOCK_METHOD4(OnUserSubmittedAuth,
               void(const std::string& username,
                    const std::string& password,
                    const std::string& captcha,
                    const std::string& access_code));
  MOCK_METHOD0(OnUserCancelledDialog, void());
  MOCK_CONST_METHOD0(GetAuthenticatedUsername, string16());
  MOCK_METHOD2(OnUserChoseDatatypes,
               void(bool sync_everything,
                    syncer::ModelTypeSet chosen_types));

  MOCK_METHOD2(OnUnrecoverableError,
               void(const tracked_objects::Location& location,
               const std::string& message));
  MOCK_METHOD3(DisableBrokenDatatype, void(syncer::ModelType,
               const tracked_objects::Location&,
               std::string message));
  MOCK_CONST_METHOD0(GetUserShare, syncer::UserShare*());
  MOCK_METHOD3(ActivateDataType,
               void(syncer::ModelType, syncer::ModelSafeGroup,
                    browser_sync::ChangeProcessor*));
  MOCK_METHOD1(DeactivateDataType, void(syncer::ModelType));

  MOCK_METHOD0(InitializeBackend, void());
  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD0(GetJsController, base::WeakPtr<syncer::JsController>());
  MOCK_CONST_METHOD0(HasSyncSetupCompleted, bool());

  MOCK_CONST_METHOD0(EncryptEverythingEnabled, bool());
  MOCK_METHOD0(EnableEncryptEverything, void());

  MOCK_METHOD1(ChangePreferredDataTypes,
               void(syncer::ModelTypeSet preferred_types));
  MOCK_CONST_METHOD0(GetPreferredDataTypes, syncer::ModelTypeSet());
  MOCK_CONST_METHOD0(GetRegisteredDataTypes, syncer::ModelTypeSet());
  MOCK_CONST_METHOD0(GetLastSessionSnapshot,
                     syncer::sessions::SyncSessionSnapshot());

  MOCK_METHOD1(QueryDetailedSyncStatus,
               bool(browser_sync::SyncBackendHost::Status* result));
  MOCK_CONST_METHOD0(GetAuthError, const GoogleServiceAuthError&());
  MOCK_CONST_METHOD0(FirstSetupInProgress, bool());
  MOCK_CONST_METHOD0(GetLastSyncedTimeString, string16());
  MOCK_CONST_METHOD0(HasUnrecoverableError, bool());
  MOCK_CONST_METHOD0(sync_initialized, bool());
  MOCK_CONST_METHOD0(waiting_for_auth, bool());
  MOCK_METHOD1(OnActionableError, void(
      const syncer::SyncProtocolError&));

  // DataTypeManagerObserver mocks.
  MOCK_METHOD0(OnConfigureBlocked, void());
  MOCK_METHOD1(OnConfigureDone,
               void(const browser_sync::DataTypeManager::ConfigureResult&));
  MOCK_METHOD0(OnConfigureRetry, void());
  MOCK_METHOD0(OnConfigureStart, void());

  MOCK_METHOD0(IsSyncEnabledAndLoggedIn, bool());
  MOCK_METHOD0(IsSyncTokenAvailable, bool());

  MOCK_CONST_METHOD0(IsPassphraseRequired, bool());
  MOCK_CONST_METHOD0(IsPassphraseRequiredForDecryption, bool());
  MOCK_CONST_METHOD0(IsUsingSecondaryPassphrase, bool());

  MOCK_METHOD1(SetDecryptionPassphrase, bool(const std::string& passphrase));
  MOCK_METHOD2(SetEncryptionPassphrase, void(const std::string& passphrase,
                                             PassphraseType type));
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_MOCK_H_
