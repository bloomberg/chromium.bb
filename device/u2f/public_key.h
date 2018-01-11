// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_PUBLIC_KEY_H_
#define DEVICE_U2F_PUBLIC_KEY_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/macros.h"

namespace device {

// https://www.w3.org/TR/2017/WD-webauthn-20170505/#sec-attestation-data.
class PublicKey {
 public:
  virtual ~PublicKey();

  // The credential public key as a COSE_Key map as defined in Section 7
  // of https://tools.ietf.org/html/rfc8152.
  virtual std::vector<uint8_t> EncodeAsCOSEKey() const = 0;

 protected:
  explicit PublicKey(std::string algorithm);

  const std::string algorithm_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PublicKey);
};

}  // namespace device

#endif  // DEVICE_U2F_PUBLIC_KEY_H_
