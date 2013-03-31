// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/crypto_test_utils.h"

#include "base/string_piece.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_crypto_server_stream.h"
#include "net/quic/quic_crypto_stream.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/simple_quic_framer.h"

using base::StringPiece;
using std::string;

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
    QuicCryptoClientStream* client) {
  QuicGuid guid(1);
  IPAddressNumber ip;
  CHECK(ParseIPLiteralToNumber("192.0.2.33", &ip));
  IPEndPoint addr = IPEndPoint(ip, 1);
  PacketSavingConnection* server_conn =
      new PacketSavingConnection(guid, addr, true);
  TestSession server_session(server_conn, true);
  QuicCryptoServerStream server(&server_session);

  // The client's handshake must have been started already.
  CHECK_NE(0u, client_conn->packets_.size());

  CommunicateHandshakeMessages(client_conn, client, server_conn, &server);

  CompareClientAndServerKeys(client, &server);
}

// static
void CryptoTestUtils::HandshakeWithFakeClient(
    PacketSavingConnection* server_conn,
    QuicCryptoServerStream* server) {
  QuicGuid guid(1);
  IPAddressNumber ip;
  CHECK(ParseIPLiteralToNumber("192.0.2.33", &ip));
  IPEndPoint addr = IPEndPoint(ip, 1);
  PacketSavingConnection* client_conn =
      new PacketSavingConnection(guid, addr, false);
  TestSession client_session(client_conn, true);
  QuicCryptoClientStream client(&client_session, "test.example.com");

  CHECK(client.CryptoConnect());
  CHECK_EQ(1u, client_conn->packets_.size());

  CommunicateHandshakeMessages(client_conn, &client, server_conn, server);

  CompareClientAndServerKeys(&client, server);
}

// static
string CryptoTestUtils::GetValueForTag(const CryptoHandshakeMessage& message,
                                       CryptoTag tag) {
  CryptoTagValueMap::const_iterator it = message.tag_value_map().find(tag);
  if (it == message.tag_value_map().end()) {
    return string();
  }
  return it->second;
}

void CryptoTestUtils::CompareClientAndServerKeys(
    QuicCryptoClientStream* client,
    QuicCryptoServerStream* server) {
  StringPiece client_encrypter_key =
      client->session()->connection()->encrypter()->GetKey();
  StringPiece client_encrypter_iv =
      client->session()->connection()->encrypter()->GetNoncePrefix();
  StringPiece client_decrypter_key =
      client->session()->connection()->decrypter()->GetKey();
  StringPiece client_decrypter_iv =
      client->session()->connection()->decrypter()->GetNoncePrefix();
  StringPiece server_encrypter_key =
      server->session()->connection()->encrypter()->GetKey();
  StringPiece server_encrypter_iv =
      server->session()->connection()->encrypter()->GetNoncePrefix();
  StringPiece server_decrypter_key =
      server->session()->connection()->decrypter()->GetKey();
  StringPiece server_decrypter_iv =
      server->session()->connection()->decrypter()->GetNoncePrefix();
  CompareCharArraysWithHexError("client write key",
                                client_encrypter_key.data(),
                                client_encrypter_key.length(),
                                server_decrypter_key.data(),
                                server_decrypter_key.length());
  CompareCharArraysWithHexError("client write IV",
                                client_encrypter_iv.data(),
                                client_encrypter_iv.length(),
                                server_decrypter_iv.data(),
                                server_decrypter_iv.length());
  CompareCharArraysWithHexError("server write key",
                                server_encrypter_key.data(),
                                server_encrypter_key.length(),
                                client_decrypter_key.data(),
                                client_decrypter_key.length());
  CompareCharArraysWithHexError("server write IV",
                                server_encrypter_iv.data(),
                                server_encrypter_iv.length(),
                                client_decrypter_iv.data(),
                                client_decrypter_iv.length());
}
}  // namespace test
}  // namespace net
