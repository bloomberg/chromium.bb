// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/supervised_user_manager_impl.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/managed/locally_managed_user_constants.h"
#include "chrome/browser/chromeos/login/managed/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/user_manager_impl.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"

using content::BrowserThread;

namespace {

// Names for pref keys in Local State.
// A map from locally managed user local user id to sync user id.
const char kManagedUserSyncId[] =
    "ManagedUserSyncId";

// A map from locally managed user id to manager user id.
const char kManagedUserManagers[] =
    "ManagedUserManagers";

// A map from locally managed user id to manager display name.
const char kManagedUserManagerNames[] =
    "ManagedUserManagerNames";

// A map from locally managed user id to manager display e-mail.
const char kManagedUserManagerDisplayEmails[] =
    "ManagedUserManagerDisplayEmails";

// A vector pref of the locally managed accounts defined on this device, that
// had not logged in yet.
const char kLocallyManagedUsersFirstRun[] = "LocallyManagedUsersFirstRun";

// A pref of the next id for locally managed users generation.
const char kLocallyManagedUsersNextId[] =
    "LocallyManagedUsersNextId";

// A pref of the next id for locally managed users generation.
const char kLocallyManagedUserCreationTransactionDisplayName[] =
    "LocallyManagedUserCreationTransactionDisplayName";

// A pref of the next id for locally managed users generation.
const char kLocallyManagedUserCreationTransactionUserId[] =
    "LocallyManagedUserCreationTransactionUserId";

// A map from user id to password schema id.
const char kSupervisedUserPasswordSchema[] =
    "SupervisedUserPasswordSchema";

// A map from user id to password salt.
const char kSupervisedUserPasswordSalt[] =
    "SupervisedUserPasswordSalt";

// A map from user id to password revision.
const char kSupervisedUserPasswordRevision[] =
    "SupervisedUserPasswordRevision";

// A map from user id to flag indicating if password should be updated upon
// signin.
const char kSupervisedUserNeedPasswordUpdate[] =
    "SupervisedUserNeedPasswordUpdate";

// A map from user id to flag indicating if cryptohome does not have signature
// key.
const char kSupervisedUserIncompleteKey[] = "SupervisedUserHasIncompleteKey";

std::string LoadSyncToken(base::FilePath profile_dir) {
  std::string token;
  base::FilePath token_file =
      profile_dir.Append(chromeos::kManagedUserTokenFilename);
  VLOG(1) << "Loading" << token_file.value();
  if (!base::ReadFileToString(token_file, &token))
    return std::string();
  return token;
}

} // namespace

namespace chromeos {

const char kSchemaVersion[] = "SchemaVersion";
const char kPasswordRevision[] = "PasswordRevision";
const char kSalt[] = "PasswordSalt";
const char kPasswordSignature[] = "PasswordSignature";
const char kEncryptedPassword[] = "EncryptedPassword";
const char kRequirePasswordUpdate[] = "RequirePasswordUpdate";
const char kHasIncompleteKey[] = "HasIncompleteKey";
const char kPasswordEncryptionKey[] = "password.hmac.encryption";
const char kPasswordSignatureKey[] = "password.hmac.signature";

const char kPasswordUpdateFile[] = "password.update";
const int kMinPasswordRevision = 1;

// static
void SupervisedUserManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kLocallyManagedUsersFirstRun);
  registry->RegisterIntegerPref(kLocallyManagedUsersNextId, 0);
  registry->RegisterStringPref(
      kLocallyManagedUserCreationTransactionDisplayName, "");
  registry->RegisterStringPref(
      kLocallyManagedUserCreationTransactionUserId, "");
  registry->RegisterDictionaryPref(kManagedUserSyncId);
  registry->RegisterDictionaryPref(kManagedUserManagers);
  registry->RegisterDictionaryPref(kManagedUserManagerNames);
  registry->RegisterDictionaryPref(kManagedUserManagerDisplayEmails);

  registry->RegisterDictionaryPref(kSupervisedUserPasswordSchema);
  registry->RegisterDictionaryPref(kSupervisedUserPasswordSalt);
  registry->RegisterDictionaryPref(kSupervisedUserPasswordRevision);

  registry->RegisterDictionaryPref(kSupervisedUserNeedPasswordUpdate);
  registry->RegisterDictionaryPref(kSupervisedUserIncompleteKey);
}

SupervisedUserManagerImpl::SupervisedUserManagerImpl(UserManagerImpl* owner)
    : owner_(owner),
      cros_settings_(CrosSettings::Get()) {
  // SupervisedUserManager instance should be used only on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  authentication_.reset(new SupervisedUserAuthentication(this));
}

SupervisedUserManagerImpl::~SupervisedUserManagerImpl() {
}

std::string SupervisedUserManagerImpl::GenerateUserId() {
  int counter = g_browser_process->local_state()->
      GetInteger(kLocallyManagedUsersNextId);
  std::string id;
  bool user_exists;
  do {
    id = base::StringPrintf("%d@%s", counter,
        UserManager::kLocallyManagedUserDomain);
    counter++;
    user_exists = (NULL != owner_->FindUser(id));
    DCHECK(!user_exists);
    if (user_exists) {
      LOG(ERROR) << "Supervised user with id " << id << " already exists.";
    }
  } while (user_exists);

  g_browser_process->local_state()->
      SetInteger(kLocallyManagedUsersNextId, counter);

  g_browser_process->local_state()->CommitPendingWrite();
  return id;
}

bool SupervisedUserManagerImpl::HasSupervisedUsers(
      const std::string& manager_id) const {
  const UserList& users = owner_->GetUsers();
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if ((*it)->GetType() == User::USER_TYPE_LOCALLY_MANAGED) {
      if (manager_id == GetManagerUserId((*it)->email()))
        return true;
    }
  }
  return false;
}

const User* SupervisedUserManagerImpl::CreateUserRecord(
      const std::string& manager_id,
      const std::string& local_user_id,
      const std::string& sync_user_id,
      const base::string16& display_name) {
  const User* user = FindByDisplayName(display_name);
  DCHECK(!user);
  if (user)
    return user;
  const User* manager = owner_->FindUser(manager_id);
  CHECK(manager);

  PrefService* local_state = g_browser_process->local_state();

  User* new_user = User::CreateLocallyManagedUser(local_user_id);

  owner_->AddUserRecord(new_user);

  ListPrefUpdate prefs_new_users_update(local_state,
                                        kLocallyManagedUsersFirstRun);
  DictionaryPrefUpdate sync_id_update(local_state, kManagedUserSyncId);
  DictionaryPrefUpdate manager_update(local_state, kManagedUserManagers);
  DictionaryPrefUpdate manager_name_update(local_state,
                                           kManagedUserManagerNames);
  DictionaryPrefUpdate manager_email_update(local_state,
                                            kManagedUserManagerDisplayEmails);

  prefs_new_users_update->Insert(0, new base::StringValue(local_user_id));

  sync_id_update->SetWithoutPathExpansion(local_user_id,
      new base::StringValue(sync_user_id));
  manager_update->SetWithoutPathExpansion(local_user_id,
      new base::StringValue(manager->email()));
  manager_name_update->SetWithoutPathExpansion(local_user_id,
      new base::StringValue(manager->GetDisplayName()));
  manager_email_update->SetWithoutPathExpansion(local_user_id,
      new base::StringValue(manager->display_email()));

  owner_->SaveUserDisplayName(local_user_id, display_name);

  g_browser_process->local_state()->CommitPendingWrite();
  return new_user;
}

std::string SupervisedUserManagerImpl::GetUserSyncId(const std::string& user_id)
    const {
  std::string result;
  GetUserStringValue(user_id, kManagedUserSyncId, &result);
  return result;
}

base::string16 SupervisedUserManagerImpl::GetManagerDisplayName(
    const std::string& user_id) const {
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* manager_names =
      local_state->GetDictionary(kManagedUserManagerNames);
  base::string16 result;
  if (manager_names->GetStringWithoutPathExpansion(user_id, &result) &&
      !result.empty())
    return result;
  return base::UTF8ToUTF16(GetManagerDisplayEmail(user_id));
}

std::string SupervisedUserManagerImpl::GetManagerUserId(
      const std::string& user_id) const {
  std::string result;
  GetUserStringValue(user_id, kManagedUserManagers, &result);
  return result;
}

std::string SupervisedUserManagerImpl::GetManagerDisplayEmail(
      const std::string& user_id) const {
  std::string result;
  if (GetUserStringValue(user_id, kManagedUserManagerDisplayEmails, &result) &&
      !result.empty())
    return result;
  return GetManagerUserId(user_id);
}

void SupervisedUserManagerImpl::GetPasswordInformation(
    const std::string& user_id,
    base::DictionaryValue* result) {
  int value;
  if (GetUserIntegerValue(user_id, kSupervisedUserPasswordSchema, &value))
    result->SetIntegerWithoutPathExpansion(kSchemaVersion, value);
  if (GetUserIntegerValue(user_id, kSupervisedUserPasswordRevision, &value))
    result->SetIntegerWithoutPathExpansion(kPasswordRevision, value);

  bool flag;
  if (GetUserBooleanValue(user_id, kSupervisedUserNeedPasswordUpdate, &flag))
    result->SetBooleanWithoutPathExpansion(kRequirePasswordUpdate, flag);
  if (GetUserBooleanValue(user_id, kSupervisedUserIncompleteKey, &flag))
    result->SetBooleanWithoutPathExpansion(kHasIncompleteKey, flag);

  std::string salt;
  if (GetUserStringValue(user_id, kSupervisedUserPasswordSalt, &salt))
    result->SetStringWithoutPathExpansion(kSalt, salt);
}

void SupervisedUserManagerImpl::SetPasswordInformation(
      const std::string& user_id,
      const base::DictionaryValue* password_info) {
  int value;
  if (password_info->GetIntegerWithoutPathExpansion(kSchemaVersion, &value))
    SetUserIntegerValue(user_id, kSupervisedUserPasswordSchema, value);
  if (password_info->GetIntegerWithoutPathExpansion(kPasswordRevision, &value))
    SetUserIntegerValue(user_id, kSupervisedUserPasswordRevision, value);

  bool flag;
  if (password_info->GetBooleanWithoutPathExpansion(kRequirePasswordUpdate,
                                                    &flag)) {
    SetUserBooleanValue(user_id, kSupervisedUserNeedPasswordUpdate, flag);
  }
  if (password_info->GetBooleanWithoutPathExpansion(kHasIncompleteKey, &flag))
    SetUserBooleanValue(user_id, kSupervisedUserIncompleteKey, flag);

  std::string salt;
  if (password_info->GetStringWithoutPathExpansion(kSalt, &salt))
    SetUserStringValue(user_id, kSupervisedUserPasswordSalt, salt);
  g_browser_process->local_state()->CommitPendingWrite();
}

bool SupervisedUserManagerImpl::GetUserStringValue(
    const std::string& user_id,
    const char* key,
    std::string* out_value) const {
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* dictionary = local_state->GetDictionary(key);
  return dictionary->GetStringWithoutPathExpansion(user_id, out_value);
}

bool SupervisedUserManagerImpl::GetUserIntegerValue(
    const std::string& user_id,
    const char* key,
    int* out_value) const {
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* dictionary = local_state->GetDictionary(key);
  return dictionary->GetIntegerWithoutPathExpansion(user_id, out_value);
}

bool SupervisedUserManagerImpl::GetUserBooleanValue(const std::string& user_id,
                                                    const char* key,
                                                    bool* out_value) const {
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* dictionary = local_state->GetDictionary(key);
  return dictionary->GetBooleanWithoutPathExpansion(user_id, out_value);
}

void SupervisedUserManagerImpl::SetUserStringValue(
    const std::string& user_id,
    const char* key,
    const std::string& value) {
  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate update(local_state, key);
  update->SetStringWithoutPathExpansion(user_id, value);
}

void SupervisedUserManagerImpl::SetUserIntegerValue(
    const std::string& user_id,
    const char* key,
    const int value) {
  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate update(local_state, key);
  update->SetIntegerWithoutPathExpansion(user_id, value);
}

void SupervisedUserManagerImpl::SetUserBooleanValue(const std::string& user_id,
                                                    const char* key,
                                                    const bool value) {
  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate update(local_state, key);
  update->SetBooleanWithoutPathExpansion(user_id, value);
}

const User* SupervisedUserManagerImpl::FindByDisplayName(
    const base::string16& display_name) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const UserList& users = owner_->GetUsers();
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if (((*it)->GetType() == User::USER_TYPE_LOCALLY_MANAGED) &&
        ((*it)->display_name() == display_name)) {
      return *it;
    }
  }
  return NULL;
}

const User* SupervisedUserManagerImpl::FindBySyncId(
    const std::string& sync_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const UserList& users = owner_->GetUsers();
  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    if (((*it)->GetType() == User::USER_TYPE_LOCALLY_MANAGED) &&
        (GetUserSyncId((*it)->email()) == sync_id)) {
      return *it;
    }
  }
  return NULL;
}

void SupervisedUserManagerImpl::StartCreationTransaction(
      const base::string16& display_name) {
  g_browser_process->local_state()->
      SetString(kLocallyManagedUserCreationTransactionDisplayName,
                base::UTF16ToASCII(display_name));
  g_browser_process->local_state()->CommitPendingWrite();
}

void SupervisedUserManagerImpl::SetCreationTransactionUserId(
      const std::string& email) {
  g_browser_process->local_state()->
      SetString(kLocallyManagedUserCreationTransactionUserId,
                email);
  g_browser_process->local_state()->CommitPendingWrite();
}

void SupervisedUserManagerImpl::CommitCreationTransaction() {
  g_browser_process->local_state()->
      ClearPref(kLocallyManagedUserCreationTransactionDisplayName);
  g_browser_process->local_state()->
      ClearPref(kLocallyManagedUserCreationTransactionUserId);
  g_browser_process->local_state()->CommitPendingWrite();
}

bool SupervisedUserManagerImpl::HasFailedUserCreationTransaction() {
  return !(g_browser_process->local_state()->
               GetString(kLocallyManagedUserCreationTransactionDisplayName).
                   empty());
}

void SupervisedUserManagerImpl::RollbackUserCreationTransaction() {
  PrefService* prefs = g_browser_process->local_state();

  std::string display_name = prefs->
      GetString(kLocallyManagedUserCreationTransactionDisplayName);
  std::string user_id = prefs->
      GetString(kLocallyManagedUserCreationTransactionUserId);

  LOG(WARNING) << "Cleaning up transaction for "
               << display_name << "/" << user_id;

  if (user_id.empty()) {
    // Not much to do - just remove transaction.
    prefs->ClearPref(kLocallyManagedUserCreationTransactionDisplayName);
    prefs->CommitPendingWrite();
    return;
  }

  if (gaia::ExtractDomainName(user_id) !=
          UserManager::kLocallyManagedUserDomain) {
    LOG(WARNING) << "Clean up transaction for  non-locally managed user found :"
                 << user_id << ", will not remove data";
    prefs->ClearPref(kLocallyManagedUserCreationTransactionDisplayName);
    prefs->ClearPref(kLocallyManagedUserCreationTransactionUserId);
    prefs->CommitPendingWrite();
    return;
  }
  owner_->RemoveNonOwnerUserInternal(user_id, NULL);

  prefs->ClearPref(kLocallyManagedUserCreationTransactionDisplayName);
  prefs->ClearPref(kLocallyManagedUserCreationTransactionUserId);
  prefs->CommitPendingWrite();
}

void SupervisedUserManagerImpl::RemoveNonCryptohomeData(
    const std::string& user_id) {
  PrefService* prefs = g_browser_process->local_state();
  ListPrefUpdate prefs_new_users_update(prefs, kLocallyManagedUsersFirstRun);
  prefs_new_users_update->Remove(base::StringValue(user_id), NULL);

  CleanPref(user_id, kManagedUserSyncId);
  CleanPref(user_id, kManagedUserManagers);
  CleanPref(user_id, kManagedUserManagerNames);
  CleanPref(user_id, kManagedUserManagerDisplayEmails);
  CleanPref(user_id, kSupervisedUserPasswordSalt);
  CleanPref(user_id, kSupervisedUserPasswordSchema);
  CleanPref(user_id, kSupervisedUserPasswordRevision);
  CleanPref(user_id, kSupervisedUserNeedPasswordUpdate);
  CleanPref(user_id, kSupervisedUserIncompleteKey);
}

void SupervisedUserManagerImpl::CleanPref(const std::string& user_id,
                                          const char* key) {
  PrefService* prefs = g_browser_process->local_state();
  DictionaryPrefUpdate dict_update(prefs, key);
  dict_update->RemoveWithoutPathExpansion(user_id, NULL);
}

bool SupervisedUserManagerImpl::CheckForFirstRun(const std::string& user_id) {
  ListPrefUpdate prefs_new_users_update(g_browser_process->local_state(),
                                        kLocallyManagedUsersFirstRun);
  return prefs_new_users_update->Remove(base::StringValue(user_id), NULL);
}

void SupervisedUserManagerImpl::UpdateManagerName(const std::string& manager_id,
    const base::string16& new_display_name) {
  PrefService* local_state = g_browser_process->local_state();

  const base::DictionaryValue* manager_ids =
      local_state->GetDictionary(kManagedUserManagers);

  DictionaryPrefUpdate manager_name_update(local_state,
                                           kManagedUserManagerNames);
  for (base::DictionaryValue::Iterator it(*manager_ids); !it.IsAtEnd();
      it.Advance()) {
    std::string user_id;
    bool has_manager_id = it.value().GetAsString(&user_id);
    DCHECK(has_manager_id);
    if (user_id == manager_id) {
      manager_name_update->SetWithoutPathExpansion(
          it.key(),
          new base::StringValue(new_display_name));
    }
  }
}

SupervisedUserAuthentication* SupervisedUserManagerImpl::GetAuthentication() {
  return authentication_.get();
}

void SupervisedUserManagerImpl::LoadSupervisedUserToken(
    Profile* profile,
    const LoadTokenCallback& callback) {
  // TODO(antrim): use profile->GetPath() once we sure it is safe.
  base::FilePath profile_dir = ProfileHelper::GetProfilePathByUserIdHash(
      UserManager::Get()->GetUserByProfile(profile)->username_hash());
  PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&LoadSyncToken, profile_dir),
      callback);
}

void SupervisedUserManagerImpl::ConfigureSyncWithToken(
    Profile* profile,
    const std::string& token) {
  if (!token.empty())
    ManagedUserServiceFactory::GetForProfile(profile)->InitSync(token);
}

}  // namespace chromeos
