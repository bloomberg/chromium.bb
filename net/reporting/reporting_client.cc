// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_client.h"

#include <string>
#include <tuple>

#include "base/time/time.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

ReportingEndpointGroupKey::ReportingEndpointGroupKey(url::Origin origin,
                                                     std::string group_name)
    : origin(std::move(origin)), group_name(std::move(group_name)) {}

bool operator==(const ReportingEndpointGroupKey& lhs,
                const ReportingEndpointGroupKey& rhs) {
  return lhs.origin == rhs.origin && lhs.group_name == rhs.group_name;
}

bool operator!=(const ReportingEndpointGroupKey& lhs,
                const ReportingEndpointGroupKey& rhs) {
  return !(lhs == rhs);
}

bool operator<(const ReportingEndpointGroupKey& lhs,
               const ReportingEndpointGroupKey& rhs) {
  return std::tie(lhs.origin, lhs.group_name) <
         std::tie(rhs.origin, rhs.group_name);
}

const int ReportingClient::EndpointInfo::kDefaultPriority = 1;
const int ReportingClient::EndpointInfo::kDefaultWeight = 1;

ReportingClient::ReportingClient() : group_key(url::Origin(), std::string()) {}

ReportingClient::ReportingClient(url::Origin origin,
                                 std::string group_name,
                                 EndpointInfo endpoint_info)
    : group_key(std::move(origin), std::move(group_name)),
      info(std::move(endpoint_info)) {
  DCHECK_LE(0, info.weight);
  DCHECK_LE(0, info.priority);
}

ReportingClient::ReportingClient(const ReportingClient& other) = default;
ReportingClient::ReportingClient(ReportingClient&& other) = default;

ReportingClient::~ReportingClient() = default;

bool ReportingClient::is_valid() const {
  return info.url.is_valid();
}

ReportingEndpointGroup::ReportingEndpointGroup()
    : name(std::string()), include_subdomains(OriginSubdomains::DEFAULT) {}

ReportingEndpointGroup::ReportingEndpointGroup(
    const ReportingEndpointGroup& other)
    : name(other.name),
      include_subdomains(other.include_subdomains),
      ttl(other.ttl),
      endpoints(other.endpoints) {}

ReportingEndpointGroup::~ReportingEndpointGroup() = default;

CachedReportingEndpointGroup::CachedReportingEndpointGroup(
    url::Origin origin,
    std::string name,
    OriginSubdomains include_subdomains,
    base::Time expires,
    base::Time last_used)
    : group_key(std::move(origin), std::move(name)),
      include_subdomains(include_subdomains),
      expires(expires),
      last_used(last_used) {}

CachedReportingEndpointGroup::CachedReportingEndpointGroup(
    url::Origin origin,
    const ReportingEndpointGroup& endpoint_group,
    base::Time now)
    : CachedReportingEndpointGroup(std::move(origin),
                                   endpoint_group.name,
                                   endpoint_group.include_subdomains,
                                   now + endpoint_group.ttl /* expires */,
                                   now /* last_used */) {}

}  // namespace net
