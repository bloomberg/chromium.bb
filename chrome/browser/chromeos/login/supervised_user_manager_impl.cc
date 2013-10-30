// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/supervised_user_manager_impl.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user_manager_impl.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"

using content::BrowserThread;

namespace {

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

} // namespace

namespace chromeos {

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
}

SupervisedUserManagerImpl::SupervisedUserManagerImpl(UserManagerImpl* owner)
    : owner_(owner),
      cros_settings_(CrosSettings::Get()) {
  // SupervisedUserManager instance should be used only on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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

const User* SupervisedUserManagerImpl::CreateUserRecord(
      const std::string& manager_id,
      const std::string& local_user_id,
      const std::string& sync_user_id,
      const string16& display_name) {
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
  PrefService* local_state = g_browser_process->local_state();
  const DictionaryValue* sync_ids =
      local_state->GetDictionary(kManagedUserSyncId);
  std::string result;
  sync_ids->GetStringWithoutPathExpansion(user_id, &result);
  return result;
}

string16 SupervisedUserManagerImpl::GetManagerDisplayName(
    const std::string& user_id) const {
  PrefService* local_state = g_browser_process->local_state();
  const DictionaryValue* manager_names =
      local_state->GetDictionary(kManagedUserManagerNames);
  string16 result;
  if (manager_names->GetStringWithoutPathExpansion(user_id, &result) &&
      !result.empty())
    return result;
  return UTF8ToUTF16(GetManagerDisplayEmail(user_id));
}

std::string SupervisedUserManagerImpl::GetManagerUserId(
      const std::string& user_id) const {
  PrefService* local_state = g_browser_process->local_state();
  const DictionaryValue* manager_ids =
      local_state->GetDictionary(kManagedUserManagers);
  std::string result;
  manager_ids->GetStringWithoutPathExpansion(user_id, &result);
  return result;
}

std::string SupervisedUserManagerImpl::GetManagerDisplayEmail(
      const std::string& user_id) const {
  PrefService* local_state = g_browser_process->local_state();
  const DictionaryValue* manager_mails =
      local_state->GetDictionary(kManagedUserManagerDisplayEmails);
  std::string result;
  if (manager_mails->GetStringWithoutPathExpansion(user_id, &result) &&
      !result.empty()) {
    return result;
  }
  return GetManagerUserId(user_id);
}

const User* SupervisedUserManagerImpl::FindByDisplayName(
    const string16& display_name) const {
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
      const string16& display_name) {
  g_browser_process->local_state()->
      SetString(kLocallyManagedUserCreationTransactionDisplayName,
           UTF16ToASCII(display_name));
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
    return;
  }

  if (gaia::ExtractDomainName(user_id) !=
          UserManager::kLocallyManagedUserDomain) {
    LOG(WARNING) << "Clean up transaction for  non-locally managed user found :"
                 << user_id << ", will not remove data";
    prefs->ClearPref(kLocallyManagedUserCreationTransactionDisplayName);
    prefs->ClearPref(kLocallyManagedUserCreationTransactionUserId);
    return;
  }

  owner_->RemoveUser(user_id, NULL);

  prefs->ClearPref(kLocallyManagedUserCreationTransactionDisplayName);
  prefs->ClearPref(kLocallyManagedUserCreationTransactionUserId);
  prefs->CommitPendingWrite();
}

void SupervisedUserManagerImpl::RemoveNonCryptohomeData(
    const std::string& user_id) {
  PrefService* prefs = g_browser_process->local_state();
  ListPrefUpdate prefs_new_users_update(prefs, kLocallyManagedUsersFirstRun);
  prefs_new_users_update->Remove(base::StringValue(user_id), NULL);

  DictionaryPrefUpdate managers_update(prefs, kManagedUserManagers);
  managers_update->RemoveWithoutPathExpansion(user_id, NULL);

  DictionaryPrefUpdate manager_names_update(prefs,
                                            kManagedUserManagerNames);
  manager_names_update->RemoveWithoutPathExpansion(user_id, NULL);

  DictionaryPrefUpdate manager_emails_update(prefs,
                                             kManagedUserManagerDisplayEmails);
  manager_emails_update->RemoveWithoutPathExpansion(user_id, NULL);
}

bool SupervisedUserManagerImpl::CheckForFirstRun(const std::string& user_id) {
  ListPrefUpdate prefs_new_users_update(g_browser_process->local_state(),
                                        kLocallyManagedUsersFirstRun);
  return prefs_new_users_update->Remove(base::StringValue(user_id), NULL);
}

void SupervisedUserManagerImpl::UpdateManagerName(const std::string& manager_id,
    const string16& new_display_name) {
  PrefService* local_state = g_browser_process->local_state();

  const DictionaryValue* manager_ids =
      local_state->GetDictionary(kManagedUserManagers);

  DictionaryPrefUpdate manager_name_update(local_state,
                                           kManagedUserManagerNames);
  for (DictionaryValue::Iterator it(*manager_ids); !it.IsAtEnd();
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


}  // namespace chromeos
