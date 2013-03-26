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

class QuicCryptoClientStream;
class QuicCryptoServerStream;

namespace test {

class PacketSavingConnection;

class CryptoTestUtils {
 public:
  static void HandshakeWithFakeServer(PacketSavingConnection* client_conn,
                                      QuicCryptoClientStream* client);

  static void HandshakeWithFakeClient(PacketSavingConnection* server_conn,
                                      QuicCryptoServerStream* server);

  // Returns the value for the tag |tag| in the tag value map of |message|.
  static std::string GetValueForTag(const CryptoHandshakeMessage& message,
                                    CryptoTag tag);

 private:
  static void CompareClientAndServerKeys(QuicCryptoClientStream* client,
                                         QuicCryptoServerStream* server);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_CRYPTO_TEST_UTILS_H_
