// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_host_info.h"

#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_host_info.pb.h"

namespace net {

SSLHostInfo::SSLHostInfo() {
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

SSLHostInfoFactory::~SSLHostInfoFactory() {}

}  // namespace net
