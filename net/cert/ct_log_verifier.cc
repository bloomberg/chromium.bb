// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_log_verifier.h"

#include "base/logging.h"
#include "net/cert/ct_serialization.h"

namespace net {

// static
scoped_ptr<CTLogVerifier> CTLogVerifier::Create(
    const base::StringPiece& public_key,
    const base::StringPiece& description) {
  scoped_ptr<CTLogVerifier> result(new CTLogVerifier());
  if (!result->Init(public_key, description))
    result.reset();
  return result.Pass();
}

bool CTLogVerifier::Verify(const ct::LogEntry& entry,
                           const ct::SignedCertificateTimestamp& sct) {
  if (sct.log_id != key_id()) {
    DVLOG(1) << "SCT is not signed by this log.";
    return false;
  }

  if (sct.signature.hash_algorithm != hash_algorithm_) {
    DVLOG(1) << "Mismatched hash algorithm. Expected " << hash_algorithm_
             << ", got " << sct.signature.hash_algorithm << ".";
    return false;
  }

  if (sct.signature.signature_algorithm != signature_algorithm_) {
    DVLOG(1) << "Mismatched sig algorithm. Expected " << signature_algorithm_
             << ", got " << sct.signature.signature_algorithm << ".";
    return false;
  }

  std::string serialized_log_entry;
  if (!ct::EncodeLogEntry(entry, &serialized_log_entry)) {
    DVLOG(1) << "Unable to serialize entry.";
    return false;
  }
  std::string serialized_data;
  if (!ct::EncodeV1SCTSignedData(sct.timestamp, serialized_log_entry,
                                 sct.extensions, &serialized_data)) {
    DVLOG(1) << "Unable to create SCT to verify.";
    return false;
  }

  return VerifySignature(serialized_data, sct.signature.signature_data);
}

}  // namespace net
