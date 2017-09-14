// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_GENERAL_NAMES_H_
#define NET_CERT_INTERNAL_GENERAL_NAMES_H_

#include <memory>
#include <vector>

#include "net/base/ip_address.h"
#include "net/base/net_export.h"
#include "net/cert/internal/cert_error_id.h"

namespace net {

class CertErrors;

NET_EXPORT extern const CertErrorId kFailedParsingGeneralName;

namespace der {
class Input;
}  // namespace der

// Bitfield values for the GeneralName types defined in RFC 5280. The ordering
// and exact values are not important, but match the order from the RFC for
// convenience.
enum GeneralNameTypes {
  GENERAL_NAME_NONE = 0,
  GENERAL_NAME_OTHER_NAME = 1 << 0,
  GENERAL_NAME_RFC822_NAME = 1 << 1,
  GENERAL_NAME_DNS_NAME = 1 << 2,
  GENERAL_NAME_X400_ADDRESS = 1 << 3,
  GENERAL_NAME_DIRECTORY_NAME = 1 << 4,
  GENERAL_NAME_EDI_PARTY_NAME = 1 << 5,
  GENERAL_NAME_UNIFORM_RESOURCE_IDENTIFIER = 1 << 6,
  GENERAL_NAME_IP_ADDRESS = 1 << 7,
  GENERAL_NAME_REGISTERED_ID = 1 << 8,
  GENERAL_NAME_ALL_TYPES = (1 << 9) - 1,
};

// Represents a GeneralNames structure. When processing GeneralNames, it is
// often necessary to know which types of names were present, and to check
// all the names of a certain type. Therefore, a bitfield of all the name
// types is kept, and the names are split into members for each type. Only
// name types that are handled by this code are stored (though all types are
// recorded in the bitfield.)
struct NET_EXPORT GeneralNames {
  // Controls parsing of iPAddress names in ParseGeneralName.
  // IP_ADDRESS_ONLY parses the iPAddress names as a 4 or 16 byte IP address.
  // IP_ADDRESS_AND_NETMASK parses the iPAddress names as 8 or 32 bytes
  // containing an IP address followed by a netmask.
  enum ParseGeneralNameIPAddressType {
    IP_ADDRESS_ONLY,
    IP_ADDRESS_AND_NETMASK,
  };

  GeneralNames();
  ~GeneralNames();

  // Create a GeneralNames object representing the DER-encoded
  // |general_names_tlv|. Returns nullptr on failure, and may fill |errors| with
  // additional information. |errors| must be non-null.
  static std::unique_ptr<GeneralNames> Create(
      const der::Input& general_names_tlv,
      CertErrors* errors);

  // ASCII hostnames.
  std::vector<std::string> dns_names;

  // DER-encoded Name values (not including the Sequence tag).
  std::vector<std::vector<uint8_t>> directory_names;

  // iPAddresses as sequences of octets in network byte order. This will be
  // populated if the GeneralNames represents a Subject Alternative Name.
  std::vector<IPAddress> ip_addresses;

  // iPAddress ranges, as <IP, prefix length> pairs. This will be populated
  // if the GeneralNames represents a Name Constraints.
  std::vector<std::pair<IPAddress, unsigned>> ip_address_ranges;

  // Which name types were present, as a bitfield of GeneralNameTypes.
  // Includes both the supported and unsupported types (although unsupported
  // ones may not be recorded depending on the context, like non-critical name
  // constraints.)
  int present_name_types = GENERAL_NAME_NONE;
};

// Parses a GeneralName value and adds it to |subtrees|.
// |types_to_record| is a bitfield of GeneralNameTypes of the types to be
// recorded in |subtrees->present_name_types|. It does not affect the recording
// of the other fields. TODO(mattm): this seems weird.
// |ip_address_type| specifies how to parse iPAddress names.
// Returns false on failure, and may fill |errors| with additional information.
// |errors| must be non-null.
// TODO(mattm): should this be a method on GeneralNames?
WARN_UNUSED_RESULT bool ParseGeneralName(
    const der::Input& input,
    int types_to_record,
    GeneralNames::ParseGeneralNameIPAddressType ip_address_type,
    GeneralNames* subtrees,
    CertErrors* errors);

}  // namespace net

#endif  // NET_CERT_INTERNAL_GENERAL_NAMES_H_
