// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_NIGORI_H_
#define CHROME_BROWSER_SYNC_UTIL_NIGORI_H_

#include <string>

#include "base/crypto/symmetric_key.h"
#include "base/scoped_ptr.h"

namespace browser_sync {

// A (partial) implementation of Nigori, a protocol to securely store secrets in
// the cloud. This implementation does not support server authentication or
// assisted key derivation.
//
// To store secrets securely, use the |Permute| method to derive a lookup name
// for your secret (basically a map key), and |Encrypt| and |Decrypt| to store
// and retrieve the secret.
//
// TODO: Link to doc.
class Nigori {
 public:
  enum Type {
    Password = 1,
  };

  // Creates a Nigori client for communicating with |hostname|. Note that
  // |hostname| is used to derive the keys used to encrypt and decrypt data.
  explicit Nigori(const std::string& hostname);
  virtual ~Nigori();

  // Initialize the client with the supplied |username| and |password|.
  bool Init(const std::string& username, const std::string& password);

  // Derives a secure lookup name from |type| and |name|. If |hostname|,
  // |username| and |password| are kept constant, a given |type| and |name| pair
  // always yields the same |permuted| value. Note that |permuted| will be
  // Base64 encoded.
  bool Permute(Type type, const std::string& name, std::string* permuted) const;

  // Encrypts |value|. Note that on success, |encrypted| will be Base64
  // encoded.
  bool Encrypt(const std::string& value, std::string* encrypted) const;

  // Decrypts |value| into |decrypted|. It is assumed that |value| is Base64
  // encoded.
  bool Decrypt(const std::string& value, std::string* decrypted) const;

  // The next three getters return the parameters used to initialize the keys.
  // Given the hostname, username and password, another Nigori object capable of
  // encrypting and decrypting the same data as this one could be initialized.
  const std::string& hostname() const { return hostname_; }
  const std::string& username() const { return username_; }
  const std::string& password() const { return password_; }

  static const char kSaltSalt[];  // The salt used to derive the user salt.
  static const size_t kSaltKeySizeInBits = 128;
  static const size_t kDerivedKeySizeInBits = 128;
  static const size_t kIvSize = 16;
  static const size_t kHashSize = 32;

  static const size_t kSaltIterations = 1001;
  static const size_t kUserIterations = 1002;
  static const size_t kEncryptionIterations = 1003;
  static const size_t kSigningIterations = 1004;

 private:
  std::string hostname_;
  std::string username_;
  std::string password_;

  scoped_ptr<base::SymmetricKey> user_key_;
  scoped_ptr<base::SymmetricKey> encryption_key_;
  scoped_ptr<base::SymmetricKey> mac_key_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_UTIL_NIGORI_H_
