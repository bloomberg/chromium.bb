// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/permission_request_creator_apiary.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/user_manager/user_manager.h"
#endif

const char kChildAccountDetectionFieldTrialName[] = "ChildAccountDetection";

const char kIsChildAccountServiceFlagName[] = "uca";

// Normally, re-check the child account flag and the family info once per day.
const int kUpdateIntervalSeconds = 60 * 60 * 24;

// In case of an error while getting the flag or the family info, retry with
// exponential backoff.
const net::BackoffEntry::Policy kBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential backoff in ms.
  2000,

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0.2, // 20%

  // Maximum amount of time we are willing to delay our request in ms.
  1000 * 60 * 60 * 4, // 4 hours.

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

ChildAccountService::ChildAccountService(Profile* profile)
    : profile_(profile), active_(false),
      flag_fetch_backoff_(&kBackoffPolicy),
      family_fetch_backoff_(&kBackoffPolicy),
      weak_ptr_factory_(this) {}

ChildAccountService::~ChildAccountService() {}

// static
bool ChildAccountService::IsChildAccountDetectionEnabled() {
  // Note: It's important to query the field trial state first, to ensure that
  // UMA reports the correct group.
  const std::string group_name =
      base::FieldTrialList::FindFullName(kChildAccountDetectionFieldTrialName);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableChildAccountDetection))
    return false;
  if (command_line->HasSwitch(switches::kEnableChildAccountDetection))
    return true;

  if (group_name == "Disabled")
    return false;
  return true;
}

void ChildAccountService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kChildAccountStatusKnown,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void ChildAccountService::SetIsChildAccount(bool is_child_account) {
  PropagateChildStatusToUser(is_child_account);
  if (profile_->IsChild() == is_child_account)
    return;

  if (is_child_account) {
    profile_->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                    supervised_users::kChildAccountSUID);
  } else {
    profile_->GetPrefs()->ClearPref(prefs::kSupervisedUserId);
  }
}

void ChildAccountService::Init() {
  SigninManagerFactory::GetForProfile(profile_)->AddObserver(this);
  SupervisedUserServiceFactory::GetForProfile(profile_)->SetDelegate(this);

  PropagateChildStatusToUser(profile_->IsChild());

  // If we're already signed in, fetch the flag again just to be sure.
  // (Previously, the browser might have been closed before we got the flag.
  // This also handles the graduation use case in a basic way.)
  if (SigninManagerFactory::GetForProfile(profile_)->IsAuthenticated())
    StartFetchingServiceFlags();
}

bool ChildAccountService::IsChildAccountStatusKnown() {
  return profile_->GetPrefs()->GetBoolean(prefs::kChildAccountStatusKnown);
}

void ChildAccountService::Shutdown() {
  family_fetcher_.reset();
  CancelFetchingServiceFlags();
  SupervisedUserServiceFactory::GetForProfile(profile_)->SetDelegate(NULL);
  DCHECK(!active_);
  SigninManagerFactory::GetForProfile(profile_)->RemoveObserver(this);
}


void ChildAccountService::AddChildStatusReceivedCallback(
    const base::Closure& callback) {
  if (IsChildAccountStatusKnown())
    callback.Run();
  else
    status_received_callback_list_.push_back(callback);
}

bool ChildAccountService::SetActive(bool active) {
  if (!profile_->IsChild() && !active_)
    return false;
  if (active_ == active)
    return true;
  active_ = active;

  if (active_) {
    // In contrast to local SUs, child account SUs must sign in.
    scoped_ptr<base::Value> allow_signin(new base::FundamentalValue(true));
    SupervisedUserSettingsService* settings_service =
        SupervisedUserSettingsServiceFactory::GetForProfile(profile_);
    settings_service->SetLocalSetting(supervised_users::kSigninAllowed,
                                      allow_signin.Pass());
#if !defined(OS_CHROMEOS)
    // This is also used by user policies (UserPolicySigninService), but since
    // child accounts can not also be Dasher accounts, there shouldn't be any
    // problems.
    SigninManagerFactory::GetForProfile(profile_)->ProhibitSignout(true);
#endif

    // TODO(treib): Maybe store the last update time in a pref, so we don't
    // have to re-fetch on every start.
    StartFetchingFamilyInfo();

    SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(profile_);
    service->AddPermissionRequestCreator(
        PermissionRequestCreatorApiary::CreateWithProfile(profile_));

    EnableExperimentalFiltering();
  } else {
    SupervisedUserSettingsService* settings_service =
        SupervisedUserSettingsServiceFactory::GetForProfile(profile_);
    settings_service->SetLocalSetting(supervised_users::kSigninAllowed,
                                      scoped_ptr<base::Value>());
#if !defined(OS_CHROMEOS)
    SigninManagerFactory::GetForProfile(profile_)->ProhibitSignout(false);
#endif

    ClearFirstCustodianPrefs();
    ClearSecondCustodianPrefs();
  }

  // Trigger a sync reconfig to enable/disable the right SU data types.
  // The logic to do this lives in the SupervisedUserSyncDataTypeController.
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (sync_service->HasSyncSetupCompleted())
    sync_service->ReconfigureDatatypeManager();

  return true;
}

base::FilePath ChildAccountService::GetBlacklistPath() const {
  if (!active_)
    return base::FilePath();
  base::FilePath blacklist_path;
  PathService::Get(chrome::DIR_USER_DATA, &blacklist_path);
  blacklist_path = blacklist_path.AppendASCII("su-blacklist.bin");
  return blacklist_path;
}

GURL ChildAccountService::GetBlacklistURL() const {
  if (!active_)
    return GURL();
  return GURL("https://www.gstatic.com/chrome/supervised_user/"
              "blacklist-20141001-1k.bin");
}

std::string ChildAccountService::GetSafeSitesCx() const {
  if (!active_)
    return std::string();
  return "017993620680222980993%3A1wdumejvx5i";
}

void ChildAccountService::GoogleSigninSucceeded(const std::string& account_id,
                                                const std::string& username,
                                                const std::string& password) {
  DCHECK(!account_id.empty());
  DCHECK_EQ(SigninManagerFactory::GetForProfile(profile_)
                ->GetAuthenticatedAccountId(),
            account_id);

  StartFetchingServiceFlags();
}

void ChildAccountService::GoogleSignedOut(const std::string& account_id,
                                          const std::string& username) {
  DCHECK(!profile_->IsChild());
  CancelFetchingServiceFlags();
  CancelFetchingFamilyInfo();
}

void ChildAccountService::OnGetFamilyMembersSuccess(
    const std::vector<FamilyInfoFetcher::FamilyMember>& members) {
  bool hoh_found = false;
  bool parent_found = false;
  for (const FamilyInfoFetcher::FamilyMember& member : members) {
    if (member.role == FamilyInfoFetcher::HEAD_OF_HOUSEHOLD) {
      hoh_found = true;
      SetFirstCustodianPrefs(member);
    } else if (member.role == FamilyInfoFetcher::PARENT) {
      parent_found = true;
      SetSecondCustodianPrefs(member);
    }
    if (hoh_found && parent_found)
      break;
  }
  if (!hoh_found) {
    DLOG(WARNING) << "GetFamilyMembers didn't return a HOH?!";
    ClearFirstCustodianPrefs();
  }
  if (!parent_found)
    ClearSecondCustodianPrefs();
  family_fetcher_.reset();

  family_fetch_backoff_.InformOfRequest(true);

  ScheduleNextFamilyInfoUpdate(
      base::TimeDelta::FromSeconds(kUpdateIntervalSeconds));
}

void ChildAccountService::OnFailure(FamilyInfoFetcher::ErrorCode error) {
  DLOG(WARNING) << "GetFamilyMembers failed with code " << error;
  family_fetch_backoff_.InformOfRequest(false);
  ScheduleNextFamilyInfoUpdate(family_fetch_backoff_.GetTimeUntilRelease());
}

void ChildAccountService::StartFetchingFamilyInfo() {
  family_fetcher_.reset(new FamilyInfoFetcher(
      this,
      SigninManagerFactory::GetForProfile(profile_)
          ->GetAuthenticatedAccountId(),
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_),
      profile_->GetRequestContext()));
  family_fetcher_->StartGetFamilyMembers();
}

void ChildAccountService::CancelFetchingFamilyInfo() {
  family_fetcher_.reset();
  family_fetch_timer_.Stop();
}

void ChildAccountService::ScheduleNextFamilyInfoUpdate(base::TimeDelta delay) {
  family_fetch_timer_.Start(
      FROM_HERE, delay, this, &ChildAccountService::StartFetchingFamilyInfo);
}

void ChildAccountService::StartFetchingServiceFlags() {
  if (!IsChildAccountDetectionEnabled()) {
    SetIsChildAccount(false);
    return;
  }
  account_id_ = SigninManagerFactory::GetForProfile(profile_)
      ->GetAuthenticatedAccountId();
  flag_fetcher_.reset(new AccountServiceFlagFetcher(
      account_id_,
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_),
      profile_->GetRequestContext(),
      base::Bind(&ChildAccountService::OnFlagsFetched,
                 weak_ptr_factory_.GetWeakPtr())));
}

void ChildAccountService::CancelFetchingServiceFlags() {
  flag_fetcher_.reset();
  account_id_.clear();
  flag_fetch_timer_.Stop();
}

void ChildAccountService::OnFlagsFetched(
    AccountServiceFlagFetcher::ResultCode result,
    const std::vector<std::string>& flags) {
  // If we've been signed out again (or signed in to a different account),
  // ignore the fetched flags.
  const std::string& new_account_id =
      SigninManagerFactory::GetForProfile(profile_)
          ->GetAuthenticatedAccountId();
  if (account_id_.empty() || account_id_ != new_account_id)
    return;

  account_id_.clear();

  // In case of an error, retry with exponential backoff.
  if (result != AccountServiceFlagFetcher::SUCCESS) {
    DLOG(WARNING) << "AccountServiceFlagFetcher returned error code " << result;
    flag_fetch_backoff_.InformOfRequest(false);
    ScheduleNextStatusFlagUpdate(flag_fetch_backoff_.GetTimeUntilRelease());
    return;
  }

  flag_fetch_backoff_.InformOfRequest(true);

  bool is_child_account =
      std::find(flags.begin(), flags.end(),
                kIsChildAccountServiceFlagName) != flags.end();

  bool status_was_known = profile_->GetPrefs()->GetBoolean(
      prefs::kChildAccountStatusKnown);
  profile_->GetPrefs()->SetBoolean(prefs::kChildAccountStatusKnown, true);

  if (!status_was_known) {
    for (auto& callback : status_received_callback_list_)
      callback.Run();
    status_received_callback_list_.clear();
  }

  SetIsChildAccount(is_child_account);

  ScheduleNextStatusFlagUpdate(
      base::TimeDelta::FromSeconds(kUpdateIntervalSeconds));
}

void ChildAccountService::ScheduleNextStatusFlagUpdate(base::TimeDelta delay) {
  flag_fetch_timer_.Start(
      FROM_HERE, delay, this, &ChildAccountService::StartFetchingServiceFlags);
}

void ChildAccountService::PropagateChildStatusToUser(bool is_child) {
#if defined(OS_CHROMEOS)
  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  if (user) {
    user_manager::UserManager::Get()->ChangeUserChildStatus(user, is_child);
  } else {
    LOG(WARNING) <<
        "User instance wasn't found while setting child account flag.";
  }
#endif
}

void ChildAccountService::SetFirstCustodianPrefs(
    const FamilyInfoFetcher::FamilyMember& custodian) {
  profile_->GetPrefs()->SetString(prefs::kSupervisedUserCustodianName,
                                  custodian.display_name);
  profile_->GetPrefs()->SetString(prefs::kSupervisedUserCustodianEmail,
                                  custodian.email);
  profile_->GetPrefs()->SetString(prefs::kSupervisedUserCustodianProfileURL,
                                  custodian.profile_url);
  profile_->GetPrefs()->SetString(
      prefs::kSupervisedUserCustodianProfileImageURL,
      custodian.profile_image_url);
}

void ChildAccountService::SetSecondCustodianPrefs(
    const FamilyInfoFetcher::FamilyMember& custodian) {
  profile_->GetPrefs()->SetString(prefs::kSupervisedUserSecondCustodianName,
                                  custodian.display_name);
  profile_->GetPrefs()->SetString(prefs::kSupervisedUserSecondCustodianEmail,
                                  custodian.email);
  profile_->GetPrefs()->SetString(
      prefs::kSupervisedUserSecondCustodianProfileURL,
      custodian.profile_url);
  profile_->GetPrefs()->SetString(
      prefs::kSupervisedUserSecondCustodianProfileImageURL,
      custodian.profile_image_url);
}

void ChildAccountService::ClearFirstCustodianPrefs() {
  profile_->GetPrefs()->ClearPref(prefs::kSupervisedUserCustodianName);
  profile_->GetPrefs()->ClearPref(prefs::kSupervisedUserCustodianEmail);
  profile_->GetPrefs()->ClearPref(prefs::kSupervisedUserCustodianProfileURL);
  profile_->GetPrefs()->ClearPref(
      prefs::kSupervisedUserCustodianProfileImageURL);
}

void ChildAccountService::ClearSecondCustodianPrefs() {
  profile_->GetPrefs()->ClearPref(prefs::kSupervisedUserSecondCustodianName);
  profile_->GetPrefs()->ClearPref(prefs::kSupervisedUserSecondCustodianEmail);
  profile_->GetPrefs()->ClearPref(
      prefs::kSupervisedUserSecondCustodianProfileURL);
  profile_->GetPrefs()->ClearPref(
      prefs::kSupervisedUserSecondCustodianProfileImageURL);
}

void ChildAccountService::EnableExperimentalFiltering() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Static blacklist defaults to enabled.
  bool has_enable_blacklist =
      command_line->HasSwitch(switches::kEnableSupervisedUserBlacklist);
  bool has_disable_blacklist =
      command_line->HasSwitch(switches::kDisableSupervisedUserBlacklist);
  if (!has_enable_blacklist && !has_disable_blacklist)
    command_line->AppendSwitch(switches::kEnableSupervisedUserBlacklist);

  // Query-based filtering also defaults to enabled.
  bool has_enable_safesites =
      command_line->HasSwitch(switches::kEnableSupervisedUserSafeSites);
  bool has_disable_safesites =
      command_line->HasSwitch(switches::kDisableSupervisedUserSafeSites);
  if (!has_enable_safesites && !has_disable_safesites)
    command_line->AppendSwitch(switches::kEnableSupervisedUserSafeSites);
}
