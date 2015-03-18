// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// OpenSSL binding for SSLClientSocket. The class layout and general principle
// of operation is derived from SSLClientSocketNSS.

#include "net/socket/ssl_client_socket_openssl.h"

#include <errno.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/environment.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local.h"
#include "crypto/ec_private_key.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_policy_enforcer.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_ev_whitelist.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/single_request_cert_verifier.h"
#include "net/cert/x509_certificate_net_log_param.h"
#include "net/cert/x509_util_openssl.h"
#include "net/http/transport_security_state.h"
#include "net/socket/ssl_session_cache_openssl.h"
#include "net/ssl/scoped_openssl_types.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(USE_OPENSSL_CERTS)
#include "net/ssl/openssl_client_key_store.h"
#else
#include "net/ssl/openssl_platform_key.h"
#endif

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

// Default size of the internal BoringSSL buffers.
const int KDefaultOpenSSLBufferSize = 17 * 1024;

void FreeX509Stack(STACK_OF(X509)* ptr) {
  sk_X509_pop_free(ptr, X509_free);
}

using ScopedX509Stack = crypto::ScopedOpenSSL<STACK_OF(X509), FreeX509Stack>;

#if OPENSSL_VERSION_NUMBER < 0x1000103fL
// This method doesn't seem to have made it into the OpenSSL headers.
unsigned long SSL_CIPHER_get_id(const SSL_CIPHER* cipher) { return cipher->id; }
#endif

// Used for encoding the |connection_status| field of an SSLInfo object.
int EncodeSSLConnectionStatus(uint16 cipher_suite,
                              int compression,
                              int version) {
  return cipher_suite |
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
    case TLS1_1_VERSION:
      return SSL_CONNECTION_VERSION_TLS1_1;
    case TLS1_2_VERSION:
      return SSL_CONNECTION_VERSION_TLS1_2;
    default:
      return SSL_CONNECTION_VERSION_UNKNOWN;
  }
}

ScopedX509 OSCertHandleToOpenSSL(
    X509Certificate::OSCertHandle os_handle) {
#if defined(USE_OPENSSL_CERTS)
  return ScopedX509(X509Certificate::DupOSCertHandle(os_handle));
#else  // !defined(USE_OPENSSL_CERTS)
  std::string der_encoded;
  if (!X509Certificate::GetDEREncoded(os_handle, &der_encoded))
    return ScopedX509();
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(der_encoded.data());
  return ScopedX509(d2i_X509(NULL, &bytes, der_encoded.size()));
#endif  // defined(USE_OPENSSL_CERTS)
}

ScopedX509Stack OSCertHandlesToOpenSSL(
    const X509Certificate::OSCertHandles& os_handles) {
  ScopedX509Stack stack(sk_X509_new_null());
  for (size_t i = 0; i < os_handles.size(); i++) {
    ScopedX509 x509 = OSCertHandleToOpenSSL(os_handles[i]);
    if (!x509)
      return ScopedX509Stack();
    sk_X509_push(stack.get(), x509.release());
  }
  return stack.Pass();
}

int LogErrorCallback(const char* str, size_t len, void* context) {
  LOG(ERROR) << base::StringPiece(str, len);
  return 1;
}

bool IsOCSPStaplingSupported() {
#if defined(OS_WIN)
  // CERT_OCSP_RESPONSE_PROP_ID is only implemented on Vista+, but it can be
  // set on Windows XP without error. There is some overhead from the server
  // sending the OCSP response if it supports the extension, for the subset of
  // XP clients who will request it but be unable to use it, but this is an
  // acceptable trade-off for simplicity of implementation.
  return true;
#else
  return false;
#endif
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
    SSL_CTX_set_cert_cb(ssl_ctx_.get(), ClientCertRequestCallback, NULL);
    SSL_CTX_set_verify(ssl_ctx_.get(), SSL_VERIFY_PEER, NULL);
    // This stops |SSL_shutdown| from generating the close_notify message, which
    // is currently not sent on the network.
    // TODO(haavardm): Remove setting quiet shutdown once 118366 is fixed.
    SSL_CTX_set_quiet_shutdown(ssl_ctx_.get(), 1);
    // TODO(kristianm): Only select this if ssl_config_.next_proto is not empty.
    // It would be better if the callback were not a global setting,
    // but that is an OpenSSL issue.
    SSL_CTX_set_next_proto_select_cb(ssl_ctx_.get(), SelectNextProtoCallback,
                                     NULL);
    ssl_ctx_->tlsext_channel_id_enabled_new = 1;

    scoped_ptr<base::Environment> env(base::Environment::Create());
    std::string ssl_keylog_file;
    if (env->GetVar("SSLKEYLOGFILE", &ssl_keylog_file) &&
        !ssl_keylog_file.empty()) {
      crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
      BIO* bio = BIO_new_file(ssl_keylog_file.c_str(), "a");
      if (!bio) {
        LOG(ERROR) << "Failed to open " << ssl_keylog_file;
        ERR_print_errors_cb(&LogErrorCallback, NULL);
      } else {
        SSL_CTX_set_keylog_bio(ssl_ctx_.get(), bio);
      }
    }
  }

  static std::string GetSessionCacheKey(const SSL* ssl) {
    SSLClientSocketOpenSSL* socket = GetInstance()->GetClientSocketFromSSL(ssl);
    CHECK(socket);
    return socket->GetSessionCacheKey();
  }

  static SSLSessionCacheOpenSSL::Config kDefaultSessionCacheConfig;

  static int ClientCertRequestCallback(SSL* ssl, void* arg) {
    SSLClientSocketOpenSSL* socket = GetInstance()->GetClientSocketFromSSL(ssl);
    DCHECK(socket);
    return socket->ClientCertRequestCallback(ssl);
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

  ScopedSSL_CTX ssl_ctx_;
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
  scoped_refptr<X509Certificate> AsOSChain() const;

  size_t size() const {
    if (!openssl_chain_.get())
      return 0;
    return sk_X509_num(openssl_chain_.get());
  }

  bool empty() const {
    return size() == 0;
  }

  X509* Get(size_t index) const {
    DCHECK_LT(index, size());
    return sk_X509_value(openssl_chain_.get(), index);
  }

 private:
  ScopedX509Stack openssl_chain_;
};

SSLClientSocketOpenSSL::PeerCertificateChain&
SSLClientSocketOpenSSL::PeerCertificateChain::operator=(
    const PeerCertificateChain& other) {
  if (this == &other)
    return *this;

  openssl_chain_.reset(X509_chain_up_ref(other.openssl_chain_.get()));
  return *this;
}

void SSLClientSocketOpenSSL::PeerCertificateChain::Reset(
    STACK_OF(X509)* chain) {
  openssl_chain_.reset(chain ? X509_chain_up_ref(chain) : NULL);
}

scoped_refptr<X509Certificate>
SSLClientSocketOpenSSL::PeerCertificateChain::AsOSChain() const {
#if defined(USE_OPENSSL_CERTS)
  // When OSCertHandle is typedef'ed to X509, this implementation does a short
  // cut to avoid converting back and forth between DER and the X509 struct.
  X509Certificate::OSCertHandles intermediates;
  for (size_t i = 1; i < sk_X509_num(openssl_chain_.get()); ++i) {
    intermediates.push_back(sk_X509_value(openssl_chain_.get(), i));
  }

  return make_scoped_refptr(X509Certificate::CreateFromHandle(
      sk_X509_value(openssl_chain_.get(), 0), intermediates));
#else
  // DER-encode the chain and convert to a platform certificate handle.
  std::vector<base::StringPiece> der_chain;
  for (size_t i = 0; i < sk_X509_num(openssl_chain_.get()); ++i) {
    X509* x = sk_X509_value(openssl_chain_.get(), i);
    base::StringPiece der;
    if (!x509_util::GetDER(x, &der))
      return NULL;
    der_chain.push_back(der);
  }

  return make_scoped_refptr(X509Certificate::CreateFromDERCertChain(der_chain));
#endif
}

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
}

// static
uint16 SSLClientSocket::GetMaxSupportedSSLVersion() {
  return SSL_PROTOCOL_VERSION_TLS1_2;
}

SSLClientSocketOpenSSL::SSLClientSocketOpenSSL(
    scoped_ptr<ClientSocketHandle> transport_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config,
    const SSLClientSocketContext& context)
    : transport_send_busy_(false),
      transport_recv_busy_(false),
      pending_read_error_(kNoPendingReadResult),
      pending_read_ssl_error_(SSL_ERROR_NONE),
      transport_read_error_(OK),
      transport_write_error_(OK),
      server_cert_chain_(new PeerCertificateChain(NULL)),
      completed_connect_(false),
      was_ever_used_(false),
      client_auth_cert_needed_(false),
      cert_verifier_(context.cert_verifier),
      cert_transparency_verifier_(context.cert_transparency_verifier),
      channel_id_service_(context.channel_id_service),
      ssl_(NULL),
      transport_bio_(NULL),
      transport_(transport_socket.Pass()),
      host_and_port_(host_and_port),
      ssl_config_(ssl_config),
      ssl_session_cache_shard_(context.ssl_session_cache_shard),
      trying_cached_session_(false),
      next_handshake_state_(STATE_NONE),
      npn_status_(kNextProtoUnsupported),
      channel_id_xtn_negotiated_(false),
      transport_security_state_(context.transport_security_state),
      policy_enforcer_(context.cert_policy_enforcer),
      net_log_(transport_->socket()->NetLog()),
      weak_factory_(this) {
}

SSLClientSocketOpenSSL::~SSLClientSocketOpenSSL() {
  Disconnect();
}

void SSLClientSocketOpenSSL::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  cert_request_info->host_and_port = host_and_port_;
  cert_request_info->cert_authorities = cert_authorities_;
  cert_request_info->cert_key_types = cert_key_types_;
}

SSLClientSocket::NextProtoStatus SSLClientSocketOpenSSL::GetNextProto(
    std::string* proto) {
  *proto = npn_proto_;
  return npn_status_;
}

ChannelIDService*
SSLClientSocketOpenSSL::GetChannelIDService() const {
  return channel_id_service_;
}

int SSLClientSocketOpenSSL::ExportKeyingMaterial(
    const base::StringPiece& label,
    bool has_context, const base::StringPiece& context,
    unsigned char* out, unsigned int outlen) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  int rv = SSL_export_keying_material(
      ssl_, out, outlen, label.data(), label.size(),
      reinterpret_cast<const unsigned char*>(context.data()),
      context.length(), context.length() > 0);

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
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int SSLClientSocketOpenSSL::Connect(const CompletionCallback& callback) {
  // It is an error to create an SSLClientSocket whose context has no
  // TransportSecurityState.
  DCHECK(transport_security_state_);

  net_log_.BeginEvent(NetLog::TYPE_SSL_CONNECT);

  // Set up new ssl object.
  int rv = Init();
  if (rv != OK) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_SSL_CONNECT, rv);
    return rv;
  }

  // Set SSL to client mode. Handshake happens in the loop below.
  SSL_set_connect_state(ssl_);

  // Enable fastradio padding.
  SSL_enable_fastradio_padding(ssl_,
                               ssl_config_.fastradio_padding_enabled &&
                                   ssl_config_.fastradio_padding_eligible);

  GotoState(STATE_HANDSHAKE);
  rv = DoHandshakeLoop(OK);
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
  recv_buffer_ = NULL;

  user_connect_callback_.Reset();
  user_read_callback_.Reset();
  user_write_callback_.Reset();
  user_read_buf_         = NULL;
  user_read_buf_len_     = 0;
  user_write_buf_        = NULL;
  user_write_buf_len_    = 0;

  pending_read_error_ = kNoPendingReadResult;
  pending_read_ssl_error_ = SSL_ERROR_NONE;
  pending_read_error_info_ = OpenSSLErrorInfo();

  transport_read_error_ = OK;
  transport_write_error_ = OK;

  server_cert_verify_result_.Reset();
  completed_connect_ = false;

  cert_authorities_.clear();
  cert_key_types_.clear();
  client_auth_cert_needed_ = false;

  start_cert_verification_time_ = base::TimeTicks();

  npn_status_ = kNextProtoUnsupported;
  npn_proto_.clear();

  channel_id_xtn_negotiated_ = false;
  channel_id_request_handle_.Cancel();
}

bool SSLClientSocketOpenSSL::IsConnected() const {
  // If the handshake has not yet completed.
  if (!completed_connect_)
    return false;
  // If an asynchronous operation is still pending.
  if (user_read_buf_.get() || user_write_buf_.get())
    return true;

  return transport_->socket()->IsConnected();
}

bool SSLClientSocketOpenSSL::IsConnectedAndIdle() const {
  // If the handshake has not yet completed.
  if (!completed_connect_)
    return false;
  // If an asynchronous operation is still pending.
  if (user_read_buf_.get() || user_write_buf_.get())
    return false;
  // If there is data waiting to be sent, or data read from the network that
  // has not yet been consumed.
  if (BIO_pending(transport_bio_) > 0 ||
      BIO_wpending(transport_bio_) > 0) {
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
  if (server_cert_chain_->empty())
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
  ssl_info->pinning_failure_log = pinning_failure_log_;

  AddSCTInfoToSSLInfo(ssl_info);

  const SSL_CIPHER* cipher = SSL_get_current_cipher(ssl_);
  CHECK(cipher);
  ssl_info->security_bits = SSL_CIPHER_get_bits(cipher, NULL);

  ssl_info->connection_status = EncodeSSLConnectionStatus(
      static_cast<uint16>(SSL_CIPHER_get_id(cipher)), 0 /* no compression */,
      GetNetSSLVersion(ssl_));

  if (!SSL_get_secure_renegotiation_support(ssl_))
    ssl_info->connection_status |= SSL_CONNECTION_NO_RENEGOTIATION_EXTENSION;

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

  int rv = DoReadLoop();

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

  int rv = DoWriteLoop();

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

int SSLClientSocketOpenSSL::SetReceiveBufferSize(int32 size) {
  return transport_->socket()->SetReceiveBufferSize(size);
}

int SSLClientSocketOpenSSL::SetSendBufferSize(int32 size) {
  return transport_->socket()->SetSendBufferSize(size);
}

int SSLClientSocketOpenSSL::Init() {
  DCHECK(!ssl_);
  DCHECK(!transport_bio_);

  SSLContext* context = SSLContext::GetInstance();
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  ssl_ = SSL_new(context->ssl_ctx());
  if (!ssl_ || !context->SetClientSocketForSSL(ssl_, this))
    return ERR_UNEXPECTED;

  if (!SSL_set_tlsext_host_name(ssl_, host_and_port_.host().c_str()))
    return ERR_UNEXPECTED;

  trying_cached_session_ = context->session_cache()->SetSSLSessionWithKey(
      ssl_, GetSessionCacheKey());

  send_buffer_ = new GrowableIOBuffer();
  send_buffer_->SetCapacity(KDefaultOpenSSLBufferSize);
  recv_buffer_ = new GrowableIOBuffer();
  recv_buffer_->SetCapacity(KDefaultOpenSSLBufferSize);

  BIO* ssl_bio = NULL;

  // SSLClientSocketOpenSSL retains ownership of the BIO buffers.
  if (!BIO_new_bio_pair_external_buf(
          &ssl_bio, send_buffer_->capacity(),
          reinterpret_cast<uint8_t*>(send_buffer_->data()), &transport_bio_,
          recv_buffer_->capacity(),
          reinterpret_cast<uint8_t*>(recv_buffer_->data())))
    return ERR_UNEXPECTED;
  DCHECK(ssl_bio);
  DCHECK(transport_bio_);

  // Install a callback on OpenSSL's end to plumb transport errors through.
  BIO_set_callback(ssl_bio, &SSLClientSocketOpenSSL::BIOCallback);
  BIO_set_callback_arg(ssl_bio, reinterpret_cast<char*>(this));

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
  bool tls1_1_enabled =
      (ssl_config_.version_min <= SSL_PROTOCOL_VERSION_TLS1_1 &&
       ssl_config_.version_max >= SSL_PROTOCOL_VERSION_TLS1_1);
  options.ConfigureFlag(SSL_OP_NO_TLSv1_1, !tls1_1_enabled);
  bool tls1_2_enabled =
      (ssl_config_.version_min <= SSL_PROTOCOL_VERSION_TLS1_2 &&
       ssl_config_.version_max >= SSL_PROTOCOL_VERSION_TLS1_2);
  options.ConfigureFlag(SSL_OP_NO_TLSv1_2, !tls1_2_enabled);

  options.ConfigureFlag(SSL_OP_NO_COMPRESSION, true);

  // TODO(joth): Set this conditionally, see http://crbug.com/55410
  options.ConfigureFlag(SSL_OP_LEGACY_SERVER_CONNECT, true);

  SSL_set_options(ssl_, options.set_mask);
  SSL_clear_options(ssl_, options.clear_mask);

  // Same as above, this time for the SSL mode.
  SslSetClearMask mode;

  mode.ConfigureFlag(SSL_MODE_RELEASE_BUFFERS, true);
  mode.ConfigureFlag(SSL_MODE_CBC_RECORD_SPLITTING, true);

  mode.ConfigureFlag(SSL_MODE_ENABLE_FALSE_START,
                     ssl_config_.false_start_enabled);

  mode.ConfigureFlag(SSL_MODE_SEND_FALLBACK_SCSV, ssl_config_.version_fallback);

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
  std::string command(
      "DEFAULT:!NULL:!aNULL:!SHA256:!SHA384:!aECDH:!AESGCM+AES256:!aPSK");
  // Walk through all the installed ciphers, seeing if any need to be
  // appended to the cipher removal |command|.
  for (size_t i = 0; i < sk_SSL_CIPHER_num(ciphers); ++i) {
    const SSL_CIPHER* cipher = sk_SSL_CIPHER_value(ciphers, i);
    const uint16 id = static_cast<uint16>(SSL_CIPHER_get_id(cipher));
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

  // Disable ECDSA cipher suites on platforms that do not support ECDSA
  // signed certificates, as servers may use the presence of such
  // ciphersuites as a hint to send an ECDSA certificate.
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    command.append(":!ECDSA");
#endif

  int rv = SSL_set_cipher_list(ssl_, command.c_str());
  // If this fails (rv = 0) it means there are no ciphers enabled on this SSL.
  // This will almost certainly result in the socket failing to complete the
  // handshake at which point the appropriate error is bubbled up to the client.
  LOG_IF(WARNING, rv != 1) << "SSL_set_cipher_list('" << command << "') "
                              "returned " << rv;

  // TLS channel ids.
  if (IsChannelIDEnabled(ssl_config_, channel_id_service_)) {
    SSL_enable_tls_channel_id(ssl_);
  }

  if (!ssl_config_.next_protos.empty()) {
    // Get list of ciphers that are enabled.
    STACK_OF(SSL_CIPHER)* enabled_ciphers = SSL_get_ciphers(ssl_);
    DCHECK(enabled_ciphers);
    std::vector<uint16> enabled_ciphers_vector;
    for (size_t i = 0; i < sk_SSL_CIPHER_num(enabled_ciphers); ++i) {
      const SSL_CIPHER* cipher = sk_SSL_CIPHER_value(enabled_ciphers, i);
      const uint16 id = static_cast<uint16>(SSL_CIPHER_get_id(cipher));
      enabled_ciphers_vector.push_back(id);
    }

    std::vector<uint8_t> wire_protos =
        SerializeNextProtos(ssl_config_.next_protos,
                            HasCipherAdequateForHTTP2(enabled_ciphers_vector) &&
                                IsTLSVersionAdequateForHTTP2(ssl_config_));
    SSL_set_alpn_protos(ssl_, wire_protos.empty() ? NULL : &wire_protos[0],
                        wire_protos.size());
  }

  if (ssl_config_.signed_cert_timestamps_enabled) {
    SSL_enable_signed_cert_timestamps(ssl_);
    SSL_enable_ocsp_stapling(ssl_);
  }

  if (IsOCSPStaplingSupported())
    SSL_enable_ocsp_stapling(ssl_);

  return OK;
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
  if (transport_read_error_ == OK && BufferRecv() != ERR_IO_PENDING)
    network_moved = true;
  return network_moved;
}

// TODO(vadimt): Remove including "base/threading/thread_local.h" and
// g_first_run_completed once crbug.com/424386 is fixed.
base::LazyInstance<base::ThreadLocalBoolean>::Leaky g_first_run_completed =
    LAZY_INSTANCE_INITIALIZER;

int SSLClientSocketOpenSSL::DoHandshake() {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int net_error = OK;

  int rv;

  // TODO(vadimt): Leave only 1 call to SSL_do_handshake once crbug.com/424386
  // is fixed.
  if (ssl_config_.send_client_cert && ssl_config_.client_cert.get()) {
    // TODO(vadimt): Remove ScopedTracker below once crbug.com/424386 is fixed.
    tracked_objects::ScopedTracker tracking_profile1(
        FROM_HERE_WITH_EXPLICIT_FUNCTION("424386 DoHandshake_WithCert"));

    rv = SSL_do_handshake(ssl_);
  } else {
    if (g_first_run_completed.Get().Get()) {
      // TODO(vadimt): Remove ScopedTracker below once crbug.com/424386 is
      // fixed.
      tracked_objects::ScopedTracker tracking_profile1(
          FROM_HERE_WITH_EXPLICIT_FUNCTION(
              "424386 DoHandshake_WithoutCert Not First"));

      rv = SSL_do_handshake(ssl_);
    } else {
      g_first_run_completed.Get().Set(true);

      // TODO(vadimt): Remove ScopedTracker below once crbug.com/424386 is
      // fixed.
      tracked_objects::ScopedTracker tracking_profile1(
          FROM_HERE_WITH_EXPLICIT_FUNCTION(
              "424386 DoHandshake_WithoutCert First"));

      rv = SSL_do_handshake(ssl_);
    }
  }

  if (client_auth_cert_needed_) {
    // TODO(vadimt): Remove ScopedTracker below once crbug.com/424386 is fixed.
    tracked_objects::ScopedTracker tracking_profile2(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "424386 SSLClientSocketOpenSSL::DoHandshake2"));

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
    // TODO(vadimt): Remove ScopedTracker below once crbug.com/424386 is fixed.
    tracked_objects::ScopedTracker tracking_profile3(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "424386 SSLClientSocketOpenSSL::DoHandshake3"));

    if (trying_cached_session_ && logging::DEBUG_MODE) {
      DVLOG(2) << "Result of session reuse for " << host_and_port_.ToString()
               << " is: " << (SSL_session_reused(ssl_) ? "Success" : "Fail");
    }

    if (ssl_config_.version_fallback &&
        ssl_config_.version_max < ssl_config_.version_fallback_min) {
      return ERR_SSL_FALLBACK_BEYOND_MINIMUM_VERSION;
    }

    // SSL handshake is completed. If NPN wasn't negotiated, see if ALPN was.
    if (npn_status_ == kNextProtoUnsupported) {
      const uint8_t* alpn_proto = NULL;
      unsigned alpn_len = 0;
      SSL_get0_alpn_selected(ssl_, &alpn_proto, &alpn_len);
      if (alpn_len > 0) {
        npn_proto_.assign(reinterpret_cast<const char*>(alpn_proto), alpn_len);
        npn_status_ = kNextProtoNegotiated;
        set_negotiation_extension(kExtensionALPN);
      }
    }

    RecordChannelIDSupport(channel_id_service_,
                           channel_id_xtn_negotiated_,
                           ssl_config_.channel_id_enabled,
                           crypto::ECPrivateKey::IsSupported());

    // Only record OCSP histograms if OCSP was requested.
    if (ssl_config_.signed_cert_timestamps_enabled ||
        IsOCSPStaplingSupported()) {
      const uint8_t* ocsp_response;
      size_t ocsp_response_len;
      SSL_get0_ocsp_response(ssl_, &ocsp_response, &ocsp_response_len);

      set_stapled_ocsp_response_received(ocsp_response_len != 0);
      UMA_HISTOGRAM_BOOLEAN("Net.OCSPResponseStapled", ocsp_response_len != 0);
    }

    const uint8_t* sct_list;
    size_t sct_list_len;
    SSL_get0_signed_cert_timestamp_list(ssl_, &sct_list, &sct_list_len);
    set_signed_cert_timestamps_received(sct_list_len != 0);

    // Verify the certificate.
    UpdateServerCert();
    GotoState(STATE_VERIFY_CERT);
  } else {
    // TODO(vadimt): Remove ScopedTracker below once crbug.com/424386 is fixed.
    tracked_objects::ScopedTracker tracking_profile4(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "424386 SSLClientSocketOpenSSL::DoHandshake4"));

    int ssl_error = SSL_get_error(ssl_, rv);

    if (ssl_error == SSL_ERROR_WANT_CHANNEL_ID_LOOKUP) {
      // The server supports channel ID. Stop to look one up before returning to
      // the handshake.
      channel_id_xtn_negotiated_ = true;
      GotoState(STATE_CHANNEL_ID_LOOKUP);
      return OK;
    }

    OpenSSLErrorInfo error_info;
    net_error = MapOpenSSLErrorWithDetails(ssl_error, err_tracer, &error_info);

    // If not done, stay in this state
    if (net_error == ERR_IO_PENDING) {
      GotoState(STATE_HANDSHAKE);
    } else {
      LOG(ERROR) << "handshake failed; returned " << rv
                 << ", SSL error code " << ssl_error
                 << ", net_error " << net_error;
      net_log_.AddEvent(
          NetLog::TYPE_SSL_HANDSHAKE_ERROR,
          CreateNetLogOpenSSLErrorCallback(net_error, ssl_error, error_info));
    }
  }
  return net_error;
}

int SSLClientSocketOpenSSL::DoChannelIDLookup() {
  net_log_.AddEvent(NetLog::TYPE_SSL_CHANNEL_ID_REQUESTED);
  GotoState(STATE_CHANNEL_ID_LOOKUP_COMPLETE);
  return channel_id_service_->GetOrCreateChannelID(
      host_and_port_.host(),
      &channel_id_private_key_,
      &channel_id_cert_,
      base::Bind(&SSLClientSocketOpenSSL::OnHandshakeIOComplete,
                 base::Unretained(this)),
      &channel_id_request_handle_);
}

int SSLClientSocketOpenSSL::DoChannelIDLookupComplete(int result) {
  if (result < 0)
    return result;

  DCHECK_LT(0u, channel_id_private_key_.size());
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
          ChannelIDService::kEPKIPassword,
          encrypted_private_key_info,
          subject_public_key_info));
  if (!ec_private_key) {
    LOG(ERROR) << "Failed to import Channel ID.";
    return ERR_CHANNEL_ID_IMPORT_FAILED;
  }

  // Hand the key to OpenSSL. Check for error in case OpenSSL rejects the key
  // type.
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  int rv = SSL_set1_tls_channel_id(ssl_, ec_private_key->key());
  if (!rv) {
    LOG(ERROR) << "Failed to set Channel ID.";
    int err = SSL_get_error(ssl_, rv);
    return MapOpenSSLError(err, err_tracer);
  }

  // Return to the handshake.
  set_channel_id_sent(true);
  net_log_.AddEvent(NetLog::TYPE_SSL_CHANNEL_ID_PROVIDED);
  GotoState(STATE_HANDSHAKE);
  return OK;
}

int SSLClientSocketOpenSSL::DoVerifyCert(int result) {
  DCHECK(!server_cert_chain_->empty());
  DCHECK(start_cert_verification_time_.is_null());

  GotoState(STATE_VERIFY_CERT_COMPLETE);

  // If the certificate is bad and has been previously accepted, use
  // the previous status and bypass the error.
  base::StringPiece der_cert;
  if (!x509_util::GetDER(server_cert_chain_->Get(0), &der_cert)) {
    NOTREACHED();
    return ERR_CERT_INVALID;
  }
  CertStatus cert_status;
  if (ssl_config_.IsAllowedBadCert(der_cert, &cert_status)) {
    VLOG(1) << "Received an expected bad cert with status: " << cert_status;
    server_cert_verify_result_.Reset();
    server_cert_verify_result_.cert_status = cert_status;
    server_cert_verify_result_.verified_cert = server_cert_;
    return OK;
  }

  // When running in a sandbox, it may not be possible to create an
  // X509Certificate*, as that may depend on OS functionality blocked
  // in the sandbox.
  if (!server_cert_.get()) {
    server_cert_verify_result_.Reset();
    server_cert_verify_result_.cert_status = CERT_STATUS_INVALID;
    return ERR_CERT_INVALID;
  }

  start_cert_verification_time_ = base::TimeTicks::Now();

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
      // TODO(davidben): Route the CRLSet through SSLConfig so
      // SSLClientSocket doesn't depend on SSLConfigService.
      SSLConfigService::GetCRLSet().get(),
      &server_cert_verify_result_,
      base::Bind(&SSLClientSocketOpenSSL::OnHandshakeIOComplete,
                 base::Unretained(this)),
      net_log_);
}

int SSLClientSocketOpenSSL::DoVerifyCertComplete(int result) {
  verifier_.reset();

  if (!start_cert_verification_time_.is_null()) {
    base::TimeDelta verify_time =
        base::TimeTicks::Now() - start_cert_verification_time_;
    if (result == OK) {
      UMA_HISTOGRAM_TIMES("Net.SSLCertVerificationTime", verify_time);
    } else {
      UMA_HISTOGRAM_TIMES("Net.SSLCertVerificationTimeError", verify_time);
    }
  }

  if (result == OK) {
    if (SSL_session_reused(ssl_)) {
      // Record whether or not the server tried to resume a session for a
      // different version. See https://crbug.com/441456.
      UMA_HISTOGRAM_BOOLEAN(
          "Net.SSLSessionVersionMatch",
          SSL_version(ssl_) == SSL_get_session(ssl_)->ssl_version);
    }
  }

  const CertStatus cert_status = server_cert_verify_result_.cert_status;
  if (transport_security_state_ &&
      (result == OK ||
       (IsCertificateError(result) && IsCertStatusMinorError(cert_status))) &&
      !transport_security_state_->CheckPublicKeyPins(
          host_and_port_.host(),
          server_cert_verify_result_.is_issued_by_known_root,
          server_cert_verify_result_.public_key_hashes,
          &pinning_failure_log_)) {
    result = ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN;
  }

  if (result == OK) {
    // Only check Certificate Transparency if there were no other errors with
    // the connection.
    VerifyCT();

    // TODO(joth): Work out if we need to remember the intermediate CA certs
    // when the server sends them to us, and do so here.
    SSLContext::GetInstance()->session_cache()->MarkSSLSessionAsGood(ssl_);
  } else {
    DVLOG(1) << "DoVerifyCertComplete error " << ErrorToString(result)
             << " (" << result << ")";
  }

  completed_connect_ = true;
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

void SSLClientSocketOpenSSL::UpdateServerCert() {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/424386 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "424386 SSLClientSocketOpenSSL::UpdateServerCert"));

  server_cert_chain_->Reset(SSL_get_peer_cert_chain(ssl_));

  // TODO(vadimt): Remove ScopedTracker below once crbug.com/424386 is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "424386 SSLClientSocketOpenSSL::UpdateServerCert1"));
  server_cert_ = server_cert_chain_->AsOSChain();

  if (server_cert_.get()) {
    net_log_.AddEvent(
        NetLog::TYPE_SSL_CERTIFICATES_RECEIVED,
        base::Bind(&NetLogX509CertificateCallback,
                   base::Unretained(server_cert_.get())));

    // TODO(rsleevi): Plumb an OCSP response into the Mac system library and
    // update IsOCSPStaplingSupported for Mac. https://crbug.com/430714
    if (IsOCSPStaplingSupported()) {
#if defined(OS_WIN)
      // TODO(vadimt): Remove ScopedTracker below once crbug.com/424386 is
      // fixed.
      tracked_objects::ScopedTracker tracking_profile2(
          FROM_HERE_WITH_EXPLICIT_FUNCTION(
              "424386 SSLClientSocketOpenSSL::UpdateServerCert2"));

      const uint8_t* ocsp_response_raw;
      size_t ocsp_response_len;
      SSL_get0_ocsp_response(ssl_, &ocsp_response_raw, &ocsp_response_len);

      CRYPT_DATA_BLOB ocsp_response_blob;
      ocsp_response_blob.cbData = ocsp_response_len;
      ocsp_response_blob.pbData = const_cast<BYTE*>(ocsp_response_raw);
      BOOL ok = CertSetCertificateContextProperty(
          server_cert_->os_cert_handle(),
          CERT_OCSP_RESPONSE_PROP_ID,
          CERT_SET_PROPERTY_IGNORE_PERSIST_ERROR_FLAG,
          &ocsp_response_blob);
      if (!ok) {
        VLOG(1) << "Failed to set OCSP response property: "
                << GetLastError();
      }
#else
      NOTREACHED();
#endif
    }
  }
}

void SSLClientSocketOpenSSL::VerifyCT() {
  if (!cert_transparency_verifier_)
    return;

  const uint8_t* ocsp_response_raw;
  size_t ocsp_response_len;
  SSL_get0_ocsp_response(ssl_, &ocsp_response_raw, &ocsp_response_len);
  std::string ocsp_response;
  if (ocsp_response_len > 0) {
    ocsp_response.assign(reinterpret_cast<const char*>(ocsp_response_raw),
                         ocsp_response_len);
  }

  const uint8_t* sct_list_raw;
  size_t sct_list_len;
  SSL_get0_signed_cert_timestamp_list(ssl_, &sct_list_raw, &sct_list_len);
  std::string sct_list;
  if (sct_list_len > 0)
    sct_list.assign(reinterpret_cast<const char*>(sct_list_raw), sct_list_len);

  // Note that this is a completely synchronous operation: The CT Log Verifier
  // gets all the data it needs for SCT verification and does not do any
  // external communication.
  cert_transparency_verifier_->Verify(
      server_cert_verify_result_.verified_cert.get(), ocsp_response, sct_list,
      &ct_verify_result_, net_log_);

  if (!policy_enforcer_) {
    server_cert_verify_result_.cert_status &= ~CERT_STATUS_IS_EV;
  } else {
    if (server_cert_verify_result_.cert_status & CERT_STATUS_IS_EV) {
      scoped_refptr<ct::EVCertsWhitelist> ev_whitelist =
          SSLConfigService::GetEVCertsWhitelist();
      if (!policy_enforcer_->DoesConformToCTEVPolicy(
              server_cert_verify_result_.verified_cert.get(),
              ev_whitelist.get(), ct_verify_result_, net_log_)) {
        // TODO(eranm): Log via the BoundNetLog, see crbug.com/437766
        VLOG(1) << "EV certificate for "
                << server_cert_verify_result_.verified_cert->subject()
                       .GetDisplayName()
                << " does not conform to CT policy, removing EV status.";
        server_cert_verify_result_.cert_status &= ~CERT_STATUS_IS_EV;
      }
    }
  }
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

  int rv = DoReadLoop();
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
      case STATE_CHANNEL_ID_LOOKUP:
        DCHECK_EQ(OK, rv);
        rv = DoChannelIDLookup();
       break;
      case STATE_CHANNEL_ID_LOOKUP_COMPLETE:
        rv = DoChannelIDLookupComplete(rv);
        break;
      case STATE_VERIFY_CERT:
        DCHECK_EQ(OK, rv);
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

int SSLClientSocketOpenSSL::DoReadLoop() {
  bool network_moved;
  int rv;
  do {
    rv = DoPayloadRead();
    network_moved = DoTransportIO();
  } while (rv == ERR_IO_PENDING && network_moved);

  return rv;
}

int SSLClientSocketOpenSSL::DoWriteLoop() {
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
    } else {
      net_log_.AddEvent(
          NetLog::TYPE_SSL_READ_ERROR,
          CreateNetLogOpenSSLErrorCallback(rv, pending_read_ssl_error_,
                                           pending_read_error_info_));
    }
    pending_read_ssl_error_ = SSL_ERROR_NONE;
    pending_read_error_info_ = OpenSSLErrorInfo();
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
    } else if (*next_result <= 0) {
      // A zero return from SSL_read may mean any of:
      // - The underlying BIO_read returned 0.
      // - The peer sent a close_notify.
      // - Any arbitrary error. https://crbug.com/466303
      //
      // TransportReadComplete converts the first to an ERR_CONNECTION_CLOSED
      // error, so it does not occur. The second and third are distinguished by
      // SSL_ERROR_ZERO_RETURN.
      pending_read_ssl_error_ = SSL_get_error(ssl_, *next_result);
      if (pending_read_ssl_error_ == SSL_ERROR_ZERO_RETURN) {
        *next_result = 0;
      } else {
        *next_result = MapOpenSSLErrorWithDetails(
            pending_read_ssl_error_, err_tracer, &pending_read_error_info_);
      }

      // Many servers do not reliably send a close_notify alert when shutting
      // down a connection, and instead terminate the TCP connection. This is
      // reported as ERR_CONNECTION_CLOSED. Because of this, map the unclean
      // shutdown to a graceful EOF, instead of treating it as an error as it
      // should be.
      if (*next_result == ERR_CONNECTION_CLOSED)
        *next_result = 0;

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
  } else if (rv != ERR_IO_PENDING) {
    net_log_.AddEvent(
        NetLog::TYPE_SSL_READ_ERROR,
        CreateNetLogOpenSSLErrorCallback(rv, pending_read_ssl_error_,
                                         pending_read_error_info_));
    pending_read_ssl_error_ = SSL_ERROR_NONE;
    pending_read_error_info_ = OpenSSLErrorInfo();
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

  int ssl_error = SSL_get_error(ssl_, rv);
  OpenSSLErrorInfo error_info;
  int net_error = MapOpenSSLErrorWithDetails(ssl_error, err_tracer,
                                             &error_info);

  if (net_error != ERR_IO_PENDING) {
    net_log_.AddEvent(
        NetLog::TYPE_SSL_WRITE_ERROR,
        CreateNetLogOpenSSLErrorCallback(net_error, ssl_error, error_info));
  }
  return net_error;
}

int SSLClientSocketOpenSSL::BufferSend(void) {
  if (transport_send_busy_)
    return ERR_IO_PENDING;

  size_t buffer_read_offset;
  uint8_t* read_buf;
  size_t max_read;
  int status = BIO_zero_copy_get_read_buf(transport_bio_, &read_buf,
                                          &buffer_read_offset, &max_read);
  DCHECK_EQ(status, 1);  // Should never fail.
  if (!max_read)
    return 0;  // Nothing pending in the OpenSSL write BIO.
  CHECK_EQ(read_buf, reinterpret_cast<uint8_t*>(send_buffer_->StartOfBuffer()));
  CHECK_LT(buffer_read_offset, static_cast<size_t>(send_buffer_->capacity()));
  send_buffer_->set_offset(buffer_read_offset);

  int rv = transport_->socket()->Write(
      send_buffer_.get(), max_read,
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

  size_t buffer_write_offset;
  uint8_t* write_buf;
  size_t max_write;
  int status = BIO_zero_copy_get_write_buf(transport_bio_, &write_buf,
                                           &buffer_write_offset, &max_write);
  DCHECK_EQ(status, 1);  // Should never fail.
  if (!max_write)
    return ERR_IO_PENDING;

  CHECK_EQ(write_buf,
           reinterpret_cast<uint8_t*>(recv_buffer_->StartOfBuffer()));
  CHECK_LT(buffer_write_offset, static_cast<size_t>(recv_buffer_->capacity()));

  recv_buffer_->set_offset(buffer_write_offset);
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
  TransportWriteComplete(result);
  OnSendComplete(result);
}

void SSLClientSocketOpenSSL::BufferRecvComplete(int result) {
  result = TransportReadComplete(result);
  OnRecvComplete(result);
}

void SSLClientSocketOpenSSL::TransportWriteComplete(int result) {
  DCHECK(ERR_IO_PENDING != result);
  int bytes_written = 0;
  if (result < 0) {
    // Record the error. Save it to be reported in a future read or write on
    // transport_bio_'s peer.
    transport_write_error_ = result;
  } else {
    bytes_written = result;
  }
  DCHECK_GE(send_buffer_->RemainingCapacity(), bytes_written);
  int ret = BIO_zero_copy_get_read_buf_done(transport_bio_, bytes_written);
  DCHECK_EQ(1, ret);
  transport_send_busy_ = false;
}

int SSLClientSocketOpenSSL::TransportReadComplete(int result) {
  DCHECK(ERR_IO_PENDING != result);
  // If an EOF, canonicalize to ERR_CONNECTION_CLOSED here so MapOpenSSLError
  // does not report success.
  if (result == 0)
    result = ERR_CONNECTION_CLOSED;
  int bytes_read = 0;
  if (result < 0) {
    DVLOG(1) << "TransportReadComplete result " << result;
    // Received an error. Save it to be reported in a future read on
    // transport_bio_'s peer.
    transport_read_error_ = result;
  } else {
    bytes_read = result;
  }
  DCHECK_GE(recv_buffer_->RemainingCapacity(), bytes_read);
  int ret = BIO_zero_copy_get_write_buf_done(transport_bio_, bytes_read);
  DCHECK_EQ(1, ret);
  transport_recv_busy_ = false;
  return result;
}

int SSLClientSocketOpenSSL::ClientCertRequestCallback(SSL* ssl) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/424386 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "424386 SSLClientSocketOpenSSL::ClientCertRequestCallback"));

  DVLOG(3) << "OpenSSL ClientCertRequestCallback called";
  DCHECK(ssl == ssl_);

  net_log_.AddEvent(NetLog::TYPE_SSL_CLIENT_CERT_REQUESTED);

  // Clear any currently configured certificates.
  SSL_certs_clear(ssl_);

#if defined(OS_IOS)
  // TODO(droger): Support client auth on iOS. See http://crbug.com/145954).
  LOG(WARNING) << "Client auth is not supported";
#else  // !defined(OS_IOS)
  if (!ssl_config_.send_client_cert) {
    // First pass: we know that a client certificate is needed, but we do not
    // have one at hand.
    client_auth_cert_needed_ = true;
    STACK_OF(X509_NAME) *authorities = SSL_get_client_CA_list(ssl);
    for (size_t i = 0; i < sk_X509_NAME_num(authorities); i++) {
      X509_NAME *ca_name = (X509_NAME *)sk_X509_NAME_value(authorities, i);
      unsigned char* str = NULL;
      int length = i2d_X509_NAME(ca_name, &str);
      cert_authorities_.push_back(std::string(
          reinterpret_cast<const char*>(str),
          static_cast<size_t>(length)));
      OPENSSL_free(str);
    }

    const unsigned char* client_cert_types;
    size_t num_client_cert_types =
        SSL_get0_certificate_types(ssl, &client_cert_types);
    for (size_t i = 0; i < num_client_cert_types; i++) {
      cert_key_types_.push_back(
          static_cast<SSLClientCertType>(client_cert_types[i]));
    }

    return -1;  // Suspends handshake.
  }

  // Second pass: a client certificate should have been selected.
  if (ssl_config_.client_cert.get()) {
    ScopedX509 leaf_x509 =
        OSCertHandleToOpenSSL(ssl_config_.client_cert->os_cert_handle());
    if (!leaf_x509) {
      LOG(WARNING) << "Failed to import certificate";
      OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_CERT_BAD_FORMAT);
      return -1;
    }

    ScopedX509Stack chain = OSCertHandlesToOpenSSL(
        ssl_config_.client_cert->GetIntermediateCertificates());
    if (!chain) {
      LOG(WARNING) << "Failed to import intermediate certificates";
      OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_CERT_BAD_FORMAT);
      return -1;
    }

    // TODO(davidben): With Linux client auth support, this should be
    // conditioned on OS_ANDROID and then, with https://crbug.com/394131,
    // removed altogether. OpenSSLClientKeyStore is mostly an artifact of the
    // net/ client auth API lacking a private key handle.
#if defined(USE_OPENSSL_CERTS)
    crypto::ScopedEVP_PKEY privkey =
        OpenSSLClientKeyStore::GetInstance()->FetchClientCertPrivateKey(
            ssl_config_.client_cert.get());
#else  // !defined(USE_OPENSSL_CERTS)
    crypto::ScopedEVP_PKEY privkey =
        FetchClientCertPrivateKey(ssl_config_.client_cert.get());
#endif  // defined(USE_OPENSSL_CERTS)
    if (!privkey) {
      // Could not find the private key. Fail the handshake and surface an
      // appropriate error to the caller.
      LOG(WARNING) << "Client cert found without private key";
      OpenSSLPutNetError(FROM_HERE, ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY);
      return -1;
    }

    if (!SSL_use_certificate(ssl_, leaf_x509.get()) ||
        !SSL_use_PrivateKey(ssl_, privkey.get()) ||
        !SSL_set1_chain(ssl_, chain.get())) {
      LOG(WARNING) << "Failed to set client certificate";
      return -1;
    }

    int cert_count = 1 + sk_X509_num(chain.get());
    net_log_.AddEvent(NetLog::TYPE_SSL_CLIENT_CERT_PROVIDED,
                      NetLog::IntegerCallback("cert_count", cert_count));
    return 1;
  }
#endif  // defined(OS_IOS)

  // Send no client certificate.
  net_log_.AddEvent(NetLog::TYPE_SSL_CLIENT_CERT_PROVIDED,
                    NetLog::IntegerCallback("cert_count", 0));
  return 1;
}

int SSLClientSocketOpenSSL::CertVerifyCallback(X509_STORE_CTX* store_ctx) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/424386 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "424386 SSLClientSocketOpenSSL::CertVerifyCallback"));

  if (!completed_connect_) {
    // If the first handshake hasn't completed then we accept any certificates
    // because we verify after the handshake.
    return 1;
  }

  // Disallow the server certificate to change in a renegotiation.
  if (server_cert_chain_->empty()) {
    LOG(ERROR) << "Received invalid certificate chain between handshakes";
    return 0;
  }
  base::StringPiece old_der, new_der;
  if (store_ctx->cert == NULL ||
      !x509_util::GetDER(server_cert_chain_->Get(0), &old_der) ||
      !x509_util::GetDER(store_ctx->cert, &new_der)) {
    LOG(ERROR) << "Failed to encode certificates";
    return 0;
  }
  if (old_der != new_der) {
    LOG(ERROR) << "Server certificate changed between handshakes";
    return 0;
  }

  return 1;
}

// SelectNextProtoCallback is called by OpenSSL during the handshake. If the
// server supports NPN, selects a protocol from the list that the server
// provides. According to third_party/openssl/openssl/ssl/ssl_lib.c, the
// callback can assume that |in| is syntactically valid.
int SSLClientSocketOpenSSL::SelectNextProtoCallback(unsigned char** out,
                                                    unsigned char* outlen,
                                                    const unsigned char* in,
                                                    unsigned int inlen) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/424386 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "424386 SSLClientSocketOpenSSL::SelectNextProtoCallback"));

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
    for (NextProto next_proto : ssl_config_.next_protos) {
      const std::string proto = NextProtoToString(next_proto);
      if (in[i] == proto.size() &&
          memcmp(&in[i + 1], proto.data(), in[i]) == 0) {
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
    // NextProtoToString returns a pointer to a static string.
    const char* proto = NextProtoToString(ssl_config_.next_protos[0]);
    *out = reinterpret_cast<unsigned char*>(const_cast<char*>(proto));
    *outlen = strlen(proto);
  }

  npn_proto_.assign(reinterpret_cast<const char*>(*out), *outlen);
  DVLOG(2) << "next protocol: '" << npn_proto_ << "' status: " << npn_status_;
  set_negotiation_extension(kExtensionNPN);
  return SSL_TLSEXT_ERR_OK;
}

long SSLClientSocketOpenSSL::MaybeReplayTransportError(
    BIO *bio,
    int cmd,
    const char *argp, int argi, long argl,
    long retvalue) {
  if (cmd == (BIO_CB_READ|BIO_CB_RETURN) && retvalue <= 0) {
    // If there is no more data in the buffer, report any pending errors that
    // were observed. Note that both the readbuf and the writebuf are checked
    // for errors, since the application may have encountered a socket error
    // while writing that would otherwise not be reported until the application
    // attempted to write again - which it may never do. See
    // https://crbug.com/249848.
    if (transport_read_error_ != OK) {
      OpenSSLPutNetError(FROM_HERE, transport_read_error_);
      return -1;
    }
    if (transport_write_error_ != OK) {
      OpenSSLPutNetError(FROM_HERE, transport_write_error_);
      return -1;
    }
  } else if (cmd == BIO_CB_WRITE) {
    // Because of the write buffer, this reports a failure from the previous
    // write payload. If the current payload fails to write, the error will be
    // reported in a future write or read to |bio|.
    if (transport_write_error_ != OK) {
      OpenSSLPutNetError(FROM_HERE, transport_write_error_);
      return -1;
    }
  }
  return retvalue;
}

// static
long SSLClientSocketOpenSSL::BIOCallback(
    BIO *bio,
    int cmd,
    const char *argp, int argi, long argl,
    long retvalue) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/424386 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "424386 SSLClientSocketOpenSSL::BIOCallback"));

  SSLClientSocketOpenSSL* socket = reinterpret_cast<SSLClientSocketOpenSSL*>(
      BIO_get_callback_arg(bio));
  CHECK(socket);
  return socket->MaybeReplayTransportError(
      bio, cmd, argp, argi, argl, retvalue);
}

void SSLClientSocketOpenSSL::AddSCTInfoToSSLInfo(SSLInfo* ssl_info) const {
  for (ct::SCTList::const_iterator iter =
       ct_verify_result_.verified_scts.begin();
       iter != ct_verify_result_.verified_scts.end(); ++iter) {
    ssl_info->signed_certificate_timestamps.push_back(
        SignedCertificateTimestampAndStatus(*iter, ct::SCT_STATUS_OK));
  }
  for (ct::SCTList::const_iterator iter =
       ct_verify_result_.invalid_scts.begin();
       iter != ct_verify_result_.invalid_scts.end(); ++iter) {
    ssl_info->signed_certificate_timestamps.push_back(
        SignedCertificateTimestampAndStatus(*iter, ct::SCT_STATUS_INVALID));
  }
  for (ct::SCTList::const_iterator iter =
       ct_verify_result_.unknown_logs_scts.begin();
       iter != ct_verify_result_.unknown_logs_scts.end(); ++iter) {
    ssl_info->signed_certificate_timestamps.push_back(
        SignedCertificateTimestampAndStatus(*iter,
                                            ct::SCT_STATUS_LOG_UNKNOWN));
  }
}

std::string SSLClientSocketOpenSSL::GetSessionCacheKey() const {
  std::string result = host_and_port_.ToString();
  result.append("/");
  result.append(ssl_session_cache_shard_);

  // Shard the session cache based on maximum protocol version. This causes
  // fallback connections to use a separate session cache.
  result.append("/");
  switch (ssl_config_.version_max) {
    case SSL_PROTOCOL_VERSION_SSL3:
      result.append("ssl3");
      break;
    case SSL_PROTOCOL_VERSION_TLS1:
      result.append("tls1");
      break;
    case SSL_PROTOCOL_VERSION_TLS1_1:
      result.append("tls1.1");
      break;
    case SSL_PROTOCOL_VERSION_TLS1_2:
      result.append("tls1.2");
      break;
    default:
      NOTREACHED();
  }

  return result;
}

scoped_refptr<X509Certificate>
SSLClientSocketOpenSSL::GetUnverifiedServerCertificateChain() const {
  return server_cert_;
}

}  // namespace net
