// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_CRYPTO_TEST_UTILS_H_
#define NET_QUIC_TEST_TOOLS_CRYPTO_TEST_UTILS_H_

#include <vector>

#include "base/logging.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"

namespace net {

class ProofSource;
class ProofVerifier;
class QuicClock;
class QuicConfig;
class QuicCryptoClientStream;
class QuicCryptoServerConfig;
class QuicCryptoServerStream;
class QuicCryptoStream;
class QuicRandom;

namespace test {

class PacketSavingConnection;

class CryptoTestUtils {
 public:
  // returns: the number of client hellos that the client sent.
  static int HandshakeWithFakeServer(PacketSavingConnection* client_conn,
                                     QuicCryptoClientStream* client);

  // returns: the number of client hellos that the client sent.
  static int HandshakeWithFakeClient(PacketSavingConnection* server_conn,
                                     QuicCryptoServerStream* server);

  // SetupCryptoServerConfigForTest configures |config| and |crypto_config|
  // with sensible defaults for testing.
  static void SetupCryptoServerConfigForTest(
      const QuicClock* clock,
      QuicRandom* rand,
      QuicConfig* config,
      QuicCryptoServerConfig* crypto_config);

  // CommunicateHandshakeMessages moves messages from |a| to |b| and back until
  // |a|'s handshake has completed.
  static void CommunicateHandshakeMessages(PacketSavingConnection* a_conn,
                                           QuicCryptoStream* a,
                                           PacketSavingConnection* b_conn,
                                           QuicCryptoStream* b);

  // Returns the value for the tag |tag| in the tag value map of |message|.
  static std::string GetValueForTag(const CryptoHandshakeMessage& message,
                                    CryptoTag tag);

  // Returns a |ProofSource| that serves up test certificates.
  static ProofSource* ProofSourceForTesting();

  // Returns a |ProofVerifier| that uses the QUIC testing root CA.
  static ProofVerifier* ProofVerifierForTesting();

 private:
  static void CompareClientAndServerKeys(QuicCryptoClientStream* client,
                                         QuicCryptoServerStream* server);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_CRYPTO_TEST_UTILS_H_
