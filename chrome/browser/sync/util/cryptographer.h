// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_CRYPTOGRAPHER_H_
#define CHROME_BROWSER_SYNC_UTIL_CRYPTOGRAPHER_H_
#pragma once

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/linked_ptr.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/protocol/nigori_specifics.pb.h"
#include "chrome/browser/sync/util/nigori.h"

namespace browser_sync {

extern const char kNigoriTag[];

// The parameters used to initialize a Nigori instance.
struct KeyParams {
  std::string hostname;
  std::string username;
  std::string password;
};

// This class manages the Nigori objects used to encrypt and decrypt sensitive
// sync data (eg. passwords). Each Nigori object knows how to handle data
// protected with a particular passphrase.
//
// Whenever an update to the Nigori sync node is received from the server,
// SetPendingKeys should be called with the encrypted contents of that node.
// Most likely, an updated Nigori node means that a new passphrase has been set
// and that future node updates won't be decryptable. To remedy this, the user
// should be prompted for the new passphrase and DecryptPendingKeys be called.
//
// Whenever a update to an encrypted node is received from the server,
// CanDecrypt should be used to verify whether the Cryptographer can decrypt
// that node. If it cannot, then the application of that update should be
// delayed until after it can be decrypted.
class Cryptographer {
 public:
  Cryptographer();
  ~Cryptographer();

  // |restored_bootstrap_token| can be provided via this method to bootstrap
  // Cryptographer instance into the ready state (is_ready will be true).
  // It must be a string that was previously built by the
  // GetSerializedBootstrapToken function.  It is possible that the token is no
  // longer valid (due to server key change), in which case the normal
  // decryption code paths will fail and the user will need to provide a new
  // passphrase.
  // It is an error to call this if is_ready() == true, though it is fair to
  // never call Bootstrap at all.
  void Bootstrap(const std::string& restored_bootstrap_token);

  // Returns whether we can decrypt |encrypted| using the keys we currently know
  // about.
  bool CanDecrypt(const sync_pb::EncryptedData& encrypted) const;

  // Encrypts |message| into |encrypted|. Returns true unless encryption fails.
  // Note that encryption will fail if |message| isn't valid (eg. a required
  // field isn't set).
  bool Encrypt(const ::google::protobuf::MessageLite& message,
               sync_pb::EncryptedData* encrypted) const;

  // Decrypts |encrypted| into |message|. Returns true unless decryption fails,
  // or |message| fails to parse the decrypted data.
  bool Decrypt(const sync_pb::EncryptedData& encrypted,
               ::google::protobuf::MessageLite* message) const;

  // Decrypts |encrypted| and returns plaintext decrypted data. If decryption
  // fails, returns empty string.
  std::string DecryptToString(const sync_pb::EncryptedData& encrypted) const;

  // Encrypts the set of currently known keys into |encrypted|. Returns true if
  // successful.
  bool GetKeys(sync_pb::EncryptedData* encrypted) const;

  // Creates a new Nigori instance using |params|. If successful, |params| will
  // become the default encryption key and be used for all future calls to
  // Encrypt.
  bool AddKey(const KeyParams& params);

  // Decrypts |encrypted| and uses its contents to initialize Nigori instances.
  // Returns true unless decryption of |encrypted| fails. The caller is
  // responsible for checking that CanDecrypt(encrypted) == true.
  bool SetKeys(const sync_pb::EncryptedData& encrypted);

  // Makes a local copy of |encrypted| to later be decrypted by
  // DecryptPendingKeys. This should only be used if CanDecrypt(encrypted) ==
  // false.
  void SetPendingKeys(const sync_pb::EncryptedData& encrypted);

  // Attepmts to decrypt the set of keys that was copied in the previous call to
  // SetPendingKeys using |params|. Returns true if the pending keys were
  // successfully decrypted and installed.
  bool DecryptPendingKeys(const KeyParams& params);

  // Returns whether this Cryptographer is ready to encrypt and decrypt data.
  bool is_ready() const { return !nigoris_.empty() && default_nigori_; }

  // Returns whether there is a pending set of keys that needs to be decrypted.
  bool has_pending_keys() const { return NULL != pending_keys_.get(); }

  // Obtain a token that can be provided on construction to a future
  // Cryptographer instance to bootstrap itself.  Returns false if such a token
  // can't be created (i.e. if this Cryptograhper doesn't have valid keys).
  bool GetBootstrapToken(std::string* token) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(CryptographerTest, PackUnpack);
  typedef std::map<std::string, linked_ptr<const Nigori> > NigoriMap;

  // Helper method to instantiate Nigori instances for each set of key
  // parameters in |bag| and setting the default encryption key to
  // |default_key_name|.
  void InstallKeys(const std::string& default_key_name,
                   const sync_pb::NigoriKeyBag& bag);

  bool AddKeyImpl(Nigori* nigori);

  // Functions to serialize + encrypt a Nigori object in an opaque format for
  // persistence by sync infrastructure.
  bool PackBootstrapToken(const Nigori* nigori, std::string* pack_into) const;
  Nigori* UnpackBootstrapToken(const std::string& token) const;

  NigoriMap nigoris_;  // The Nigoris we know about, mapped by key name.
  NigoriMap::value_type* default_nigori_;  // The Nigori used for encryption.

  scoped_ptr<sync_pb::EncryptedData> pending_keys_;

  DISALLOW_COPY_AND_ASSIGN(Cryptographer);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_UTIL_CRYPTOGRAPHER_H_
