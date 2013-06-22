// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REGISTRATION_SERVICE_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REGISTRATION_SERVICE_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/strings/string16.h"
#include "base/timer.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "sync/api/syncable_service.h"

class GoogleServiceAuthError;
class ManagedUserRefreshTokenFetcher;
class PrefService;

namespace browser_sync {
class DeviceInfo;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Structure to store registration information.
struct ManagedUserRegistrationInfo {
  explicit ManagedUserRegistrationInfo(const string16& name);
  string16 name;
  std::string master_key;
};

// Holds the state necessary for registering a new managed user with the
// management server and associating it with its custodian. It is owned by the
// custodian's profile.
class ManagedUserRegistrationService : public BrowserContextKeyedService,
                                       public syncer::SyncableService,
                                       public ProfileDownloaderDelegate {
 public:
  // Callback for Register() below. If registration is successful, |token| will
  // contain an OAuth2 refresh token for the newly registered managed user,
  // otherwise |token| will be empty and |error| will contain the authentication
  // error for the custodian.
  typedef base::Callback<void(const GoogleServiceAuthError& /* error */,
                              const std::string& /* token */)>
      RegistrationCallback;

  // Callback for DownloadProfile() below. If the GAIA profile download is
  // successful, the profile's full (display) name will be returned.
  typedef base::Callback<void(const string16& /* full name */)>
      DownloadProfileCallback;

  ManagedUserRegistrationService(
      PrefService* prefs,
      scoped_ptr<ManagedUserRefreshTokenFetcher> token_fetcher);
  virtual ~ManagedUserRegistrationService();

  // ProfileDownloaderDelegate:
  virtual bool NeedsProfilePicture() const OVERRIDE;
  virtual int GetDesiredImageSideLength() const OVERRIDE;
  virtual std::string GetCachedPictureURL() const OVERRIDE;
  virtual Profile* GetBrowserProfile() OVERRIDE;
  virtual void OnProfileDownloadSuccess(ProfileDownloader* downloader) OVERRIDE;
  virtual void OnProfileDownloadFailure(
      ProfileDownloader* downloader,
      ProfileDownloaderDelegate::FailureReason reason) OVERRIDE;

  static void RegisterUserPrefs(user_prefs::PrefRegistrySyncable* registry);

  // Registers a new managed user with the server. |info| contains necessary
  // information like the display name of the  the user. |callback| is called
  // with the result of the registration. We use the info here and not the
  // profile, because on Chrome OS the profile of the managed user does
  // not yet exist.
  void Register(const ManagedUserRegistrationInfo& info,
                const RegistrationCallback& callback);

  // Downloads the GAIA account information for the |profile|. This is a best-
  // effort attempt with no error reporting nor timeout. If the download is
  // successful, the profile's full (display) name will be returned via the
  // callback. If the download fails or never completes, the callback will
  // not be called.
  void DownloadProfile(Profile* profile,
                       const DownloadProfileCallback& callback);

  // Cancels any registration currently in progress, without calling the
  // callback or reporting an error. This should be called when the user
  // actively cancels the registration by canceling profile creation.
  void CancelPendingRegistration();

  // ProfileKeyedService implementation:
  virtual void Shutdown() OVERRIDE;

  // SyncableService implementation:
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const
      OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

 private:
  void OnLastSignedInUsernameChange();

  // Called when the Sync server has acknowledged a newly created managed user.
  void OnManagedUserAcknowledged(const std::string& managed_user_id);

  // Fetches the managed user token when we have the device info.
  void FetchToken(const string16& name,
                  const browser_sync::DeviceInfo& device_info);

  // Called when we have received a token for the managed user.
  void OnReceivedToken(const GoogleServiceAuthError& error,
                       const std::string& token);

  // Dispatches the callback and cleans up if all the conditions have been met.
  void CompleteRegistrationIfReady();

  // Aborts any registration currently in progress. If |run_callback| is true,
  // calls the callback specified in Register() with the given |error|.
  void AbortPendingRegistration(bool run_callback,
                                const GoogleServiceAuthError& error);

  // If |run_callback| is true, dispatches the callback with the saved token
  // (which may be empty) and the given |error|. In any case, resets internal
  // variables to be ready for the next registration.
  void CompleteRegistration(bool run_callback,
                            const GoogleServiceAuthError& error);

  void OnProfileDownloadComplete();

  base::WeakPtrFactory<ManagedUserRegistrationService> weak_ptr_factory_;
  PrefService* prefs_;
  PrefChangeRegistrar pref_change_registrar_;
  scoped_ptr<ManagedUserRefreshTokenFetcher> token_fetcher_;

  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;
  scoped_ptr<syncer::SyncErrorFactory> error_handler_;

  // Provides a timeout during profile creation.
  base::OneShotTimer<ManagedUserRegistrationService> registration_timer_;

  std::string pending_managed_user_id_;
  std::string pending_managed_user_token_;
  bool pending_managed_user_acknowledged_;
  RegistrationCallback callback_;

  Profile* download_profile_;
  scoped_ptr<ProfileDownloader> profile_downloader_;
  DownloadProfileCallback download_callback_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserRegistrationService);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REGISTRATION_SERVICE_H_
