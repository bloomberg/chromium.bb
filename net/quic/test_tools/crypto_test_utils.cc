// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/crypto_test_utils.h"

#include "net/quic/crypto/common_cert_set.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_server_config.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_crypto_server_stream.h"
#include "net/quic/quic_crypto_stream.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/simple_quic_framer.h"

using base::StringPiece;
using std::string;
using std::vector;

namespace net {
namespace test {

namespace {

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
                 QuicCryptoStream* dest_stream,
                 PacketSavingConnection* dest_conn) {
  SimpleQuicFramer framer;
  CryptoFramer crypto_framer;
  CryptoFramerVisitor crypto_visitor;

  // In order to properly test the code we need to perform encryption and
  // decryption so that the crypters latch when expected. The crypters are in
  // |dest_conn|, but we don't want to try and use them there. Instead we swap
  // them into |framer|, perform the decryption with them, and then swap them
  // back.
  QuicConnectionPeer::SwapCrypters(dest_conn, framer.framer());

  crypto_framer.set_visitor(&crypto_visitor);

  size_t index = *inout_packet_index;
  for (; index < source_conn->encrypted_packets_.size(); index++) {
    ASSERT_TRUE(framer.ProcessPacket(*source_conn->encrypted_packets_[index]));
    for (vector<QuicStreamFrame>::const_iterator
         i =  framer.stream_frames().begin();
         i != framer.stream_frames().end(); ++i) {
      ASSERT_TRUE(crypto_framer.ProcessInput(i->data));
      ASSERT_FALSE(crypto_visitor.error());
    }
  }
  *inout_packet_index = index;

  QuicConnectionPeer::SwapCrypters(dest_conn, framer.framer());

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
    MovePackets(a_conn, &a_i, b, b_conn);

    ASSERT_GT(b_conn->packets_.size(), b_i);
    LOG(INFO) << "Processing " << b_conn->packets_.size() - b_i
              << " packets b->a";
    if (b_conn->packets_.size() - b_i == 2) {
      LOG(INFO) << "here";
    }
    MovePackets(b_conn, &b_i, a, a_conn);
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
  server_session.SetCryptoStream(&server);

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
  client_session.SetCryptoStream(&client);

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
      crypto_config->AddDefaultConfig(
          rand, clock, extra_tags, QuicCryptoServerConfig::kDefaultExpiry));
  if (!config->SetFromHandshakeMessage(*scfg)) {
    CHECK(false) << "Crypto config could not be parsed by QuicConfig.";
  }
}

// static
string CryptoTestUtils::GetValueForTag(const CryptoHandshakeMessage& message,
                                       QuicTag tag) {
  QuicTagValueMap::const_iterator it = message.tag_value_map().find(tag);
  if (it == message.tag_value_map().end()) {
    return string();
  }
  return it->second;
}

class MockCommonCertSets : public CommonCertSets {
 public:
  MockCommonCertSets(StringPiece cert, uint64 hash, uint32 index)
      : cert_(cert.as_string()),
        hash_(hash),
        index_(index) {
  }

  virtual StringPiece GetCommonHashes() const OVERRIDE {
    CHECK(false) << "not implemented";
    return StringPiece();
  }

  virtual StringPiece GetCert(uint64 hash, uint32 index) const OVERRIDE {
    if (hash == hash_ && index == index_) {
      return cert_;
    }
    return StringPiece();
  }

  virtual bool MatchCert(StringPiece cert,
                         StringPiece common_set_hashes,
                         uint64* out_hash,
                         uint32* out_index) const OVERRIDE {
    if (cert != cert_) {
      return false;
    }

    if (common_set_hashes.size() % sizeof(uint64) != 0) {
      return false;
    }
    bool client_has_set = false;
    for (size_t i = 0; i < common_set_hashes.size(); i += sizeof(uint64)) {
      uint64 hash;
      memcpy(&hash, common_set_hashes.data() + i, sizeof(hash));
      if (hash == hash_) {
        client_has_set = true;
        break;
      }
    }

    if (!client_has_set) {
      return false;
    }

    *out_hash = hash_;
    *out_index = index_;
    return true;
  }

 private:
  const string cert_;
  const uint64 hash_;
  const uint32 index_;
};

CommonCertSets* CryptoTestUtils::MockCommonCertSets(StringPiece cert,
                                                   uint64 hash,
                                                   uint32 index) {
  return new class MockCommonCertSets(cert, hash, index);
}

void CryptoTestUtils::CompareClientAndServerKeys(
    QuicCryptoClientStream* client,
    QuicCryptoServerStream* server) {
  const QuicEncrypter* client_encrypter(
      client->session()->connection()->encrypter(ENCRYPTION_INITIAL));
  const QuicDecrypter* client_decrypter(
      client->session()->connection()->decrypter());
  const QuicEncrypter* client_forward_secure_encrypter(
      client->session()->connection()->encrypter(ENCRYPTION_FORWARD_SECURE));
  const QuicDecrypter* client_forward_secure_decrypter(
      client->session()->connection()->alternative_decrypter());
  const QuicEncrypter* server_encrypter(
      server->session()->connection()->encrypter(ENCRYPTION_INITIAL));
  const QuicDecrypter* server_decrypter(
      server->session()->connection()->decrypter());
  const QuicEncrypter* server_forward_secure_encrypter(
      server->session()->connection()->encrypter(ENCRYPTION_FORWARD_SECURE));
  const QuicDecrypter* server_forward_secure_decrypter(
      server->session()->connection()->alternative_decrypter());

  StringPiece client_encrypter_key = client_encrypter->GetKey();
  StringPiece client_encrypter_iv = client_encrypter->GetNoncePrefix();
  StringPiece client_decrypter_key = client_decrypter->GetKey();
  StringPiece client_decrypter_iv = client_decrypter->GetNoncePrefix();
  StringPiece client_forward_secure_encrypter_key =
      client_forward_secure_encrypter->GetKey();
  StringPiece client_forward_secure_encrypter_iv =
      client_forward_secure_encrypter->GetNoncePrefix();
  StringPiece client_forward_secure_decrypter_key =
      client_forward_secure_decrypter->GetKey();
  StringPiece client_forward_secure_decrypter_iv =
      client_forward_secure_decrypter->GetNoncePrefix();
  StringPiece server_encrypter_key = server_encrypter->GetKey();
  StringPiece server_encrypter_iv = server_encrypter->GetNoncePrefix();
  StringPiece server_decrypter_key = server_decrypter->GetKey();
  StringPiece server_decrypter_iv = server_decrypter->GetNoncePrefix();
  StringPiece server_forward_secure_encrypter_key =
      server_forward_secure_encrypter->GetKey();
  StringPiece server_forward_secure_encrypter_iv =
      server_forward_secure_encrypter->GetNoncePrefix();
  StringPiece server_forward_secure_decrypter_key =
      server_forward_secure_decrypter->GetKey();
  StringPiece server_forward_secure_decrypter_iv =
      server_forward_secure_decrypter->GetNoncePrefix();

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
  CompareCharArraysWithHexError("client forward secure write key",
                                client_forward_secure_encrypter_key.data(),
                                client_forward_secure_encrypter_key.length(),
                                server_forward_secure_decrypter_key.data(),
                                server_forward_secure_decrypter_key.length());
  CompareCharArraysWithHexError("client forward secure write IV",
                                client_forward_secure_encrypter_iv.data(),
                                client_forward_secure_encrypter_iv.length(),
                                server_forward_secure_decrypter_iv.data(),
                                server_forward_secure_decrypter_iv.length());
  CompareCharArraysWithHexError("server forward secure write key",
                                server_forward_secure_encrypter_key.data(),
                                server_forward_secure_encrypter_key.length(),
                                client_forward_secure_decrypter_key.data(),
                                client_forward_secure_decrypter_key.length());
  CompareCharArraysWithHexError("server forward secure write IV",
                                server_forward_secure_encrypter_iv.data(),
                                server_forward_secure_encrypter_iv.length(),
                                client_forward_secure_decrypter_iv.data(),
                                client_forward_secure_decrypter_iv.length());
}
}  // namespace test
}  // namespace net
