// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_DOMAIN_NAME_H_
#define DISCOVERY_MDNS_DOMAIN_NAME_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "platform/api/logging.h"

namespace openscreen {
namespace mdns {

struct DomainName {
  // TODO(issues/2): Replace bool here and elsewhere with ErrorOr<DomainName>.
  static bool Append(const DomainName& first,
                     const DomainName& second,
                     DomainName* result);

  template <typename It>
  static bool FromLabels(It first, It last, DomainName* result) {
    size_t total_length = 1;
    for (auto label = first; label != last; ++label) {
      if (label->size() > kDomainNameMaxLabelLength) {
        return false;
      }
      total_length += label->size() + 1;
    }
    if (total_length > kDomainNameMaxLength) {
      return false;
    }
    result->domain_name_.resize(total_length);
    auto result_it = result->domain_name_.begin();
    for (auto label = first; label != last; ++label) {
      *result_it++ = static_cast<uint8_t>(label->size());
      result_it = std::copy(label->begin(), label->end(), result_it);
    }
    *result_it = 0;
    return true;
  }

  static const DomainName kLocalDomain;
  static constexpr uint8_t kDomainNameMaxLabelLength = 63u;
  static constexpr uint16_t kDomainNameMaxLength = 256u;

  DomainName();
  explicit DomainName(std::vector<uint8_t>&& domain_name);
  DomainName(const DomainName&);
  DomainName(DomainName&&);
  ~DomainName();
  DomainName& operator=(const DomainName&);
  DomainName& operator=(DomainName&&);

  bool operator==(const DomainName& other) const;
  bool operator!=(const DomainName& other) const;

  bool EndsWithLocalDomain() const;
  bool IsEmpty() const { return domain_name_.size() > 1 && domain_name_[0]; }

  bool Append(const DomainName& after);
  // TODO: If there's significant use of this, we would rather have string_span
  // or similar for this so we could use iterators for zero-copy.
  std::vector<std::string> GetLabels() const;

  const std::vector<uint8_t>& domain_name() const { return domain_name_; }

 private:
  // RFC 1035 domain name format: sequence of 1 octet label length followed by
  // label data, ending with a 0 octet.  May not exceed 256 bytes (including
  // terminating 0).
  // For example, openscreen.org would be encoded as:
  // {10, 'o', 'p', 'e', 'n', 's', 'c', 'r', 'e', 'e', 'n',
  //   3, 'o', 'r', 'g', 0}
  std::vector<uint8_t> domain_name_;
};

class DomainNameComparator {
 public:
  bool operator()(const DomainName& a, const DomainName& b) const;
};

std::ostream& operator<<(std::ostream& os, const DomainName& domain_name);

}  // namespace mdns
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_DOMAIN_NAME_H_
