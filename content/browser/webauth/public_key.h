// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_PUBLIC_KEY_H_
#define CONTENT_BROWSER_WEBAUTH_PUBLIC_KEY_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/macros.h"

namespace content {

// https://www.w3.org/TR/2017/WD-webauthn-20170505/#sec-attestation-data.
class PublicKey {
 public:
  virtual ~PublicKey();

  // The credential public key as a CBOR map according to a format
  // defined per public key type.
  virtual std::vector<uint8_t> EncodeAsCBOR() = 0;

 protected:
  PublicKey(const std::string algorithm);
  const std::string algorithm_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PublicKey);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_PUBLIC_KEY_H_
