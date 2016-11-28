// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service.h"

#include <stddef.h>

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/sync_merge_result.h"
#include "components/sync/protocol/sync.pb.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/default_user_image/default_user_images.h"
#endif

using base::DictionaryValue;
using user_prefs::PrefRegistrySyncable;
using syncer::SUPERVISED_USERS;
using syncer::ModelType;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncChangeProcessor;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using syncer::SyncErrorFactory;
using syncer::SyncMergeResult;
using sync_pb::ManagedUserSpecifics;

namespace {

#if defined(OS_CHROMEOS)
const char kChromeOSAvatarPrefix[] = "chromeos-avatar-index:";
#else
const char kChromeAvatarPrefix[] = "chrome-avatar-index:";
#endif

SyncData CreateLocalSyncData(const std::string& id,
                             const std::string& name,
                             bool acknowledged,
                             const std::string& master_key,
                             const std::string& chrome_avatar,
                             const std::string& chromeos_avatar,
                             const std::string& password_signature_key,
                             const std::string& password_encryption_key) {
  ::sync_pb::EntitySpecifics specifics;
  specifics.mutable_managed_user()->set_id(id);
  specifics.mutable_managed_user()->set_name(name);
  if (!chrome_avatar.empty())
    specifics.mutable_managed_user()->set_chrome_avatar(chrome_avatar);
  if (!chromeos_avatar.empty())
    specifics.mutable_managed_user()->set_chromeos_avatar(chromeos_avatar);
  if (!master_key.empty())
    specifics.mutable_managed_user()->set_master_key(master_key);
  if (acknowledged)
    specifics.mutable_managed_user()->set_acknowledged(true);
  if (!password_signature_key.empty()) {
    specifics.mutable_managed_user()->
        set_password_signature_key(password_signature_key);
  }
  if (!password_encryption_key.empty()) {
    specifics.mutable_managed_user()->
        set_password_encryption_key(password_encryption_key);
  }
  return SyncData::CreateLocalData(id, name, specifics);
}

SyncData CreateSyncDataFromDictionaryEntry(const std::string& id,
                                           const base::Value& value) {
  const base::DictionaryValue* dict = NULL;
  bool success = value.GetAsDictionary(&dict);
  DCHECK(success);
  bool acknowledged = false;
  dict->GetBoolean(SupervisedUserSyncService::kAcknowledged, &acknowledged);
  std::string name;
  dict->GetString(SupervisedUserSyncService::kName, &name);
  DCHECK(!name.empty());
  std::string master_key;
  dict->GetString(SupervisedUserSyncService::kMasterKey, &master_key);
  std::string chrome_avatar;
  dict->GetString(SupervisedUserSyncService::kChromeAvatar, &chrome_avatar);
  std::string chromeos_avatar;
  dict->GetString(SupervisedUserSyncService::kChromeOsAvatar, &chromeos_avatar);
  std::string signature;
  dict->GetString(SupervisedUserSyncService::kPasswordSignatureKey, &signature);
  std::string encryption;
  dict->GetString(SupervisedUserSyncService::kPasswordEncryptionKey,
                  &encryption);

  return CreateLocalSyncData(id,
                             name,
                             acknowledged,
                             master_key,
                             chrome_avatar,
                             chromeos_avatar,
                             signature,
                             encryption);
}

}  // namespace

const char SupervisedUserSyncService::kAcknowledged[] = "acknowledged";
const char SupervisedUserSyncService::kChromeAvatar[] = "chromeAvatar";
const char SupervisedUserSyncService::kChromeOsAvatar[] = "chromeOsAvatar";
const char SupervisedUserSyncService::kMasterKey[] = "masterKey";
const char SupervisedUserSyncService::kName[] = "name";
const char SupervisedUserSyncService::kPasswordSignatureKey[] =
    "passwordSignatureKey";
const char SupervisedUserSyncService::kPasswordEncryptionKey[] =
    "passwordEncryptionKey";
const int SupervisedUserSyncService::kNoAvatar = -100;

SupervisedUserSyncService::SupervisedUserSyncService(Profile* profile)
    : profile_(profile), prefs_(profile->GetPrefs()) {
  SigninManagerFactory::GetForProfile(profile_)->AddObserver(this);
}

SupervisedUserSyncService::~SupervisedUserSyncService() {
}

// static
void SupervisedUserSyncService::RegisterProfilePrefs(
    PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kSupervisedUsers);
}

// static
bool SupervisedUserSyncService::GetAvatarIndex(const std::string& avatar_str,
                                               int* avatar_index) {
  DCHECK(avatar_index);
  if (avatar_str.empty()) {
    *avatar_index = kNoAvatar;
    return true;
  }
#if defined(OS_CHROMEOS)
  const char* prefix = kChromeOSAvatarPrefix;
#else
  const char* prefix = kChromeAvatarPrefix;
#endif
  size_t prefix_len = strlen(prefix);
  if (avatar_str.size() <= prefix_len ||
      avatar_str.substr(0, prefix_len) != prefix) {
    return false;
  }

  if (!base::StringToInt(avatar_str.substr(prefix_len), avatar_index))
    return false;

  const int kChromeOSDummyAvatarIndex = -111;

#if defined(OS_CHROMEOS)
  return (
      *avatar_index == kChromeOSDummyAvatarIndex ||
      (*avatar_index >= chromeos::default_user_image::kFirstDefaultImageIndex &&
       *avatar_index < chromeos::default_user_image::kDefaultImagesCount));
#else
  // Check if the Chrome avatar index is set to a dummy value. Some early
  // supervised user profiles on ChromeOS stored a dummy avatar index as a
  // Chrome Avatar before there was logic to sync the ChromeOS avatar
  // separately. Handle this as if the Chrome Avatar was not chosen yet (which
  // is correct for these profiles).
  if (*avatar_index == kChromeOSDummyAvatarIndex)
    *avatar_index = kNoAvatar;
  return (*avatar_index == kNoAvatar ||
          (*avatar_index >= 0 &&
           static_cast<size_t>(*avatar_index) <
               profiles::GetDefaultAvatarIconCount()));
#endif
}

// static
std::string SupervisedUserSyncService::BuildAvatarString(int avatar_index) {
#if defined(OS_CHROMEOS)
  const char* prefix = kChromeOSAvatarPrefix;
#else
  const char* prefix = kChromeAvatarPrefix;
#endif
  return base::StringPrintf("%s%d", prefix, avatar_index);
}

void SupervisedUserSyncService::AddObserver(
    SupervisedUserSyncServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void SupervisedUserSyncService::RemoveObserver(
    SupervisedUserSyncServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

std::unique_ptr<base::DictionaryValue>
SupervisedUserSyncService::CreateDictionary(const std::string& name,
                                            const std::string& master_key,
                                            const std::string& signature_key,
                                            const std::string& encryption_key,
                                            int avatar_index) {
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->SetString(kName, name);
  result->SetString(kMasterKey, master_key);
  result->SetString(kPasswordSignatureKey, signature_key);
  result->SetString(kPasswordEncryptionKey, encryption_key);
  // TODO(akuegel): Get rid of the avatar stuff here when Chrome OS switches
  // to the avatar index that is stored as a shared setting.
  std::string chrome_avatar;
  std::string chromeos_avatar;
#if defined(OS_CHROMEOS)
  chromeos_avatar = BuildAvatarString(avatar_index);
#else
  chrome_avatar = BuildAvatarString(avatar_index);
#endif
  result->SetString(kChromeAvatar, chrome_avatar);
  result->SetString(kChromeOsAvatar, chromeos_avatar);
  return result;
}

void SupervisedUserSyncService::AddSupervisedUser(
    const std::string& id,
    const std::string& name,
    const std::string& master_key,
    const std::string& signature_key,
    const std::string& encryption_key,
    int avatar_index) {
  UpdateSupervisedUserImpl(id,
                           name,
                           master_key,
                           signature_key,
                           encryption_key,
                           avatar_index,
                           true /* add */);
}

void SupervisedUserSyncService::UpdateSupervisedUser(
    const std::string& id,
    const std::string& name,
    const std::string& master_key,
    const std::string& signature_key,
    const std::string& encryption_key,
    int avatar_index) {
  UpdateSupervisedUserImpl(id,
                           name,
                           master_key,
                           signature_key,
                           encryption_key,
                           avatar_index,
                           false /* update */);
}

void SupervisedUserSyncService::UpdateSupervisedUserImpl(
    const std::string& id,
    const std::string& name,
    const std::string& master_key,
    const std::string& signature_key,
    const std::string& encryption_key,
    int avatar_index,
    bool add_user) {
  DictionaryPrefUpdate update(prefs_, prefs::kSupervisedUsers);
  base::DictionaryValue* dict = update.Get();
  std::unique_ptr<base::DictionaryValue> value = CreateDictionary(
      name, master_key, signature_key, encryption_key, avatar_index);

  DCHECK_EQ(add_user, !dict->HasKey(id));
  base::DictionaryValue* entry = value.get();
  dict->SetWithoutPathExpansion(id, value.release());

  if (!sync_processor_)
    return;

  // If we're already syncing, create a new change and upload it.
  SyncChangeList change_list;
  change_list.push_back(
      SyncChange(FROM_HERE,
                 add_user ? SyncChange::ACTION_ADD : SyncChange::ACTION_UPDATE,
                 CreateSyncDataFromDictionaryEntry(id, *entry)));
  SyncError error =
      sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  DCHECK(!error.IsSet()) << error.ToString();
}

void SupervisedUserSyncService::DeleteSupervisedUser(const std::string& id) {
  DictionaryPrefUpdate update(prefs_, prefs::kSupervisedUsers);
  bool success = update->RemoveWithoutPathExpansion(id, NULL);
  DCHECK(success);

  if (!sync_processor_)
    return;

  SyncChangeList change_list;
  change_list.push_back(SyncChange(
      FROM_HERE,
      SyncChange::ACTION_DELETE,
      SyncData::CreateLocalDelete(id, SUPERVISED_USERS)));
  SyncError sync_error =
      sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  DCHECK(!sync_error.IsSet());
}

const base::DictionaryValue* SupervisedUserSyncService::GetSupervisedUsers() {
  DCHECK(sync_processor_);
  return prefs_->GetDictionary(prefs::kSupervisedUsers);
}

bool SupervisedUserSyncService::UpdateSupervisedUserAvatarIfNeeded(
    const std::string& id,
    int avatar_index) {
  DictionaryPrefUpdate update(prefs_, prefs::kSupervisedUsers);
  base::DictionaryValue* dict = update.Get();
  DCHECK(dict->HasKey(id));
  base::DictionaryValue* value = NULL;
  bool success = dict->GetDictionaryWithoutPathExpansion(id, &value);
  DCHECK(success);

  bool acknowledged = false;
  value->GetBoolean(SupervisedUserSyncService::kAcknowledged, &acknowledged);
  std::string name;
  value->GetString(SupervisedUserSyncService::kName, &name);
  std::string master_key;
  value->GetString(SupervisedUserSyncService::kMasterKey, &master_key);
  std::string signature;
  value->GetString(SupervisedUserSyncService::kPasswordSignatureKey,
                   &signature);
  std::string encryption;
  value->GetString(SupervisedUserSyncService::kPasswordEncryptionKey,
                   &encryption);
  std::string chromeos_avatar;
  value->GetString(SupervisedUserSyncService::kChromeOsAvatar,
                   &chromeos_avatar);
  std::string chrome_avatar;
  value->GetString(SupervisedUserSyncService::kChromeAvatar, &chrome_avatar);
  // The following check is just for safety. We want to avoid that the existing
  // avatar selection is overwritten. Currently we don't allow the user to
  // choose a different avatar in the recreation dialog, anyway, if there is
  // already an avatar selected.
#if defined(OS_CHROMEOS)
  if (!chromeos_avatar.empty() && avatar_index != kNoAvatar)
    return false;
#else
  if (!chrome_avatar.empty() && avatar_index != kNoAvatar)
    return false;
#endif

  chrome_avatar = avatar_index == kNoAvatar ?
      std::string() : BuildAvatarString(avatar_index);
#if defined(OS_CHROMEOS)
  value->SetString(kChromeOsAvatar, chrome_avatar);
#else
  value->SetString(kChromeAvatar, chrome_avatar);
#endif

  if (!sync_processor_)
    return true;

  SyncChangeList change_list;
  change_list.push_back(SyncChange(
      FROM_HERE,
      SyncChange::ACTION_UPDATE,
      CreateLocalSyncData(id, name, acknowledged, master_key,
                          chrome_avatar, chromeos_avatar,
                          signature, encryption)));
  SyncError error =
      sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  DCHECK(!error.IsSet()) << error.ToString();
  return true;
}

void SupervisedUserSyncService::ClearSupervisedUserAvatar(
    const std::string& id) {
  bool cleared = UpdateSupervisedUserAvatarIfNeeded(id, kNoAvatar);
  DCHECK(cleared);
}

void SupervisedUserSyncService::GetSupervisedUsersAsync(
    const SupervisedUsersCallback& callback) {
  // If we are already syncing, just run the callback.
  if (sync_processor_) {
    callback.Run(GetSupervisedUsers());
    return;
  }

  // Otherwise queue it up until we start syncing.
  callbacks_.push_back(callback);
}

void SupervisedUserSyncService::Shutdown() {
  NotifySupervisedUsersSyncingStopped();
  SigninManagerFactory::GetForProfile(profile_)->RemoveObserver(this);
}

SyncMergeResult SupervisedUserSyncService::MergeDataAndStartSyncing(
    ModelType type,
    const SyncDataList& initial_sync_data,
    std::unique_ptr<SyncChangeProcessor> sync_processor,
    std::unique_ptr<SyncErrorFactory> error_handler) {
  DCHECK_EQ(SUPERVISED_USERS, type);
  sync_processor_ = std::move(sync_processor);
  error_handler_ = std::move(error_handler);

  SyncChangeList change_list;
  SyncMergeResult result(SUPERVISED_USERS);

  DictionaryPrefUpdate update(prefs_, prefs::kSupervisedUsers);
  base::DictionaryValue* dict = update.Get();
  result.set_num_items_before_association(dict->size());
  std::set<std::string> seen_ids;
  int num_items_added = 0;
  int num_items_modified = 0;
  for (const SyncData& sync_data : initial_sync_data) {
    DCHECK_EQ(SUPERVISED_USERS, sync_data.GetDataType());
    const ManagedUserSpecifics& supervised_user =
        sync_data.GetSpecifics().managed_user();
    std::unique_ptr<base::DictionaryValue> value =
        base::MakeUnique<base::DictionaryValue>();
    value->SetString(kName, supervised_user.name());
    value->SetBoolean(kAcknowledged, supervised_user.acknowledged());
    value->SetString(kMasterKey, supervised_user.master_key());
    value->SetString(kChromeAvatar, supervised_user.chrome_avatar());
    value->SetString(kChromeOsAvatar, supervised_user.chromeos_avatar());
    value->SetString(kPasswordSignatureKey,
                     supervised_user.password_signature_key());
    value->SetString(kPasswordEncryptionKey,
                     supervised_user.password_encryption_key());
    if (dict->HasKey(supervised_user.id()))
      num_items_modified++;
    else
      num_items_added++;
    dict->SetWithoutPathExpansion(supervised_user.id(), std::move(value));
    seen_ids.insert(supervised_user.id());
  }

  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    if (seen_ids.find(it.key()) != seen_ids.end())
      continue;

    change_list.push_back(
        SyncChange(FROM_HERE,
                   SyncChange::ACTION_ADD,
                   CreateSyncDataFromDictionaryEntry(it.key(), it.value())));
  }
  result.set_error(sync_processor_->ProcessSyncChanges(FROM_HERE, change_list));

  result.set_num_items_modified(num_items_modified);
  result.set_num_items_added(num_items_added);
  result.set_num_items_after_association(dict->size());

  DispatchCallbacks();

  return result;
}

void SupervisedUserSyncService::StopSyncing(ModelType type) {
  DCHECK_EQ(SUPERVISED_USERS, type);
  // The observers may want to change the Sync data, so notify them before
  // resetting the |sync_processor_|.
  NotifySupervisedUsersSyncingStopped();
  sync_processor_.reset();
  error_handler_.reset();
}

SyncDataList SupervisedUserSyncService::GetAllSyncData(
    ModelType type) const {
  SyncDataList data;
  DictionaryPrefUpdate update(prefs_, prefs::kSupervisedUsers);
  base::DictionaryValue* dict = update.Get();
  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance())
    data.push_back(CreateSyncDataFromDictionaryEntry(it.key(), it.value()));

  return data;
}

SyncError SupervisedUserSyncService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& change_list) {
  SyncError error;
  DictionaryPrefUpdate update(prefs_, prefs::kSupervisedUsers);
  base::DictionaryValue* dict = update.Get();
  for (const SyncChange& sync_change : change_list) {
    SyncData data = sync_change.sync_data();
    DCHECK_EQ(SUPERVISED_USERS, data.GetDataType());
    const ManagedUserSpecifics& supervised_user =
        data.GetSpecifics().managed_user();
    switch (sync_change.change_type()) {
      case SyncChange::ACTION_ADD:
      case SyncChange::ACTION_UPDATE: {
        // Every item we get from the server should be acknowledged.
        DCHECK(supervised_user.acknowledged());
        const base::DictionaryValue* old_value = NULL;
        dict->GetDictionaryWithoutPathExpansion(supervised_user.id(),
                                                &old_value);

        // For an update action, the supervised user should already exist, for
        // an add action, it should not.
        DCHECK_EQ(
            old_value ? SyncChange::ACTION_UPDATE : SyncChange::ACTION_ADD,
            sync_change.change_type());

        // If the supervised user switched from unacknowledged to acknowledged,
        // we might need to continue with a registration.
        if (old_value && !old_value->HasKey(kAcknowledged))
          NotifySupervisedUserAcknowledged(supervised_user.id());

        std::unique_ptr<base::DictionaryValue> value =
            base::MakeUnique<base::DictionaryValue>();
        value->SetString(kName, supervised_user.name());
        value->SetBoolean(kAcknowledged, supervised_user.acknowledged());
        value->SetString(kMasterKey, supervised_user.master_key());
        value->SetString(kChromeAvatar, supervised_user.chrome_avatar());
        value->SetString(kChromeOsAvatar, supervised_user.chromeos_avatar());
        value->SetString(kPasswordSignatureKey,
                         supervised_user.password_signature_key());
        value->SetString(kPasswordEncryptionKey,
                         supervised_user.password_encryption_key());
        dict->SetWithoutPathExpansion(supervised_user.id(), std::move(value));

        NotifySupervisedUsersChanged();
        break;
      }
      case SyncChange::ACTION_DELETE: {
        DCHECK(dict->HasKey(supervised_user.id())) << supervised_user.id();
        dict->RemoveWithoutPathExpansion(supervised_user.id(), NULL);
        break;
      }
      case SyncChange::ACTION_INVALID: {
        NOTREACHED();
        break;
      }
    }
  }
  return error;
}

void SupervisedUserSyncService::GoogleSignedOut(
    const std::string& account_id,
    const std::string& username) {
  // Clear all data on signout, to avoid supervised users from one custodian
  // appearing in another one's profile.
  prefs_->ClearPref(prefs::kSupervisedUsers);
}

void SupervisedUserSyncService::NotifySupervisedUserAcknowledged(
    const std::string& supervised_user_id) {
  for (SupervisedUserSyncServiceObserver& observer : observers_)
    observer.OnSupervisedUserAcknowledged(supervised_user_id);
}

void SupervisedUserSyncService::NotifySupervisedUsersSyncingStopped() {
  for (SupervisedUserSyncServiceObserver& observer : observers_)
    observer.OnSupervisedUsersSyncingStopped();
}

void SupervisedUserSyncService::NotifySupervisedUsersChanged() {
  for (SupervisedUserSyncServiceObserver& observer : observers_)
    observer.OnSupervisedUsersChanged();
}

void SupervisedUserSyncService::DispatchCallbacks() {
  const base::DictionaryValue* supervised_users =
      prefs_->GetDictionary(prefs::kSupervisedUsers);
  for (const auto& callback : callbacks_)
    callback.Run(supervised_users);
  callbacks_.clear();
}
