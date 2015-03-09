// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket.h"

#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_util.h"
#include "crypto/ec_private_key.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_connection_status_flags.h"

namespace net {

SSLClientSocket::SSLClientSocket()
    : was_npn_negotiated_(false),
      was_spdy_negotiated_(false),
      protocol_negotiated_(kProtoUnknown),
      channel_id_sent_(false),
      signed_cert_timestamps_received_(false),
      stapled_ocsp_response_received_(false),
      negotiation_extension_(kExtensionUnknown) {
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
    // For internal consistency, HTTP/2 is named SPDY4 within Chromium.
    // This is the HTTP/2 draft-14 identifier.
    return kProtoSPDY4_14;
  } else if (proto_string == "h2-15") {
    // This is the HTTP/2 draft-15 identifier.
    return kProtoSPDY4_15;
  } else if (proto_string == "h2") {
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
    case kProtoSPDY4_14:
      // For internal consistency, HTTP/2 is named SPDY4 within Chromium.
      // This is the HTTP/2 draft-14 identifier.
      return "h2-14";
    case kProtoSPDY4_15:
      // This is the HTTP/2 draft-15 identifier.
      return "h2-15";
    case kProtoSPDY4:
      return "h2";
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
  if (error == OK)
    return true;
  return (load_flags & LOAD_IGNORE_ALL_CERT_ERRORS) &&
         IsCertificateError(error);
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

void SSLClientSocket::set_negotiation_extension(
    SSLNegotiationExtension negotiation_extension) {
  negotiation_extension_ = negotiation_extension;
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
bool SSLClientSocket::HasCipherAdequateForHTTP2(
    const std::vector<uint16>& cipher_suites) {
  for (uint16 cipher : cipher_suites) {
    if (IsSecureTLSCipherSuite(cipher))
      return true;
  }
  return false;
}

// static
bool SSLClientSocket::IsTLSVersionAdequateForHTTP2(
    const SSLConfig& ssl_config) {
  return ssl_config.version_max >= SSL_PROTOCOL_VERSION_TLS1_2;
}

// static
std::vector<uint8_t> SSLClientSocket::SerializeNextProtos(
    const NextProtoVector& next_protos,
    bool can_advertise_http2) {
  std::vector<uint8_t> wire_protos;
  for (const NextProto next_proto : next_protos) {
    if (!can_advertise_http2 && kProtoSPDY4MinimumVersion <= next_proto &&
        next_proto <= kProtoSPDY4MaximumVersion) {
      continue;
    }
    const std::string proto = NextProtoToString(next_proto);
    if (proto.size() > 255) {
      LOG(WARNING) << "Ignoring overlong NPN/ALPN protocol: " << proto;
      continue;
    }
    if (proto.size() == 0) {
      LOG(WARNING) << "Ignoring empty NPN/ALPN protocol";
      continue;
    }
    wire_protos.push_back(proto.size());
    for (const char ch : proto) {
      wire_protos.push_back(static_cast<uint8_t>(ch));
    }
  }

  return wire_protos;
}

void SSLClientSocket::RecordNegotiationExtension() {
  if (negotiation_extension_ == kExtensionUnknown)
    return;
  std::string proto;
  SSLClientSocket::NextProtoStatus status = GetNextProto(&proto);
  if (status == kNextProtoUnsupported)
    return;
  // Convert protocol into numerical value for histogram.
  NextProto protocol_negotiated = SSLClientSocket::NextProtoFromString(proto);
  base::HistogramBase::Sample sample =
      static_cast<base::HistogramBase::Sample>(protocol_negotiated);
  // In addition to the protocol negotiated, we want to record which TLS
  // extension was used, and in case of NPN, whether there was overlap between
  // server and client list of supported protocols.
  if (negotiation_extension_ == kExtensionNPN) {
    if (status == kNextProtoNoOverlap) {
      sample += 1000;
    } else {
      sample += 500;
    }
  } else {
    DCHECK_EQ(kExtensionALPN, negotiation_extension_);
  }
  UMA_HISTOGRAM_SPARSE_SLOWLY("Net.SSLProtocolNegotiation", sample);
}

}  // namespace net
