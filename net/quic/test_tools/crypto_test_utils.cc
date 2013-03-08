// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/crypto_test_utils.h"
/*
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_crypto_server_stream.h"
#include "net/quic/quic_crypto_stream.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/simple_quic_framer.h"
#include "net/util/ipaddress.h"

namespace net {
namespace test {

namespace {

class TestSession : public QuicSession {
 public:
  TestSession(QuicConnection* connection, bool is_server)
      : QuicSession(connection, is_server) {
  }

  MOCK_METHOD1(CreateIncomingReliableStream,
               ReliableQuicStream*(QuicStreamId id));
  MOCK_METHOD0(GetCryptoStream, QuicCryptoStream*());
  MOCK_METHOD0(CreateOutgoingReliableStream, ReliableQuicStream*());
};

// CommunicateHandshakeMessages moves messages from |a| to |b| and back until
// |a|'s handshake has completed.
void CommunicateHandshakeMessages(
    PacketSavingConnection* a_conn,
    QuicCryptoStream* a,
    PacketSavingConnection* b_conn,
    QuicCryptoStream* b) {
  scoped_ptr<SimpleQuicFramer> framer;

  for (size_t i = 0; !a->handshake_complete(); i++) {
    framer.reset(new SimpleQuicFramer);

    ASSERT_LT(i, a_conn->packets_.size());
    ASSERT_TRUE(framer->ProcessPacket(*a_conn->packets_[i]));
    ASSERT_EQ(1u, framer->stream_frames().size());

    scoped_ptr<CryptoHandshakeMessage> a_msg(framer->HandshakeMessage(0));
    b->OnHandshakeMessage(*(a_msg.get()));

    framer.reset(new SimpleQuicFramer);
    ASSERT_LT(i, b_conn->packets_.size());
    ASSERT_TRUE(framer->ProcessPacket(*b_conn->packets_[i]));
    ASSERT_EQ(1u, framer->stream_frames().size());

    scoped_ptr<CryptoHandshakeMessage> b_msg(framer->HandshakeMessage(0));
    a->OnHandshakeMessage(*(b_msg.get()));
  }
}

}  // anonymous namespace

// static
void CryptoTestUtils::HandshakeWithFakeServer(
    PacketSavingConnection* client_conn,
    QuicCryptoStream* client) {
  QuicGuid guid(1);
  SocketAddress addr(IPAddress::Loopback4(), 1);
  PacketSavingConnection* server_conn =
      new PacketSavingConnection(guid, addr);
  TestSession server_session(server_conn, true);
  QuicCryptoServerStream server(&server_session);

  // The client's handshake must have been started already.
  CHECK_NE(0u, client_conn->packets_.size());

  CommunicateHandshakeMessages(client_conn, client, server_conn, &server);
}

// static
void CryptoTestUtils::HandshakeWithFakeClient(
    PacketSavingConnection* server_conn,
    QuicCryptoStream* server) {
  QuicGuid guid(1);
  SocketAddress addr(IPAddress::Loopback4(), 1);
  PacketSavingConnection* client_conn =
      new PacketSavingConnection(guid, addr);
  TestSession client_session(client_conn, true);
  QuicCryptoClientStream client(&client_session, "test.example.com");

  CHECK(client.CryptoConnect());
  CHECK_EQ(1u, client_conn->packets_.size());

  CommunicateHandshakeMessages(client_conn, &client, server_conn, server);
}
}  // namespace test
}  // namespace net
*/
