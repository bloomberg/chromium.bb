// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/signature_creator.h"

#include "base/logging.h"

namespace base {

// static
SignatureCreator* SignatureCreator::Create(RSAPrivateKey* key) {
  return NULL;
}

SignatureCreator::SignatureCreator() {
}

SignatureCreator::~SignatureCreator() {
}

bool SignatureCreator::Update(const uint8* data_part, int data_part_len) {
  NOTIMPLEMENTED();
  return false;
}

bool SignatureCreator::Final(std::vector<uint8>* signature) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace base
