// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_H_
#define NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_H_

#include <string>

#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/quic_config.h"
#include "net/quic/quic_crypto_stream.h"

namespace net {

class QuicSession;
class SSLInfo;

namespace test {
class CryptoTestUtils;
}  // namespace test

class NET_EXPORT_PRIVATE QuicCryptoClientStream : public QuicCryptoStream {
 public:
  QuicCryptoClientStream(const string& server_hostname,
                         QuicSession* session,
                         QuicCryptoClientConfig* crypto_config);
  virtual ~QuicCryptoClientStream();

  // CryptoFramerVisitorInterface implementation
  virtual void OnHandshakeMessage(
      const CryptoHandshakeMessage& message) OVERRIDE;

  // Performs a crypto handshake with the server. Returns true if the crypto
  // handshake is started successfully.
  // TODO(agl): this should probably return void.
  virtual bool CryptoConnect();

  // num_sent_client_hellos returns the number of client hello messages that
  // have been sent. If the handshake has completed then this is one greater
  // than the number of round-trips needed for the handshake.
  int num_sent_client_hellos() const;

  // Gets the SSL connection information.
  bool GetSSLInfo(SSLInfo* ssl_info);

 private:
  friend class test::CryptoTestUtils;

  enum State {
    STATE_IDLE,
    STATE_SEND_CHLO,
    STATE_RECV_REJ,
    STATE_VERIFY_PROOF,
    STATE_VERIFY_PROOF_COMPLETE,
    STATE_RECV_SHLO,
  };

  // DoHandshakeLoop performs a step of the handshake state machine. Note that
  // |in| is NULL for the first call. OnVerifyProofComplete passes the |result|
  // it has received from VerifyProof call (from all other places |result| is
  // set to OK).
  void DoHandshakeLoop(const CryptoHandshakeMessage* in, int result);

  // OnVerifyProofComplete is passed as the callback method to VerifyProof.
  // ProofVerifier calls this method with the result of proof verification when
  // verification is performed asynchronously.
  void OnVerifyProofComplete(int result);

  base::WeakPtrFactory<QuicCryptoClientStream> weak_factory_;

  State next_state_;
  // num_client_hellos_ contains the number of client hello messages that this
  // connection has sent.
  int num_client_hellos_;

  QuicCryptoClientConfig* const crypto_config_;

  // Client's connection nonce (4-byte timestamp + 28 random bytes)
  std::string nonce_;
  // Server's hostname
  std::string server_hostname_;

  // Generation counter from QuicCryptoClientConfig's CachedState.
  uint64 generation_counter_;

  // The result of certificate verification.
  // TODO(rtenneti): should we change CertVerifyResult to be
  // RefCountedThreadSafe object to avoid copying.
  CertVerifyResult cert_verify_result_;

  // Error details for ProofVerifier's VerifyProof call.
  std::string error_details_;

  DISALLOW_COPY_AND_ASSIGN(QuicCryptoClientStream);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_H_
