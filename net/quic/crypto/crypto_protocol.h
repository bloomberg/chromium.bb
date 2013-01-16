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
#include "net/quic/quic_time.h"

namespace net {

typedef uint32 CryptoTag;
typedef std::map<CryptoTag, std::string> CryptoTagValueMap;
typedef std::vector<CryptoTag> CryptoTagVector;
struct NET_EXPORT_PRIVATE CryptoHandshakeMessage {
  CryptoHandshakeMessage();
  ~CryptoHandshakeMessage();
  CryptoTag tag;
  CryptoTagValueMap tag_value_map;
};

// Crypto tags are written to the wire with a big-endian
// representation of the name of the tag.  For example
// the client hello tag (CHLO) will be written as the
// following 4 bytes: 'C' 'H' 'L' 'O'.  Since it is
// stored in memory as a little endian uint32, we need
// to reverse the order of the bytes.
#define MAKE_TAG(a, b, c, d) (d << 24) + (c << 16) + (b << 8) + a

const CryptoTag kCHLO = MAKE_TAG('C', 'H', 'L', 'O');  // Client hello
const CryptoTag kSHLO = MAKE_TAG('S', 'H', 'L', 'O');  // Server hello

// Key exchange methods
const CryptoTag kP256 = MAKE_TAG('P', '2', '5', '6');  // ECDH, Curve P-256
const CryptoTag kC255 = MAKE_TAG('C', '2', '5', '5');  // ECDH, Curve25519

// AEAD algorithms
const CryptoTag kNULL = MAKE_TAG('N', 'U', 'L', 'L');  // null algorithm
const CryptoTag kAESH = MAKE_TAG('A', 'E', 'S', 'H');  // AES128 + SHA256
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

const size_t kMaxEntries = 16;  // Max number of entries in a message.

const size_t kNonceSize = 32;  // Size in bytes of the connection nonce.

// Client-side crypto configuration settings.
struct NET_EXPORT_PRIVATE QuicClientCryptoConfig {
  // Initializes the members to 0 or empty values.
  QuicClientCryptoConfig();
  ~QuicClientCryptoConfig();

  // Sets the members to default values.
  void SetDefaults();

  // Protocol version
  uint16 version;
  // Key exchange methods
  CryptoTagVector key_exchange;
  // Authenticated encryption with associated data (AEAD) algorithms
  CryptoTagVector aead;
  // Congestion control feedback types
  CryptoTagVector congestion_control;
  // Idle connection state lifetime
  QuicTime::Delta idle_connection_state_lifetime;
  // Keepalive timeout, or 0 to turn off keepalive probes
  QuicTime::Delta keepalive_timeout;
  // Server's hostname
  std::string server_hostname;
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CRYPTO_PROTOCOL_H_
