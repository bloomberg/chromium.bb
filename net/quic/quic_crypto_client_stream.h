// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_H_
#define NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/quic/crypto/proof_verifier.h"
#include "net/quic/crypto/quic_crypto_client_config.h"
#include "net/quic/quic_config.h"
#include "net/quic/quic_crypto_stream.h"

namespace net {

class ProofVerifyDetails;
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
  virtual bool GetSSLInfo(SSLInfo* ssl_info);

  void OnIOComplete(int result);

 private:
  // ProofVerifierCallbackImpl is passed as the callback method to VerifyProof.
  // The ProofVerifier calls this class with the result of proof verification
  // when verification is performed asynchronously.
  class ProofVerifierCallbackImpl : public ProofVerifierCallback {
   public:
    explicit ProofVerifierCallbackImpl(QuicCryptoClientStream* stream);
    virtual ~ProofVerifierCallbackImpl();

    // ProofVerifierCallback interface.
    virtual void Run(bool ok,
                     const string& error_details,
                     scoped_ptr<ProofVerifyDetails>* details) OVERRIDE;

    // Cancel causes any future callbacks to be ignored. It must be called on
    // the same thread as the callback will be made on.
    void Cancel();

   private:
    QuicCryptoClientStream* stream_;
  };

  friend class test::CryptoTestUtils;
  friend class ProofVerifierCallbackImpl;

  enum State {
    STATE_IDLE,
    STATE_LOAD_QUIC_SERVER_INFO,
    STATE_LOAD_QUIC_SERVER_INFO_COMPLETE,
    STATE_SEND_CHLO,
    STATE_RECV_REJ,
    STATE_VERIFY_PROOF,
    STATE_VERIFY_PROOF_COMPLETE,
    STATE_RECV_SHLO,
  };

  // DoHandshakeLoop performs a step of the handshake state machine. Note that
  // |in| may be NULL if the call did not result from a received message
  void DoHandshakeLoop(const CryptoHandshakeMessage* in);

  // TODO(rtenneti): convert the other states of the state machine into DoXXX
  // functions.

  // Call QuicServerInfo's WaitForDataReady to load the server information from
  // the disk cache.
  int DoLoadQuicServerInfo(QuicCryptoClientConfig::CachedState* cached);
  void DoLoadQuicServerInfoComplete(
      QuicCryptoClientConfig::CachedState* cached);
  // LoadQuicServerInfo is a helper function for DoLoadQuicServerInfoComplete.
  void LoadQuicServerInfo(QuicCryptoClientConfig::CachedState* cached);

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

  // proof_verify_callback_ contains the callback object that we passed to an
  // asynchronous proof verification. The ProofVerifier owns this object.
  ProofVerifierCallbackImpl* proof_verify_callback_;

  // These members are used to store the result of an asynchronous proof
  // verification. These members must not be used after
  // STATE_VERIFY_PROOF_COMPLETE.
  bool verify_ok_;
  string verify_error_details_;
  scoped_ptr<ProofVerifyDetails> verify_details_;

  // The result of certificate verification.
  scoped_ptr<CertVerifyResult> cert_verify_result_;

  // This member is used to store the result of an asynchronous disk cache read.
  // It must not be used after STATE_LOAD_QUIC_SERVER_INFO_COMPLETE.
  int disk_cache_load_result_;

  // Time when call to WaitForDataReady was made, used for computing time spent
  // to load QUIC server information from disk cache.
  base::TimeTicks disk_cache_load_start_time_;

  base::WeakPtrFactory<QuicCryptoClientStream> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuicCryptoClientStream);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CRYPTO_CLIENT_STREAM_H_
