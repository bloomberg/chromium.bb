// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ssl_errors/error_classification.h"

#include <limits.h>
#include <stddef.h>

#include <vector>

#include "base/build_time.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/ssl_errors/error_info.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#endif

using base::Time;
using base::TimeTicks;
using base::TimeDelta;

namespace ssl_errors {
namespace {

// Events for UMA. Do not reorder or change!
enum SSLInterstitialCause {
  CLOCK_PAST,
  CLOCK_FUTURE,
  WWW_SUBDOMAIN_MATCH,
  SUBDOMAIN_MATCH,
  SUBDOMAIN_INVERSE_MATCH,
  SUBDOMAIN_OUTSIDE_WILDCARD,
  HOST_NAME_NOT_KNOWN_TLD,
  LIKELY_MULTI_TENANT_HOSTING,
  LOCALHOST,
  PRIVATE_URL,
  AUTHORITY_ERROR_CAPTIVE_PORTAL,  // Deprecated in M47.
  SELF_SIGNED,
  EXPIRED_RECENTLY,
  LIKELY_SAME_DOMAIN,
  UNUSED_INTERSTITIAL_CAUSE_ENTRY,
};

void RecordSSLInterstitialCause(bool overridable, SSLInterstitialCause event) {
  if (overridable) {
    UMA_HISTOGRAM_ENUMERATION("interstitial.ssl.cause.overridable", event,
                              UNUSED_INTERSTITIAL_CAUSE_ENTRY);
  } else {
    UMA_HISTOGRAM_ENUMERATION("interstitial.ssl.cause.nonoverridable", event,
                              UNUSED_INTERSTITIAL_CAUSE_ENTRY);
  }
}

// Returns the Levenshtein distance between |str1| and |str2|.
// Which is the minimum number of single-character edits (i.e. insertions,
// deletions or substitutions) required to change one word into the other.
// https://en.wikipedia.org/wiki/Levenshtein_distance
size_t GetLevenshteinDistance(const std::string& str1,
                              const std::string& str2) {
  if (str1 == str2)
    return 0;
  if (str1.size() == 0)
    return str2.size();
  if (str2.size() == 0)
    return str1.size();
  std::vector<size_t> kFirstRow(str2.size() + 1, 0);
  std::vector<size_t> kSecondRow(str2.size() + 1, 0);

  for (size_t i = 0; i < kFirstRow.size(); ++i)
    kFirstRow[i] = i;
  for (size_t i = 0; i < str1.size(); ++i) {
    kSecondRow[0] = i + 1;
    for (size_t j = 0; j < str2.size(); ++j) {
      int cost = str1[i] == str2[j] ? 0 : 1;
      kSecondRow[j + 1] =
          std::min(std::min(kSecondRow[j] + 1, kFirstRow[j + 1] + 1),
                   kFirstRow[j] + cost);
    }
    for (size_t j = 0; j < kFirstRow.size(); j++)
      kFirstRow[j] = kSecondRow[j];
  }
  return kSecondRow[str2.size()];
}

std::vector<HostnameTokens> GetTokenizedDNSNames(
    const std::vector<std::string>& dns_names) {
  std::vector<HostnameTokens> dns_name_tokens;
  for (const auto& dns_name : dns_names) {
    HostnameTokens dns_name_token_single;
    if (dns_name.empty() || dns_name.find('\0') != std::string::npos ||
        !(IsHostNameKnownTLD(dns_name))) {
      dns_name_token_single.push_back(std::string());
    } else {
      dns_name_token_single = Tokenize(dns_name);
    }
    dns_name_tokens.push_back(dns_name_token_single);
  }
  return dns_name_tokens;
}

// If |potential_subdomain| is a subdomain of |parent|, return the number of
// different tokens. Otherwise returns -1.
int FindSubdomainDifference(const HostnameTokens& potential_subdomain,
                            const HostnameTokens& parent) {
  // A check to ensure that the number of tokens in the tokenized_parent is
  // less than the tokenized_potential_subdomain.
  if (parent.size() >= potential_subdomain.size())
    return -1;

  // Don't be ridiculous. Also, don't overflow.
  if (potential_subdomain.size() >= INT_MAX || parent.size() >= INT_MAX)
    return -1;

  int diff_size = static_cast<int>(potential_subdomain.size() - parent.size());
  for (size_t i = 0; i < parent.size(); i++) {
    if (parent[i] != potential_subdomain[i + diff_size])
      return -1;
  }
  return diff_size;
}

// We accept the inverse case for www for historical reasons.
bool IsWWWSubDomainMatch(const GURL& request_url,
                         const net::X509Certificate& cert) {
  std::string www_host;
  std::vector<std::string> dns_names;
  cert.GetDNSNames(&dns_names);
  return GetWWWSubDomainMatch(request_url, dns_names, &www_host);
}

// The time to use when doing build time operations in browser tests.
base::LazyInstance<base::Time> g_testing_build_time = LAZY_INSTANCE_INITIALIZER;

}  // namespace

void RecordUMAStatistics(bool overridable,
                         const base::Time& current_time,
                         const GURL& request_url,
                         int cert_error,
                         const net::X509Certificate& cert) {
  ssl_errors::ErrorInfo::ErrorType type =
      ssl_errors::ErrorInfo::NetErrorToErrorType(cert_error);
  UMA_HISTOGRAM_ENUMERATION("interstitial.ssl_error_type", type,
                            ssl_errors::ErrorInfo::END_OF_ENUM);
  switch (type) {
    case ssl_errors::ErrorInfo::CERT_DATE_INVALID: {
      if (IsUserClockInThePast(base::Time::NowFromSystemTime())) {
        RecordSSLInterstitialCause(overridable, CLOCK_PAST);
      } else if (IsUserClockInTheFuture(base::Time::NowFromSystemTime())) {
        RecordSSLInterstitialCause(overridable, CLOCK_FUTURE);
      } else if (cert.HasExpired() &&
                 (current_time - cert.valid_expiry()).InDays() < 28) {
        RecordSSLInterstitialCause(overridable, EXPIRED_RECENTLY);
      }
      break;
    }
    case ssl_errors::ErrorInfo::CERT_COMMON_NAME_INVALID: {
      std::string host_name = request_url.host();
      if (IsHostNameKnownTLD(host_name)) {
        HostnameTokens host_name_tokens = Tokenize(host_name);
        if (IsWWWSubDomainMatch(request_url, cert))
          RecordSSLInterstitialCause(overridable, WWW_SUBDOMAIN_MATCH);
        if (IsSubDomainOutsideWildcard(request_url, cert))
          RecordSSLInterstitialCause(overridable, SUBDOMAIN_OUTSIDE_WILDCARD);
        std::vector<std::string> dns_names;
        cert.GetDNSNames(&dns_names);
        std::vector<HostnameTokens> dns_name_tokens =
            GetTokenizedDNSNames(dns_names);
        if (NameUnderAnyNames(host_name_tokens, dns_name_tokens))
          RecordSSLInterstitialCause(overridable, SUBDOMAIN_MATCH);
        if (AnyNamesUnderName(dns_name_tokens, host_name_tokens))
          RecordSSLInterstitialCause(overridable, SUBDOMAIN_INVERSE_MATCH);
        if (IsCertLikelyFromMultiTenantHosting(request_url, cert))
          RecordSSLInterstitialCause(overridable, LIKELY_MULTI_TENANT_HOSTING);
        if (IsCertLikelyFromSameDomain(request_url, cert))
          RecordSSLInterstitialCause(overridable, LIKELY_SAME_DOMAIN);
      } else {
        RecordSSLInterstitialCause(overridable, HOST_NAME_NOT_KNOWN_TLD);
      }
      break;
    }
    case ssl_errors::ErrorInfo::CERT_AUTHORITY_INVALID: {
      const std::string& hostname = request_url.HostNoBrackets();
      if (net::IsLocalhost(hostname))
        RecordSSLInterstitialCause(overridable, LOCALHOST);
      if (IsHostnameNonUniqueOrDotless(hostname))
        RecordSSLInterstitialCause(overridable, PRIVATE_URL);
      if (net::X509Certificate::IsSelfSigned(cert.os_cert_handle()))
        RecordSSLInterstitialCause(overridable, SELF_SIGNED);
      break;
    }
    default:
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("interstitial.ssl.connection_type",
                            net::NetworkChangeNotifier::GetConnectionType(),
                            net::NetworkChangeNotifier::CONNECTION_LAST);
}

bool IsUserClockInThePast(const base::Time& time_now) {
  base::Time build_time;
  if (!g_testing_build_time.Get().is_null()) {
    build_time = g_testing_build_time.Get();
  } else {
    build_time = base::GetBuildTime();
  }

  if (time_now < build_time - base::TimeDelta::FromDays(2))
    return true;
  return false;
}

bool IsUserClockInTheFuture(const base::Time& time_now) {
  base::Time build_time;
  if (!g_testing_build_time.Get().is_null()) {
    build_time = g_testing_build_time.Get();
  } else {
    build_time = base::GetBuildTime();
  }

  if (time_now > build_time + base::TimeDelta::FromDays(365))
    return true;
  return false;
}

void SetBuildTimeForTesting(const base::Time& testing_time) {
  g_testing_build_time.Get() = testing_time;
}

bool IsHostNameKnownTLD(const std::string& host_name) {
  size_t tld_length = net::registry_controlled_domains::GetRegistryLength(
      host_name, net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (tld_length == 0 || tld_length == std::string::npos)
    return false;
  return true;
}

HostnameTokens Tokenize(const std::string& name) {
  return base::SplitString(name, ".", base::KEEP_WHITESPACE,
                           base::SPLIT_WANT_ALL);
}

bool GetWWWSubDomainMatch(const GURL& request_url,
                          const std::vector<std::string>& dns_names,
                          std::string* www_match_host_name) {
  const std::string& host_name = request_url.host();

  if (IsHostNameKnownTLD(host_name)) {
    // Need to account for all possible domains given in the SSL certificate.
    for (const auto& dns_name : dns_names) {
      if (dns_name.empty() || dns_name.find('\0') != std::string::npos ||
          dns_name.length() == host_name.length() ||
          !IsHostNameKnownTLD(dns_name)) {
        continue;
      } else if (dns_name.length() > host_name.length()) {
        if (url_formatter::StripWWW(base::ASCIIToUTF16(dns_name)) ==
            base::ASCIIToUTF16(host_name)) {
          *www_match_host_name = dns_name;
          return true;
        }
      } else {
        if (url_formatter::StripWWW(base::ASCIIToUTF16(host_name)) ==
            base::ASCIIToUTF16(dns_name)) {
          *www_match_host_name = dns_name;
          return true;
        }
      }
    }
  }
  return false;
}

bool NameUnderAnyNames(const HostnameTokens& child,
                       const std::vector<HostnameTokens>& potential_parents) {
  // Need to account for all the possible domains given in the SSL certificate.
  for (const auto& potential_parent : potential_parents) {
    if (potential_parent.empty() || potential_parent.size() >= child.size()) {
      continue;
    }
    int domain_diff = FindSubdomainDifference(child, potential_parent);
    if (domain_diff == 1 && child[0] != "www")
      return true;
  }
  return false;
}

bool AnyNamesUnderName(const std::vector<HostnameTokens>& potential_children,
                       const HostnameTokens& parent) {
  // Need to account for all the possible domains given in the SSL certificate.
  for (const auto& potential_child : potential_children) {
    if (potential_child.empty() || potential_child.size() <= parent.size()) {
      continue;
    }
    int domain_diff = FindSubdomainDifference(potential_child, parent);
    if (domain_diff == 1 && potential_child[0] != "www")
      return true;
  }
  return false;
}

bool IsSubDomainOutsideWildcard(const GURL& request_url,
                                const net::X509Certificate& cert) {
  std::string host_name = request_url.host();
  HostnameTokens host_name_tokens = Tokenize(host_name);
  std::vector<std::string> dns_names;
  cert.GetDNSNames(&dns_names);
  bool result = false;

  // This method requires that the host name be longer than the dns name on
  // the certificate.
  for (const auto& dns_name : dns_names) {
    if (dns_name.length() < 2 || dns_name.length() >= host_name.length() ||
        dns_name.find('\0') != std::string::npos ||
        !IsHostNameKnownTLD(dns_name) || dns_name[0] != '*' ||
        dns_name[1] != '.') {
      continue;
    }

    // Move past the "*.".
    std::string extracted_dns_name = dns_name.substr(2);
    if (FindSubdomainDifference(host_name_tokens,
                                Tokenize(extracted_dns_name)) == 2) {
      return true;
    }
  }
  return result;
}

bool IsCertLikelyFromMultiTenantHosting(const GURL& request_url,
                                        const net::X509Certificate& cert) {
  std::string host_name = request_url.host();
  std::vector<std::string> dns_names;
  std::vector<std::string> dns_names_domain;
  cert.GetDNSNames(&dns_names);
  size_t dns_names_size = dns_names.size();

  // If there is only 1 DNS name then it is definitely not a shared certificate.
  if (dns_names_size == 0 || dns_names_size == 1)
    return false;

  // Check to see if all the domains in the SAN field in the SSL certificate are
  // the same or not.
  for (size_t i = 0; i < dns_names_size; ++i) {
    dns_names_domain.push_back(
        net::registry_controlled_domains::GetDomainAndRegistry(
            dns_names[i],
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  }
  for (size_t i = 1; i < dns_names_domain.size(); ++i) {
    if (dns_names_domain[i] != dns_names_domain[0])
      return false;
  }

  // If the number of DNS names is more than 5 then assume that it is a shared
  // certificate.
  static const int kDistinctNameThreshold = 5;
  if (dns_names_size > kDistinctNameThreshold)
    return true;

  // Heuristic - The edit distance between all the strings should be at least 5
  // for it to be counted as a shared SSLCertificate. If even one pair of
  // strings edit distance is below 5 then the certificate is no longer
  // considered as a shared certificate. Include the host name in the URL also
  // while comparing.
  dns_names.push_back(host_name);
  static const size_t kMinimumEditDsitance = 5;
  for (size_t i = 0; i < dns_names_size; ++i) {
    for (size_t j = i + 1; j < dns_names_size; ++j) {
      size_t edit_distance = GetLevenshteinDistance(dns_names[i], dns_names[j]);
      if (edit_distance < kMinimumEditDsitance)
        return false;
    }
  }
  return true;
}

bool IsCertLikelyFromSameDomain(const GURL& request_url,
                                const net::X509Certificate& cert) {
  std::string host_name = request_url.host();
  std::vector<std::string> dns_names;
  cert.GetDNSNames(&dns_names);

  dns_names.push_back(host_name);
  std::vector<std::string> dns_names_domain;

  for (const std::string& dns_name : dns_names) {
    dns_names_domain.push_back(
        net::registry_controlled_domains::GetDomainAndRegistry(
            dns_name,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  }

  DCHECK(!dns_names_domain.empty());
  const std::string& host_name_domain = dns_names_domain.back();

  // Last element is the original domain. So, excluding it.
  return std::find(dns_names_domain.begin(), dns_names_domain.end() - 1,
                   host_name_domain) != dns_names_domain.end() - 1;
}

bool IsHostnameNonUniqueOrDotless(const std::string& hostname) {
  return net::IsHostnameNonUnique(hostname) ||
         hostname.find('.') == std::string::npos;
}

}  // namespace ssl_errors
