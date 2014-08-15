// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRX_FILE_CONSTANTS_H_
#define COMPONENTS_CRX_FILE_CONSTANTS_H_

#include "base/basictypes.h"

namespace crx_file {

// Note: this structure is an ASN.1 which encodes the algorithm used
// with its parameters. This is defined in PKCS #1 v2.1 (RFC 3447).
// It is encoding: { OID sha1WithRSAEncryption      PARAMETERS NULL }
const uint8 kSignatureAlgorithm[15] = {0x30, 0x0d, 0x06, 0x09, 0x2a,
                                       0x86, 0x48, 0x86, 0xf7, 0x0d,
                                       0x01, 0x01, 0x05, 0x05, 0x00};

}  // namespace crx_file

#endif  // COMPONENTS_CRX_FILE_CONSTANTS_H_
