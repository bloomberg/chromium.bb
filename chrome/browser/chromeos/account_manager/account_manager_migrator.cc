// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/account_manager/account_manager_migrator.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/account_mapper_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chromeos/account_manager/account_manager.h"
#include "chromeos/account_manager/account_manager_factory.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/webdata/token_web_data.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace chromeos {

namespace {

AccountManager::AccountKey GetDeviceAccount(const Profile* profile) {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  const AccountId& account_id = user->GetAccountId();
  switch (account_id.GetAccountType()) {
    case AccountType::ACTIVE_DIRECTORY:
      return AccountManager::AccountKey{
          account_id.GetObjGuid(),
          account_manager::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY};
    case AccountType::GOOGLE:
      return AccountManager::AccountKey{
          account_id.GetGaiaId(),
          account_manager::AccountType::ACCOUNT_TYPE_GAIA};
    case AccountType::UNKNOWN:
      NOTREACHED();
      return AccountManager::AccountKey{
          std::string(),
          account_manager::AccountType::ACCOUNT_TYPE_UNSPECIFIED};
  }
}

std::string RemoveAccountIdPrefix(const std::string& prefixed_account_id) {
  return prefixed_account_id.substr(10 /* length of "AccountId-" */);
}

// An |AccountMigrationRunner::Step| to migrate the Chrome OS Device Account's
// LST to |AccountManager|.
class DeviceAccountMigration : public AccountMigrationRunner::Step,
                               public WebDataServiceConsumer {
 public:
  DeviceAccountMigration(AccountManager::AccountKey device_account,
                         AccountManager* account_manager,
                         AccountTrackerService* account_tracker_service,
                         scoped_refptr<TokenWebData> token_web_data)
      : AccountMigrationRunner::Step("DeviceAccountMigration"),
        account_manager_(account_manager),
        account_mapper_util_(account_tracker_service),
        token_web_data_(token_web_data),
        device_account_(device_account),
        weak_factory_(this) {}
  ~DeviceAccountMigration() override = default;

  void Run(base::OnceCallback<void(bool)> callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    callback_ = std::move(callback);

    account_manager_->GetAccounts(base::BindOnce(
        &DeviceAccountMigration::OnGetAccounts, weak_factory_.GetWeakPtr()));
  }

 private:
  // Callback for |AccountManager::GetAccounts|. Checks the list of accounts in
  // Account Manager and early exists if the Device Account has already been
  // migrated.
  void OnGetAccounts(std::vector<AccountManager::AccountKey> accounts) {
    if (base::ContainsValue(accounts, device_account_)) {
      FinishWithSuccess();
      return;
    }

    switch (device_account_.account_type) {
      case account_manager::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY:
        MigrateActiveDirectoryAccount();
        break;
      case account_manager::AccountType::ACCOUNT_TYPE_GAIA:
        MigrateGaiaAccount();
        break;
      case account_manager::AccountType::ACCOUNT_TYPE_UNSPECIFIED:
        NOTREACHED();
        break;
    }
  }

  void MigrateActiveDirectoryAccount() {
    // TODO(sinhak): Migrate Active Directory account.
    FinishWithSuccess();
  }

  void MigrateGaiaAccount() { token_web_data_->GetAllTokens(this); }

  // WebDataServiceConsumer override.
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!result) {
      LOG(ERROR) << "Could not load the token database. Aborting.";
      FinishWithFailure();
      return;
    }

    DCHECK(result->GetType() == TOKEN_RESULT);
    const WDResult<TokenResult>* token_result =
        static_cast<const WDResult<TokenResult>*>(result.get());

    const std::map<std::string, std::string>& token_map =
        token_result->GetValue().tokens;

    bool is_success = false;
    for (auto it = token_map.begin(); it != token_map.end(); ++it) {
      std::string account_id = RemoveAccountIdPrefix(it->first);
      if (device_account_ !=
          account_mapper_util_.OAuthAccountIdToAccountKey(account_id)) {
        continue;
      }

      account_manager_->UpsertToken(device_account_, it->second /* token */);
      is_success = true;
      break;
    }

    if (is_success) {
      DVLOG(1) << "Device Account migrated.";
      FinishWithSuccess();
    } else {
      LOG(ERROR) << "Could not find a refresh token for the Device Account.";
      FinishWithFailure();
    }
  }

  void FinishWithSuccess() {
    DCHECK(callback_);
    std::move(callback_).Run(true);
  }

  void FinishWithFailure() {
    DCHECK(callback_);
    std::move(callback_).Run(false);
  }

  // A non-owning pointer to |AccountManager|.
  AccountManager* const account_manager_;

  // For translating between OAuth account ids and
  // |AccountManager::AccountKey|.
  AccountMapperUtil account_mapper_util_;

  // Current storage of LSTs.
  scoped_refptr<TokenWebData> token_web_data_;

  // Device Account on Chrome OS.
  const AccountManager::AccountKey device_account_;

  // Callback to invoke at the end of this |Step|, with the final result of the
  // operation.
  base::OnceCallback<void(bool)> callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DeviceAccountMigration> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceAccountMigration);
};

// An |AccountMigrationRunner::Step| to migrate the Chrome content area accounts
// to |AccountManager|. The objective is to migrate the account names only. We
// cannot migrate any credentials (cookies).
class ContentAreaAccountsMigration : public AccountMigrationRunner::Step,
                                     GaiaCookieManagerService::Observer {
 public:
  ContentAreaAccountsMigration(
      AccountManager* account_manager,
      AccountTrackerService* const account_tracker_service,
      GaiaCookieManagerService* gaia_cookie_manager_service)
      : AccountMigrationRunner::Step("ContentAreaAccountsMigration"),
        account_manager_(account_manager),
        account_tracker_service_(account_tracker_service),
        gaia_cookie_manager_service_(gaia_cookie_manager_service),
        weak_factory_(this) {}
  ~ContentAreaAccountsMigration() override = default;

  void Run(base::OnceCallback<void(bool)> callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    callback_ = std::move(callback);

    account_manager_->GetAccounts(
        base::BindOnce(&ContentAreaAccountsMigration::OnGetAccounts,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void OnGetAccounts(
      std::vector<AccountManager::AccountKey> account_manager_accounts) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    account_manager_accounts_ = std::move(account_manager_accounts);

    std::vector<gaia::ListedAccount> signed_in_content_area_accounts;
    std::vector<gaia::ListedAccount> signed_out_content_area_accounts;
    gaia_cookie_manager_service_->AddObserver(this);
    if (gaia_cookie_manager_service_->ListAccounts(
            &signed_in_content_area_accounts,
            &signed_out_content_area_accounts)) {
      OnGaiaAccountsInCookieUpdated(
          signed_in_content_area_accounts, signed_out_content_area_accounts,
          GoogleServiceAuthError(GoogleServiceAuthError::NONE));
    }
  }

  void OnGaiaAccountsInCookieUpdated(
      const std::vector<gaia::ListedAccount>& signed_in_content_area_accounts,
      const std::vector<gaia::ListedAccount>& signed_out_content_area_accounts,
      const GoogleServiceAuthError& error) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // We should not have reached here without |OnGetAccounts| having been
    // called and |account_manager_accounts_| empty.
    // Furthermore, Account Manager must have been populated with the Device
    // Account before this |Step| is run.
    DCHECK(!account_manager_accounts_.empty());
    gaia_cookie_manager_service_->RemoveObserver(this);

    MigrateAccounts(signed_in_content_area_accounts,
                    signed_out_content_area_accounts);

    std::move(callback_).Run(true);
  }

  void MigrateAccounts(
      const std::vector<gaia::ListedAccount>& signed_in_content_area_accounts,
      const std::vector<gaia::ListedAccount>&
          signed_out_content_area_accounts) {
    for (const gaia::ListedAccount& account : signed_in_content_area_accounts) {
      MigrateAccount(account);
    }
    for (const gaia::ListedAccount& account :
         signed_out_content_area_accounts) {
      MigrateAccount(account);
    }
  }

  void MigrateAccount(const gaia::ListedAccount& account) {
    AccountManager::AccountKey account_key{
        account.gaia_id, account_manager::AccountType::ACCOUNT_TYPE_GAIA};
    if (base::ContainsValue(account_manager_accounts_, account_key)) {
      // Do not overwrite any existing account in |AccountManager|.
      return;
    }

    // |AccountTrackerService::SeedAccountInfo| must be called before
    // |AccountManager::UpsertToken|. |AccountManager| observers will need to
    // translate |AccountManager::AccountKey| to other formats using
    // |AccountTrackerService| and hence |AccountTrackerService| should be
    // updated first.
    account_tracker_service_->SeedAccountInfo(account.gaia_id,
                                              account.raw_email);
    account_manager_->UpsertToken(account_key, AccountManager::kInvalidToken);
  }

  // A non-owning pointer to |AccountManager|.
  AccountManager* const account_manager_;

  // A non-owning pointer to |AccountTrackerService|.
  AccountTrackerService* const account_tracker_service_;

  // A non-owning pointer to |GaiaCookieManagerService|.
  GaiaCookieManagerService* const gaia_cookie_manager_service_;

  // A temporary cache of accounts in |AccountManager|.
  std::vector<AccountManager::AccountKey> account_manager_accounts_;

  // Callback to invoke at the end of this |Step|, with the final result of the
  // operation.
  base::OnceCallback<void(bool)> callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ContentAreaAccountsMigration> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentAreaAccountsMigration);
};

}  // namespace

AccountManagerMigrator::AccountManagerMigrator(Profile* profile)
    : profile_(profile), weak_factory_(this) {}

AccountManagerMigrator::~AccountManagerMigrator() = default;

void AccountManagerMigrator::Start() {
  DVLOG(1) << "AccountManagerMigrator::Start";

  if (!chromeos::switches::IsAccountManagerEnabled())
    return;

  // Account migration does not make sense for ephemeral (Guest, Managed
  // Session, Kiosk, Demo etc.) sessions.
  if (user_manager::UserManager::Get()->IsCurrentUserCryptohomeDataEphemeral())
    return;

  chromeos::AccountManagerFactory* factory =
      g_browser_process->platform_part()->GetAccountManagerFactory();
  chromeos::AccountManager* account_manager =
      factory->GetAccountManager(profile_->GetPath().value());

  AccountTrackerService* account_tracker_service =
      AccountTrackerServiceFactory::GetForProfile(profile_);

  migration_runner_.AddStep(std::make_unique<DeviceAccountMigration>(
      GetDeviceAccount(profile_), account_manager, account_tracker_service,
      WebDataServiceFactory::GetTokenWebDataForProfile(
          profile_, ServiceAccessType::EXPLICIT_ACCESS) /* token_web_data */));
  migration_runner_.AddStep(std::make_unique<ContentAreaAccountsMigration>(
      account_manager, account_tracker_service,
      GaiaCookieManagerServiceFactory::GetForProfile(
          profile_) /* gaia_cookie_manager_service */));

  // TODO(sinhak): Migrate Secondary Accounts in ARC.
  // TODO(sinhak): Store success state in Preferences.
  // TODO(sinhak): Verify Device Account LST state.

  migration_runner_.Run(
      base::BindOnce(&AccountManagerMigrator::OnMigrationRunComplete,
                     weak_factory_.GetWeakPtr()));
}

AccountMigrationRunner::Status AccountManagerMigrator::GetStatus() const {
  return migration_runner_.GetStatus();
}

void AccountManagerMigrator::OnMigrationRunComplete(
    const AccountMigrationRunner::MigrationResult& result) {
  DCHECK_NE(AccountMigrationRunner::Status::kNotStarted,
            migration_runner_.GetStatus());
  DCHECK_NE(AccountMigrationRunner::Status::kRunning,
            migration_runner_.GetStatus());
  // TODO(sinhak): Gather UMA stats.

  // Migration could have finished with a failure but we need to start account
  // reconciliation anyways. This may cause us to lose Chrome content area
  // Secondary Accounts but if we do not enable reconciliation, users will not
  // be able to add any account to Chrome content area which is a much worse
  // experience than losing Chrome content area Secondary Accounts.
  AccountReconcilorFactory::GetForProfile(profile_)->Initialize(
      true /* start_reconcile_if_tokens_available */);
}

// static
AccountManagerMigrator* AccountManagerMigratorFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AccountManagerMigrator*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AccountManagerMigratorFactory* AccountManagerMigratorFactory::GetInstance() {
  static base::NoDestructor<AccountManagerMigratorFactory> instance;
  return instance.get();
}

AccountManagerMigratorFactory::AccountManagerMigratorFactory()
    : BrowserContextKeyedServiceFactory(
          "AccountManagerMigrator",
          BrowserContextDependencyManager::GetInstance()) {
  // Stores the LSTs, that need to be copied over to |AccountManager|.
  DependsOn(WebDataServiceFactory::GetInstance());
  // For translating between account identifiers.
  DependsOn(AccountTrackerServiceFactory::GetInstance());
  // Account reconciliation is paused for the duration of migration and needs to
  // be re-enabled once migration is done.
  DependsOn(AccountReconcilorFactory::GetInstance());
  // For getting Chrome content area accounts.
  DependsOn(GaiaCookieManagerServiceFactory::GetInstance());
}

AccountManagerMigratorFactory::~AccountManagerMigratorFactory() = default;

KeyedService* AccountManagerMigratorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AccountManagerMigrator(Profile::FromBrowserContext(context));
}

}  // namespace chromeos
