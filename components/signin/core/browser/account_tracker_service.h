// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_TRACKER_SERVICE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_TRACKER_SERVICE_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/account_info.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/gfx/image/image.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class DictionaryValue;
}

// AccountTrackerService is a KeyedService that retrieves and caches GAIA
// information about Google Accounts.
class AccountTrackerService : public KeyedService {
 public:
  // Name of the preference property that persists the account information
  // tracked by this service.
  static const char kAccountInfoPref[];

  // Value representing no hosted domain in the kGoogleServicesHostedDomain
  // preference.
  static const char kNoHostedDomainFound[];

  // Value representing no picture URL associated with an account.
  static const char kNoPictureURLFound[];

  // TODO(knn): Move to ChildAccountInfoFetcher once deprecated service flags
  // have been migrated from preferences.
  // Child account service flag name.
  static const char kChildAccountServiceFlag[];

  // Clients of AccountTrackerService can implement this interface and register
  // with AddObserver() to learn about account information changes.
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnAccountUpdated(const AccountInfo& info) {}
    virtual void OnAccountImageUpdated(const std::string& account_id,
                                       const gfx::Image& image) {}
    virtual void OnAccountUpdateFailed(const std::string& account_id) {}
    virtual void OnAccountRemoved(const AccountInfo& info) {}
  };

  // Possible values for the kAccountIdMigrationState preference.
  // Keep in sync with OAuth2LoginAccountRevokedMigrationState histogram enum.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum AccountIdMigrationState {
    MIGRATION_NOT_STARTED = 0,
    MIGRATION_IN_PROGRESS = 1,
    MIGRATION_DONE = 2,
    NUM_MIGRATION_STATES
  };

  AccountTrackerService();
  ~AccountTrackerService() override;

  // Registers the preferences used by AccountTrackerService.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // KeyedService implementation.
  void Shutdown() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Initializes the list of accounts from |pref_service| and load images from
  // |user_data_dir|. If |user_data_dir| is empty, images will not be saved to
  // nor loaded from disk.
  void Initialize(PrefService* pref_service, base::FilePath user_data_dir);

  // Returns the list of known accounts and for which gaia IDs
  // have been fetched.
  std::vector<AccountInfo> GetAccounts() const;
  AccountInfo GetAccountInfo(const std::string& account_id) const;
  AccountInfo FindAccountInfoByGaiaId(const std::string& gaia_id) const;
  AccountInfo FindAccountInfoByEmail(const std::string& email) const;

  // Returns the account image associated to the account id |account_id|.
  // If the account id is not known an empty image is returned.
  gfx::Image GetAccountImage(const std::string& account_id);

  // Picks the correct account_id for the specified account depending on the
  // migration state.
  std::string PickAccountIdForAccount(const std::string& gaia,
                                      const std::string& email) const;
  static std::string PickAccountIdForAccount(const PrefService* pref_service,
                                             const std::string& gaia,
                                             const std::string& email);

  // Seeds the account whose account_id is given by PickAccountIdForAccount()
  // with its corresponding gaia id and email address.  Returns the same
  // value PickAccountIdForAccount() when given the same arguments.
  std::string SeedAccountInfo(const std::string& gaia,
                              const std::string& email);

  // Seeds the account represented by |info|. If the account is already tracked
  // and compatible, the empty fields will be updated with values from |info|.
  // If after the update IsValid() is true, OnAccountUpdated will be fired.
  std::string SeedAccountInfo(AccountInfo info);

  // Sets whether the account is a Unicorn account.
  void SetIsChildAccount(const std::string& account_id, bool is_child_account);

  // Sets whether the account is under advanced protection.
  void SetIsAdvancedProtectionAccount(const std::string& account_id,
                                      bool is_under_advanced_protection);

  void RemoveAccount(const std::string& account_id);

  // Is migration of the account id from normalized email to gaia id supported
  // on the current platform?
  static bool IsMigrationSupported();

  AccountIdMigrationState GetMigrationState() const;
  void SetMigrationDone();

 protected:
  // Available to be called in tests.
  void SetAccountStateFromUserInfo(const std::string& account_id,
                                   const base::DictionaryValue* user_info);

  // Updates the account image. Does nothing if |account_id| does not exist in
  // |accounts_|.
  void SetAccountImage(const std::string& account_id, const gfx::Image& image);

 private:
  friend class AccountFetcherService;
  friend class FakeAccountFetcherService;
  struct AccountState {
    AccountInfo info;
    gfx::Image image;
  };

  void NotifyAccountUpdated(const AccountState& state);
  void NotifyAccountImageUpdated(const std::string& account_id,
                                 const gfx::Image& image);
  void NotifyAccountUpdateFailed(const std::string& account_id);
  void NotifyAccountRemoved(const AccountState& state);

  void StartTrackingAccount(const std::string& account_id);
  void StopTrackingAccount(const std::string& account_id);

  // Load the current state of the account info from the preferences file.
  void LoadFromPrefs();
  void SaveToPrefs(const AccountState& account);
  void RemoveFromPrefs(const AccountState& account);

  // Used to load/save account images from/to disc.
  base::FilePath GetImagePathFor(const std::string& account_id);
  void OnAccountImageLoaded(const std::string& account_id, gfx::Image image);
  void LoadAccountImagesFromDisk();
  void SaveAccountImageToDisk(const std::string& account_id,
                              const gfx::Image& image);
  void RemoveAccountImageFromDisk(const std::string& account_id);

  // Migrate accounts to be keyed by gaia id instead of normalized email.
  // Requires that the migration state is set to MIGRATION_IN_PROGRESS.
  void MigrateToGaiaId();

  // Returns whether the accounts are all keyed by gaia id. This should
  // be the case when the migration state is set to MIGRATION_DONE.
  bool IsMigrationDone() const;

  // Computes the new migration state. The state is saved to preference
  // before performing the migration in order to support resuming the
  // migration if necessary during the next load.
  AccountIdMigrationState ComputeNewMigrationState() const;

  // Updates the migration state in the preferences.
  void SetMigrationState(AccountIdMigrationState state);

  // Returns the saved migration state in the preferences.
  static AccountIdMigrationState GetMigrationState(
      const PrefService* pref_service);

  PrefService* pref_service_ = nullptr;  // Not owned.
  std::map<std::string, AccountState> accounts_;
  base::ObserverList<Observer>::Unchecked observer_list_;

  base::FilePath user_data_dir_;

  // Task runner used for file operations on avatar images.
  scoped_refptr<base::SequencedTaskRunner> image_storage_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to pass weak pointers of |this| to tasks created by
  // |image_storage_task_runner_|.
  base::WeakPtrFactory<AccountTrackerService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AccountTrackerService);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_TRACKER_SERVICE_H_
