// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_VERIFY_SIGNED_DATA_H_
#define NET_CERT_INTERNAL_VERIFY_SIGNED_DATA_H_

#include "base/compiler_specific.h"
#include "net/base/net_export.h"

namespace net {

namespace der {
class Input;
}  // namespace der

class SignatureAlgorithm;

// Verifies that |signature_value| is a valid signature of |signed_data| using
// the algorithm |signature_algorithm| and the public key |public_key|.
//
//   |signature_algorithm| - The parsed AlgorithmIdentifier
//   |signed_data| - The blob of data to verify
//   |signature_value_bit_string| - The DER-encoded BIT STRING representing the
//       signature's value (to be interpreted according to the signature
//       algorithm).
//   |public_key| - A DER-encoded SubjectPublicKeyInfo.
//
// Returns true if verification was successful.
NET_EXPORT bool VerifySignedData(const SignatureAlgorithm& signature_algorithm,
                                 const der::Input& signed_data,
                                 const der::Input& signature_value_bit_string,
                                 const der::Input& public_key)
    WARN_UNUSED_RESULT;

}  // namespace net

#endif  // NET_CERT_INTERNAL_VERIFY_SIGNED_DATA_H_
