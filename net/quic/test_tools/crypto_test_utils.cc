// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/crypto_test_utils.h"

#include "base/strings/string_piece.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_server_config.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_crypto_server_stream.h"
#include "net/quic/quic_crypto_stream.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/simple_quic_framer.h"

using base::StringPiece;
using std::string;
using std::vector;

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

// CryptoFramerVisitor is a framer visitor that records handshake messages.
class CryptoFramerVisitor : public CryptoFramerVisitorInterface {
 public:
  CryptoFramerVisitor()
      : error_(false) {
  }

  virtual void OnError(CryptoFramer* framer) OVERRIDE {
    error_ = true;
  }

  virtual void OnHandshakeMessage(
      const CryptoHandshakeMessage& message) OVERRIDE {
    messages_.push_back(message);
  }

  bool error() const {
    return error_;
  }

  const vector<CryptoHandshakeMessage>& messages() const {
    return messages_;
  }

 private:
  bool error_;
  vector<CryptoHandshakeMessage> messages_;
};

// MovePackets parses crypto handshake messages from packet number
// |*inout_packet_index| through to the last packet and has |dest_stream|
// process them. |*inout_packet_index| is updated with an index one greater
// than the last packet processed.
void MovePackets(PacketSavingConnection* source_conn,
                 size_t *inout_packet_index,
                 QuicCryptoStream* dest_stream) {
  SimpleQuicFramer framer;
  CryptoFramer crypto_framer;
  CryptoFramerVisitor crypto_visitor;

  crypto_framer.set_visitor(&crypto_visitor);

  size_t index = *inout_packet_index;
  for (; index < source_conn->packets_.size(); index++) {
    ASSERT_TRUE(framer.ProcessPacket(*source_conn->packets_[index]));
    for (vector<QuicStreamFrame>::const_iterator
         i =  framer.stream_frames().begin();
         i != framer.stream_frames().end(); ++i) {
      ASSERT_TRUE(crypto_framer.ProcessInput(i->data));
      ASSERT_FALSE(crypto_visitor.error());
    }
  }
  *inout_packet_index = index;

  ASSERT_EQ(0u, crypto_framer.InputBytesRemaining());

  for (vector<CryptoHandshakeMessage>::const_iterator
       i = crypto_visitor.messages().begin();
       i != crypto_visitor.messages().end(); ++i) {
    dest_stream->OnHandshakeMessage(*i);
  }
}

}  // anonymous namespace

// static
void CryptoTestUtils::CommunicateHandshakeMessages(
    PacketSavingConnection* a_conn,
    QuicCryptoStream* a,
    PacketSavingConnection* b_conn,
    QuicCryptoStream* b) {
  size_t a_i = 0, b_i = 0;
  while (!a->handshake_confirmed()) {
    ASSERT_GT(a_conn->packets_.size(), a_i);
    LOG(INFO) << "Processing " << a_conn->packets_.size() - a_i
              << " packets a->b";
    MovePackets(a_conn, &a_i, b);

    ASSERT_GT(b_conn->packets_.size(), b_i);
    LOG(INFO) << "Processing " << b_conn->packets_.size() - b_i
              << " packets b->a";
    if (b_conn->packets_.size() - b_i == 2) {
      LOG(INFO) << "here";
    }
    MovePackets(b_conn, &b_i, a);
  }
}

// static
int CryptoTestUtils::HandshakeWithFakeServer(
    PacketSavingConnection* client_conn,
    QuicCryptoClientStream* client) {
  QuicGuid guid(1);
  IPAddressNumber ip;
  CHECK(ParseIPLiteralToNumber("192.0.2.33", &ip));
  IPEndPoint addr = IPEndPoint(ip, 1);
  PacketSavingConnection* server_conn =
      new PacketSavingConnection(guid, addr, true);
  TestSession server_session(server_conn, true);

  QuicConfig config;
  QuicCryptoServerConfig crypto_config(QuicCryptoServerConfig::TESTING);
  SetupCryptoServerConfigForTest(
      server_session.connection()->clock(),
      server_session.connection()->random_generator(),
      &config, &crypto_config);

  QuicCryptoServerStream server(config, crypto_config, &server_session);

  // The client's handshake must have been started already.
  CHECK_NE(0u, client_conn->packets_.size());

  CommunicateHandshakeMessages(client_conn, client, server_conn, &server);

  CompareClientAndServerKeys(client, &server);

  return client->num_sent_client_hellos();
}

// static
int CryptoTestUtils::HandshakeWithFakeClient(
    PacketSavingConnection* server_conn,
    QuicCryptoServerStream* server) {
  QuicGuid guid(1);
  IPAddressNumber ip;
  CHECK(ParseIPLiteralToNumber("192.0.2.33", &ip));
  IPEndPoint addr = IPEndPoint(ip, 1);
  PacketSavingConnection* client_conn =
      new PacketSavingConnection(guid, addr, false);
  TestSession client_session(client_conn, true);
  QuicConfig config;
  QuicCryptoClientConfig crypto_config;

  config.SetDefaults();
  crypto_config.SetDefaults();
  // TODO(rtenneti): Enable testing of ProofVerifier.
  // crypto_config.SetProofVerifier(ProofVerifierForTesting());
  QuicCryptoClientStream client("test.example.com", config, &client_session,
                                &crypto_config);

  CHECK(client.CryptoConnect());
  CHECK_EQ(1u, client_conn->packets_.size());

  CommunicateHandshakeMessages(client_conn, &client, server_conn, server);

  CompareClientAndServerKeys(&client, server);

  return client.num_sent_client_hellos();
}

// static
void CryptoTestUtils::SetupCryptoServerConfigForTest(
    const QuicClock* clock,
    QuicRandom* rand,
    QuicConfig* config,
    QuicCryptoServerConfig* crypto_config) {
  config->SetDefaults();
  CryptoHandshakeMessage extra_tags;
  config->ToHandshakeMessage(&extra_tags);

  scoped_ptr<CryptoHandshakeMessage> scfg(
      crypto_config->AddDefaultConfig(rand, clock, extra_tags));
  if (!config->SetFromHandshakeMessage(*scfg)) {
    CHECK(false) << "Crypto config could not be parsed by QuicConfig.";
  }
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
  const QuicEncrypter* client_encrypter(
      client->session()->connection()->encrypter(ENCRYPTION_INITIAL));
  // Normally we would expect the client's INITIAL decrypter to have latched
  // from the receipt of the server hello. However, when using a
  // PacketSavingConnection (at the tests do) we don't actually encrypt with
  // the correct encrypter.
  // TODO(agl): make the tests more realistic.
  const QuicDecrypter* client_decrypter(
      client->session()->connection()->alternative_decrypter());
  const QuicEncrypter* server_encrypter(
      server->session()->connection()->encrypter(ENCRYPTION_INITIAL));
  const QuicDecrypter* server_decrypter(
      server->session()->connection()->decrypter());

  StringPiece client_encrypter_key = client_encrypter->GetKey();
  StringPiece client_encrypter_iv = client_encrypter->GetNoncePrefix();
  StringPiece client_decrypter_key = client_decrypter->GetKey();
  StringPiece client_decrypter_iv = client_decrypter->GetNoncePrefix();
  StringPiece server_encrypter_key = server_encrypter->GetKey();
  StringPiece server_encrypter_iv = server_encrypter->GetNoncePrefix();
  StringPiece server_decrypter_key = server_decrypter->GetKey();
  StringPiece server_decrypter_iv = server_decrypter->GetNoncePrefix();

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
