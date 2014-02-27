// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_client_stream.h"

#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/crypto/proof_verifier.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/crypto/quic_server_info.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_session.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"

namespace net {

namespace {

// Copies CertVerifyResult from |verify_details| to |cert_verify_result|.
void CopyCertVerifyResult(
    const ProofVerifyDetails* verify_details,
    scoped_ptr<CertVerifyResult>* cert_verify_result) {
  const CertVerifyResult* cert_verify_result_other =
      &(reinterpret_cast<const ProofVerifyDetailsChromium*>(
          verify_details))->cert_verify_result;
  CertVerifyResult* result_copy = new CertVerifyResult;
  result_copy->CopyFrom(*cert_verify_result_other);
  cert_verify_result->reset(result_copy);
}

}  // namespace

QuicCryptoClientStream::ProofVerifierCallbackImpl::ProofVerifierCallbackImpl(
    QuicCryptoClientStream* stream)
    : stream_(stream) {}

QuicCryptoClientStream::ProofVerifierCallbackImpl::
    ~ProofVerifierCallbackImpl() {}

void QuicCryptoClientStream::ProofVerifierCallbackImpl::Run(
    bool ok,
    const string& error_details,
    scoped_ptr<ProofVerifyDetails>* details) {
  if (stream_ == NULL) {
    return;
  }

  stream_->verify_ok_ = ok;
  stream_->verify_error_details_ = error_details;
  stream_->verify_details_.reset(details->release());
  stream_->proof_verify_callback_ = NULL;
  stream_->DoHandshakeLoop(NULL);

  // The ProofVerifier owns this object and will delete it when this method
  // returns.
}

void QuicCryptoClientStream::ProofVerifierCallbackImpl::Cancel() {
  stream_ = NULL;
}


QuicCryptoClientStream::QuicCryptoClientStream(
    const string& server_hostname,
    QuicSession* session,
    QuicCryptoClientConfig* crypto_config)
    : QuicCryptoStream(session),
      next_state_(STATE_IDLE),
      num_client_hellos_(0),
      crypto_config_(crypto_config),
      server_hostname_(server_hostname),
      generation_counter_(0),
      proof_verify_callback_(NULL),
      disk_cache_load_result_(ERR_UNEXPECTED) {
}

QuicCryptoClientStream::~QuicCryptoClientStream() {
  if (proof_verify_callback_) {
    proof_verify_callback_->Cancel();
  }
}

void QuicCryptoClientStream::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  QuicCryptoStream::OnHandshakeMessage(message);

  DoHandshakeLoop(&message);
}

bool QuicCryptoClientStream::CryptoConnect() {
  next_state_ = STATE_LOAD_QUIC_SERVER_INFO;
  DoHandshakeLoop(NULL);
  return true;
}

int QuicCryptoClientStream::num_sent_client_hellos() const {
  return num_client_hellos_;
}

// TODO(rtenneti): Add unittests for GetSSLInfo which exercise the various ways
// we learn about SSL info (sync vs async vs cached).
bool QuicCryptoClientStream::GetSSLInfo(SSLInfo* ssl_info) {
  ssl_info->Reset();
  if (!cert_verify_result_) {
    return false;
  }

  ssl_info->cert_status = cert_verify_result_->cert_status;
  ssl_info->cert = cert_verify_result_->verified_cert;

  // TODO(rtenneti): Figure out what to set for the following.
  // Temporarily hard coded cipher_suite as 0xc031 to represent
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 (from
  // net/ssl/ssl_cipher_suite_names.cc) and encryption as 256.
  int cipher_suite = 0xc02f;
  int ssl_connection_status = 0;
  ssl_connection_status |=
      (cipher_suite & SSL_CONNECTION_CIPHERSUITE_MASK) <<
       SSL_CONNECTION_CIPHERSUITE_SHIFT;
  ssl_connection_status |=
      (SSL_CONNECTION_VERSION_TLS1_2 & SSL_CONNECTION_VERSION_MASK) <<
       SSL_CONNECTION_VERSION_SHIFT;

  ssl_info->public_key_hashes = cert_verify_result_->public_key_hashes;
  ssl_info->is_issued_by_known_root =
      cert_verify_result_->is_issued_by_known_root;

  ssl_info->connection_status = ssl_connection_status;
  ssl_info->client_cert_sent = false;
  ssl_info->channel_id_sent = false;
  ssl_info->security_bits = 256;
  ssl_info->handshake_type = SSLInfo::HANDSHAKE_FULL;
  return true;
}

// kMaxClientHellos is the maximum number of times that we'll send a client
// hello. The value 3 accounts for:
//   * One failure due to an incorrect or missing source-address token.
//   * One failure due the server's certificate chain being unavailible and the
//     server being unwilling to send it without a valid source-address token.
static const int kMaxClientHellos = 3;

void QuicCryptoClientStream::DoHandshakeLoop(
    const CryptoHandshakeMessage* in) {
  CryptoHandshakeMessage out;
  QuicErrorCode error;
  string error_details;
  QuicCryptoClientConfig::CachedState* cached =
      crypto_config_->LookupOrCreate(server_hostname_);

  if (in != NULL) {
    DVLOG(1) << "Client: Received " << in->DebugString();
  }

  for (;;) {
    const State state = next_state_;
    next_state_ = STATE_IDLE;
    switch (state) {
      case STATE_LOAD_QUIC_SERVER_INFO: {
        if (DoLoadQuicServerInfo(cached) == ERR_IO_PENDING) {
          return;
        }
        break;
      }
      case STATE_LOAD_QUIC_SERVER_INFO_COMPLETE: {
        DoLoadQuicServerInfoComplete(cached);
        break;
      }
      case STATE_SEND_CHLO: {
        // Send the client hello in plaintext.
        session()->connection()->SetDefaultEncryptionLevel(ENCRYPTION_NONE);
        if (num_client_hellos_ > kMaxClientHellos) {
          CloseConnection(QUIC_CRYPTO_TOO_MANY_REJECTS);
          return;
        }
        num_client_hellos_++;

        if (!cached->IsComplete(session()->connection()->clock()->WallNow())) {
          crypto_config_->FillInchoateClientHello(
              server_hostname_,
              session()->connection()->supported_versions().front(),
              cached, &crypto_negotiated_params_, &out);
          // Pad the inchoate client hello to fill up a packet.
          const size_t kFramingOverhead = 50;  // A rough estimate.
          const size_t max_packet_size =
              session()->connection()->options()->max_packet_length;
          if (max_packet_size <= kFramingOverhead) {
            DLOG(DFATAL) << "max_packet_length (" << max_packet_size
                         << ") has no room for framing overhead.";
            CloseConnection(QUIC_INTERNAL_ERROR);
            return;
          }
          if (kClientHelloMinimumSize > max_packet_size - kFramingOverhead) {
            DLOG(DFATAL) << "Client hello won't fit in a single packet.";
            CloseConnection(QUIC_INTERNAL_ERROR);
            return;
          }
          out.set_minimum_size(max_packet_size - kFramingOverhead);
          next_state_ = STATE_RECV_REJ;
          DVLOG(1) << "Client: Sending " << out.DebugString();
          SendHandshakeMessage(out);
          return;
        }
        session()->config()->ToHandshakeMessage(&out);
        error = crypto_config_->FillClientHello(
            server_hostname_,
            session()->connection()->connection_id(),
            session()->connection()->supported_versions().front(),
            cached,
            session()->connection()->clock()->WallNow(),
            session()->connection()->random_generator(),
            &crypto_negotiated_params_,
            &out,
            &error_details);
        if (error != QUIC_NO_ERROR) {
          // Flush the cached config so that, if it's bad, the server has a
          // chance to send us another in the future.
          cached->InvalidateServerConfig();
          CloseConnectionWithDetails(error, error_details);
          return;
        }
        if (cached->proof_verify_details()) {
          CopyCertVerifyResult(cached->proof_verify_details(),
                               &cert_verify_result_);
        } else {
          cert_verify_result_.reset();
        }
        next_state_ = STATE_RECV_SHLO;
        DVLOG(1) << "Client: Sending " << out.DebugString();
        SendHandshakeMessage(out);
        // Be prepared to decrypt with the new server write key.
        session()->connection()->SetAlternativeDecrypter(
            crypto_negotiated_params_.initial_crypters.decrypter.release(),
            true /* latch once used */);
        // Send subsequent packets under encryption on the assumption that the
        // server will accept the handshake.
        session()->connection()->SetEncrypter(
            ENCRYPTION_INITIAL,
            crypto_negotiated_params_.initial_crypters.encrypter.release());
        session()->connection()->SetDefaultEncryptionLevel(
            ENCRYPTION_INITIAL);
        if (!encryption_established_) {
          encryption_established_ = true;
          session()->OnCryptoHandshakeEvent(
              QuicSession::ENCRYPTION_FIRST_ESTABLISHED);
        } else {
          session()->OnCryptoHandshakeEvent(
              QuicSession::ENCRYPTION_REESTABLISHED);
        }
        return;
      }
      case STATE_RECV_REJ:
        // We sent a dummy CHLO because we didn't have enough information to
        // perform a handshake, or we sent a full hello that the server
        // rejected. Here we hope to have a REJ that contains the information
        // that we need.
        if (in->tag() != kREJ) {
          CloseConnectionWithDetails(QUIC_INVALID_CRYPTO_MESSAGE_TYPE,
                                     "Expected REJ");
          return;
        }
        error = crypto_config_->ProcessRejection(
            *in, session()->connection()->clock()->WallNow(), cached,
            &crypto_negotiated_params_, &error_details);
        if (error != QUIC_NO_ERROR) {
          CloseConnectionWithDetails(error, error_details);
          return;
        }
        if (!cached->proof_valid()) {
          ProofVerifier* verifier = crypto_config_->proof_verifier();
          if (!verifier) {
            // If no verifier is set then we don't check the certificates.
            cached->SetProofValid();
          } else if (!cached->signature().empty()) {
            next_state_ = STATE_VERIFY_PROOF;
            break;
          }
        }
        next_state_ = STATE_SEND_CHLO;
        break;
      case STATE_VERIFY_PROOF: {
        ProofVerifier* verifier = crypto_config_->proof_verifier();
        DCHECK(verifier);
        next_state_ = STATE_VERIFY_PROOF_COMPLETE;
        generation_counter_ = cached->generation_counter();

        ProofVerifierCallbackImpl* proof_verify_callback =
            new ProofVerifierCallbackImpl(this);

        verify_ok_ = false;

        ProofVerifier::Status status = verifier->VerifyProof(
            server_hostname_,
            cached->server_config(),
            cached->certs(),
            cached->signature(),
            &verify_error_details_,
            &verify_details_,
            proof_verify_callback);

        switch (status) {
          case ProofVerifier::PENDING:
            proof_verify_callback_ = proof_verify_callback;
            DVLOG(1) << "Doing VerifyProof";
            return;
          case ProofVerifier::FAILURE:
            break;
          case ProofVerifier::SUCCESS:
            verify_ok_ = true;
            break;
        }
        break;
      }
      case STATE_VERIFY_PROOF_COMPLETE:
        if (!verify_ok_) {
          CopyCertVerifyResult(verify_details_.get(), &cert_verify_result_);
          CloseConnectionWithDetails(
              QUIC_PROOF_INVALID, "Proof invalid: " + verify_error_details_);
          return;
        }
        // Check if generation_counter has changed between STATE_VERIFY_PROOF
        // and STATE_VERIFY_PROOF_COMPLETE state changes.
        if (generation_counter_ != cached->generation_counter()) {
          next_state_ = STATE_VERIFY_PROOF;
        } else {
          cached->SetProofValid();
          cached->SetProofVerifyDetails(verify_details_.release());
          next_state_ = STATE_SEND_CHLO;
        }
        break;
      case STATE_RECV_SHLO: {
        // We sent a CHLO that we expected to be accepted and now we're hoping
        // for a SHLO from the server to confirm that.
        if (in->tag() == kREJ) {
          // alternative_decrypter will be NULL if the original alternative
          // decrypter latched and became the primary decrypter. That happens
          // if we received a message encrypted with the INITIAL key.
          if (session()->connection()->alternative_decrypter() == NULL) {
            // The rejection was sent encrypted!
            CloseConnectionWithDetails(QUIC_CRYPTO_ENCRYPTION_LEVEL_INCORRECT,
                                       "encrypted REJ message");
            return;
          }
          next_state_ = STATE_RECV_REJ;
          break;
        }
        if (in->tag() != kSHLO) {
          CloseConnectionWithDetails(QUIC_INVALID_CRYPTO_MESSAGE_TYPE,
                                     "Expected SHLO or REJ");
          return;
        }
        // alternative_decrypter will be NULL if the original alternative
        // decrypter latched and became the primary decrypter. That happens
        // if we received a message encrypted with the INITIAL key.
        if (session()->connection()->alternative_decrypter() != NULL) {
          // The server hello was sent without encryption.
          CloseConnectionWithDetails(QUIC_CRYPTO_ENCRYPTION_LEVEL_INCORRECT,
                                     "unencrypted SHLO message");
          return;
        }
        error = crypto_config_->ProcessServerHello(
            *in, session()->connection()->connection_id(),
            session()->connection()->server_supported_versions(),
            cached, &crypto_negotiated_params_, &error_details);

        if (error != QUIC_NO_ERROR) {
          CloseConnectionWithDetails(
              error, "Server hello invalid: " + error_details);
          return;
        }
        error = session()->config()->ProcessServerHello(*in, &error_details);
        if (error != QUIC_NO_ERROR) {
          CloseConnectionWithDetails(
              error, "Server hello invalid: " + error_details);
          return;
        }
        session()->OnConfigNegotiated();

        CrypterPair* crypters =
            &crypto_negotiated_params_.forward_secure_crypters;
        // TODO(agl): we don't currently latch this decrypter because the idea
        // has been floated that the server shouldn't send packets encrypted
        // with the FORWARD_SECURE key until it receives a FORWARD_SECURE
        // packet from the client.
        session()->connection()->SetAlternativeDecrypter(
            crypters->decrypter.release(), false /* don't latch */);
        session()->connection()->SetEncrypter(
            ENCRYPTION_FORWARD_SECURE, crypters->encrypter.release());
        session()->connection()->SetDefaultEncryptionLevel(
            ENCRYPTION_FORWARD_SECURE);

        handshake_confirmed_ = true;
        session()->OnCryptoHandshakeEvent(QuicSession::HANDSHAKE_CONFIRMED);
        return;
      }
      case STATE_IDLE:
        // This means that the peer sent us a message that we weren't expecting.
        CloseConnection(QUIC_INVALID_CRYPTO_MESSAGE_TYPE);
        return;
    }
  }
}

void QuicCryptoClientStream::OnIOComplete(int result) {
  DCHECK_EQ(STATE_LOAD_QUIC_SERVER_INFO_COMPLETE, next_state_);
  DCHECK_NE(ERR_IO_PENDING, result);
  disk_cache_load_result_ = result;
  DoHandshakeLoop(NULL);
}

int QuicCryptoClientStream::DoLoadQuicServerInfo(
    QuicCryptoClientConfig::CachedState* cached) {
  next_state_ = STATE_SEND_CHLO;
  QuicServerInfo* quic_server_info = cached->quic_server_info();
  if (!quic_server_info) {
    return OK;
  }

  generation_counter_ = cached->generation_counter();
  next_state_ = STATE_LOAD_QUIC_SERVER_INFO_COMPLETE;

  // TODO(rtenneti): If multiple tabs load the same URL, all requests except for
  // the first request send InchoateClientHello. Fix the code to handle multiple
  // requests. A possible solution is to wait for the first request to finish
  // and use the data from the disk cache for all requests.
  // We may need to call quic_server_info->Persist later.
  // quic_server_info->Persist requires quic_server_info to be ready, so we
  // always call WaitForDataReady, even though we might have initialized
  // |cached| config from the cached state for a canonical hostname.
  int rv = quic_server_info->WaitForDataReady(
      base::Bind(&QuicCryptoClientStream::OnIOComplete,
                 base::Unretained(this)));

  if (rv != ERR_IO_PENDING) {
    disk_cache_load_result_ = rv;
  }
  return rv;
}

void QuicCryptoClientStream::DoLoadQuicServerInfoComplete(
    QuicCryptoClientConfig::CachedState* cached) {
  LoadQuicServerInfo(cached);
  QuicServerInfo::State* state = cached->quic_server_info()->mutable_state();
  state->Clear();
}

void QuicCryptoClientStream::LoadQuicServerInfo(
    QuicCryptoClientConfig::CachedState* cached) {
  next_state_ = STATE_SEND_CHLO;

  // If someone else already saved a server config, we don't want to overwrite
  // it. Also, if someone else saved a server config and then cleared it (so
  // cached->IsEmpty() is true), we still want to load from QuicServerInfo.
  if (!cached->IsEmpty()) {
    return;
  }

  if (disk_cache_load_result_ != OK || !cached->LoadQuicServerInfo(
          session()->connection()->clock()->WallNow())) {
    // It is ok to proceed to STATE_SEND_CHLO when we cannot load QuicServerInfo
    // from the disk cache.
    DCHECK(cached->IsEmpty());
    DVLOG(1) << "Empty server_config";
    return;
  }

  ProofVerifier* verifier = crypto_config_->proof_verifier();
  if (!verifier) {
    // If no verifier is set then we don't check the certificates.
    cached->SetProofValid();
  } else if (!cached->signature().empty()) {
    next_state_ = STATE_VERIFY_PROOF;
  }
}

}  // namespace net
