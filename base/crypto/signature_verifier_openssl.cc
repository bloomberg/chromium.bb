// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/signature_verifier.h"

#include "base/logging.h"

namespace base {

SignatureVerifier::SignatureVerifier() {
}

SignatureVerifier::~SignatureVerifier() {
}

bool SignatureVerifier::VerifyInit(const uint8* signature_algorithm,
                                   int signature_algorithm_len,
                                   const uint8* signature,
                                   int signature_len,
                                   const uint8* public_key_info,
                                   int public_key_info_len) {
  NOTIMPLEMENTED();
  return false;
}

void SignatureVerifier::VerifyUpdate(const uint8* data_part,
                                     int data_part_len) {
  NOTIMPLEMENTED();
}

bool SignatureVerifier::VerifyFinal() {
  NOTIMPLEMENTED();
  return false;
}

void SignatureVerifier::Reset() {
  NOTIMPLEMENTED();
}

}  // namespace base
