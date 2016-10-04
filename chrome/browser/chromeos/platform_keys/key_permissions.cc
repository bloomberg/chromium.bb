// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/key_permissions.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/state_store.h"

namespace chromeos {

namespace {

// The key at which platform key specific data is stored in each extension's
// state store.
//
// From older versions of ChromeOS, this key can hold a list of base64 and
// DER-encoded SPKIs. A key can be used for signing at most once if it is part
// of that list.
//
// The current format of data that is written to the PlatformKeys field is a
// list of serialized KeyEntry objects:
//   { 'SPKI': string,
//     'signOnce': bool,  // if not present, defaults to false
//     'signUnlimited': bool  // if not present, defaults to false
//   }
//
// Do not change this constant as clients will lose their existing state.
const char kStateStorePlatformKeys[] = "PlatformKeys";
const char kStateStoreSPKI[] = "SPKI";
const char kStateStoreSignOnce[] = "signOnce";
const char kStateStoreSignUnlimited[] = "signUnlimited";

// The profile pref prefs::kPlatformKeys stores a dictionary mapping from
// public key (base64 encoding of an DER-encoded SPKI) to key properties. The
// currently only key property is the key usage, which can either be undefined
// or "corporate". If a key is not present in the pref, the default for the key
// usage is undefined, which in particular means "not for corporate usage".
// E.g. the entry in the profile pref might look like:
// "platform_keys" : {
//   "ABCDEF123" : {
//     "keyUsage" : "corporate"
//   },
//   "abcdef567" : {
//     "keyUsage" : "corporate"
//   }
// }
const char kPrefKeyUsage[] = "keyUsage";
const char kPrefKeyUsageCorporate[] = "corporate";

const char kPolicyAllowCorporateKeyUsage[] = "allowCorporateKeyUsage";

}  // namespace

struct KeyPermissions::PermissionsForExtension::KeyEntry {
  explicit KeyEntry(const std::string& public_key_spki_der_b64)
      : spki_b64(public_key_spki_der_b64) {}

  // The base64-encoded DER of a X.509 Subject Public Key Info.
  std::string spki_b64;

  // True if the key can be used once for singing.
  // This permission is granted if an extension generated a key using the
  // enterprise.platformKeys API, so that it can build a certification request.
  // After the first signing operation this permission will be revoked.
  bool sign_once = false;

  // True if the key can be used for signing an unlimited number of times.
  // This permission is granted by the user to allow the extension to use the
  // key for signing through the enterprise.platformKeys or platformKeys API.
  // This permission is granted until revoked by the user or the policy.
  bool sign_unlimited = false;
};

KeyPermissions::PermissionsForExtension::PermissionsForExtension(
    const std::string& extension_id,
    std::unique_ptr<base::Value> state_store_value,
    PrefService* profile_prefs,
    policy::PolicyService* profile_policies,
    KeyPermissions* key_permissions)
    : extension_id_(extension_id),
      profile_prefs_(profile_prefs),
      profile_policies_(profile_policies),
      key_permissions_(key_permissions) {
  DCHECK(profile_prefs_);
  DCHECK(profile_policies_);
  DCHECK(key_permissions_);
  if (state_store_value)
    KeyEntriesFromState(*state_store_value);
}

KeyPermissions::PermissionsForExtension::~PermissionsForExtension() {
}

bool KeyPermissions::PermissionsForExtension::CanUseKeyForSigning(
    const std::string& public_key_spki_der) {
  std::string public_key_spki_der_b64;
  base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);

  KeyEntry* matching_entry = GetStateStoreEntry(public_key_spki_der_b64);

  // In any case, we allow the generating extension to use the generated key a
  // single time for signing arbitrary data. The reason is, that the extension
  // usually has to sign a certification request containing the public key in
  // order to obtain a certificate for the key.
  // That means, once a certificate authority generated a certificate for the
  // key, the generating extension doesn't have access to the key anymore,
  // except if explicitly permitted by the administrator.
  if (matching_entry->sign_once)
    return true;

  // Usage of corporate keys is solely determined by policy. The user must not
  // circumvent this decision.
  if (key_permissions_->IsCorporateKey(public_key_spki_der_b64))
    return PolicyAllowsCorporateKeyUsage();

  // Only permissions for keys that are not designated for corporate usage are
  // determined by user decisions.
  return matching_entry->sign_unlimited;
}

void KeyPermissions::PermissionsForExtension::SetKeyUsedForSigning(
    const std::string& public_key_spki_der) {
  std::string public_key_spki_der_b64;
  base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);

  KeyEntry* matching_entry = GetStateStoreEntry(public_key_spki_der_b64);

  if (!matching_entry->sign_once) {
    if (matching_entry->sign_unlimited)
      VLOG(1) << "Key is already marked as not usable for signing, skipping.";
    else
      LOG(ERROR) << "Key was not allowed for signing.";
    return;
  }

  matching_entry->sign_once = false;
  WriteToStateStore();
}

void KeyPermissions::PermissionsForExtension::RegisterKeyForCorporateUsage(
    const std::string& public_key_spki_der) {
  std::string public_key_spki_der_b64;
  base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);

  KeyEntry* matching_entry = GetStateStoreEntry(public_key_spki_der_b64);

  if (matching_entry->sign_once) {
    VLOG(1) << "Key is already allowed for signing, skipping.";
    return;
  }

  matching_entry->sign_once = true;
  WriteToStateStore();

  DictionaryPrefUpdate update(profile_prefs_, prefs::kPlatformKeys);

  std::unique_ptr<base::DictionaryValue> new_pref_entry(
      new base::DictionaryValue);
  new_pref_entry->SetStringWithoutPathExpansion(kPrefKeyUsage,
                                                kPrefKeyUsageCorporate);

  update->SetWithoutPathExpansion(public_key_spki_der_b64,
                                  new_pref_entry.release());
}

void KeyPermissions::PermissionsForExtension::SetUserGrantedPermission(
    const std::string& public_key_spki_der) {
  if (!key_permissions_->CanUserGrantPermissionFor(public_key_spki_der)) {
    LOG(WARNING) << "Tried to grant permission for a key although prohibited "
                    "(either key is a corporate key or this account is "
                    "managed).";
    return;
  }

  std::string public_key_spki_der_b64;
  base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);
  KeyEntry* matching_entry = GetStateStoreEntry(public_key_spki_der_b64);

  if (matching_entry->sign_unlimited) {
    VLOG(1) << "Key is already allowed for signing, skipping.";
    return;
  }

  matching_entry->sign_unlimited = true;
  WriteToStateStore();
}

bool KeyPermissions::PermissionsForExtension::PolicyAllowsCorporateKeyUsage()
    const {
  const policy::PolicyMap& policies = profile_policies_->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
  const base::Value* policy_value =
      policies.GetValue(policy::key::kKeyPermissions);
  if (!policy_value)
    return false;

  const base::DictionaryValue* key_permissions_map = nullptr;
  policy_value->GetAsDictionary(&key_permissions_map);
  if (!key_permissions_map) {
    LOG(ERROR) << "Expected policy to be a dictionary.";
    return false;
  }

  const base::DictionaryValue* key_permissions_for_ext = nullptr;
  key_permissions_map->GetDictionaryWithoutPathExpansion(
      extension_id_, &key_permissions_for_ext);
  if (!key_permissions_for_ext)
    return false;

  bool allow_corporate_key_usage = false;
  key_permissions_for_ext->GetBooleanWithoutPathExpansion(
      kPolicyAllowCorporateKeyUsage, &allow_corporate_key_usage);

  VLOG_IF(allow_corporate_key_usage, 2)
      << "Policy allows usage of corporate keys by extension " << extension_id_;
  return allow_corporate_key_usage;
}

void KeyPermissions::PermissionsForExtension::WriteToStateStore() {
  key_permissions_->SetPlatformKeysOfExtension(extension_id_,
                                               KeyEntriesToState());
}

void KeyPermissions::PermissionsForExtension::KeyEntriesFromState(
    const base::Value& state) {
  state_store_entries_.clear();

  const base::ListValue* entries = nullptr;
  if (!state.GetAsList(&entries)) {
    LOG(ERROR) << "Found a state store of wrong type.";
    return;
  }
  for (const auto& entry : *entries) {
    if (!entry) {
      LOG(ERROR) << "Found invalid NULL entry in PlatformKeys state store.";
      continue;
    }

    std::string spki_b64;
    const base::DictionaryValue* dict_entry = nullptr;
    if (entry->GetAsString(&spki_b64)) {
      // This handles the case that the store contained a plain list of base64
      // and DER-encoded SPKIs from an older version of ChromeOS.
      KeyEntry new_entry(spki_b64);
      new_entry.sign_once = true;
      state_store_entries_.push_back(new_entry);
    } else if (entry->GetAsDictionary(&dict_entry)) {
      dict_entry->GetStringWithoutPathExpansion(kStateStoreSPKI, &spki_b64);
      KeyEntry new_entry(spki_b64);
      dict_entry->GetBooleanWithoutPathExpansion(kStateStoreSignOnce,
                                                 &new_entry.sign_once);
      dict_entry->GetBooleanWithoutPathExpansion(kStateStoreSignUnlimited,
                                                 &new_entry.sign_unlimited);
      state_store_entries_.push_back(new_entry);
    } else {
      LOG(ERROR) << "Found invalid entry of type " << entry->GetType()
                 << " in PlatformKeys state store.";
      continue;
    }
  }
}

std::unique_ptr<base::Value>
KeyPermissions::PermissionsForExtension::KeyEntriesToState() {
  std::unique_ptr<base::ListValue> new_state(new base::ListValue);
  for (const KeyEntry& entry : state_store_entries_) {
    // Drop entries that the extension doesn't have any permissions for anymore.
    if (!entry.sign_once && !entry.sign_unlimited)
      continue;

    std::unique_ptr<base::DictionaryValue> new_entry(new base::DictionaryValue);
    new_entry->SetStringWithoutPathExpansion(kStateStoreSPKI, entry.spki_b64);
    // Omit writing default values, namely |false|.
    if (entry.sign_once) {
      new_entry->SetBooleanWithoutPathExpansion(kStateStoreSignOnce,
                                                entry.sign_once);
    }
    if (entry.sign_unlimited) {
      new_entry->SetBooleanWithoutPathExpansion(kStateStoreSignUnlimited,
                                                entry.sign_unlimited);
    }
    new_state->Append(std::move(new_entry));
  }
  return std::move(new_state);
}

KeyPermissions::PermissionsForExtension::KeyEntry*
KeyPermissions::PermissionsForExtension::GetStateStoreEntry(
    const std::string& public_key_spki_der_b64) {
  for (KeyEntry& entry : state_store_entries_) {
    // For every ASN.1 value there is exactly one DER encoding, so it is fine to
    // compare the DER (or its base64 encoding).
    if (entry.spki_b64 == public_key_spki_der_b64)
      return &entry;
  }

  state_store_entries_.push_back(KeyEntry(public_key_spki_der_b64));
  return &state_store_entries_.back();
}

KeyPermissions::KeyPermissions(bool profile_is_managed,
                               PrefService* profile_prefs,
                               policy::PolicyService* profile_policies,
                               extensions::StateStore* extensions_state_store)
    : profile_is_managed_(profile_is_managed),
      profile_prefs_(profile_prefs),
      profile_policies_(profile_policies),
      extensions_state_store_(extensions_state_store),
      weak_factory_(this) {
  DCHECK(profile_prefs_);
  DCHECK(extensions_state_store_);
  DCHECK(!profile_is_managed_ || profile_policies_);
}

KeyPermissions::~KeyPermissions() {
}

void KeyPermissions::GetPermissionsForExtension(
    const std::string& extension_id,
    const PermissionsCallback& callback) {
  extensions_state_store_->GetExtensionValue(
      extension_id, kStateStorePlatformKeys,
      base::Bind(&KeyPermissions::CreatePermissionObjectAndPassToCallback,
                 weak_factory_.GetWeakPtr(), extension_id, callback));
}

bool KeyPermissions::CanUserGrantPermissionFor(
    const std::string& public_key_spki_der) const {
  // As keys cannot be tagged for non-corporate usage, the user can currently
  // not grant any permissions if the profile is managed.
  if (profile_is_managed_)
    return false;

  std::string public_key_spki_der_b64;
  base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);

  // If this profile is not managed but we find a corporate key, don't allow
  // the user to grant permissions.
  return !IsCorporateKey(public_key_spki_der_b64);
}

bool KeyPermissions::IsCorporateKey(
    const std::string& public_key_spki_der_b64) const {
  const base::DictionaryValue* prefs_entry =
      GetPrefsEntry(public_key_spki_der_b64);
  if (prefs_entry) {
    std::string key_usage;
    prefs_entry->GetStringWithoutPathExpansion(kPrefKeyUsage, &key_usage);
    return key_usage == kPrefKeyUsageCorporate;
  }
  return false;
}

void KeyPermissions::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // For the format of the dictionary see the documentation at kPrefKeyUsage.
  registry->RegisterDictionaryPref(prefs::kPlatformKeys);
}

void KeyPermissions::CreatePermissionObjectAndPassToCallback(
    const std::string& extension_id,
    const PermissionsCallback& callback,
    std::unique_ptr<base::Value> value) {
  callback.Run(base::MakeUnique<PermissionsForExtension>(
      extension_id, std::move(value), profile_prefs_, profile_policies_, this));
}

void KeyPermissions::SetPlatformKeysOfExtension(
    const std::string& extension_id,
    std::unique_ptr<base::Value> value) {
  extensions_state_store_->SetExtensionValue(
      extension_id, kStateStorePlatformKeys, std::move(value));
}

const base::DictionaryValue* KeyPermissions::GetPrefsEntry(
    const std::string& public_key_spki_der_b64) const {
  const base::DictionaryValue* platform_keys =
      profile_prefs_->GetDictionary(prefs::kPlatformKeys);

  const base::DictionaryValue* key_entry = nullptr;
  platform_keys->GetDictionaryWithoutPathExpansion(public_key_spki_der_b64,
                                                   &key_entry);
  return key_entry;
}

}  // namespace chromeos
