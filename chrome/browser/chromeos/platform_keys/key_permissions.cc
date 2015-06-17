// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/key_permissions.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/values.h"
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
  // This permission is granted by the user or by policy to allow the extension
  // to use the key for signing through the enterprise.platformKeys or
  // platformKeys API.
  // This permission is granted until revoked by the user or the policy.
  bool sign_unlimited = false;
};

KeyPermissions::PermissionsForExtension::PermissionsForExtension(
    const std::string& extension_id,
    scoped_ptr<base::Value> state_store_value,
    KeyPermissions* key_permissions)
    : extension_id_(extension_id), key_permissions_(key_permissions) {
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

  return matching_entry->sign_once || matching_entry->sign_unlimited;
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
}

void KeyPermissions::PermissionsForExtension::SetUserGrantedPermission(
    const std::string& public_key_spki_der) {
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
  for (const base::Value* entry : *entries) {
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

scoped_ptr<base::Value>
KeyPermissions::PermissionsForExtension::KeyEntriesToState() {
  scoped_ptr<base::ListValue> new_state(new base::ListValue);
  for (const KeyEntry& entry : state_store_entries_) {
    // Drop entries that the extension doesn't have any permissions for anymore.
    if (!entry.sign_once && !entry.sign_unlimited)
      continue;

    scoped_ptr<base::DictionaryValue> new_entry(new base::DictionaryValue);
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
    new_state->Append(new_entry.release());
  }
  return new_state.Pass();
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
                               extensions::StateStore* extensions_state_store)
    : profile_is_managed_(profile_is_managed),
      extensions_state_store_(extensions_state_store),
      weak_factory_(this) {
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
    const std::string& public_key_spki_der) {
  // As keys cannot be tagged for non-corporate usage, the user can currently
  // not grant any permissions if the profile is managed.
  return !profile_is_managed_;
}

void KeyPermissions::SetPlatformKeysOfExtension(const std::string& extension_id,
                                                scoped_ptr<base::Value> value) {
  extensions_state_store_->SetExtensionValue(
      extension_id, kStateStorePlatformKeys, value.Pass());
}

void KeyPermissions::CreatePermissionObjectAndPassToCallback(
    const std::string& extension_id,
    const PermissionsCallback& callback,
    scoped_ptr<base::Value> value) {
  callback.Run(make_scoped_ptr(
      new PermissionsForExtension(extension_id, value.Pass(), this)));
}

}  // namespace chromeos
