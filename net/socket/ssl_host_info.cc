// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_host_info.h"

#include "base/string_piece.h"
#include "net/base/cert_verifier.h"
#include "net/base/ssl_config_service.h"
#include "net/base/x509_certificate.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_host_info.pb.h"

namespace net {

SSLHostInfo::State::State()
    : npn_valid(false),
      npn_status(SSLClientSocket::kNextProtoUnsupported) {
}

SSLHostInfo::State::~State() {}

SSLHostInfo::SSLHostInfo(
    const std::string& hostname,
    const SSLConfig& ssl_config)
    : hostname_(hostname),
      cert_valid_(false),
      rev_checking_enabled_(ssl_config.rev_checking_enabled),
      verify_ev_cert_(ssl_config.verify_ev_cert),
      callback_(new CancelableCompletionCallback<SSLHostInfo>(
                        ALLOW_THIS_IN_INITIALIZER_LIST(this),
                        &SSLHostInfo::VerifyCallback)) {
  state_.npn_valid = false;
}

SSLHostInfo::~SSLHostInfo() {}

// This array and the next two functions serve to map between the internal NPN
// status enum (which might change across versions) and the protocol buffer
// based enum (which will not).
static const struct {
  SSLClientSocket::NextProtoStatus npn_status;
  SSLHostInfoProto::NextProtoStatus proto_status;
} kNPNStatusMapping[] = {
  { SSLClientSocket::kNextProtoUnsupported, SSLHostInfoProto::UNSUPPORTED },
  { SSLClientSocket::kNextProtoNegotiated, SSLHostInfoProto::NEGOTIATED },
  { SSLClientSocket::kNextProtoNoOverlap, SSLHostInfoProto::NO_OVERLAP },
};

static SSLClientSocket::NextProtoStatus NPNStatusFromProtoStatus(
    SSLHostInfoProto::NextProtoStatus proto_status) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kNPNStatusMapping) - 1; i++) {
    if (kNPNStatusMapping[i].proto_status == proto_status)
      return kNPNStatusMapping[i].npn_status;
  }
  return kNPNStatusMapping[ARRAYSIZE_UNSAFE(kNPNStatusMapping) - 1].npn_status;
}

static SSLHostInfoProto::NextProtoStatus ProtoStatusFromNPNStatus(
    SSLClientSocket::NextProtoStatus npn_status) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kNPNStatusMapping) - 1; i++) {
    if (kNPNStatusMapping[i].npn_status == npn_status)
      return kNPNStatusMapping[i].proto_status;
  }
  return kNPNStatusMapping[ARRAYSIZE_UNSAFE(kNPNStatusMapping)-1].proto_status;
}

const SSLHostInfo::State& SSLHostInfo::state() const {
  return state_;
}

SSLHostInfo::State* SSLHostInfo::mutable_state() {
  return &state_;
}

bool SSLHostInfo::Parse(const std::string& data) {
  SSLHostInfoProto proto;
  State* state = mutable_state();

  state->certs.clear();
  state->server_hello.clear();
  state->npn_valid = false;
  cert_valid_ = false;

  if (!proto.ParseFromString(data))
    return false;

  for (int i = 0; i < proto.certificate_der_size(); i++)
    state->certs.push_back(proto.certificate_der(i));
  if (proto.has_server_hello())
    state->server_hello = proto.server_hello();
  if (proto.has_npn_status() && proto.has_npn_protocol()) {
    state->npn_valid = true;
    state->npn_status = NPNStatusFromProtoStatus(proto.npn_status());
    state->npn_protocol = proto.npn_protocol();
  }

  if (state->certs.size() > 0) {
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
      verifier_.reset(new CertVerifier);
      VLOG(1) << "Kicking off validation for " << hostname_;
      if (verifier_->Verify(cert_.get(), hostname_, flags,
                            &cert_verify_result_, callback_) == OK) {
        cert_valid_ = true;
      }
    }
  }

  return true;
}

std::string SSLHostInfo::Serialize() const {
  SSLHostInfoProto proto;

  for (std::vector<std::string>::const_iterator
       i = state_.certs.begin(); i != state_.certs.end(); i++) {
    proto.add_certificate_der(*i);
  }
  if (!state_.server_hello.empty())
    proto.set_server_hello(state_.server_hello);

  if (state_.npn_valid) {
    proto.set_npn_status(ProtoStatusFromNPNStatus(state_.npn_status));
    proto.set_npn_protocol(state_.npn_protocol);
  }

  return proto.SerializeAsString();
}

bool SSLHostInfo::cert_valid() const {
  return cert_valid_;
}

const CertVerifyResult& SSLHostInfo::cert_verify_result() const {
  return cert_verify_result_;
}

void SSLHostInfo::VerifyCallback(int rv) {
  cert_valid_ = rv == OK;
}

SSLHostInfoFactory::~SSLHostInfoFactory() {}

}  // namespace net
