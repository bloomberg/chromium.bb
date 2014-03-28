// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// OpenSSL binding for SSLClientSocket. The class layout and general principle
// of operation is derived from SSLClientSocketNSS.

#include "net/socket/ssl_client_socket_openssl.h"

#include <openssl/err.h>
#include <openssl/opensslv.h>
#include <openssl/ssl.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/debug/alias.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/synchronization/lock.h"
#include "crypto/ec_private_key.h"
#include "crypto/openssl_util.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/single_request_cert_verifier.h"
#include "net/cert/x509_certificate_net_log_param.h"
#include "net/socket/ssl_error_params.h"
#include "net/socket/ssl_session_cache_openssl.h"
#include "net/ssl/openssl_client_key_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"

namespace net {

namespace {

// Enable this to see logging for state machine state transitions.
#if 0
#define GotoState(s) do { DVLOG(2) << (void *)this << " " << __FUNCTION__ << \
                           " jump to state " << s; \
                           next_handshake_state_ = s; } while (0)
#else
#define GotoState(s) next_handshake_state_ = s
#endif

// This constant can be any non-negative/non-zero value (eg: it does not
// overlap with any value of the net::Error range, including net::OK).
const int kNoPendingReadResult = 1;

// If a client doesn't have a list of protocols that it supports, but
// the server supports NPN, choosing "http/1.1" is the best answer.
const char kDefaultSupportedNPNProtocol[] = "http/1.1";

#if OPENSSL_VERSION_NUMBER < 0x1000103fL
// This method doesn't seem to have made it into the OpenSSL headers.
unsigned long SSL_CIPHER_get_id(const SSL_CIPHER* cipher) { return cipher->id; }
#endif

// Used for encoding the |connection_status| field of an SSLInfo object.
int EncodeSSLConnectionStatus(int cipher_suite,
                              int compression,
                              int version) {
  return ((cipher_suite & SSL_CONNECTION_CIPHERSUITE_MASK) <<
          SSL_CONNECTION_CIPHERSUITE_SHIFT) |
         ((compression & SSL_CONNECTION_COMPRESSION_MASK) <<
          SSL_CONNECTION_COMPRESSION_SHIFT) |
         ((version & SSL_CONNECTION_VERSION_MASK) <<
          SSL_CONNECTION_VERSION_SHIFT);
}

// Returns the net SSL version number (see ssl_connection_status_flags.h) for
// this SSL connection.
int GetNetSSLVersion(SSL* ssl) {
  switch (SSL_version(ssl)) {
    case SSL2_VERSION:
      return SSL_CONNECTION_VERSION_SSL2;
    case SSL3_VERSION:
      return SSL_CONNECTION_VERSION_SSL3;
    case TLS1_VERSION:
      return SSL_CONNECTION_VERSION_TLS1;
    case 0x0302:
      return SSL_CONNECTION_VERSION_TLS1_1;
    case 0x0303:
      return SSL_CONNECTION_VERSION_TLS1_2;
    default:
      return SSL_CONNECTION_VERSION_UNKNOWN;
  }
}

int MapOpenSSLErrorSSL() {
  // Walk down the error stack to find the SSLerr generated reason.
  unsigned long error_code;
  do {
    error_code = ERR_get_error();
    if (error_code == 0)
      return ERR_SSL_PROTOCOL_ERROR;
  } while (ERR_GET_LIB(error_code) != ERR_LIB_SSL);

  DVLOG(1) << "OpenSSL SSL error, reason: " << ERR_GET_REASON(error_code)
           << ", name: " << ERR_error_string(error_code, NULL);
  switch (ERR_GET_REASON(error_code)) {
    case SSL_R_READ_TIMEOUT_EXPIRED:
      return ERR_TIMED_OUT;
    case SSL_R_BAD_RESPONSE_ARGUMENT:
      return ERR_INVALID_ARGUMENT;
    case SSL_R_UNKNOWN_CERTIFICATE_TYPE:
    case SSL_R_UNKNOWN_CIPHER_TYPE:
    case SSL_R_UNKNOWN_KEY_EXCHANGE_TYPE:
    case SSL_R_UNKNOWN_PKEY_TYPE:
    case SSL_R_UNKNOWN_REMOTE_ERROR_TYPE:
    case SSL_R_UNKNOWN_SSL_VERSION:
      return ERR_NOT_IMPLEMENTED;
    case SSL_R_UNSUPPORTED_SSL_VERSION:
    case SSL_R_NO_CIPHER_MATCH:
    case SSL_R_NO_SHARED_CIPHER:
    case SSL_R_TLSV1_ALERT_INSUFFICIENT_SECURITY:
    case SSL_R_TLSV1_ALERT_PROTOCOL_VERSION:
    case SSL_R_UNSUPPORTED_PROTOCOL:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
    case SSL_R_SSLV3_ALERT_BAD_CERTIFICATE:
    case SSL_R_SSLV3_ALERT_UNSUPPORTED_CERTIFICATE:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_REVOKED:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_EXPIRED:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_UNKNOWN:
    case SSL_R_TLSV1_ALERT_ACCESS_DENIED:
    case SSL_R_TLSV1_ALERT_UNKNOWN_CA:
      return ERR_BAD_SSL_CLIENT_AUTH_CERT;
    case SSL_R_BAD_DECOMPRESSION:
    case SSL_R_SSLV3_ALERT_DECOMPRESSION_FAILURE:
      return ERR_SSL_DECOMPRESSION_FAILURE_ALERT;
    case SSL_R_SSLV3_ALERT_BAD_RECORD_MAC:
      return ERR_SSL_BAD_RECORD_MAC_ALERT;
    case SSL_R_TLSV1_ALERT_DECRYPT_ERROR:
      return ERR_SSL_DECRYPT_ERROR_ALERT;
    case SSL_R_TLSV1_UNRECOGNIZED_NAME:
      return ERR_SSL_UNRECOGNIZED_NAME_ALERT;
    case SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED:
      return ERR_SSL_UNSAFE_NEGOTIATION;
    case SSL_R_WRONG_NUMBER_OF_KEY_BITS:
      return ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY;
    // SSL_R_UNKNOWN_PROTOCOL is reported if premature application data is
    // received (see http://crbug.com/42538), and also if all the protocol
    // versions supported by the server were disabled in this socket instance.
    // Mapped to ERR_SSL_PROTOCOL_ERROR for compatibility with other SSL sockets
    // in the former scenario.
    case SSL_R_UNKNOWN_PROTOCOL:
    case SSL_R_SSL_HANDSHAKE_FAILURE:
    case SSL_R_DECRYPTION_FAILED:
    case SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC:
    case SSL_R_DH_PUBLIC_VALUE_LENGTH_IS_WRONG:
    case SSL_R_DIGEST_CHECK_FAILED:
    case SSL_R_DUPLICATE_COMPRESSION_ID:
    case SSL_R_ECGROUP_TOO_LARGE_FOR_CIPHER:
    case SSL_R_ENCRYPTED_LENGTH_TOO_LONG:
    case SSL_R_ERROR_IN_RECEIVED_CIPHER_LIST:
    case SSL_R_EXCESSIVE_MESSAGE_SIZE:
    case SSL_R_EXTRA_DATA_IN_MESSAGE:
    case SSL_R_GOT_A_FIN_BEFORE_A_CCS:
    case SSL_R_ILLEGAL_PADDING:
    case SSL_R_INVALID_CHALLENGE_LENGTH:
    case SSL_R_INVALID_COMMAND:
    case SSL_R_INVALID_PURPOSE:
    case SSL_R_INVALID_STATUS_RESPONSE:
    case SSL_R_INVALID_TICKET_KEYS_LENGTH:
    case SSL_R_KEY_ARG_TOO_LONG:
    case SSL_R_READ_WRONG_PACKET_TYPE:
    // SSL_do_handshake reports this error when the server responds to a
    // ClientHello with a fatal close_notify alert.
    case SSL_AD_REASON_OFFSET + SSL_AD_CLOSE_NOTIFY:
    case SSL_R_SSLV3_ALERT_UNEXPECTED_MESSAGE:
    // TODO(joth): SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE may be returned from the
    // server after receiving ClientHello if there's no common supported cipher.
    // Ideally we'd map that specific case to ERR_SSL_VERSION_OR_CIPHER_MISMATCH
    // to match the NSS implementation. See also http://goo.gl/oMtZW
    case SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE:
    case SSL_R_SSLV3_ALERT_NO_CERTIFICATE:
    case SSL_R_SSLV3_ALERT_ILLEGAL_PARAMETER:
    case SSL_R_TLSV1_ALERT_DECODE_ERROR:
    case SSL_R_TLSV1_ALERT_DECRYPTION_FAILED:
    case SSL_R_TLSV1_ALERT_EXPORT_RESTRICTION:
    case SSL_R_TLSV1_ALERT_INTERNAL_ERROR:
    case SSL_R_TLSV1_ALERT_NO_RENEGOTIATION:
    case SSL_R_TLSV1_ALERT_RECORD_OVERFLOW:
    case SSL_R_TLSV1_ALERT_USER_CANCELLED:
      return ERR_SSL_PROTOCOL_ERROR;
    case SSL_R_CERTIFICATE_VERIFY_FAILED:
      // The only way that the certificate verify callback can fail is if
      // the leaf certificate changed during a renegotiation.
      return ERR_SSL_SERVER_CERT_CHANGED;
    default:
      LOG(WARNING) << "Unmapped error reason: " << ERR_GET_REASON(error_code);
      return ERR_FAILED;
  }
}

// Converts an OpenSSL error code into a net error code, walking the OpenSSL
// error stack if needed. Note that |tracer| is not currently used in the
// implementation, but is passed in anyway as this ensures the caller will clear
// any residual codes left on the error stack.
int MapOpenSSLError(int err, const crypto::OpenSSLErrStackTracer& tracer) {
  switch (err) {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      return ERR_IO_PENDING;
    case SSL_ERROR_SYSCALL:
      LOG(ERROR) << "OpenSSL SYSCALL error, earliest error code in "
                    "error queue: " << ERR_peek_error() << ", errno: "
                 << errno;
      return ERR_SSL_PROTOCOL_ERROR;
    case SSL_ERROR_SSL:
      return MapOpenSSLErrorSSL();
    default:
      // TODO(joth): Implement full mapping.
      LOG(WARNING) << "Unknown OpenSSL error " << err;
      return ERR_SSL_PROTOCOL_ERROR;
  }
}

// Utility to construct the appropriate set & clear masks for use the OpenSSL
// options and mode configuration functions. (SSL_set_options etc)
struct SslSetClearMask {
  SslSetClearMask() : set_mask(0), clear_mask(0) {}
  void ConfigureFlag(long flag, bool state) {
    (state ? set_mask : clear_mask) |= flag;
    // Make sure we haven't got any intersection in the set & clear options.
    DCHECK_EQ(0, set_mask & clear_mask) << flag << ":" << state;
  }
  long set_mask;
  long clear_mask;
};

// Compute a unique key string for the SSL session cache. |socket| is an
// input socket object. Return a string.
std::string GetSocketSessionCacheKey(const SSLClientSocketOpenSSL& socket) {
  std::string result = socket.host_and_port().ToString();
  result.append("/");
  result.append(socket.ssl_session_cache_shard());
  return result;
}

}  // namespace

class SSLClientSocketOpenSSL::SSLContext {
 public:
  static SSLContext* GetInstance() { return Singleton<SSLContext>::get(); }
  SSL_CTX* ssl_ctx() { return ssl_ctx_.get(); }
  SSLSessionCacheOpenSSL* session_cache() { return &session_cache_; }

  SSLClientSocketOpenSSL* GetClientSocketFromSSL(const SSL* ssl) {
    DCHECK(ssl);
    SSLClientSocketOpenSSL* socket = static_cast<SSLClientSocketOpenSSL*>(
        SSL_get_ex_data(ssl, ssl_socket_data_index_));
    DCHECK(socket);
    return socket;
  }

  bool SetClientSocketForSSL(SSL* ssl, SSLClientSocketOpenSSL* socket) {
    return SSL_set_ex_data(ssl, ssl_socket_data_index_, socket) != 0;
  }

 private:
  friend struct DefaultSingletonTraits<SSLContext>;

  SSLContext() {
    crypto::EnsureOpenSSLInit();
    ssl_socket_data_index_ = SSL_get_ex_new_index(0, 0, 0, 0, 0);
    DCHECK_NE(ssl_socket_data_index_, -1);
    ssl_ctx_.reset(SSL_CTX_new(SSLv23_client_method()));
    session_cache_.Reset(ssl_ctx_.get(), kDefaultSessionCacheConfig);
    SSL_CTX_set_cert_verify_callback(ssl_ctx_.get(), CertVerifyCallback, NULL);
    SSL_CTX_set_client_cert_cb(ssl_ctx_.get(), ClientCertCallback);
    SSL_CTX_set_channel_id_cb(ssl_ctx_.get(), ChannelIDCallback);
    SSL_CTX_set_verify(ssl_ctx_.get(), SSL_VERIFY_PEER, NULL);
#if defined(OPENSSL_NPN_NEGOTIATED)
    // TODO(kristianm): Only select this if ssl_config_.next_proto is not empty.
    // It would be better if the callback were not a global setting,
    // but that is an OpenSSL issue.
    SSL_CTX_set_next_proto_select_cb(ssl_ctx_.get(), SelectNextProtoCallback,
                                     NULL);
#endif
  }

  static std::string GetSessionCacheKey(const SSL* ssl) {
    SSLClientSocketOpenSSL* socket = GetInstance()->GetClientSocketFromSSL(ssl);
    DCHECK(socket);
    return GetSocketSessionCacheKey(*socket);
  }

  static SSLSessionCacheOpenSSL::Config kDefaultSessionCacheConfig;

  static int ClientCertCallback(SSL* ssl, X509** x509, EVP_PKEY** pkey) {
    SSLClientSocketOpenSSL* socket = GetInstance()->GetClientSocketFromSSL(ssl);
    CHECK(socket);
    return socket->ClientCertRequestCallback(ssl, x509, pkey);
  }

  static void ChannelIDCallback(SSL* ssl, EVP_PKEY** pkey) {
    SSLClientSocketOpenSSL* socket = GetInstance()->GetClientSocketFromSSL(ssl);
    CHECK(socket);
    socket->ChannelIDRequestCallback(ssl, pkey);
  }

  static int CertVerifyCallback(X509_STORE_CTX *store_ctx, void *arg) {
    SSL* ssl = reinterpret_cast<SSL*>(X509_STORE_CTX_get_ex_data(
        store_ctx, SSL_get_ex_data_X509_STORE_CTX_idx()));
    SSLClientSocketOpenSSL* socket = GetInstance()->GetClientSocketFromSSL(ssl);
    CHECK(socket);

    return socket->CertVerifyCallback(store_ctx);
  }

  static int SelectNextProtoCallback(SSL* ssl,
                                     unsigned char** out, unsigned char* outlen,
                                     const unsigned char* in,
                                     unsigned int inlen, void* arg) {
    SSLClientSocketOpenSSL* socket = GetInstance()->GetClientSocketFromSSL(ssl);
    return socket->SelectNextProtoCallback(out, outlen, in, inlen);
  }

  // This is the index used with SSL_get_ex_data to retrieve the owner
  // SSLClientSocketOpenSSL object from an SSL instance.
  int ssl_socket_data_index_;

  crypto::ScopedOpenSSL<SSL_CTX, SSL_CTX_free> ssl_ctx_;
  // |session_cache_| must be destroyed before |ssl_ctx_|.
  SSLSessionCacheOpenSSL session_cache_;
};

// PeerCertificateChain is a helper object which extracts the certificate
// chain, as given by the server, from an OpenSSL socket and performs the needed
// resource management. The first element of the chain is the leaf certificate
// and the other elements are in the order given by the server.
class SSLClientSocketOpenSSL::PeerCertificateChain {
 public:
  explicit PeerCertificateChain(STACK_OF(X509)* chain) { Reset(chain); }
  PeerCertificateChain(const PeerCertificateChain& other) { *this = other; }
  ~PeerCertificateChain() {}
  PeerCertificateChain& operator=(const PeerCertificateChain& other);

  // Resets the PeerCertificateChain to the set of certificates in|chain|,
  // which may be NULL, indicating to empty the store certificates.
  // Note: If an error occurs, such as being unable to parse the certificates,
  // this will behave as if Reset(NULL) was called.
  void Reset(STACK_OF(X509)* chain);

  // Note that when USE_OPENSSL is defined, OSCertHandle is X509*
  const scoped_refptr<X509Certificate>& AsOSChain() const { return os_chain_; }

  size_t size() const {
    if (!openssl_chain_.get())
      return 0;
    return sk_X509_num(openssl_chain_.get());
  }

  X509* operator[](size_t index) const {
    DCHECK_LT(index, size());
    return sk_X509_value(openssl_chain_.get(), index);
  }

  bool IsValid() { return os_chain_.get() && openssl_chain_.get(); }

 private:
  static void FreeX509Stack(STACK_OF(X509)* cert_chain) {
    sk_X509_pop_free(cert_chain, X509_free);
  }

  friend class crypto::ScopedOpenSSL<STACK_OF(X509), FreeX509Stack>;

  crypto::ScopedOpenSSL<STACK_OF(X509), FreeX509Stack> openssl_chain_;

  scoped_refptr<X509Certificate> os_chain_;
};

SSLClientSocketOpenSSL::PeerCertificateChain&
SSLClientSocketOpenSSL::PeerCertificateChain::operator=(
    const PeerCertificateChain& other) {
  if (this == &other)
    return *this;

  // os_chain_ is reference counted by scoped_refptr;
  os_chain_ = other.os_chain_;

  // Must increase the reference count manually for sk_X509_dup
  openssl_chain_.reset(sk_X509_dup(other.openssl_chain_.get()));
  for (int i = 0; i < sk_X509_num(openssl_chain_.get()); ++i) {
    X509* x = sk_X509_value(openssl_chain_.get(), i);
    CRYPTO_add(&x->references, 1, CRYPTO_LOCK_X509);
  }
  return *this;
}

#if defined(USE_OPENSSL_CERTS)
// When OSCertHandle is typedef'ed to X509, this implementation does a short cut
// to avoid converting back and forth between der and X509 struct.
void SSLClientSocketOpenSSL::PeerCertificateChain::Reset(
    STACK_OF(X509)* chain) {
  openssl_chain_.reset(NULL);
  os_chain_ = NULL;

  if (!chain)
    return;

  X509Certificate::OSCertHandles intermediates;
  for (int i = 1; i < sk_X509_num(chain); ++i)
    intermediates.push_back(sk_X509_value(chain, i));

  os_chain_ =
      X509Certificate::CreateFromHandle(sk_X509_value(chain, 0), intermediates);

  // sk_X509_dup does not increase reference count on the certs in the stack.
  openssl_chain_.reset(sk_X509_dup(chain));

  std::vector<base::StringPiece> der_chain;
  for (int i = 0; i < sk_X509_num(openssl_chain_.get()); ++i) {
    X509* x = sk_X509_value(openssl_chain_.get(), i);
    // Increase the reference count for the certs in openssl_chain_.
    CRYPTO_add(&x->references, 1, CRYPTO_LOCK_X509);
  }
}
#else  // !defined(USE_OPENSSL_CERTS)
void SSLClientSocketOpenSSL::PeerCertificateChain::Reset(
    STACK_OF(X509)* chain) {
  openssl_chain_.reset(NULL);
  os_chain_ = NULL;

  if (!chain)
    return;

  // sk_X509_dup does not increase reference count on the certs in the stack.
  openssl_chain_.reset(sk_X509_dup(chain));

  std::vector<base::StringPiece> der_chain;
  for (int i = 0; i < sk_X509_num(openssl_chain_.get()); ++i) {
    X509* x = sk_X509_value(openssl_chain_.get(), i);

    // Increase the reference count for the certs in openssl_chain_.
    CRYPTO_add(&x->references, 1, CRYPTO_LOCK_X509);

    unsigned char* cert_data = NULL;
    int cert_data_length = i2d_X509(x, &cert_data);
    if (cert_data_length && cert_data)
      der_chain.push_back(base::StringPiece(reinterpret_cast<char*>(cert_data),
                                            cert_data_length));
  }

  os_chain_ = X509Certificate::CreateFromDERCertChain(der_chain);

  for (size_t i = 0; i < der_chain.size(); ++i) {
    OPENSSL_free(const_cast<char*>(der_chain[i].data()));
  }

  if (der_chain.size() !=
      static_cast<size_t>(sk_X509_num(openssl_chain_.get()))) {
    openssl_chain_.reset(NULL);
    os_chain_ = NULL;
  }
}
#endif  // defined(USE_OPENSSL_CERTS)

// static
SSLSessionCacheOpenSSL::Config
    SSLClientSocketOpenSSL::SSLContext::kDefaultSessionCacheConfig = {
        &GetSessionCacheKey,  // key_func
        1024,                 // max_entries
        256,                  // expiration_check_count
        60 * 60,              // timeout_seconds
};

// static
void SSLClientSocket::ClearSessionCache() {
  SSLClientSocketOpenSSL::SSLContext* context =
      SSLClientSocketOpenSSL::SSLContext::GetInstance();
  context->session_cache()->Flush();
#if defined(USE_OPENSSL_CERTS)
  OpenSSLClientKeyStore::GetInstance()->Flush();
#endif
}

SSLClientSocketOpenSSL::SSLClientSocketOpenSSL(
    scoped_ptr<ClientSocketHandle> transport_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    const SSLClientSocketContext& context)
    : transport_send_busy_(false),
      transport_recv_busy_(false),
      transport_recv_eof_(false),
      weak_factory_(this),
      pending_read_error_(kNoPendingReadResult),
      transport_write_error_(OK),
      server_cert_chain_(new PeerCertificateChain(NULL)),
      completed_handshake_(false),
      was_ever_used_(false),
      client_auth_cert_needed_(false),
      cert_verifier_(context.cert_verifier),
      server_bound_cert_service_(context.server_bound_cert_service),
      ssl_(NULL),
      transport_bio_(NULL),
      transport_(transport_socket.Pass()),
      host_and_port_(host_and_port),
      ssl_config_(ssl_config),
      ssl_session_cache_shard_(context.ssl_session_cache_shard),
      trying_cached_session_(false),
      next_handshake_state_(STATE_NONE),
      npn_status_(kNextProtoUnsupported),
      channel_id_request_return_value_(ERR_UNEXPECTED),
      channel_id_xtn_negotiated_(false),
      net_log_(transport_->socket()->NetLog()) {}

SSLClientSocketOpenSSL::~SSLClientSocketOpenSSL() {
  Disconnect();
}

void SSLClientSocketOpenSSL::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  cert_request_info->host_and_port = host_and_port_;
  cert_request_info->cert_authorities = cert_authorities_;
}

SSLClientSocket::NextProtoStatus SSLClientSocketOpenSSL::GetNextProto(
    std::string* proto, std::string* server_protos) {
  *proto = npn_proto_;
  *server_protos = server_protos_;
  return npn_status_;
}

ServerBoundCertService*
SSLClientSocketOpenSSL::GetServerBoundCertService() const {
  return server_bound_cert_service_;
}

int SSLClientSocketOpenSSL::ExportKeyingMaterial(
    const base::StringPiece& label,
    bool has_context, const base::StringPiece& context,
    unsigned char* out, unsigned int outlen) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  int rv = SSL_export_keying_material(
      ssl_, out, outlen, const_cast<char*>(label.data()),
      label.size(),
      reinterpret_cast<unsigned char*>(const_cast<char*>(context.data())),
      context.length(),
      context.length() > 0);

  if (rv != 1) {
    int ssl_error = SSL_get_error(ssl_, rv);
    LOG(ERROR) << "Failed to export keying material;"
               << " returned " << rv
               << ", SSL error code " << ssl_error;
    return MapOpenSSLError(ssl_error, err_tracer);
  }
  return OK;
}

int SSLClientSocketOpenSSL::GetTLSUniqueChannelBinding(std::string* out) {
  return ERR_NOT_IMPLEMENTED;
}

int SSLClientSocketOpenSSL::Connect(const CompletionCallback& callback) {
  net_log_.BeginEvent(NetLog::TYPE_SSL_CONNECT);

  // Set up new ssl object.
  if (!Init()) {
    int result = ERR_UNEXPECTED;
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, result);
    return result;
  }

  // Set SSL to client mode. Handshake happens in the loop below.
  SSL_set_connect_state(ssl_);

  GotoState(STATE_HANDSHAKE);
  int rv = DoHandshakeLoop(net::OK);
  if (rv == ERR_IO_PENDING) {
    user_connect_callback_ = callback;
  } else {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, rv);
  }

  return rv > OK ? OK : rv;
}

void SSLClientSocketOpenSSL::Disconnect() {
  if (ssl_) {
    // Calling SSL_shutdown prevents the session from being marked as
    // unresumable.
    SSL_shutdown(ssl_);
    SSL_free(ssl_);
    ssl_ = NULL;
  }
  if (transport_bio_) {
    BIO_free_all(transport_bio_);
    transport_bio_ = NULL;
  }

  // Shut down anything that may call us back.
  verifier_.reset();
  transport_->socket()->Disconnect();

  // Null all callbacks, delete all buffers.
  transport_send_busy_ = false;
  send_buffer_ = NULL;
  transport_recv_busy_ = false;
  transport_recv_eof_ = false;
  recv_buffer_ = NULL;

  user_connect_callback_.Reset();
  user_read_callback_.Reset();
  user_write_callback_.Reset();
  user_read_buf_         = NULL;
  user_read_buf_len_     = 0;
  user_write_buf_        = NULL;
  user_write_buf_len_    = 0;

  pending_read_error_ = kNoPendingReadResult;
  transport_write_error_ = OK;

  server_cert_verify_result_.Reset();
  completed_handshake_ = false;

  cert_authorities_.clear();
  client_auth_cert_needed_ = false;
}

bool SSLClientSocketOpenSSL::IsConnected() const {
  // If the handshake has not yet completed.
  if (!completed_handshake_)
    return false;
  // If an asynchronous operation is still pending.
  if (user_read_buf_.get() || user_write_buf_.get())
    return true;

  return transport_->socket()->IsConnected();
}

bool SSLClientSocketOpenSSL::IsConnectedAndIdle() const {
  // If the handshake has not yet completed.
  if (!completed_handshake_)
    return false;
  // If an asynchronous operation is still pending.
  if (user_read_buf_.get() || user_write_buf_.get())
    return false;
  // If there is data waiting to be sent, or data read from the network that
  // has not yet been consumed.
  if (BIO_ctrl_pending(transport_bio_) > 0 ||
      BIO_ctrl_wpending(transport_bio_) > 0) {
    return false;
  }

  return transport_->socket()->IsConnectedAndIdle();
}

int SSLClientSocketOpenSSL::GetPeerAddress(IPEndPoint* addressList) const {
  return transport_->socket()->GetPeerAddress(addressList);
}

int SSLClientSocketOpenSSL::GetLocalAddress(IPEndPoint* addressList) const {
  return transport_->socket()->GetLocalAddress(addressList);
}

const BoundNetLog& SSLClientSocketOpenSSL::NetLog() const {
  return net_log_;
}

void SSLClientSocketOpenSSL::SetSubresourceSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetSubresourceSpeculation();
  } else {
    NOTREACHED();
  }
}

void SSLClientSocketOpenSSL::SetOmniboxSpeculation() {
  if (transport_.get() && transport_->socket()) {
    transport_->socket()->SetOmniboxSpeculation();
  } else {
    NOTREACHED();
  }
}

bool SSLClientSocketOpenSSL::WasEverUsed() const {
  return was_ever_used_;
}

bool SSLClientSocketOpenSSL::UsingTCPFastOpen() const {
  if (transport_.get() && transport_->socket())
    return transport_->socket()->UsingTCPFastOpen();

  NOTREACHED();
  return false;
}

bool SSLClientSocketOpenSSL::GetSSLInfo(SSLInfo* ssl_info) {
  ssl_info->Reset();
  if (!server_cert_.get())
    return false;

  ssl_info->cert = server_cert_verify_result_.verified_cert;
  ssl_info->cert_status = server_cert_verify_result_.cert_status;
  ssl_info->is_issued_by_known_root =
      server_cert_verify_result_.is_issued_by_known_root;
  ssl_info->public_key_hashes =
    server_cert_verify_result_.public_key_hashes;
  ssl_info->client_cert_sent =
      ssl_config_.send_client_cert && ssl_config_.client_cert.get();
  ssl_info->channel_id_sent = WasChannelIDSent();

  RecordChannelIDSupport(server_bound_cert_service_,
                         channel_id_xtn_negotiated_,
                         ssl_config_.channel_id_enabled,
                         crypto::ECPrivateKey::IsSupported());

  const SSL_CIPHER* cipher = SSL_get_current_cipher(ssl_);
  CHECK(cipher);
  ssl_info->security_bits = SSL_CIPHER_get_bits(cipher, NULL);
  const COMP_METHOD* compression = SSL_get_current_compression(ssl_);

  ssl_info->connection_status = EncodeSSLConnectionStatus(
      SSL_CIPHER_get_id(cipher),
      compression ? compression->type : 0,
      GetNetSSLVersion(ssl_));

  bool peer_supports_renego_ext = !!SSL_get_secure_renegotiation_support(ssl_);
  if (!peer_supports_renego_ext)
    ssl_info->connection_status |= SSL_CONNECTION_NO_RENEGOTIATION_EXTENSION;
  UMA_HISTOGRAM_ENUMERATION("Net.RenegotiationExtensionSupported",
                            implicit_cast<int>(peer_supports_renego_ext), 2);

  if (ssl_config_.version_fallback)
    ssl_info->connection_status |= SSL_CONNECTION_VERSION_FALLBACK;

  ssl_info->handshake_type = SSL_session_reused(ssl_) ?
      SSLInfo::HANDSHAKE_RESUME : SSLInfo::HANDSHAKE_FULL;

  DVLOG(3) << "Encoded connection status: cipher suite = "
      << SSLConnectionStatusToCipherSuite(ssl_info->connection_status)
      << " version = "
      << SSLConnectionStatusToVersion(ssl_info->connection_status);
  return true;
}

int SSLClientSocketOpenSSL::Read(IOBuffer* buf,
                                 int buf_len,
                                 const CompletionCallback& callback) {
  user_read_buf_ = buf;
  user_read_buf_len_ = buf_len;

  int rv = DoReadLoop(OK);

  if (rv == ERR_IO_PENDING) {
    user_read_callback_ = callback;
  } else {
    if (rv > 0)
      was_ever_used_ = true;
    user_read_buf_ = NULL;
    user_read_buf_len_ = 0;
  }

  return rv;
}

int SSLClientSocketOpenSSL::Write(IOBuffer* buf,
                                  int buf_len,
                                  const CompletionCallback& callback) {
  user_write_buf_ = buf;
  user_write_buf_len_ = buf_len;

  int rv = DoWriteLoop(OK);

  if (rv == ERR_IO_PENDING) {
    user_write_callback_ = callback;
  } else {
    if (rv > 0)
      was_ever_used_ = true;
    user_write_buf_ = NULL;
    user_write_buf_len_ = 0;
  }

  return rv;
}

bool SSLClientSocketOpenSSL::SetReceiveBufferSize(int32 size) {
  return transport_->socket()->SetReceiveBufferSize(size);
}

bool SSLClientSocketOpenSSL::SetSendBufferSize(int32 size) {
  return transport_->socket()->SetSendBufferSize(size);
}

bool SSLClientSocketOpenSSL::Init() {
  DCHECK(!ssl_);
  DCHECK(!transport_bio_);

  SSLContext* context = SSLContext::GetInstance();
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  ssl_ = SSL_new(context->ssl_ctx());
  if (!ssl_ || !context->SetClientSocketForSSL(ssl_, this))
    return false;

  if (!SSL_set_tlsext_host_name(ssl_, host_and_port_.host().c_str()))
    return false;

  trying_cached_session_ = context->session_cache()->SetSSLSessionWithKey(
      ssl_, GetSocketSessionCacheKey(*this));

  BIO* ssl_bio = NULL;
  // 0 => use default buffer sizes.
  if (!BIO_new_bio_pair(&ssl_bio, 0, &transport_bio_, 0))
    return false;
  DCHECK(ssl_bio);
  DCHECK(transport_bio_);

  SSL_set_bio(ssl_, ssl_bio, ssl_bio);

  // OpenSSL defaults some options to on, others to off. To avoid ambiguity,
  // set everything we care about to an absolute value.
  SslSetClearMask options;
  options.ConfigureFlag(SSL_OP_NO_SSLv2, true);
  bool ssl3_enabled = (ssl_config_.version_min == SSL_PROTOCOL_VERSION_SSL3);
  options.ConfigureFlag(SSL_OP_NO_SSLv3, !ssl3_enabled);
  bool tls1_enabled = (ssl_config_.version_min <= SSL_PROTOCOL_VERSION_TLS1 &&
                       ssl_config_.version_max >= SSL_PROTOCOL_VERSION_TLS1);
  options.ConfigureFlag(SSL_OP_NO_TLSv1, !tls1_enabled);
#if defined(SSL_OP_NO_TLSv1_1)
  bool tls1_1_enabled =
      (ssl_config_.version_min <= SSL_PROTOCOL_VERSION_TLS1_1 &&
       ssl_config_.version_max >= SSL_PROTOCOL_VERSION_TLS1_1);
  options.ConfigureFlag(SSL_OP_NO_TLSv1_1, !tls1_1_enabled);
#endif
#if defined(SSL_OP_NO_TLSv1_2)
  bool tls1_2_enabled =
      (ssl_config_.version_min <= SSL_PROTOCOL_VERSION_TLS1_2 &&
       ssl_config_.version_max >= SSL_PROTOCOL_VERSION_TLS1_2);
  options.ConfigureFlag(SSL_OP_NO_TLSv1_2, !tls1_2_enabled);
#endif

#if defined(SSL_OP_NO_COMPRESSION)
  options.ConfigureFlag(SSL_OP_NO_COMPRESSION, true);
#endif

  // TODO(joth): Set this conditionally, see http://crbug.com/55410
  options.ConfigureFlag(SSL_OP_LEGACY_SERVER_CONNECT, true);

  SSL_set_options(ssl_, options.set_mask);
  SSL_clear_options(ssl_, options.clear_mask);

  // Same as above, this time for the SSL mode.
  SslSetClearMask mode;

#if defined(SSL_MODE_RELEASE_BUFFERS)
  mode.ConfigureFlag(SSL_MODE_RELEASE_BUFFERS, true);
#endif

#if defined(SSL_MODE_SMALL_BUFFERS)
  mode.ConfigureFlag(SSL_MODE_SMALL_BUFFERS, true);
#endif

  SSL_set_mode(ssl_, mode.set_mask);
  SSL_clear_mode(ssl_, mode.clear_mask);

  // Removing ciphers by ID from OpenSSL is a bit involved as we must use the
  // textual name with SSL_set_cipher_list because there is no public API to
  // directly remove a cipher by ID.
  STACK_OF(SSL_CIPHER)* ciphers = SSL_get_ciphers(ssl_);
  DCHECK(ciphers);
  // See SSLConfig::disabled_cipher_suites for description of the suites
  // disabled by default. Note that !SHA256 and !SHA384 only remove HMAC-SHA256
  // and HMAC-SHA384 cipher suites, not GCM cipher suites with SHA256 or SHA384
  // as the handshake hash.
  std::string command("DEFAULT:!NULL:!aNULL:!IDEA:!FZA:!SRP:!SHA256:!SHA384:"
                      "!aECDH:!AESGCM+AES256");
  // Walk through all the installed ciphers, seeing if any need to be
  // appended to the cipher removal |command|.
  for (int i = 0; i < sk_SSL_CIPHER_num(ciphers); ++i) {
    const SSL_CIPHER* cipher = sk_SSL_CIPHER_value(ciphers, i);
    const uint16 id = SSL_CIPHER_get_id(cipher);
    // Remove any ciphers with a strength of less than 80 bits. Note the NSS
    // implementation uses "effective" bits here but OpenSSL does not provide
    // this detail. This only impacts Triple DES: reports 112 vs. 168 bits,
    // both of which are greater than 80 anyway.
    bool disable = SSL_CIPHER_get_bits(cipher, NULL) < 80;
    if (!disable) {
      disable = std::find(ssl_config_.disabled_cipher_suites.begin(),
                          ssl_config_.disabled_cipher_suites.end(), id) !=
                    ssl_config_.disabled_cipher_suites.end();
    }
    if (disable) {
       const char* name = SSL_CIPHER_get_name(cipher);
       DVLOG(3) << "Found cipher to remove: '" << name << "', ID: " << id
                << " strength: " << SSL_CIPHER_get_bits(cipher, NULL);
       command.append(":!");
       command.append(name);
     }
  }
  int rv = SSL_set_cipher_list(ssl_, command.c_str());
  // If this fails (rv = 0) it means there are no ciphers enabled on this SSL.
  // This will almost certainly result in the socket failing to complete the
  // handshake at which point the appropriate error is bubbled up to the client.
  LOG_IF(WARNING, rv != 1) << "SSL_set_cipher_list('" << command << "') "
                              "returned " << rv;

  // TLS channel ids.
  if (IsChannelIDEnabled(ssl_config_, server_bound_cert_service_)) {
    SSL_enable_tls_channel_id(ssl_);
  }

  return true;
}

void SSLClientSocketOpenSSL::DoReadCallback(int rv) {
  // Since Run may result in Read being called, clear |user_read_callback_|
  // up front.
  if (rv > 0)
    was_ever_used_ = true;
  user_read_buf_ = NULL;
  user_read_buf_len_ = 0;
  base::ResetAndReturn(&user_read_callback_).Run(rv);
}

void SSLClientSocketOpenSSL::DoWriteCallback(int rv) {
  // Since Run may result in Write being called, clear |user_write_callback_|
  // up front.
  if (rv > 0)
    was_ever_used_ = true;
  user_write_buf_ = NULL;
  user_write_buf_len_ = 0;
  base::ResetAndReturn(&user_write_callback_).Run(rv);
}

bool SSLClientSocketOpenSSL::DoTransportIO() {
  bool network_moved = false;
  int rv;
  // Read and write as much data as possible. The loop is necessary because
  // Write() may return synchronously.
  do {
    rv = BufferSend();
    if (rv != ERR_IO_PENDING && rv != 0)
      network_moved = true;
  } while (rv > 0);
  if (!transport_recv_eof_ && BufferRecv() != ERR_IO_PENDING)
    network_moved = true;
  return network_moved;
}

int SSLClientSocketOpenSSL::DoHandshake() {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int net_error = net::OK;
  int rv = SSL_do_handshake(ssl_);

  if (client_auth_cert_needed_) {
    net_error = ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
    // If the handshake already succeeded (because the server requests but
    // doesn't require a client cert), we need to invalidate the SSL session
    // so that we won't try to resume the non-client-authenticated session in
    // the next handshake.  This will cause the server to ask for a client
    // cert again.
    if (rv == 1) {
      // Remove from session cache but don't clear this connection.
      SSL_SESSION* session = SSL_get_session(ssl_);
      if (session) {
        int rv = SSL_CTX_remove_session(SSL_get_SSL_CTX(ssl_), session);
        LOG_IF(WARNING, !rv) << "Couldn't invalidate SSL session: " << session;
      }
    }
  } else if (rv == 1) {
    if (trying_cached_session_ && logging::DEBUG_MODE) {
      DVLOG(2) << "Result of session reuse for " << host_and_port_.ToString()
               << " is: " << (SSL_session_reused(ssl_) ? "Success" : "Fail");
    }
    // SSL handshake is completed.  Let's verify the certificate.
    const bool got_cert = !!UpdateServerCert();
    DCHECK(got_cert);
    net_log_.AddEvent(
        NetLog::TYPE_SSL_CERTIFICATES_RECEIVED,
        base::Bind(&NetLogX509CertificateCallback,
                   base::Unretained(server_cert_.get())));
    GotoState(STATE_VERIFY_CERT);
  } else {
    int ssl_error = SSL_get_error(ssl_, rv);

    if (ssl_error == SSL_ERROR_WANT_CHANNEL_ID_LOOKUP) {
      // The server supports TLS channel id and the lookup is asynchronous.
      // Retrieve the error from the call to |server_bound_cert_service_|.
      net_error = channel_id_request_return_value_;
    } else {
      net_error = MapOpenSSLError(ssl_error, err_tracer);
    }

    // If not done, stay in this state
    if (net_error == ERR_IO_PENDING) {
      GotoState(STATE_HANDSHAKE);
    } else {
      LOG(ERROR) << "handshake failed; returned " << rv
                 << ", SSL error code " << ssl_error
                 << ", net_error " << net_error;
      net_log_.AddEvent(
          NetLog::TYPE_SSL_HANDSHAKE_ERROR,
          CreateNetLogSSLErrorCallback(net_error, ssl_error));
    }
  }
  return net_error;
}

int SSLClientSocketOpenSSL::DoVerifyCert(int result) {
  DCHECK(server_cert_.get());
  GotoState(STATE_VERIFY_CERT_COMPLETE);

  CertStatus cert_status;
  if (ssl_config_.IsAllowedBadCert(server_cert_.get(), &cert_status)) {
    VLOG(1) << "Received an expected bad cert with status: " << cert_status;
    server_cert_verify_result_.Reset();
    server_cert_verify_result_.cert_status = cert_status;
    server_cert_verify_result_.verified_cert = server_cert_;
    return OK;
  }

  int flags = 0;
  if (ssl_config_.rev_checking_enabled)
    flags |= CertVerifier::VERIFY_REV_CHECKING_ENABLED;
  if (ssl_config_.verify_ev_cert)
    flags |= CertVerifier::VERIFY_EV_CERT;
  if (ssl_config_.cert_io_enabled)
    flags |= CertVerifier::VERIFY_CERT_IO_ENABLED;
  if (ssl_config_.rev_checking_required_local_anchors)
    flags |= CertVerifier::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  verifier_.reset(new SingleRequestCertVerifier(cert_verifier_));
  return verifier_->Verify(
      server_cert_.get(),
      host_and_port_.host(),
      flags,
      NULL /* no CRL set */,
      &server_cert_verify_result_,
      base::Bind(&SSLClientSocketOpenSSL::OnHandshakeIOComplete,
                 base::Unretained(this)),
      net_log_);
}

int SSLClientSocketOpenSSL::DoVerifyCertComplete(int result) {
  verifier_.reset();

  if (result == OK) {
    // TODO(joth): Work out if we need to remember the intermediate CA certs
    // when the server sends them to us, and do so here.
    SSLContext::GetInstance()->session_cache()->MarkSSLSessionAsGood(ssl_);
  } else {
    DVLOG(1) << "DoVerifyCertComplete error " << ErrorToString(result)
             << " (" << result << ")";
  }

  completed_handshake_ = true;
  // Exit DoHandshakeLoop and return the result to the caller to Connect.
  DCHECK_EQ(STATE_NONE, next_handshake_state_);
  return result;
}

void SSLClientSocketOpenSSL::DoConnectCallback(int rv) {
  if (!user_connect_callback_.is_null()) {
    CompletionCallback c = user_connect_callback_;
    user_connect_callback_.Reset();
    c.Run(rv > OK ? OK : rv);
  }
}

X509Certificate* SSLClientSocketOpenSSL::UpdateServerCert() {
  server_cert_chain_->Reset(SSL_get_peer_cert_chain(ssl_));
  server_cert_ = server_cert_chain_->AsOSChain();

  if (!server_cert_chain_->IsValid())
    DVLOG(1) << "UpdateServerCert received invalid certificate chain from peer";

  return server_cert_.get();
}

void SSLClientSocketOpenSSL::OnHandshakeIOComplete(int result) {
  int rv = DoHandshakeLoop(result);
  if (rv != ERR_IO_PENDING) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, rv);
    DoConnectCallback(rv);
  }
}

void SSLClientSocketOpenSSL::OnSendComplete(int result) {
  if (next_handshake_state_ == STATE_HANDSHAKE) {
    // In handshake phase.
    OnHandshakeIOComplete(result);
    return;
  }

  // OnSendComplete may need to call DoPayloadRead while the renegotiation
  // handshake is in progress.
  int rv_read = ERR_IO_PENDING;
  int rv_write = ERR_IO_PENDING;
  bool network_moved;
  do {
    if (user_read_buf_.get())
      rv_read = DoPayloadRead();
    if (user_write_buf_.get())
      rv_write = DoPayloadWrite();
    network_moved = DoTransportIO();
  } while (rv_read == ERR_IO_PENDING && rv_write == ERR_IO_PENDING &&
           (user_read_buf_.get() || user_write_buf_.get()) && network_moved);

  // Performing the Read callback may cause |this| to be deleted. If this
  // happens, the Write callback should not be invoked. Guard against this by
  // holding a WeakPtr to |this| and ensuring it's still valid.
  base::WeakPtr<SSLClientSocketOpenSSL> guard(weak_factory_.GetWeakPtr());
  if (user_read_buf_.get() && rv_read != ERR_IO_PENDING)
    DoReadCallback(rv_read);

  if (!guard.get())
    return;

  if (user_write_buf_.get() && rv_write != ERR_IO_PENDING)
    DoWriteCallback(rv_write);
}

void SSLClientSocketOpenSSL::OnRecvComplete(int result) {
  if (next_handshake_state_ == STATE_HANDSHAKE) {
    // In handshake phase.
    OnHandshakeIOComplete(result);
    return;
  }

  // Network layer received some data, check if client requested to read
  // decrypted data.
  if (!user_read_buf_.get())
    return;

  int rv = DoReadLoop(result);
  if (rv != ERR_IO_PENDING)
    DoReadCallback(rv);
}

int SSLClientSocketOpenSSL::DoHandshakeLoop(int last_io_result) {
  int rv = last_io_result;
  do {
    // Default to STATE_NONE for next state.
    // (This is a quirk carried over from the windows
    // implementation.  It makes reading the logs a bit harder.)
    // State handlers can and often do call GotoState just
    // to stay in the current state.
    State state = next_handshake_state_;
    GotoState(STATE_NONE);
    switch (state) {
      case STATE_HANDSHAKE:
        rv = DoHandshake();
        break;
      case STATE_VERIFY_CERT:
        DCHECK(rv == OK);
        rv = DoVerifyCert(rv);
       break;
      case STATE_VERIFY_CERT_COMPLETE:
        rv = DoVerifyCertComplete(rv);
        break;
      case STATE_NONE:
      default:
        rv = ERR_UNEXPECTED;
        NOTREACHED() << "unexpected state" << state;
        break;
    }

    bool network_moved = DoTransportIO();
    if (network_moved && next_handshake_state_ == STATE_HANDSHAKE) {
      // In general we exit the loop if rv is ERR_IO_PENDING.  In this
      // special case we keep looping even if rv is ERR_IO_PENDING because
      // the transport IO may allow DoHandshake to make progress.
      rv = OK;  // This causes us to stay in the loop.
    }
  } while (rv != ERR_IO_PENDING && next_handshake_state_ != STATE_NONE);
  return rv;
}

int SSLClientSocketOpenSSL::DoReadLoop(int result) {
  if (result < 0)
    return result;

  bool network_moved;
  int rv;
  do {
    rv = DoPayloadRead();
    network_moved = DoTransportIO();
  } while (rv == ERR_IO_PENDING && network_moved);

  return rv;
}

int SSLClientSocketOpenSSL::DoWriteLoop(int result) {
  if (result < 0)
    return result;

  bool network_moved;
  int rv;
  do {
    rv = DoPayloadWrite();
    network_moved = DoTransportIO();
  } while (rv == ERR_IO_PENDING && network_moved);

  return rv;
}

int SSLClientSocketOpenSSL::DoPayloadRead() {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  int rv;
  if (pending_read_error_ != kNoPendingReadResult) {
    rv = pending_read_error_;
    pending_read_error_ = kNoPendingReadResult;
    if (rv == 0) {
      net_log_.AddByteTransferEvent(NetLog::TYPE_SSL_SOCKET_BYTES_RECEIVED,
                                    rv, user_read_buf_->data());
    }
    return rv;
  }

  int total_bytes_read = 0;
  do {
    rv = SSL_read(ssl_, user_read_buf_->data() + total_bytes_read,
                  user_read_buf_len_ - total_bytes_read);
    if (rv > 0)
      total_bytes_read += rv;
  } while (total_bytes_read < user_read_buf_len_ && rv > 0);

  if (total_bytes_read == user_read_buf_len_) {
    rv = total_bytes_read;
  } else {
    // Otherwise, an error occurred (rv <= 0). The error needs to be handled
    // immediately, while the OpenSSL errors are still available in
    // thread-local storage. However, the handled/remapped error code should
    // only be returned if no application data was already read; if it was, the
    // error code should be deferred until the next call of DoPayloadRead.
    //
    // If no data was read, |*next_result| will point to the return value of
    // this function. If at least some data was read, |*next_result| will point
    // to |pending_read_error_|, to be returned in a future call to
    // DoPayloadRead() (e.g.: after the current data is handled).
    int *next_result = &rv;
    if (total_bytes_read > 0) {
      pending_read_error_ = rv;
      rv = total_bytes_read;
      next_result = &pending_read_error_;
    }

    if (client_auth_cert_needed_) {
      *next_result = ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
    } else if (*next_result < 0) {
      int err = SSL_get_error(ssl_, *next_result);
      *next_result = MapOpenSSLError(err, err_tracer);
      if (rv > 0 && *next_result == ERR_IO_PENDING) {
          // If at least some data was read from SSL_read(), do not treat
          // insufficient data as an error to return in the next call to
          // DoPayloadRead() - instead, let the call fall through to check
          // SSL_read() again. This is because DoTransportIO() may complete
          // in between the next call to DoPayloadRead(), and thus it is
          // important to check SSL_read() on subsequent invocations to see
          // if a complete record may now be read.
        *next_result = kNoPendingReadResult;
      }
    }
  }

  if (rv >= 0) {
    net_log_.AddByteTransferEvent(NetLog::TYPE_SSL_SOCKET_BYTES_RECEIVED, rv,
                                  user_read_buf_->data());
  }
  return rv;
}

int SSLClientSocketOpenSSL::DoPayloadWrite() {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int rv = SSL_write(ssl_, user_write_buf_->data(), user_write_buf_len_);

  if (rv >= 0) {
    net_log_.AddByteTransferEvent(NetLog::TYPE_SSL_SOCKET_BYTES_SENT, rv,
                                  user_write_buf_->data());
    return rv;
  }

  int err = SSL_get_error(ssl_, rv);
  return MapOpenSSLError(err, err_tracer);
}

int SSLClientSocketOpenSSL::BufferSend(void) {
  if (transport_send_busy_)
    return ERR_IO_PENDING;

  if (!send_buffer_.get()) {
    // Get a fresh send buffer out of the send BIO.
    size_t max_read = BIO_ctrl_pending(transport_bio_);
    if (!max_read)
      return 0;  // Nothing pending in the OpenSSL write BIO.
    send_buffer_ = new DrainableIOBuffer(new IOBuffer(max_read), max_read);
    int read_bytes = BIO_read(transport_bio_, send_buffer_->data(), max_read);
    DCHECK_GT(read_bytes, 0);
    CHECK_EQ(static_cast<int>(max_read), read_bytes);
  }

  int rv = transport_->socket()->Write(
      send_buffer_.get(),
      send_buffer_->BytesRemaining(),
      base::Bind(&SSLClientSocketOpenSSL::BufferSendComplete,
                 base::Unretained(this)));
  if (rv == ERR_IO_PENDING) {
    transport_send_busy_ = true;
  } else {
    TransportWriteComplete(rv);
  }
  return rv;
}

int SSLClientSocketOpenSSL::BufferRecv(void) {
  if (transport_recv_busy_)
    return ERR_IO_PENDING;

  // Determine how much was requested from |transport_bio_| that was not
  // actually available.
  size_t requested = BIO_ctrl_get_read_request(transport_bio_);
  if (requested == 0) {
    // This is not a perfect match of error codes, as no operation is
    // actually pending. However, returning 0 would be interpreted as
    // a possible sign of EOF, which is also an inappropriate match.
    return ERR_IO_PENDING;
  }

  // Known Issue: While only reading |requested| data is the more correct
  // implementation, it has the downside of resulting in frequent reads:
  // One read for the SSL record header (~5 bytes) and one read for the SSL
  // record body. Rather than issuing these reads to the underlying socket
  // (and constantly allocating new IOBuffers), a single Read() request to
  // fill |transport_bio_| is issued. As long as an SSL client socket cannot
  // be gracefully shutdown (via SSL close alerts) and re-used for non-SSL
  // traffic, this over-subscribed Read()ing will not cause issues.
  size_t max_write = BIO_ctrl_get_write_guarantee(transport_bio_);
  if (!max_write)
    return ERR_IO_PENDING;

  recv_buffer_ = new IOBuffer(max_write);
  int rv = transport_->socket()->Read(
      recv_buffer_.get(),
      max_write,
      base::Bind(&SSLClientSocketOpenSSL::BufferRecvComplete,
                 base::Unretained(this)));
  if (rv == ERR_IO_PENDING) {
    transport_recv_busy_ = true;
  } else {
    rv = TransportReadComplete(rv);
  }
  return rv;
}

void SSLClientSocketOpenSSL::BufferSendComplete(int result) {
  transport_send_busy_ = false;
  TransportWriteComplete(result);
  OnSendComplete(result);
}

void SSLClientSocketOpenSSL::BufferRecvComplete(int result) {
  result = TransportReadComplete(result);
  OnRecvComplete(result);
}

void SSLClientSocketOpenSSL::TransportWriteComplete(int result) {
  DCHECK(ERR_IO_PENDING != result);
  if (result < 0) {
    // Got a socket write error; close the BIO to indicate this upward.
    //
    // TODO(davidben): The value of |result| gets lost. Feed the error back into
    // the BIO so it gets (re-)detected in OnSendComplete. Perhaps with
    // BIO_set_callback.
    DVLOG(1) << "TransportWriteComplete error " << result;
    (void)BIO_shutdown_wr(SSL_get_wbio(ssl_));

    // Match the fix for http://crbug.com/249848 in NSS by erroring future reads
    // from the socket after a write error.
    //
    // TODO(davidben): Avoid having read and write ends interact this way.
    transport_write_error_ = result;
    (void)BIO_shutdown_wr(transport_bio_);
    send_buffer_ = NULL;
  } else {
    DCHECK(send_buffer_.get());
    send_buffer_->DidConsume(result);
    DCHECK_GE(send_buffer_->BytesRemaining(), 0);
    if (send_buffer_->BytesRemaining() <= 0)
      send_buffer_ = NULL;
  }
}

int SSLClientSocketOpenSSL::TransportReadComplete(int result) {
  DCHECK(ERR_IO_PENDING != result);
  if (result <= 0) {
    DVLOG(1) << "TransportReadComplete result " << result;
    // Received 0 (end of file) or an error. Either way, bubble it up to the
    // SSL layer via the BIO. TODO(joth): consider stashing the error code, to
    // relay up to the SSL socket client (i.e. via DoReadCallback).
    if (result == 0)
      transport_recv_eof_ = true;
    (void)BIO_shutdown_wr(transport_bio_);
  } else if (transport_write_error_ < 0) {
    // Mirror transport write errors as read failures; transport_bio_ has been
    // shut down by TransportWriteComplete, so the BIO_write will fail, failing
    // the CHECK. http://crbug.com/335557.
    result = transport_write_error_;
  } else {
    DCHECK(recv_buffer_.get());
    int ret = BIO_write(transport_bio_, recv_buffer_->data(), result);
    // A write into a memory BIO should always succeed.
    // Force values on the stack for http://crbug.com/335557
    base::debug::Alias(&result);
    base::debug::Alias(&ret);
    CHECK_EQ(result, ret);
  }
  recv_buffer_ = NULL;
  transport_recv_busy_ = false;
  return result;
}

int SSLClientSocketOpenSSL::ClientCertRequestCallback(SSL* ssl,
                                                      X509** x509,
                                                      EVP_PKEY** pkey) {
  DVLOG(3) << "OpenSSL ClientCertRequestCallback called";
  DCHECK(ssl == ssl_);
  DCHECK(*x509 == NULL);
  DCHECK(*pkey == NULL);
#if defined(USE_OPENSSL_CERTS)
  if (!ssl_config_.send_client_cert) {
    // First pass: we know that a client certificate is needed, but we do not
    // have one at hand.
    client_auth_cert_needed_ = true;
    STACK_OF(X509_NAME) *authorities = SSL_get_client_CA_list(ssl);
    for (int i = 0; i < sk_X509_NAME_num(authorities); i++) {
      X509_NAME *ca_name = (X509_NAME *)sk_X509_NAME_value(authorities, i);
      unsigned char* str = NULL;
      int length = i2d_X509_NAME(ca_name, &str);
      cert_authorities_.push_back(std::string(
          reinterpret_cast<const char*>(str),
          static_cast<size_t>(length)));
      OPENSSL_free(str);
    }

    return -1;  // Suspends handshake.
  }

  // Second pass: a client certificate should have been selected.
  if (ssl_config_.client_cert.get()) {
    // A note about ownership: FetchClientCertPrivateKey() increments
    // the reference count of the EVP_PKEY. Ownership of this reference
    // is passed directly to OpenSSL, which will release the reference
    // using EVP_PKEY_free() when the SSL object is destroyed.
    OpenSSLClientKeyStore::ScopedEVP_PKEY privkey;
    if (OpenSSLClientKeyStore::GetInstance()->FetchClientCertPrivateKey(
            ssl_config_.client_cert.get(), &privkey)) {
      // TODO(joth): (copied from NSS) We should wait for server certificate
      // verification before sending our credentials. See http://crbug.com/13934
      *x509 = X509Certificate::DupOSCertHandle(
          ssl_config_.client_cert->os_cert_handle());
      *pkey = privkey.release();
      return 1;
    }
    LOG(WARNING) << "Client cert found without private key";
  }
#else  // !defined(USE_OPENSSL_CERTS)
  // OS handling of client certificates is not yet implemented.
  NOTIMPLEMENTED();
#endif  // defined(USE_OPENSSL_CERTS)

  // Send no client certificate.
  return 0;
}

void SSLClientSocketOpenSSL::ChannelIDRequestCallback(SSL* ssl,
                                                      EVP_PKEY** pkey) {
  DVLOG(3) << "OpenSSL ChannelIDRequestCallback called";
  DCHECK_EQ(ssl, ssl_);
  DCHECK(!*pkey);

  channel_id_xtn_negotiated_ = true;
  if (!channel_id_private_key_.size()) {
    channel_id_request_return_value_ =
        server_bound_cert_service_->GetOrCreateDomainBoundCert(
            host_and_port_.host(),
            &channel_id_private_key_,
            &channel_id_cert_,
            base::Bind(&SSLClientSocketOpenSSL::OnHandshakeIOComplete,
                       base::Unretained(this)),
            &channel_id_request_handle_);
    if (channel_id_request_return_value_ != OK)
      return;
  }

  // Decode key.
  std::vector<uint8> encrypted_private_key_info;
  std::vector<uint8> subject_public_key_info;
  encrypted_private_key_info.assign(
      channel_id_private_key_.data(),
      channel_id_private_key_.data() + channel_id_private_key_.size());
  subject_public_key_info.assign(
      channel_id_cert_.data(),
      channel_id_cert_.data() + channel_id_cert_.size());
  scoped_ptr<crypto::ECPrivateKey> ec_private_key(
      crypto::ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(
          ServerBoundCertService::kEPKIPassword,
          encrypted_private_key_info,
          subject_public_key_info));
  if (!ec_private_key)
    return;
  set_channel_id_sent(true);
  *pkey = EVP_PKEY_dup(ec_private_key->key());
}

int SSLClientSocketOpenSSL::CertVerifyCallback(X509_STORE_CTX* store_ctx) {
  if (!completed_handshake_) {
    // If the first handshake hasn't completed then we accept any certificates
    // because we verify after the handshake.
    return 1;
  }

  CHECK(server_cert_.get());

  PeerCertificateChain chain(store_ctx->chain);
  if (chain.IsValid() && server_cert_->Equals(chain.AsOSChain()))
    return 1;

  if (!chain.IsValid())
    LOG(ERROR) << "Received invalid certificate chain between handshakes";
  else
    LOG(ERROR) << "Server certificate changed between handshakes";
  return 0;
}

// SelectNextProtoCallback is called by OpenSSL during the handshake. If the
// server supports NPN, selects a protocol from the list that the server
// provides. According to third_party/openssl/openssl/ssl/ssl_lib.c, the
// callback can assume that |in| is syntactically valid.
int SSLClientSocketOpenSSL::SelectNextProtoCallback(unsigned char** out,
                                                    unsigned char* outlen,
                                                    const unsigned char* in,
                                                    unsigned int inlen) {
#if defined(OPENSSL_NPN_NEGOTIATED)
  if (ssl_config_.next_protos.empty()) {
    *out = reinterpret_cast<uint8*>(
        const_cast<char*>(kDefaultSupportedNPNProtocol));
    *outlen = arraysize(kDefaultSupportedNPNProtocol) - 1;
    npn_status_ = kNextProtoUnsupported;
    return SSL_TLSEXT_ERR_OK;
  }

  // Assume there's no overlap between our protocols and the server's list.
  npn_status_ = kNextProtoNoOverlap;

  // For each protocol in server preference order, see if we support it.
  for (unsigned int i = 0; i < inlen; i += in[i] + 1) {
    for (std::vector<std::string>::const_iterator
             j = ssl_config_.next_protos.begin();
         j != ssl_config_.next_protos.end(); ++j) {
      if (in[i] == j->size() &&
          memcmp(&in[i + 1], j->data(), in[i]) == 0) {
        // We found a match.
        *out = const_cast<unsigned char*>(in) + i + 1;
        *outlen = in[i];
        npn_status_ = kNextProtoNegotiated;
        break;
      }
    }
    if (npn_status_ == kNextProtoNegotiated)
      break;
  }

  // If we didn't find a protocol, we select the first one from our list.
  if (npn_status_ == kNextProtoNoOverlap) {
    *out = reinterpret_cast<uint8*>(const_cast<char*>(
        ssl_config_.next_protos[0].data()));
    *outlen = ssl_config_.next_protos[0].size();
  }

  npn_proto_.assign(reinterpret_cast<const char*>(*out), *outlen);
  server_protos_.assign(reinterpret_cast<const char*>(in), inlen);
  DVLOG(2) << "next protocol: '" << npn_proto_ << "' status: " << npn_status_;
#endif
  return SSL_TLSEXT_ERR_OK;
}

scoped_refptr<X509Certificate>
SSLClientSocketOpenSSL::GetUnverifiedServerCertificateChain() const {
  return server_cert_;
}

}  // namespace net
