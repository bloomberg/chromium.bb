// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/quic_versions.h"

#include "net/quic/core/quic_error_codes.h"
#include "net/quic/core/quic_tag.h"
#include "net/quic/core/quic_types.h"
#include "net/quic/platform/api/quic_bug_tracker.h"
#include "net/quic/platform/api/quic_endian.h"
#include "net/quic/platform/api/quic_flag_utils.h"
#include "net/quic/platform/api/quic_flags.h"
#include "net/quic/platform/api/quic_logging.h"
#include "net/quic/platform/api/quic_string.h"

namespace net {
namespace {

// Constructs a version label from the 4 bytes such that the on-the-wire
// order will be: d, c, b, a.
QuicVersionLabel MakeVersionLabel(char a, char b, char c, char d) {
  return MakeQuicTag(d, c, b, a);
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const ParsedQuicVersion& version) {
  os << ParsedQuicVersionToString(version);
  return os;
}

QuicVersionLabel CreateQuicVersionLabel(ParsedQuicVersion parsed_version) {
  char proto = 0;
  switch (parsed_version.handshake_protocol) {
    case PROTOCOL_QUIC_CRYPTO:
      proto = 'Q';
      break;
    case PROTOCOL_TLS1_3:
      if (!FLAGS_quic_supports_tls_handshake) {
        QUIC_BUG << "TLS use attempted when not enabled";
      }
      proto = 'T';
      break;
    default:
      QUIC_LOG(ERROR) << "Invalid HandshakeProtocol: "
                      << parsed_version.handshake_protocol;
      return 0;
  }
  switch (parsed_version.transport_version) {
    case QUIC_VERSION_35:
      return MakeVersionLabel(proto, '0', '3', '5');
    case QUIC_VERSION_37:
      return MakeVersionLabel(proto, '0', '3', '7');
    case QUIC_VERSION_38:
      return MakeVersionLabel(proto, '0', '3', '8');
    case QUIC_VERSION_39:
      return MakeVersionLabel(proto, '0', '3', '9');
    case QUIC_VERSION_41:
      return MakeVersionLabel(proto, '0', '4', '1');
    case QUIC_VERSION_42:
      return MakeVersionLabel(proto, '0', '4', '2');
    case QUIC_VERSION_43:
      return MakeVersionLabel(proto, '0', '4', '3');
    case QUIC_VERSION_99:
      return MakeVersionLabel(proto, '0', '9', '9');
    default:
      // This shold be an ERROR because we should never attempt to convert an
      // invalid QuicTransportVersion to be written to the wire.
      QUIC_LOG(ERROR) << "Unsupported QuicTransportVersion: "
                      << parsed_version.transport_version;
      return 0;
  }
}

ParsedQuicVersion ParseQuicVersionLabel(QuicVersionLabel version_label) {
  std::vector<HandshakeProtocol> protocols = {PROTOCOL_QUIC_CRYPTO};
  if (FLAGS_quic_supports_tls_handshake) {
    protocols.push_back(PROTOCOL_TLS1_3);
  }
  for (QuicTransportVersion version : kSupportedTransportVersions) {
    for (HandshakeProtocol handshake : protocols) {
      if (version_label ==
          CreateQuicVersionLabel(ParsedQuicVersion(handshake, version))) {
        return ParsedQuicVersion(handshake, version);
      }
    }
  }
  // Reading from the client so this should not be considered an ERROR.
  QUIC_DLOG(INFO) << "Unsupported QuicVersionLabel version: "
                  << QuicVersionLabelToString(version_label);
  return ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED);
}

QuicTransportVersionVector AllSupportedTransportVersions() {
  QuicTransportVersionVector supported_versions;
  for (QuicTransportVersion version : kSupportedTransportVersions) {
    supported_versions.push_back(version);
  }
  return supported_versions;
}

ParsedQuicVersionVector AllSupportedVersions() {
  ParsedQuicVersionVector supported_versions;
  for (HandshakeProtocol protocol : kSupportedHandshakeProtocols) {
    if (protocol == PROTOCOL_TLS1_3 && !FLAGS_quic_supports_tls_handshake) {
      continue;
    }
    for (QuicTransportVersion version : kSupportedTransportVersions) {
      supported_versions.push_back(ParsedQuicVersion(protocol, version));
    }
  }
  return supported_versions;
}

// TODO(nharper): Remove this function when it is no longer in use.
QuicTransportVersionVector CurrentSupportedTransportVersions() {
  return FilterSupportedTransportVersions(AllSupportedTransportVersions());
}

ParsedQuicVersionVector CurrentSupportedVersions() {
  return FilterSupportedVersions(AllSupportedVersions());
}

// TODO(nharper): Remove this function when it is no longer in use.
QuicTransportVersionVector FilterSupportedTransportVersions(
    QuicTransportVersionVector versions) {
  ParsedQuicVersionVector parsed_versions;
  for (QuicTransportVersion version : versions) {
    parsed_versions.push_back(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, version));
  }
  ParsedQuicVersionVector filtered_parsed_versions =
      FilterSupportedVersions(parsed_versions);
  QuicTransportVersionVector filtered_versions;
  for (ParsedQuicVersion version : filtered_parsed_versions) {
    filtered_versions.push_back(version.transport_version);
  }
  return filtered_versions;
}

ParsedQuicVersionVector FilterSupportedVersions(
    ParsedQuicVersionVector versions) {
  ParsedQuicVersionVector filtered_versions;
  filtered_versions.reserve(versions.size());
  for (ParsedQuicVersion version : versions) {
    if (version.transport_version == QUIC_VERSION_99) {
      if (GetQuicFlag(FLAGS_quic_enable_version_99) &&
          GetQuicReloadableFlag(quic_enable_version_43) &&
          GetQuicReloadableFlag(quic_enable_version_42_2)) {
        filtered_versions.push_back(version);
      }
    } else if (version.transport_version == QUIC_VERSION_43) {
      if (GetQuicReloadableFlag(quic_enable_version_43) &&
          GetQuicReloadableFlag(quic_enable_version_42_2)) {
        filtered_versions.push_back(version);
      }
    } else if (version.transport_version == QUIC_VERSION_42) {
      if (GetQuicReloadableFlag(quic_enable_version_42_2)) {
        filtered_versions.push_back(version);
      }
    } else if (version.transport_version == QUIC_VERSION_41) {
      if (!GetQuicReloadableFlag(quic_disable_version_41)) {
        filtered_versions.push_back(version);
      }
    } else if (version.transport_version == QUIC_VERSION_38) {
      if (!GetQuicReloadableFlag(quic_disable_version_38)) {
        filtered_versions.push_back(version);
      }
    } else if (version.transport_version == QUIC_VERSION_37) {
      if (!GetQuicReloadableFlag(quic_disable_version_37)) {
        filtered_versions.push_back(version);
      }
    } else {
      filtered_versions.push_back(version);
    }
  }
  return filtered_versions;
}

QuicTransportVersionVector VersionOfIndex(
    const QuicTransportVersionVector& versions,
    int index) {
  QuicTransportVersionVector version;
  int version_count = versions.size();
  if (index >= 0 && index < version_count) {
    version.push_back(versions[index]);
  } else {
    version.push_back(QUIC_VERSION_UNSUPPORTED);
  }
  return version;
}

ParsedQuicVersionVector ParsedVersionOfIndex(
    const ParsedQuicVersionVector& versions,
    int index) {
  ParsedQuicVersionVector version;
  int version_count = versions.size();
  if (index >= 0 && index < version_count) {
    version.push_back(versions[index]);
  } else {
    version.push_back(
        ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED));
  }
  return version;
}

QuicTransportVersionVector ParsedVersionsToTransportVersions(
    const ParsedQuicVersionVector& versions) {
  QuicTransportVersionVector transport_versions;
  transport_versions.resize(versions.size());
  for (size_t i = 0; i < versions.size(); ++i) {
    transport_versions[i] = versions[i].transport_version;
  }
  return transport_versions;
}

QuicVersionLabel QuicVersionToQuicVersionLabel(
    QuicTransportVersion transport_version) {
  return CreateQuicVersionLabel(
      ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, transport_version));
}

QuicString QuicVersionLabelToString(QuicVersionLabel version_label) {
  return QuicTagToString(QuicEndian::HostToNet32(version_label));
}

QuicTransportVersion QuicVersionLabelToQuicVersion(
    QuicVersionLabel version_label) {
  return ParseQuicVersionLabel(version_label).transport_version;
}

HandshakeProtocol QuicVersionLabelToHandshakeProtocol(
    QuicVersionLabel version_label) {
  return ParseQuicVersionLabel(version_label).handshake_protocol;
}

#define RETURN_STRING_LITERAL(x) \
  case x:                        \
    return #x

QuicString QuicVersionToString(QuicTransportVersion transport_version) {
  switch (transport_version) {
    RETURN_STRING_LITERAL(QUIC_VERSION_35);
    RETURN_STRING_LITERAL(QUIC_VERSION_37);
    RETURN_STRING_LITERAL(QUIC_VERSION_38);
    RETURN_STRING_LITERAL(QUIC_VERSION_39);
    RETURN_STRING_LITERAL(QUIC_VERSION_41);
    RETURN_STRING_LITERAL(QUIC_VERSION_42);
    RETURN_STRING_LITERAL(QUIC_VERSION_43);
    RETURN_STRING_LITERAL(QUIC_VERSION_99);
    default:
      return "QUIC_VERSION_UNSUPPORTED";
  }
}

QuicString ParsedQuicVersionToString(ParsedQuicVersion version) {
  return QuicVersionLabelToString(CreateQuicVersionLabel(version));
}

QuicString QuicTransportVersionVectorToString(
    const QuicTransportVersionVector& versions) {
  QuicString result = "";
  for (size_t i = 0; i < versions.size(); ++i) {
    if (i != 0) {
      result.append(",");
    }
    result.append(QuicVersionToString(versions[i]));
  }
  return result;
}

QuicString ParsedQuicVersionVectorToString(
    const ParsedQuicVersionVector& versions) {
  QuicString result = "";
  for (size_t i = 0; i < versions.size(); ++i) {
    if (i != 0) {
      result.append(",");
    }
    result.append(ParsedQuicVersionToString(versions[i]));
  }
  return result;
}

}  // namespace net
