// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket.h"

#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "crypto/ec_private_key.h"
#include "net/base/host_port_pair.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/ssl_config_service.h"

namespace net {

SSLClientSocket::SSLClientSocket()
    : was_npn_negotiated_(false),
      was_spdy_negotiated_(false),
      protocol_negotiated_(kProtoUnknown),
      channel_id_sent_(false),
      signed_cert_timestamps_received_(false),
      stapled_ocsp_response_received_(false) {
}

// static
NextProto SSLClientSocket::NextProtoFromString(
    const std::string& proto_string) {
  if (proto_string == "http1.1" || proto_string == "http/1.1") {
    return kProtoHTTP11;
  } else if (proto_string == "spdy/2") {
    return kProtoDeprecatedSPDY2;
  } else if (proto_string == "spdy/3") {
    return kProtoSPDY3;
  } else if (proto_string == "spdy/3.1") {
    return kProtoSPDY31;
  } else if (proto_string == "h2-14") {
    // This is the HTTP/2 draft 14 identifier. For internal
    // consistency, HTTP/2 is named SPDY4 within Chromium.
    return kProtoSPDY4;
  } else if (proto_string == "quic/1+spdy/3") {
    return kProtoQUIC1SPDY3;
  } else {
    return kProtoUnknown;
  }
}

// static
const char* SSLClientSocket::NextProtoToString(NextProto next_proto) {
  switch (next_proto) {
    case kProtoHTTP11:
      return "http/1.1";
    case kProtoDeprecatedSPDY2:
      return "spdy/2";
    case kProtoSPDY3:
      return "spdy/3";
    case kProtoSPDY31:
      return "spdy/3.1";
    case kProtoSPDY4:
      // This is the HTTP/2 draft 14 identifier. For internal
      // consistency, HTTP/2 is named SPDY4 within Chromium.
      return "h2-14";
    case kProtoQUIC1SPDY3:
      return "quic/1+spdy/3";
    case kProtoUnknown:
      break;
  }
  return "unknown";
}

// static
const char* SSLClientSocket::NextProtoStatusToString(
    const SSLClientSocket::NextProtoStatus status) {
  switch (status) {
    case kNextProtoUnsupported:
      return "unsupported";
    case kNextProtoNegotiated:
      return "negotiated";
    case kNextProtoNoOverlap:
      return "no-overlap";
  }
  return NULL;
}

bool SSLClientSocket::WasNpnNegotiated() const {
  return was_npn_negotiated_;
}

NextProto SSLClientSocket::GetNegotiatedProtocol() const {
  return protocol_negotiated_;
}

bool SSLClientSocket::IgnoreCertError(int error, int load_flags) {
  if (error == OK || load_flags & LOAD_IGNORE_ALL_CERT_ERRORS)
    return true;

  if (error == ERR_CERT_COMMON_NAME_INVALID &&
      (load_flags & LOAD_IGNORE_CERT_COMMON_NAME_INVALID))
    return true;

  if (error == ERR_CERT_DATE_INVALID &&
      (load_flags & LOAD_IGNORE_CERT_DATE_INVALID))
    return true;

  if (error == ERR_CERT_AUTHORITY_INVALID &&
      (load_flags & LOAD_IGNORE_CERT_AUTHORITY_INVALID))
    return true;

  return false;
}

bool SSLClientSocket::set_was_npn_negotiated(bool negotiated) {
  return was_npn_negotiated_ = negotiated;
}

bool SSLClientSocket::was_spdy_negotiated() const {
  return was_spdy_negotiated_;
}

bool SSLClientSocket::set_was_spdy_negotiated(bool negotiated) {
  return was_spdy_negotiated_ = negotiated;
}

void SSLClientSocket::set_protocol_negotiated(NextProto protocol_negotiated) {
  protocol_negotiated_ = protocol_negotiated;
}

bool SSLClientSocket::WasChannelIDSent() const {
  return channel_id_sent_;
}

void SSLClientSocket::set_channel_id_sent(bool channel_id_sent) {
  channel_id_sent_ = channel_id_sent;
}

void SSLClientSocket::set_signed_cert_timestamps_received(
    bool signed_cert_timestamps_received) {
  signed_cert_timestamps_received_ = signed_cert_timestamps_received;
}

void SSLClientSocket::set_stapled_ocsp_response_received(
    bool stapled_ocsp_response_received) {
  stapled_ocsp_response_received_ = stapled_ocsp_response_received;
}

// static
void SSLClientSocket::RecordChannelIDSupport(
    ChannelIDService* channel_id_service,
    bool negotiated_channel_id,
    bool channel_id_enabled,
    bool supports_ecc) {
  // Since this enum is used for a histogram, do not change or re-use values.
  enum {
    DISABLED = 0,
    CLIENT_ONLY = 1,
    CLIENT_AND_SERVER = 2,
    CLIENT_NO_ECC = 3,
    CLIENT_BAD_SYSTEM_TIME = 4,
    CLIENT_NO_CHANNEL_ID_SERVICE = 5,
    CHANNEL_ID_USAGE_MAX
  } supported = DISABLED;
  if (negotiated_channel_id) {
    supported = CLIENT_AND_SERVER;
  } else if (channel_id_enabled) {
    if (!channel_id_service)
      supported = CLIENT_NO_CHANNEL_ID_SERVICE;
    else if (!supports_ecc)
      supported = CLIENT_NO_ECC;
    else if (!channel_id_service->IsSystemTimeValid())
      supported = CLIENT_BAD_SYSTEM_TIME;
    else
      supported = CLIENT_ONLY;
  }
  UMA_HISTOGRAM_ENUMERATION("DomainBoundCerts.Support", supported,
                            CHANNEL_ID_USAGE_MAX);
}

// static
bool SSLClientSocket::IsChannelIDEnabled(
    const SSLConfig& ssl_config,
    ChannelIDService* channel_id_service) {
  if (!ssl_config.channel_id_enabled)
    return false;
  if (!channel_id_service) {
    DVLOG(1) << "NULL channel_id_service_, not enabling channel ID.";
    return false;
  }
  if (!crypto::ECPrivateKey::IsSupported()) {
    DVLOG(1) << "Elliptic Curve not supported, not enabling channel ID.";
    return false;
  }
  if (!channel_id_service->IsSystemTimeValid()) {
    DVLOG(1) << "System time is not within the supported range for certificate "
                "generation, not enabling channel ID.";
    return false;
  }
  return true;
}

// static
std::vector<uint8_t> SSLClientSocket::SerializeNextProtos(
    const std::vector<std::string>& next_protos) {
  // Do a first pass to determine the total length.
  size_t wire_length = 0;
  for (std::vector<std::string>::const_iterator i = next_protos.begin();
       i != next_protos.end(); ++i) {
    if (i->size() > 255) {
      LOG(WARNING) << "Ignoring overlong NPN/ALPN protocol: " << *i;
      continue;
    }
    if (i->size() == 0) {
      LOG(WARNING) << "Ignoring empty NPN/ALPN protocol";
      continue;
    }
    wire_length += i->size();
    wire_length++;
  }

  // Allocate memory for the result and fill it in.
  std::vector<uint8_t> wire_protos;
  wire_protos.reserve(wire_length);
  for (std::vector<std::string>::const_iterator i = next_protos.begin();
       i != next_protos.end(); i++) {
    if (i->size() == 0 || i->size() > 255)
      continue;
    wire_protos.push_back(i->size());
    wire_protos.resize(wire_protos.size() + i->size());
    memcpy(&wire_protos[wire_protos.size() - i->size()],
           i->data(), i->size());
  }
  DCHECK_EQ(wire_protos.size(), wire_length);

  return wire_protos;
}

}  // namespace net
