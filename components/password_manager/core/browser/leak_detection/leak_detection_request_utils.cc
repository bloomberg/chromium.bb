// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"

#include <string>

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_api.pb.h"
#include "components/password_manager/core/browser/leak_detection/single_lookup_response.h"

namespace password_manager {

using google::internal::identity::passwords::leak::check::v1::
    LookupSingleLeakRequest;

LookupSingleLeakRequest MakeLookupSingleLeakRequest(
    base::StringPiece username,
    base::StringPiece password) {
  // Hash prefix of username "test".
  static constexpr char kTestUsernameHashPrefix[] = {65, -92, -91};

  // Username hash prefix length in bits.
  static constexpr size_t kUsernameHashPrefixLength = 24;

  // Encrypted lookup hash of (username: "test", password: "test") credential.
  static constexpr char kTestEncryptedLookupHash[] = {
      2,    8,    -93, 58,   107, -84, -43, 83,   83,   65, -77,
      47,   -110, -93, 117,  -69, -55, 75,  -114, 39,   10, 9,
      -103, -67,  69,  -117, -18, 11,  37,  -56,  -124, 33, -96};

  LookupSingleLeakRequest request;
  // TODO(crbug.com/086298): Implement correct hash computation of username and
  // password.
  request.set_username_hash_prefix(
      std::string(kTestUsernameHashPrefix, sizeof(kTestUsernameHashPrefix)));
  request.set_username_hash_prefix_length(kUsernameHashPrefixLength);
  request.set_encrypted_lookup_hash(
      std::string(kTestEncryptedLookupHash, sizeof(kTestEncryptedLookupHash)));
  return request;
}

bool ParseLookupSingleLeakResponse(const SingleLookupResponse& response) {
  // TODO(crbug.com/086298): Implement decrypting the response and checking
  // whether the credential was actually leaked.
  DVLOG(0) << "Number of Encrypted Leak Match Prefixes: "
           << response.encrypted_leak_match_prefixes.size();

  base::span<const char> hash = response.reencrypted_lookup_hash;
  DVLOG(0) << "Reencrypted Lookup Hash: "
           << base::HexEncode(base::as_bytes(hash));
  return false;
}

}  // namespace password_manager
