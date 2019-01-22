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
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/account_mapper_util.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/auth/arc_auth_service.h"
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

// These names are used in histograms.
constexpr char kDeviceAccountMigration[] = "DeviceAccountMigration";
constexpr char kContentAreaAccountsMigration[] = "ContentAreaAccountsMigration";
constexpr char kArcAccountsMigration[] = "ArcAccountsMigration";
constexpr char kMigrationResultMetricName[] =
    "AccountManager.Migrations.Result";

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

// A helper base class for account migration steps that need to read and write
// to Account Manager.
class AccountMigrationBaseStep : public AccountMigrationRunner::Step {
 public:
  AccountMigrationBaseStep(const std::string& id,
                           AccountManager* account_manager,
                           AccountTrackerService* account_tracker_service)
      : AccountMigrationRunner::Step(id),
        account_manager_(account_manager),
        account_tracker_service_(account_tracker_service),
        weak_factory_(this) {}
  ~AccountMigrationBaseStep() override = default;

 protected:
  bool IsAccountPresentInAccountManager(
      const AccountManager::AccountKey& account) const {
    return base::ContainsValue(account_manager_accounts_, account);
  }

  bool IsAccountManagerEmpty() const {
    return account_manager_accounts_.empty();
  }

  void MigrateSecondaryAccount(const std::string& gaia_id,
                               const std::string& email) {
    if (base::ContainsValue(
            account_manager_accounts_,
            AccountManager::AccountKey{
                gaia_id, account_manager::AccountType::ACCOUNT_TYPE_GAIA})) {
      // Do not overwrite any existing account in |AccountManager|.
      VLOG(1) << "Ignoring migration of existing account: " << email;
      return;
    }

    // |AccountTrackerService::SeedAccountInfo| must be called before
    // |AccountManager::UpsertToken|. |AccountManager| observers will need to
    // translate |AccountManager::AccountKey| to other formats using
    // |AccountTrackerService| and hence |AccountTrackerService| should be
    // updated first.
    account_tracker_service_->SeedAccountInfo(gaia_id, email);
    account_manager_->UpsertToken(
        AccountManager::AccountKey{
            gaia_id, account_manager::AccountType::ACCOUNT_TYPE_GAIA},
        AccountManager::kInvalidToken);
    VLOG(1) << "Successfully migrated: " << email;
  }

  AccountManager* account_manager() { return account_manager_; }

 private:
  // Implementations should use this to start their migration flow, instead of
  // overriding |Run|.
  virtual void StartMigration() = 0;

  // Overrides |AccountMigrationRunner::Step| and stops further overrides.
  // Subclasses should use |StartMigration| to begin their migration flow and
  // must call either of |Step::FinishWithSuccess| or |Step::FinishWithFailure|
  // when done.
  void Run() final {
    account_manager_->GetAccounts(base::BindOnce(
        &AccountMigrationBaseStep::OnGetAccounts, weak_factory_.GetWeakPtr()));
  }

  void OnGetAccounts(std::vector<AccountManager::AccountKey> accounts) {
    account_manager_accounts_ = std::move(accounts);
    StartMigration();
  }

  // A non-owning pointer to Account Manager.
  AccountManager* const account_manager_;

  // Non-owning pointer.
  AccountTrackerService* const account_tracker_service_;

  // A temporary cache of accounts in |AccountManager|, guaranteed to be
  // up-to-date when |StartMigration| is called.
  std::vector<AccountManager::AccountKey> account_manager_accounts_;

  base::WeakPtrFactory<AccountMigrationBaseStep> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(AccountMigrationBaseStep);
};

// An |AccountMigrationRunner::Step| to migrate the Chrome OS Device Account's
// LST to |AccountManager|.
class DeviceAccountMigration : public AccountMigrationBaseStep,
                               public WebDataServiceConsumer {
 public:
  DeviceAccountMigration(AccountManager::AccountKey device_account,
                         AccountManager* account_manager,
                         AccountTrackerService* account_tracker_service,
                         scoped_refptr<TokenWebData> token_web_data)
      : AccountMigrationBaseStep(kDeviceAccountMigration,
                                 account_manager,
                                 account_tracker_service),
        account_mapper_util_(account_tracker_service),
        token_web_data_(token_web_data),
        device_account_(device_account) {}
  ~DeviceAccountMigration() override = default;

 private:
  void StartMigration() override {
    if (IsAccountPresentInAccountManager(device_account_)) {
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

      account_manager()->UpsertToken(device_account_, it->second /* token */);
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

  // For translating between OAuth account ids and
  // |AccountManager::AccountKey|.
  AccountMapperUtil account_mapper_util_;

  // Current storage of LSTs.
  scoped_refptr<TokenWebData> token_web_data_;

  // Device Account on Chrome OS.
  const AccountManager::AccountKey device_account_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(DeviceAccountMigration);
};

// An |AccountMigrationRunner::Step| to migrate the Chrome content area accounts
// to |AccountManager|. The objective is to migrate the account names only. We
// cannot migrate any credentials (cookies).
class ContentAreaAccountsMigration : public AccountMigrationBaseStep,
                                     GaiaCookieManagerService::Observer {
 public:
  ContentAreaAccountsMigration(
      AccountManager* account_manager,
      AccountTrackerService* const account_tracker_service,
      GaiaCookieManagerService* gaia_cookie_manager_service)
      : AccountMigrationBaseStep(kContentAreaAccountsMigration,
                                 account_manager,
                                 account_tracker_service),
        gaia_cookie_manager_service_(gaia_cookie_manager_service) {}
  ~ContentAreaAccountsMigration() override {
    gaia_cookie_manager_service_->RemoveObserver(this);
  }

 private:
  void StartMigration() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
    DCHECK(!IsAccountManagerEmpty());
    gaia_cookie_manager_service_->RemoveObserver(this);

    MigrateAccounts(signed_in_content_area_accounts,
                    signed_out_content_area_accounts);

    FinishWithSuccess();
  }

  void MigrateAccounts(
      const std::vector<gaia::ListedAccount>& signed_in_content_area_accounts,
      const std::vector<gaia::ListedAccount>&
          signed_out_content_area_accounts) {
    for (const gaia::ListedAccount& account : signed_in_content_area_accounts) {
      MigrateSecondaryAccount(account.gaia_id, account.raw_email);
    }
    for (const gaia::ListedAccount& account :
         signed_out_content_area_accounts) {
      MigrateSecondaryAccount(account.gaia_id, account.raw_email);
    }
  }

  // A non-owning pointer to |GaiaCookieManagerService|.
  GaiaCookieManagerService* const gaia_cookie_manager_service_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ContentAreaAccountsMigration);
};

// An |AccountMigrationRunner::Step| to migrate ARC accounts to
// |AccountManager|. The objective is to migrate the account names and Gaia ids
// only. We cannot migrate any credentials.
// This is a timed |Step|. Since ARC can fail independently of Chrome, we can be
// potentially waiting forever to get a callback from ARC. If we do not have a
// timeout, this |Step| can make the rest of migration |Step|s wait forever.
class ArcAccountsMigration : public AccountMigrationBaseStep,
                             public arc::ArcSessionManager::Observer {
 public:
  ArcAccountsMigration(AccountManager* account_manager,
                       AccountTrackerService* account_tracker_service,
                       arc::ArcAuthService* arc_auth_service)
      : AccountMigrationBaseStep(kArcAccountsMigration,
                                 account_manager,
                                 account_tracker_service),
        arc_auth_service_(arc_auth_service),
        weak_factory_(this) {}
  ~ArcAccountsMigration() override { Reset(); }

 private:
  void StartMigration() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(kStepTimeoutInSeconds),
                 base::BindOnce(&ArcAccountsMigration::FinishWithFailure,
                                weak_factory_.GetWeakPtr()));
    arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
    arc_session_manager->AddObserver(this);
    if (arc_session_manager->state() == arc::ArcSessionManager::State::ACTIVE) {
      OnArcStarted();
    }
  }

  void OnArcStarted() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    arc::ArcSessionManager::Get()->RemoveObserver(this);

    arc_auth_service_->GetGoogleAccountsInArc(
        base::BindOnce(&ArcAccountsMigration::OnGetGoogleAccountsInArc,
                       weak_factory_.GetWeakPtr()));
  }

  void OnGetGoogleAccountsInArc(
      std::vector<arc::mojom::ArcAccountInfoPtr> arc_accounts) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    timer_.Stop();

    for (const arc::mojom::ArcAccountInfoPtr& arc_account : arc_accounts) {
      MigrateSecondaryAccount(arc_account->gaia_id, arc_account->email);
    }

    FinishWithSuccess();
  }

  void FinishWithSuccess() {
    Reset();
    Step::FinishWithSuccess();
  }

  void FinishWithFailure() {
    Reset();
    Step::FinishWithFailure();
  }

  void Reset() {
    timer_.Stop();
    arc::ArcSessionManager::Get()->RemoveObserver(this);
    weak_factory_.InvalidateWeakPtrs();
  }

  // A non-owning pointer to |ArcAuthService|.
  arc::ArcAuthService* const arc_auth_service_;

  base::OneShotTimer timer_;

  // Timeout duration for this |Step|.
  const int64_t kStepTimeoutInSeconds = 60;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ArcAccountsMigration> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcAccountsMigration);
};

}  // namespace

AccountManagerMigrator::AccountManagerMigrator(Profile* profile)
    : profile_(profile), weak_factory_(this) {}

AccountManagerMigrator::~AccountManagerMigrator() = default;

void AccountManagerMigrator::Start() {
  DVLOG(1) << "AccountManagerMigrator::Start";

  if (!chromeos::switches::IsAccountManagerEnabled())
    return;

  ran_migration_steps_ = false;
  if (ShouldRunMigrations()) {
    ran_migration_steps_ = true;
    AddMigrationSteps();
  }

  // Cleanup tasks (like re-enabling Chrome account reconciliation) rely on the
  // migration being run, even if they were no-op. Check
  // |OnMigrationRunComplete| and |RunCleanupTasks|.
  migration_runner_.Run(
      base::BindOnce(&AccountManagerMigrator::OnMigrationRunComplete,
                     weak_factory_.GetWeakPtr()));
}

bool AccountManagerMigrator::ShouldRunMigrations() const {
  // Account migration does not make sense for ephemeral (Guest, Managed
  // Session, Kiosk, Demo etc.) sessions.
  if (user_manager::UserManager::Get()->IsCurrentUserCryptohomeDataEphemeral())
    return false;

  // TODO(https://crbug.com/923947): Check success state from Preferences.

  return true;
}

void AccountManagerMigrator::AddMigrationSteps() {
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

  if (arc::IsArcProvisioned(profile_)) {
    // Add a migration step for ARC only if ARC has been provisioned. If ARC has
    // not been provisioned yet, there cannot be any accounts that need to be
    // migrated.
    migration_runner_.AddStep(std::make_unique<ArcAccountsMigration>(
        account_manager, account_tracker_service,
        arc::ArcAuthService::GetForBrowserContext(
            profile_) /* arc_auth_service */));
  } else {
    VLOG(1) << "Skipping migration of ARC accounts. ARC has not been "
               "provisioned yet";
  }

  // TODO(https://crbug.com/923947): Store success state in Preferences.
  // TODO(sinhak): Verify Device Account LST state.
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

  VLOG(1) << "Account migrations completed with result: "
          << static_cast<int>(result.final_status);
  if (result.final_status == AccountMigrationRunner::Status::kFailure)
    VLOG(1) << "Failed step: " << result.failed_step_id;

  if (ran_migration_steps_) {
    // Update the UMA stats only for migrations that actually ran some steps.
    base::UmaHistogramBoolean(
        kMigrationResultMetricName,
        result.final_status == AccountMigrationRunner::Status::kSuccess);
  }

  RunCleanupTasks();
}

void AccountManagerMigrator::RunCleanupTasks() {
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
