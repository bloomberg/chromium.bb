// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_CRYPTO_PROTOCOL_H_
#define NET_QUIC_CRYPTO_CRYPTO_PROTOCOL_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "net/base/net_export.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"

namespace net {

// CryptoTag is the type of a tag in the wire protocol.
typedef uint32 CryptoTag;
typedef std::string ServerConfigID;
typedef std::map<CryptoTag, std::string> CryptoTagValueMap;
typedef std::vector<CryptoTag> CryptoTagVector;

const CryptoTag kCHLO = MAKE_TAG('C', 'H', 'L', 'O');  // Client hello
const CryptoTag kSHLO = MAKE_TAG('S', 'H', 'L', 'O');  // Server hello
const CryptoTag kSCFG = MAKE_TAG('S', 'C', 'F', 'G');  // Server config
const CryptoTag kREJ  = MAKE_TAG('R', 'E', 'J', '\0');  // Reject

// Key exchange methods
const CryptoTag kP256 = MAKE_TAG('P', '2', '5', '6');  // ECDH, Curve P-256
const CryptoTag kC255 = MAKE_TAG('C', '2', '5', '5');  // ECDH, Curve25519

// AEAD algorithms
const CryptoTag kNULL = MAKE_TAG('N', 'U', 'L', 'L');  // null algorithm
const CryptoTag kAESG = MAKE_TAG('A', 'E', 'S', 'G');  // AES128 + GCM

// Congestion control feedback types
const CryptoTag kQBIC = MAKE_TAG('Q', 'B', 'I', 'C');  // TCP cubic
const CryptoTag kINAR = MAKE_TAG('I', 'N', 'A', 'R');  // Inter arrival

// Client hello tags
const CryptoTag kVERS = MAKE_TAG('V', 'E', 'R', 'S');  // Version
const CryptoTag kNONC = MAKE_TAG('N', 'O', 'N', 'C');  // The connection nonce
const CryptoTag kSSID = MAKE_TAG('S', 'S', 'I', 'D');  // Session ID
const CryptoTag kKEXS = MAKE_TAG('K', 'E', 'X', 'S');  // Key exchange methods
const CryptoTag kAEAD = MAKE_TAG('A', 'E', 'A', 'D');  // Authenticated
                                                       // encryption algorithms
const CryptoTag kCGST = MAKE_TAG('C', 'G', 'S', 'T');  // Congestion control
                                                       // feedback types
const CryptoTag kICSL = MAKE_TAG('I', 'C', 'S', 'L');  // Idle connection state
                                                       // lifetime
const CryptoTag kKATO = MAKE_TAG('K', 'A', 'T', 'O');  // Keepalive timeout
const CryptoTag kSNI = MAKE_TAG('S', 'N', 'I', '\0');  // Server name
                                                       // indication
const CryptoTag kPUBS = MAKE_TAG('P', 'U', 'B', 'S');  // Public key values
const CryptoTag kSCID = MAKE_TAG('S', 'C', 'I', 'D');  // Server config id
const CryptoTag kSRCT = MAKE_TAG('S', 'R', 'C', 'T');  // Source-address token

const size_t kMaxEntries = 16;  // Max number of entries in a message.

const size_t kNonceSize = 32;  // Size in bytes of the connection nonce.

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CRYPTO_PROTOCOL_H_
