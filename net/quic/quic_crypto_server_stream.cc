// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_server_stream.h"

#include "base/base64.h"
#include "crypto/secure_hash.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/crypto_server_config.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/quic_config.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_session.h"

namespace net {

QuicCryptoServerStream::QuicCryptoServerStream(
    const QuicCryptoServerConfig& crypto_config,
    QuicSession* session)
    : QuicCryptoStream(session),
      crypto_config_(crypto_config) {
}

QuicCryptoServerStream::~QuicCryptoServerStream() {
}

void QuicCryptoServerStream::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  // Do not process handshake messages after the handshake is confirmed.
  if (handshake_confirmed_) {
    CloseConnection(QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE);
    return;
  }

  if (message.tag() != kCHLO) {
    CloseConnection(QUIC_INVALID_CRYPTO_MESSAGE_TYPE);
    return;
  }

  string error_details;
  CryptoHandshakeMessage reply;

  QuicErrorCode error = ProcessClientHello(message, &reply, &error_details);

  if (error != QUIC_NO_ERROR) {
    CloseConnectionWithDetails(error, error_details);
    return;
  }

  if (reply.tag() != kSHLO) {
    SendHandshakeMessage(reply);
    return;
  }

  // If we are returning a SHLO then we accepted the handshake.
  QuicConfig* config = session()->config();
  error = config->ProcessClientHello(message, &error_details);
  if (error != QUIC_NO_ERROR) {
    CloseConnectionWithDetails(error, error_details);
    return;
  }

  config->ToHandshakeMessage(&reply);

  // Receiving a full CHLO implies the client is prepared to decrypt with
  // the new server write key.  We can start to encrypt with the new server
  // write key.
  //
  // NOTE: the SHLO will be encrypted with the new server write key.
  session()->connection()->SetEncrypter(
      ENCRYPTION_INITIAL,
      crypto_negotiated_params_.initial_crypters.encrypter.release());
  session()->connection()->SetDefaultEncryptionLevel(
      ENCRYPTION_INITIAL);
  // Set the decrypter immediately so that we no longer accept unencrypted
  // packets.
  session()->connection()->SetDecrypter(
      crypto_negotiated_params_.initial_crypters.decrypter.release());
  SendHandshakeMessage(reply);

  session()->connection()->SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      crypto_negotiated_params_.forward_secure_crypters.encrypter.release());
  session()->connection()->SetDefaultEncryptionLevel(
      ENCRYPTION_FORWARD_SECURE);
  session()->connection()->SetAlternativeDecrypter(
      crypto_negotiated_params_.forward_secure_crypters.decrypter.release(),
      false /* don't latch */);

  encryption_established_ = true;
  handshake_confirmed_ = true;
  session()->OnCryptoHandshakeEvent(QuicSession::HANDSHAKE_CONFIRMED);
}

bool QuicCryptoServerStream::GetBase64SHA256ClientChannelID(
    string* output) const {
  if (!encryption_established_ ||
      crypto_negotiated_params_.channel_id.empty()) {
    return false;
  }

  const string& channel_id(crypto_negotiated_params_.channel_id);
  scoped_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  hash->Update(channel_id.data(), channel_id.size());
  uint8 digest[32];
  hash->Finish(digest, sizeof(digest));

  base::Base64Encode(string(
      reinterpret_cast<const char*>(digest), sizeof(digest)), output);
  // Remove padding.
  size_t len = output->size();
  if (len >= 2) {
    if ((*output)[len - 1] == '=') {
      len--;
      if ((*output)[len - 1] == '=') {
        len--;
      }
      output->resize(len);
    }
  }
  return true;
}

QuicErrorCode QuicCryptoServerStream::ProcessClientHello(
    const CryptoHandshakeMessage& message,
    CryptoHandshakeMessage* reply,
    string* error_details) {
  return crypto_config_.ProcessClientHello(
      message,
      session()->connection()->version(),
      session()->connection()->guid(),
      session()->connection()->peer_address(),
      session()->connection()->clock(),
      session()->connection()->random_generator(),
      &crypto_negotiated_params_, reply, error_details);
}

}  // namespace net
