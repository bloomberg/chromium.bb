// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permission_message_util.h"

#include "base/macros.h"
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

std::vector<base::string16> GetHostListFromHosts(
    const std::set<std::string>& hosts,
    PermissionMessageProperties properties) {
  int host_msg_id = hosts.size() < kNumMessages
                        ? IDS_EXTENSION_PROMPT_WARNING_HOST_AND_SUBDOMAIN
                        : IDS_EXTENSION_PROMPT_WARNING_HOST_AND_SUBDOMAIN_LIST;
  std::vector<base::string16> host_list;
  for (std::set<std::string>::const_iterator it = hosts.begin();
       it != hosts.end();
       ++it) {
    std::string host = *it;
    host_list.push_back(
        host[0] == '*' && host[1] == '.'
            ? l10n_util::GetStringFUTF16(host_msg_id,
                                         base::UTF8ToUTF16(host.erase(0, 2)))
            : base::UTF8ToUTF16(host));
  }
  DCHECK(host_list.size());
  return host_list;
}

void AddHostPermissions(extensions::PermissionIDSet* permissions,
                        const std::set<std::string>& hosts,
                        PermissionMessageProperties properties) {
  std::vector<base::string16> host_list =
      GetHostListFromHosts(hosts, properties);

  // Create a separate permission for each host, and add it to the permissions
  // list.
  // TODO(sashab): Add coalescing rules for kHostReadOnly and kHostReadWrite
  // to mimic the current behavior of CreateFromHostList() above.
  for (const auto& host : host_list) {
    permissions->insert(properties == kReadOnly
                            ? extensions::APIPermission::kHostReadOnly
                            : extensions::APIPermission::kHostReadWrite,
                        host);
  }
}

std::set<std::string> GetDistinctHosts(const URLPatternSet& host_patterns,
                                       bool include_rcd,
                                       bool exclude_file_scheme) {
  // Use a vector to preserve order (also faster than a map on small sets).
  // Each item is a host split into two parts: host without RCDs and
  // current best RCD.
  typedef base::StringPairs HostVector;
  HostVector hosts_best_rcd;
  for (URLPatternSet::const_iterator i = host_patterns.begin();
       i != host_patterns.end();
       ++i) {
    if (exclude_file_scheme && i->scheme() == url::kFileScheme)
      continue;

    std::string host = i->host();

    // Add the subdomain wildcard back to the host, if necessary.
    if (i->match_subdomains())
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

  // Build up the final vector by concatenating hosts and RCDs.
  std::set<std::string> distinct_hosts;
  for (HostVector::iterator it = hosts_best_rcd.begin();
       it != hosts_best_rcd.end();
       ++it)
    distinct_hosts.insert(it->first + it->second);
  return distinct_hosts;
}

}  // namespace permission_message_util
