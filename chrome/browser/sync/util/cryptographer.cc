// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/base64.h"
#include "chrome/browser/sync/util/cryptographer.h"
#include "chrome/browser/password_manager/encryptor.h"

namespace browser_sync {

const char kNigoriTag[] = "google_chrome_nigori";

// We name a particular Nigori instance (ie. a triplet consisting of a hostname,
// a username, and a password) by calling Permute on this string. Since the
// output of Permute is always the same for a given triplet, clients will always
// assign the same name to a particular triplet.
const char kNigoriKeyName[] = "nigori-key";

Cryptographer::Observer::~Observer() {}

Cryptographer::Cryptographer()
    : default_nigori_(NULL),
      encrypt_everything_(false) {
  syncable::ModelTypeSet sensitive_types = SensitiveTypes();
  encrypted_types_.insert(sensitive_types.begin(), sensitive_types.end());
}

Cryptographer::~Cryptographer() {}

void Cryptographer::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void Cryptographer::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void Cryptographer::Bootstrap(const std::string& restored_bootstrap_token) {
  if (is_initialized()) {
    NOTREACHED();
    return;
  }

  scoped_ptr<Nigori> nigori(UnpackBootstrapToken(restored_bootstrap_token));
  if (nigori.get())
    AddKeyImpl(nigori.release());
}

bool Cryptographer::CanDecrypt(const sync_pb::EncryptedData& data) const {
  return nigoris_.end() != nigoris_.find(data.key_name());
}

bool Cryptographer::CanDecryptUsingDefaultKey(
    const sync_pb::EncryptedData& data) const {
  return default_nigori_ && (data.key_name() == default_nigori_->first);
}

bool Cryptographer::Encrypt(const ::google::protobuf::MessageLite& message,
                            sync_pb::EncryptedData* encrypted) const {
  if (!encrypted || !default_nigori_) {
    LOG(ERROR) << "Cryptographer not ready, failed to encrypt.";
    return false;
  }

  std::string serialized;
  if (!message.SerializeToString(&serialized)) {
    LOG(ERROR) << "Message is invalid/missing a required field.";
    return false;
  }

  encrypted->set_key_name(default_nigori_->first);
  if (!default_nigori_->second->Encrypt(serialized,
                                        encrypted->mutable_blob())) {
    LOG(ERROR) << "Failed to encrypt data.";
    return false;
  }
  return true;
}

bool Cryptographer::Decrypt(const sync_pb::EncryptedData& encrypted,
                            ::google::protobuf::MessageLite* message) const {
  DCHECK(message);
  std::string plaintext = DecryptToString(encrypted);
  return message->ParseFromString(plaintext);
}

std::string Cryptographer::DecryptToString(
    const sync_pb::EncryptedData& encrypted) const {
  NigoriMap::const_iterator it = nigoris_.find(encrypted.key_name());
  if (nigoris_.end() == it) {
    NOTREACHED() << "Cannot decrypt message";
    return std::string("");  // Caller should have called CanDecrypt(encrypt).
  }

  std::string plaintext;
  if (!it->second->Decrypt(encrypted.blob(), &plaintext)) {
    return std::string("");
  }

  return plaintext;
}

bool Cryptographer::GetKeys(sync_pb::EncryptedData* encrypted) const {
  DCHECK(encrypted);
  DCHECK(!nigoris_.empty());

  // Create a bag of all the Nigori parameters we know about.
  sync_pb::NigoriKeyBag bag;
  for (NigoriMap::const_iterator it = nigoris_.begin(); it != nigoris_.end();
       ++it) {
    const Nigori& nigori = *it->second;
    sync_pb::NigoriKey* key = bag.add_key();
    key->set_name(it->first);
    nigori.ExportKeys(key->mutable_user_key(),
                      key->mutable_encryption_key(),
                      key->mutable_mac_key());
  }

  // Encrypt the bag with the default Nigori.
  return Encrypt(bag, encrypted);
}

bool Cryptographer::AddKey(const KeyParams& params) {
  DCHECK(NULL == pending_keys_.get());

  // Create the new Nigori and make it the default encryptor.
  scoped_ptr<Nigori> nigori(new Nigori);
  if (!nigori->InitByDerivation(params.hostname,
                                params.username,
                                params.password)) {
    NOTREACHED();  // Invalid username or password.
    return false;
  }
  return AddKeyImpl(nigori.release());
}

bool Cryptographer::AddKeyImpl(Nigori* initialized_nigori) {
  scoped_ptr<Nigori> nigori(initialized_nigori);
  std::string name;
  if (!nigori->Permute(Nigori::Password, kNigoriKeyName, &name)) {
    NOTREACHED();
    return false;
  }
  nigoris_[name] = make_linked_ptr(nigori.release());
  default_nigori_ = &*nigoris_.find(name);
  return true;
}

bool Cryptographer::SetKeys(const sync_pb::EncryptedData& encrypted) {
  DCHECK(CanDecrypt(encrypted));

  sync_pb::NigoriKeyBag bag;
  if (!Decrypt(encrypted, &bag)) {
    return false;
  }
  InstallKeys(encrypted.key_name(), bag);
  return true;
}

void Cryptographer::SetPendingKeys(const sync_pb::EncryptedData& encrypted) {
  DCHECK(!CanDecrypt(encrypted));
  pending_keys_.reset(new sync_pb::EncryptedData(encrypted));
}

bool Cryptographer::DecryptPendingKeys(const KeyParams& params) {
  Nigori nigori;
  if (!nigori.InitByDerivation(params.hostname,
                               params.username,
                               params.password)) {
    NOTREACHED();
    return false;
  }

  std::string plaintext;
  if (!nigori.Decrypt(pending_keys_->blob(), &plaintext))
    return false;

  sync_pb::NigoriKeyBag bag;
  if (!bag.ParseFromString(plaintext)) {
    NOTREACHED();
    return false;
  }
  InstallKeys(pending_keys_->key_name(), bag);
  pending_keys_.reset();
  return true;
}

bool Cryptographer::GetBootstrapToken(std::string* token) const {
  DCHECK(token);
  if (!is_initialized())
    return false;

  return PackBootstrapToken(default_nigori_->second.get(), token);
}

bool Cryptographer::PackBootstrapToken(const Nigori* nigori,
                                       std::string* pack_into) const {
  DCHECK(pack_into);
  DCHECK(nigori);

  sync_pb::NigoriKey key;
  if (!nigori->ExportKeys(key.mutable_user_key(),
                          key.mutable_encryption_key(),
                          key.mutable_mac_key())) {
    NOTREACHED();
    return false;
  }

  std::string unencrypted_token;
  if (!key.SerializeToString(&unencrypted_token)) {
    NOTREACHED();
    return false;
  }

  std::string encrypted_token;
  if (!Encryptor::EncryptString(unencrypted_token, &encrypted_token)) {
    NOTREACHED();
    return false;
  }

  if (!base::Base64Encode(encrypted_token, pack_into)) {
    NOTREACHED();
    return false;
  }
  return true;
}

Nigori* Cryptographer::UnpackBootstrapToken(const std::string& token) const {
  if (token.empty())
    return NULL;

  std::string encrypted_data;
  if (!base::Base64Decode(token, &encrypted_data)) {
    DLOG(WARNING) << "Could not decode token.";
    return NULL;
  }

  std::string unencrypted_token;
  if (!Encryptor::DecryptString(encrypted_data, &unencrypted_token)) {
    DLOG(WARNING) << "Decryption of bootstrap token failed.";
    return NULL;
  }

  sync_pb::NigoriKey key;
  if (!key.ParseFromString(unencrypted_token)) {
    DLOG(WARNING) << "Parsing of bootstrap token failed.";
    return NULL;
  }

  scoped_ptr<Nigori> nigori(new Nigori);
  if (!nigori->InitByImport(key.user_key(), key.encryption_key(),
                            key.mac_key())) {
    NOTREACHED();
    return NULL;
  }

  return nigori.release();
}

Cryptographer::UpdateResult Cryptographer::Update(
    const sync_pb::NigoriSpecifics& nigori) {
  UpdateEncryptedTypesFromNigori(nigori);
  if (!nigori.encrypted().blob().empty()) {
    if (CanDecrypt(nigori.encrypted())) {
      SetKeys(nigori.encrypted());
      return Cryptographer::SUCCESS;
    } else {
      SetPendingKeys(nigori.encrypted());
      return Cryptographer::NEEDS_PASSPHRASE;
    }
  }
  return Cryptographer::SUCCESS;
}

// Static
syncable::ModelTypeSet Cryptographer::SensitiveTypes() {
  syncable::ModelTypeSet types;
  // Both of these have their own encryption schemes, but we include them
  // anyways.
  types.insert(syncable::PASSWORDS);
  types.insert(syncable::NIGORI);
  return types;
}

void Cryptographer::UpdateEncryptedTypesFromNigori(
    const sync_pb::NigoriSpecifics& nigori) {
  if (nigori.encrypt_everything()) {
    set_encrypt_everything();
    return;
  }

  syncable::ModelTypeSet encrypted_types(SensitiveTypes());
  if (nigori.encrypt_bookmarks())
    encrypted_types.insert(syncable::BOOKMARKS);
  if (nigori.encrypt_preferences())
    encrypted_types.insert(syncable::PREFERENCES);
  if (nigori.encrypt_autofill_profile())
    encrypted_types.insert(syncable::AUTOFILL_PROFILE);
  if (nigori.encrypt_autofill())
    encrypted_types.insert(syncable::AUTOFILL);
  if (nigori.encrypt_themes())
    encrypted_types.insert(syncable::THEMES);
  if (nigori.encrypt_typed_urls())
    encrypted_types.insert(syncable::TYPED_URLS);
  if (nigori.encrypt_extension_settings())
    encrypted_types.insert(syncable::EXTENSION_SETTINGS);
  if (nigori.encrypt_extensions())
    encrypted_types.insert(syncable::EXTENSIONS);
  if (nigori.encrypt_search_engines())
    encrypted_types.insert(syncable::SEARCH_ENGINES);
  if (nigori.encrypt_sessions())
    encrypted_types.insert(syncable::SESSIONS);
  if (nigori.encrypt_app_settings())
    encrypted_types.insert(syncable::APP_SETTINGS);
  if (nigori.encrypt_apps())
    encrypted_types.insert(syncable::APPS);
  if (nigori.encrypt_app_notifications())
    encrypted_types.insert(syncable::APP_NOTIFICATIONS);

  // Note: the initial version with encryption did not support the
  // encrypt_everything field. If anything more than the sensitive types were
  // encrypted, it meant we were encrypting everything.
  if (!nigori.has_encrypt_everything() &&
      encrypted_types.size() > SensitiveTypes().size()) {
    set_encrypt_everything();
    return;
  }

  MergeEncryptedTypes(encrypted_types);
}

void Cryptographer::UpdateNigoriFromEncryptedTypes(
    sync_pb::NigoriSpecifics* nigori) const {
  nigori->set_encrypt_everything(encrypt_everything_);
  nigori->set_encrypt_bookmarks(
      encrypted_types_.count(syncable::BOOKMARKS) > 0);
  nigori->set_encrypt_preferences(
      encrypted_types_.count(syncable::PREFERENCES) > 0);
  nigori->set_encrypt_autofill_profile(
      encrypted_types_.count(syncable::AUTOFILL_PROFILE) > 0);
  nigori->set_encrypt_autofill(encrypted_types_.count(syncable::AUTOFILL) > 0);
  nigori->set_encrypt_themes(encrypted_types_.count(syncable::THEMES) > 0);
  nigori->set_encrypt_typed_urls(
      encrypted_types_.count(syncable::TYPED_URLS) > 0);
  nigori->set_encrypt_extension_settings(
      encrypted_types_.count(syncable::EXTENSION_SETTINGS) > 0);
  nigori->set_encrypt_extensions(
      encrypted_types_.count(syncable::EXTENSIONS) > 0);
  nigori->set_encrypt_search_engines(
      encrypted_types_.count(syncable::SEARCH_ENGINES) > 0);
  nigori->set_encrypt_sessions(encrypted_types_.count(syncable::SESSIONS) > 0);
  nigori->set_encrypt_app_settings(
      encrypted_types_.count(syncable::APP_SETTINGS) > 0);
  nigori->set_encrypt_apps(encrypted_types_.count(syncable::APPS) > 0);
  nigori->set_encrypt_app_notifications(
      encrypted_types_.count(syncable::APP_NOTIFICATIONS) > 0);
}

void Cryptographer::set_encrypt_everything() {
  if (encrypt_everything_) {
    DCHECK(encrypted_types_ == syncable::GetAllRealModelTypes());
    return;
  }
  encrypt_everything_ = true;
  // Change |encrypted_types_| directly to avoid sending more than one
  // notification.
  encrypted_types_ = syncable::GetAllRealModelTypes();
  EmitEncryptedTypesChangedNotification();
}

bool Cryptographer::encrypt_everything() const {
  return encrypt_everything_;
}

syncable::ModelTypeSet Cryptographer::GetEncryptedTypes() const {
  return encrypted_types_;
}

void Cryptographer::MergeEncryptedTypesForTest(
    const syncable::ModelTypeSet& encrypted_types) {
  MergeEncryptedTypes(encrypted_types);
}

void Cryptographer::MergeEncryptedTypes(
    const syncable::ModelTypeSet& encrypted_types) {
  if (std::includes(encrypted_types_.begin(), encrypted_types_.end(),
                    encrypted_types.begin(), encrypted_types.end())) {
    return;
  }
  encrypted_types_ = encrypted_types;
  EmitEncryptedTypesChangedNotification();
}

void Cryptographer::EmitEncryptedTypesChangedNotification() {
  FOR_EACH_OBSERVER(
      Observer, observers_,
      OnEncryptedTypesChanged(encrypted_types_, encrypt_everything_));
}

void Cryptographer::InstallKeys(const std::string& default_key_name,
                                const sync_pb::NigoriKeyBag& bag) {
  int key_size = bag.key_size();
  for (int i = 0; i < key_size; ++i) {
    const sync_pb::NigoriKey key = bag.key(i);
    // Only use this key if we don't already know about it.
    if (nigoris_.end() == nigoris_.find(key.name())) {
      scoped_ptr<Nigori> new_nigori(new Nigori);
      if (!new_nigori->InitByImport(key.user_key(),
                                    key.encryption_key(),
                                    key.mac_key())) {
        NOTREACHED();
        continue;
      }
      nigoris_[key.name()] = make_linked_ptr(new_nigori.release());
    }
  }
  DCHECK(nigoris_.end() != nigoris_.find(default_key_name));
  default_nigori_ = &*nigoris_.find(default_key_name);
}

}  // namespace browser_sync
