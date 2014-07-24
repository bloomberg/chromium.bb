// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permission_message_util.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/url_pattern_set.h"
#include "grit/extensions_strings.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_constants.h"

using extensions::PermissionMessage;
using extensions::PermissionSet;
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

PermissionMessage CreateFromHostList(const std::set<std::string>& hosts) {
  typedef std::pair<PermissionMessage::ID, int> MsgPair;
  const MsgPair kMessagesList[] = {
      std::make_pair(PermissionMessage::kHosts1,
                     IDS_EXTENSION_PROMPT_WARNING_1_HOST),
      std::make_pair(PermissionMessage::kHosts2,
                     IDS_EXTENSION_PROMPT_WARNING_2_HOSTS),
      std::make_pair(PermissionMessage::kHosts3,
                     IDS_EXTENSION_PROMPT_WARNING_3_HOSTS),
      std::make_pair(PermissionMessage::kHosts4OrMore,
                     IDS_EXTENSION_PROMPT_WARNING_HOSTS_LIST)};

  int host_msg_id = hosts.size() < arraysize(kMessagesList)
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

  if (host_list.size() < arraysize(kMessagesList)) {
    return PermissionMessage(
        kMessagesList[host_list.size() - 1].first,
        l10n_util::GetStringFUTF16(
            kMessagesList[host_list.size() - 1].second, host_list, NULL));
  }

  base::string16 details;
  for (size_t i = 0; i < host_list.size(); ++i) {
    if (i > 0)
      details += base::ASCIIToUTF16("\n");
    details += l10n_util::GetStringFUTF16(
        IDS_EXTENSION_PROMPT_WARNING_HOST_LIST_ENTRY, host_list[i]);
  }
  return PermissionMessage(
      kMessagesList[arraysize(kMessagesList) - 1].first,
      l10n_util::GetStringUTF16(
          kMessagesList[arraysize(kMessagesList) - 1].second),
      details);
}

std::set<std::string> GetDistinctHosts(const URLPatternSet& host_patterns,
                                       bool include_rcd,
                                       bool exclude_file_scheme) {
  // Use a vector to preserve order (also faster than a map on small sets).
  // Each item is a host split into two parts: host without RCDs and
  // current best RCD.
  typedef std::vector<std::pair<std::string, std::string> > HostVector;
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
