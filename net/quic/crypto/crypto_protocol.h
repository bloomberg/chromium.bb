// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_CRYPTO_PROTOCOL_H_
#define NET_QUIC_CRYPTO_CRYPTO_PROTOCOL_H_

#include <map>
#include <string>
#include <vector>

#include "net/base/net_export.h"
#include "net/quic/quic_protocol.h"

// Version and Crypto tags are written to the wire with a big-endian
// representation of the name of the tag.  For example
// the client hello tag (CHLO) will be written as the
// following 4 bytes: 'C' 'H' 'L' 'O'.  Since it is
// stored in memory as a little endian uint32, we need
// to reverse the order of the bytes.
//
// We use a macro to ensure that no static initialisers are created. Use the
// MakeQuicTag function in normal code.
#define TAG(a, b, c, d) ((d << 24) + (c << 16) + (b << 8) + a)

namespace net {

typedef std::string ServerConfigID;
typedef std::map<QuicTag, std::string> QuicTagValueMap;

const QuicTag kCHLO = TAG('C', 'H', 'L', 'O');  // Client hello
const QuicTag kSHLO = TAG('S', 'H', 'L', 'O');  // Server hello
const QuicTag kSCFG = TAG('S', 'C', 'F', 'G');  // Server config
const QuicTag kREJ  = TAG('R', 'E', 'J', '\0'); // Reject
const QuicTag kCETV = TAG('C', 'E', 'T', 'V');  // Client encrypted tag-value
                                                // pairs

// Key exchange methods
const QuicTag kP256 = TAG('P', '2', '5', '6');  // ECDH, Curve P-256
const QuicTag kC255 = TAG('C', '2', '5', '5');  // ECDH, Curve25519

// AEAD algorithms
const QuicTag kNULL = TAG('N', 'U', 'L', 'L');  // null algorithm
const QuicTag kAESG = TAG('A', 'E', 'S', 'G');  // AES128 + GCM-12

// Congestion control feedback types
const QuicTag kQBIC = TAG('Q', 'B', 'I', 'C');  // TCP cubic
const QuicTag kINAR = TAG('I', 'N', 'A', 'R');  // Inter arrival

// Proof types (i.e. certificate types)
// NOTE: although it would be silly to do so, specifying both kX509 and kX59R
// is allowed and is equivalent to specifying only kX509.
const QuicTag kX509 = TAG('X', '5', '0', '9');  // X.509 certificate, all key
                                                // types
const QuicTag kX59R = TAG('X', '5', '9', 'R');  // X.509 certificate, RSA keys
                                                // only
const QuicTag kCHID = TAG('C', 'H', 'I', 'D');  // Channel ID.

// Client hello tags
const QuicTag kVERS = TAG('V', 'E', 'R', 'S');  // Version
const QuicTag kNONC = TAG('N', 'O', 'N', 'C');  // The client's nonce
const QuicTag kSSID = TAG('S', 'S', 'I', 'D');  // Session ID
const QuicTag kKEXS = TAG('K', 'E', 'X', 'S');  // Key exchange methods
const QuicTag kAEAD = TAG('A', 'E', 'A', 'D');  // Authenticated
                                                // encryption algorithms
const QuicTag kCGST = TAG('C', 'G', 'S', 'T');  // Congestion control
                                                // feedback types
const QuicTag kICSL = TAG('I', 'C', 'S', 'L');  // Idle connection state
                                                // lifetime
const QuicTag kKATO = TAG('K', 'A', 'T', 'O');  // Keepalive timeout
const QuicTag kMSPC = TAG('M', 'S', 'P', 'C');  // Max streams per connection.
const QuicTag kSNI  = TAG('S', 'N', 'I', '\0'); // Server name
                                                // indication
const QuicTag kPUBS = TAG('P', 'U', 'B', 'S');  // Public key values
const QuicTag kSCID = TAG('S', 'C', 'I', 'D');  // Server config id
const QuicTag kORBT = TAG('O', 'B', 'I', 'T');  // Server orbit.
const QuicTag kPDMD = TAG('P', 'D', 'M', 'D');  // Proof demand.
const QuicTag kPROF = TAG('P', 'R', 'O', 'F');  // Proof (signature).
const QuicTag kCCS  = TAG('C', 'C', 'S', 0);    // Common certificate set
const QuicTag kCCRT = TAG('C', 'C', 'R', 'T');  // Cached certificate
const QuicTag kEXPY = TAG('E', 'X', 'P', 'Y');  // Expiry

// CETV tags
const QuicTag kCIDK = TAG('C', 'I', 'D', 'K');  // ChannelID key
const QuicTag kCIDS = TAG('C', 'I', 'D', 'S');  // ChannelID signature

// Universal tags
const QuicTag kPAD  = TAG('P', 'A', 'D', '\0'); // Padding

// These tags have a special form so that they appear either at the beginning
// or the end of a handshake message. Since handshake messages are sorted by
// tag value, the tags with 0 at the end will sort first and those with 255 at
// the end will sort last.
//
// The certificate chain should have a tag that will cause it to be sorted at
// the end of any handshake messages because it's likely to be large and the
// client might be able to get everything that it needs from the small values at
// the beginning.
//
// Likewise tags with random values should be towards the beginning of the
// message because the server mightn't hold state for a rejected client hello
// and therefore the client may have issues reassembling the rejection message
// in the event that it sent two client hellos.
const QuicTag kServerNonceTag =
    TAG('S', 'N', 'O', 0);  // The server's nonce
const QuicTag kSourceAddressTokenTag =
    TAG('S', 'T', 'K', 0);  // Source-address token
const QuicTag kCertificateTag =
    TAG('C', 'R', 'T', 255);  // Certificate chain

#undef TAG

const size_t kMaxEntries = 128;  // Max number of entries in a message.

const size_t kNonceSize = 32;  // Size in bytes of the connection nonce.

const size_t kOrbitSize = 8;  // Number of bytes in an orbit value.

// kProofSignatureLabel is prepended to server configs before signing to avoid
// any cross-protocol attacks on the signature.
const char kProofSignatureLabel[] = "QUIC server config signature";

// kClientHelloMinimumSize is the minimum size of a client hello. Client hellos
// will have PAD tags added in order to ensure this minimum is met and client
// hellos smaller than this will be an error. This minimum size reduces the
// amplification factor of any mirror DoS attack.
const size_t kClientHelloMinimumSize = 512;

}  // namespace net

#endif  // NET_QUIC_CRYPTO_CRYPTO_PROTOCOL_H_
