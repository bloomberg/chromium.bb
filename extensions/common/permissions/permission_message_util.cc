// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permission_message_util.h"

#include <stddef.h>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/url_pattern_set.h"
#include "grit/extensions_strings.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_constants.h"

using extensions::URLPatternSet;

namespace {

// Helper for GetDistinctHosts(): com > net > org > everything else.
bool RcdBetterThan(const std::string& a, const std::string& b) {
  if (a == b)
    return false;
  if (a == "com")
    return true;
  if (a == "net")
    return b != "com";
  if (a == "org")
    return b != "com" && b != "net";
  return false;
}

}  // namespace

namespace permission_message_util {

// The number of host messages supported. The first N - 1 of these messages are
// specific for the number of hosts; the last one is a catch-all for N or more
// hosts.
static const int kNumMessages = 4;

// Gets a list of hosts to display in a permission message from the given list
// of hosts from the manifest.
std::vector<base::string16> GetHostListFromHosts(
    const std::set<std::string>& hosts) {
  int host_msg_id = hosts.size() < kNumMessages
                        ? IDS_EXTENSION_PROMPT_WARNING_HOST_AND_SUBDOMAIN
                        : IDS_EXTENSION_PROMPT_WARNING_HOST_AND_SUBDOMAIN_LIST;
  std::vector<base::string16> host_list;
  for (const std::string& host : hosts) {
    host_list.push_back(
        host[0] == '*' && host[1] == '.'
            ? l10n_util::GetStringFUTF16(host_msg_id,
                                         base::UTF8ToUTF16(host.substr(2)))
            : base::UTF8ToUTF16(host));
  }
  DCHECK(host_list.size());
  return host_list;
}

void AddHostPermissions(extensions::PermissionIDSet* permissions,
                        const std::set<std::string>& hosts,
                        PermissionMessageProperties properties) {
  std::vector<base::string16> host_list = GetHostListFromHosts(hosts);

  // Create a separate permission for each host, and add it to the permissions
  // list.
  // TODO(sashab): Add coalescing rules for kHostReadOnly and kHostReadWrite
  // to mimic the current behavior of GetHostListFromHosts() above.
  extensions::APIPermission::ID permission_id =
      properties == kReadOnly ? extensions::APIPermission::kHostReadOnly
                              : extensions::APIPermission::kHostReadWrite;
  for (const auto& host : host_list)
    permissions->insert(permission_id, host);
}

std::set<std::string> GetDistinctHosts(const URLPatternSet& host_patterns,
                                       bool include_rcd,
                                       bool exclude_file_scheme) {
  // Each item is a host split into two parts: host without RCDs and
  // current best RCD.
  typedef base::StringPairs HostVector;
  HostVector hosts_best_rcd;
  for (const URLPattern& pattern : host_patterns) {
    if (exclude_file_scheme && pattern.scheme() == url::kFileScheme)
      continue;

    std::string host = pattern.host();

    // Add the subdomain wildcard back to the host, if necessary.
    if (pattern.match_subdomains())
      host = "*." + host;

    // If the host has an RCD, split it off so we can detect duplicates.
    std::string rcd;
    size_t reg_len = net::registry_controlled_domains::GetRegistryLength(
        host,
        net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
        net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
    if (reg_len && reg_len != std::string::npos) {
      if (include_rcd)  // else leave rcd empty
        rcd = host.substr(host.size() - reg_len);
      host = host.substr(0, host.size() - reg_len);
    }

    // Check if we've already seen this host.
    HostVector::iterator it = hosts_best_rcd.begin();
    for (; it != hosts_best_rcd.end(); ++it) {
      if (it->first == host)
        break;
    }
    // If this host was found, replace the RCD if this one is better.
    if (it != hosts_best_rcd.end()) {
      if (include_rcd && RcdBetterThan(rcd, it->second))
        it->second = rcd;
    } else {  // Previously unseen host, append it.
      hosts_best_rcd.push_back(std::make_pair(host, rcd));
    }
  }

  // Build up the result by concatenating hosts and RCDs.
  std::set<std::string> distinct_hosts;
  for (const auto& host_rcd : hosts_best_rcd)
    distinct_hosts.insert(host_rcd.first + host_rcd.second);
  return distinct_hosts;
}

}  // namespace permission_message_util
