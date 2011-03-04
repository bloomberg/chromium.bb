// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_host_info.h"

#include "base/metrics/histogram.h"
#include "base/pickle.h"
#include "base/string_piece.h"
#include "net/base/dns_util.h"
#include "net/base/dnsrr_resolver.h"
#include "net/base/ssl_config_service.h"
#include "net/base/x509_certificate.h"
#include "net/socket/ssl_client_socket.h"

namespace net {

SSLHostInfo::State::State()
    : npn_valid(false),
      npn_status(SSLClientSocket::kNextProtoUnsupported) {
}

SSLHostInfo::State::~State() {}

void SSLHostInfo::State::Clear() {
  certs.clear();
  server_hello.clear();
  npn_valid = false;
}

SSLHostInfo::SSLHostInfo(
    const std::string& hostname,
    const SSLConfig& ssl_config,
    CertVerifier* cert_verifier)
    : cert_verification_complete_(false),
      cert_verification_error_(ERR_CERT_INVALID),
      hostname_(hostname),
      cert_parsing_failed_(false),
      cert_verification_callback_(NULL),
      rev_checking_enabled_(ssl_config.rev_checking_enabled),
      verify_ev_cert_(ssl_config.verify_ev_cert),
      verifier_(cert_verifier),
      callback_(new CancelableCompletionCallback<SSLHostInfo>(
                        ALLOW_THIS_IN_INITIALIZER_LIST(this),
                        &SSLHostInfo::VerifyCallback)),
      dnsrr_resolver_(NULL),
      dns_callback_(NULL),
      dns_handle_(DnsRRResolver::kInvalidHandle) {
  state_.npn_valid = false;
}

SSLHostInfo::~SSLHostInfo() {
  if (dns_handle_ != DnsRRResolver::kInvalidHandle) {
    dnsrr_resolver_->CancelResolve(dns_handle_);
    delete dns_callback_;
  }
}

void SSLHostInfo::StartDnsLookup(DnsRRResolver* dnsrr_resolver) {
  dnsrr_resolver_ = dnsrr_resolver;
  // Note: currently disabled.
}

const SSLHostInfo::State& SSLHostInfo::state() const {
  return state_;
}

SSLHostInfo::State* SSLHostInfo::mutable_state() {
  return &state_;
}

bool SSLHostInfo::Parse(const std::string& data) {
  State* state = mutable_state();

  state->Clear();
  cert_verification_complete_ = false;

  bool r = ParseInner(data);
  if (!r)
    state->Clear();
  return r;
}

bool SSLHostInfo::ParseInner(const std::string& data) {
  State* state = mutable_state();

  Pickle p(data.data(), data.size());
  void* iter = NULL;

  int num_der_certs;
  if (!p.ReadInt(&iter, &num_der_certs) ||
      num_der_certs < 0) {
    return false;
  }

  for (int i = 0; i < num_der_certs; i++) {
    std::string der_cert;
    if (!p.ReadString(&iter, &der_cert))
      return false;
    state->certs.push_back(der_cert);
  }

  if (!p.ReadString(&iter, &state->server_hello))
    return false;

  if (!p.ReadBool(&iter, &state->npn_valid))
    return false;

  if (state->npn_valid) {
    int status;
    if (!p.ReadInt(&iter, &status) ||
        !p.ReadString(&iter, &state->npn_protocol)) {
      return false;
    }
    state->npn_status = static_cast<SSLClientSocket::NextProtoStatus>(status);
  }

  if (!state->certs.empty()) {
    std::vector<base::StringPiece> der_certs(state->certs.size());
    for (size_t i = 0; i < state->certs.size(); i++)
      der_certs[i] = state->certs[i];
    cert_ = X509Certificate::CreateFromDERCertChain(der_certs);
    if (cert_.get()) {
      int flags = 0;
      if (verify_ev_cert_)
        flags |= X509Certificate::VERIFY_EV_CERT;
      if (rev_checking_enabled_)
        flags |= X509Certificate::VERIFY_REV_CHECKING_ENABLED;
      VLOG(1) << "Kicking off verification for " << hostname_;
      verification_start_time_ = base::TimeTicks::Now();
      verification_end_time_ = base::TimeTicks();
      int rv = verifier_.Verify(cert_.get(), hostname_, flags,
                           &cert_verify_result_, callback_);
      if (rv != ERR_IO_PENDING)
        VerifyCallback(rv);
    } else {
      cert_parsing_failed_ = true;
      DCHECK(!cert_verification_callback_);
    }
  }

  return true;
}

std::string SSLHostInfo::Serialize() const {
  Pickle p(sizeof(Pickle::Header));

  static const unsigned kMaxCertificatesSize = 32 * 1024;
  unsigned der_certs_size = 0;

  for (std::vector<std::string>::const_iterator
       i = state_.certs.begin(); i != state_.certs.end(); i++) {
    der_certs_size += i->size();
  }

  // We don't care to save the certificates over a certain size.
  if (der_certs_size > kMaxCertificatesSize)
    return "";

  if (!p.WriteInt(state_.certs.size()))
    return "";

  for (std::vector<std::string>::const_iterator
       i = state_.certs.begin(); i != state_.certs.end(); i++) {
    if (!p.WriteString(*i))
      return "";
  }

  if (!p.WriteString(state_.server_hello) ||
      !p.WriteBool(state_.npn_valid)) {
    return "";
  }

  if (state_.npn_valid) {
    if (!p.WriteInt(state_.npn_status) ||
        !p.WriteString(state_.npn_protocol)) {
      return "";
    }
  }

  return std::string(reinterpret_cast<const char *>(p.data()), p.size());
}

const CertVerifyResult& SSLHostInfo::cert_verify_result() const {
  return cert_verify_result_;
}

int SSLHostInfo::WaitForCertVerification(CompletionCallback* callback) {
  if (cert_verification_complete_)
    return cert_verification_error_;
  DCHECK(!cert_parsing_failed_);
  DCHECK(!cert_verification_callback_);
  DCHECK(!state_.certs.empty());
  cert_verification_callback_ = callback;
  return ERR_IO_PENDING;
}

void SSLHostInfo::VerifyCallback(int rv) {
  DCHECK(!verification_start_time_.is_null());
  base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta duration = now - verification_start_time();
  UMA_HISTOGRAM_TIMES("Net.SSLHostInfoVerificationTimeMs", duration);
  VLOG(1) << "Verification took " << duration.InMilliseconds() << "ms";
  verification_end_time_ = now;
  cert_verification_complete_ = true;
  cert_verification_error_ = rv;
  if (cert_verification_callback_) {
    CompletionCallback* callback = cert_verification_callback_;
    cert_verification_callback_ = NULL;
    callback->Run(rv);
  }
}

SSLHostInfoFactory::~SSLHostInfoFactory() {}

}  // namespace net
