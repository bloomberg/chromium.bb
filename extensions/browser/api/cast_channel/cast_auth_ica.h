// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_AUTH_ICA_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_AUTH_ICA_H_

#include <stddef.h>

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "net/base/hash_value.h"

namespace extensions {
namespace api {
namespace cast_channel {

typedef std::map<net::SHA256HashValue,
                 base::StringPiece,
                 net::SHA256HashValueLessThan> AuthorityKeysMap;

namespace proto {

// Forward declaration to avoid including generated protobuf header.
class AuthorityKeys;

}  // namespace proto

// AuthorityKeyStore is a helper class that is used to store and manipulate
// intermediate CAs (ICAs) information used to authenticate cast devices.
// A static list of ICAs is hardcoded and may optionally be replaced during
// runtime by an extension supplying a protobuf of ICAs information signed with
// known key.
class AuthorityKeyStore {
 public:
  AuthorityKeyStore();
  ~AuthorityKeyStore();

  // Returns the public key of the ICA whose fingerprint matches |fingerprint|.
  // Returns an empty StringPiece if no such ICA is found.
  // Note: the returned StringPiece is invalidated if Load() is called.
  base::StringPiece GetICAPublicKeyFromFingerprint(
      const net::SHA256HashValue& fingerprint);

  // Returns the public key of the default / original cast ICA.
  // Returns an empty StringPiece if the default cast ICA is not found.
  // Note: the returned StringPiece is invalidated if Load() is called.
  base::StringPiece GetDefaultICAPublicKey();

  // Replaces stored authority keys with the keys loaded from a serialized
  // protobuf.
  bool Load(const std::string& keys);

 private:
  // The map of trusted certificate authorities - fingerprints to public keys.
  AuthorityKeysMap certificate_authorities_;

  // Trusted certificate authorities data passed from the extension.
  scoped_ptr<proto::AuthorityKeys> authority_keys_;

  DISALLOW_COPY_AND_ASSIGN(AuthorityKeyStore);
};

// Sets trusted certificate authorities.
bool SetTrustedCertificateAuthorities(const std::string& keys,
                                      const std::string& signature);

// Gets the trusted ICA entry for the cert represented by |data|.
// Returns the serialized certificate as bytes if the ICA was found.
// Returns an empty-length StringPiece if the ICA was not found.
base::StringPiece GetTrustedICAPublicKey(const base::StringPiece& data);

// Gets the default trusted ICA for legacy compatibility.
base::StringPiece GetDefaultTrustedICAPublicKey();

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_AUTH_ICA_H_
