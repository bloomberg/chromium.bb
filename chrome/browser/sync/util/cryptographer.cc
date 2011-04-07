// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

Cryptographer::Cryptographer() : default_nigori_(NULL) {
}

Cryptographer::~Cryptographer() {}

void Cryptographer::Bootstrap(const std::string& restored_bootstrap_token) {
  if (is_ready()) {
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
  DCHECK(encrypted);
  DCHECK(default_nigori_);

  std::string serialized;
  if (!message.SerializeToString(&serialized)) {
    NOTREACHED();  // |message| is invalid/missing a required field.
    return false;
  }

  encrypted->set_key_name(default_nigori_->first);
  if (!default_nigori_->second->Encrypt(serialized,
                                        encrypted->mutable_blob())) {
    NOTREACHED();  // Encrypt should not fail.
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
  if (!is_ready())
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
