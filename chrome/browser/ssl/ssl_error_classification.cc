// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/ssl/ssl_error_classification.h"

#include "base/build_time.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"
#include "url/gurl.h"

using base::Time;
using base::TimeTicks;
using base::TimeDelta;

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

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
  UNUSED_INTERSTITIAL_CAUSE_ENTRY,
};

// Scores/weights which will be constant through all the SSL error types.
static const float kServerWeight = 0.5f;
static const float kClientWeight = 0.5f;

void RecordSSLInterstitialCause(bool overridable, SSLInterstitialCause event) {
  if (overridable) {
    UMA_HISTOGRAM_ENUMERATION("interstitial.ssl.cause.overridable", event,
                              UNUSED_INTERSTITIAL_CAUSE_ENTRY);
  } else {
    UMA_HISTOGRAM_ENUMERATION("interstitial.ssl.cause.nonoverridable", event,
                              UNUSED_INTERSTITIAL_CAUSE_ENTRY);
  }
}

int GetLevensteinDistance(const std::string& str1,
                          const std::string& str2) {
  if (str1 == str2)
    return 0;
  if (str1.size() == 0)
    return str2.size();
  if (str2.size() == 0)
    return str1.size();
  std::vector<int> kFirstRow(str2.size() + 1, 0);
  std::vector<int> kSecondRow(str2.size() + 1, 0);

  for (size_t i = 0; i < kFirstRow.size(); ++i)
    kFirstRow[i] = i;
  for (size_t i = 0; i < str1.size(); ++i) {
    kSecondRow[0] = i + 1;
    for (size_t j = 0; j < str2.size(); ++j) {
      int cost = str1[i] == str2[j] ? 0 : 1;
      kSecondRow[j+1] = std::min(std::min(
          kSecondRow[j] + 1, kFirstRow[j + 1] + 1), kFirstRow[j] + cost);
    }
    for (size_t j = 0; j < kFirstRow.size(); j++)
      kFirstRow[j] = kSecondRow[j];
  }
  return kSecondRow[str2.size()];
}

} // namespace

SSLErrorClassification::SSLErrorClassification(
    const base::Time& current_time,
    const GURL& url,
    const net::X509Certificate& cert)
  : current_time_(current_time),
    request_url_(url),
    cert_(cert) { }

SSLErrorClassification::~SSLErrorClassification() { }

float SSLErrorClassification::InvalidDateSeverityScore(
    int cert_error) const {
  SSLErrorInfo::ErrorType type =
      SSLErrorInfo::NetErrorToErrorType(cert_error);
  DCHECK(type == SSLErrorInfo::CERT_DATE_INVALID);
  // Client-side characteristics. Check whether or not the system's clock is
  // wrong and whether or not the user has already encountered this error
  // before.
  float severity_date_score = 0.0f;

  static const float kCertificateExpiredWeight = 0.3f;
  static const float kNotYetValidWeight = 0.2f;

  static const float kSystemClockWeight = 0.75f;
  static const float kSystemClockWrongWeight = 0.1f;
  static const float kSystemClockRightWeight = 1.0f;

  if (IsUserClockInThePast(current_time_)  ||
      IsUserClockInTheFuture(current_time_)) {
    severity_date_score += kClientWeight * kSystemClockWeight *
        kSystemClockWrongWeight;
  } else {
    severity_date_score += kClientWeight * kSystemClockWeight *
        kSystemClockRightWeight;
  }
  // TODO(radhikabhar): (crbug.com/393262) Check website settings.

  // Server-side characteristics. Check whether the certificate has expired or
  // is not yet valid. If the certificate has expired then factor the time which
  // has passed since expiry.
  if (cert_.HasExpired()) {
    severity_date_score += kServerWeight * kCertificateExpiredWeight *
        CalculateScoreTimePassedSinceExpiry();
  }
  if (current_time_ < cert_.valid_start())
    severity_date_score += kServerWeight * kNotYetValidWeight;
  return severity_date_score;
}

float SSLErrorClassification::InvalidCommonNameSeverityScore(
    int cert_error) const {
  SSLErrorInfo::ErrorType type =
      SSLErrorInfo::NetErrorToErrorType(cert_error);
  DCHECK(type == SSLErrorInfo::CERT_COMMON_NAME_INVALID);
  float severity_name_score = 0.0f;

  static const float kWWWDifferenceWeight = 0.3f;
  static const float kNameUnderAnyNamesWeight = 0.2f;
  static const float kAnyNamesUnderNameWeight = 1.0f;
  static const float kLikelyMultiTenantHostingWeight = 0.1f;

  std::string host_name = request_url_.host();
  if (IsHostNameKnownTLD(host_name)) {
    Tokens host_name_tokens = Tokenize(host_name);
    if (IsWWWSubDomainMatch())
      severity_name_score += kServerWeight * kWWWDifferenceWeight;
    if (IsSubDomainOutsideWildcard(host_name_tokens))
      severity_name_score += kServerWeight * kWWWDifferenceWeight;

    std::vector<std::string> dns_names;
    cert_.GetDNSNames(&dns_names);
    std::vector<Tokens> dns_name_tokens = GetTokenizedDNSNames(dns_names);
    if (NameUnderAnyNames(host_name_tokens, dns_name_tokens))
      severity_name_score += kServerWeight * kNameUnderAnyNamesWeight;
    // Inverse case is more likely to be a MITM attack.
    if (AnyNamesUnderName(dns_name_tokens, host_name_tokens))
      severity_name_score += kServerWeight * kAnyNamesUnderNameWeight;
    if (IsCertLikelyFromMultiTenantHosting())
      severity_name_score += kServerWeight * kLikelyMultiTenantHostingWeight;
  }
  return severity_name_score;
}

void SSLErrorClassification::RecordUMAStatistics(bool overridable,
                                                 int cert_error) {
  SSLErrorInfo::ErrorType type =
      SSLErrorInfo::NetErrorToErrorType(cert_error);
  switch (type) {
    case SSLErrorInfo::CERT_DATE_INVALID: {
      if (IsUserClockInThePast(base::Time::NowFromSystemTime()))
        RecordSSLInterstitialCause(overridable, CLOCK_PAST);
      if (IsUserClockInTheFuture(base::Time::NowFromSystemTime()))
        RecordSSLInterstitialCause(overridable, CLOCK_FUTURE);
      break;
    }
    case SSLErrorInfo::CERT_COMMON_NAME_INVALID: {
      std::string host_name = request_url_.host();
      if (IsHostNameKnownTLD(host_name)) {
        Tokens host_name_tokens = Tokenize(host_name);
        if (IsWWWSubDomainMatch())
          RecordSSLInterstitialCause(overridable, WWW_SUBDOMAIN_MATCH);
        if (IsSubDomainOutsideWildcard(host_name_tokens))
          RecordSSLInterstitialCause(overridable, SUBDOMAIN_OUTSIDE_WILDCARD);
        std::vector<std::string> dns_names;
        cert_.GetDNSNames(&dns_names);
        std::vector<Tokens> dns_name_tokens = GetTokenizedDNSNames(dns_names);
        if (NameUnderAnyNames(host_name_tokens, dns_name_tokens))
          RecordSSLInterstitialCause(overridable, SUBDOMAIN_MATCH);
        if (AnyNamesUnderName(dns_name_tokens, host_name_tokens))
          RecordSSLInterstitialCause(overridable, SUBDOMAIN_INVERSE_MATCH);
        if (IsCertLikelyFromMultiTenantHosting())
          RecordSSLInterstitialCause(overridable, LIKELY_MULTI_TENANT_HOSTING);
      } else {
         RecordSSLInterstitialCause(overridable, HOST_NAME_NOT_KNOWN_TLD);
      }
      break;
    }
    default: {
      break;
    }
  }
}

base::TimeDelta SSLErrorClassification::TimePassedSinceExpiry() const {
  base::TimeDelta delta = current_time_ - cert_.valid_expiry();
  return delta;
}

float SSLErrorClassification::CalculateScoreTimePassedSinceExpiry() const {
  base::TimeDelta delta = TimePassedSinceExpiry();
  int64 time_passed = delta.InDays();
  const int64 kHighThreshold = 7;
  const int64 kLowThreshold = 4;
  static const float kHighThresholdWeight = 0.4f;
  static const float kMediumThresholdWeight = 0.3f;
  static const float kLowThresholdWeight = 0.2f;
  if (time_passed >= kHighThreshold)
    return kHighThresholdWeight;
  else if (time_passed >= kLowThreshold)
    return kMediumThresholdWeight;
  else
    return kLowThresholdWeight;
}

bool SSLErrorClassification::IsUserClockInThePast(const base::Time& time_now) {
  base::Time build_time = base::GetBuildTime();
  if (time_now < build_time - base::TimeDelta::FromDays(2))
    return true;
  return false;
}

bool SSLErrorClassification::IsUserClockInTheFuture(
    const base::Time& time_now) {
  base::Time build_time = base::GetBuildTime();
  if (time_now > build_time + base::TimeDelta::FromDays(365))
    return true;
  return false;
}

bool SSLErrorClassification::IsWindowsVersionSP3OrLower() {
#if defined(OS_WIN)
  const base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  base::win::OSInfo::ServicePack service_pack = os_info->service_pack();
  if (os_info->version() < base::win::VERSION_VISTA && service_pack.major < 3)
    return true;
#endif
  return false;
}

bool SSLErrorClassification::IsHostNameKnownTLD(const std::string& host_name) {
  size_t tld_length =
      net::registry_controlled_domains::GetRegistryLength(
          host_name,
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (tld_length == 0 || tld_length == std::string::npos)
    return false;
  return true;
}

std::vector<SSLErrorClassification::Tokens> SSLErrorClassification::
GetTokenizedDNSNames(const std::vector<std::string>& dns_names) {
  std::vector<std::vector<std::string>> dns_name_tokens;
  for (size_t i = 0; i < dns_names.size(); ++i) {
    std::vector<std::string> dns_name_token_single;
    if (dns_names[i].empty() || dns_names[i].find('\0') != std::string::npos
        || !(IsHostNameKnownTLD(dns_names[i]))) {
      dns_name_token_single.push_back(std::string());
    } else {
      dns_name_token_single = Tokenize(dns_names[i]);
    }
    dns_name_tokens.push_back(dns_name_token_single);
  }
  return dns_name_tokens;
}

size_t SSLErrorClassification::FindSubDomainDifference(
    const Tokens& potential_subdomain, const Tokens& parent) const {
  // A check to ensure that the number of tokens in the tokenized_parent is
  // less than the tokenized_potential_subdomain.
  if (parent.size() >= potential_subdomain.size())
    return 0;

  size_t tokens_match = 0;
  size_t diff_size = potential_subdomain.size() - parent.size();
  for (size_t i = 0; i < parent.size(); ++i) {
    if (parent[i] == potential_subdomain[i + diff_size])
      tokens_match++;
  }
  if (tokens_match == parent.size())
    return diff_size;
  return 0;
}

SSLErrorClassification::Tokens SSLErrorClassification::
Tokenize(const std::string& name) {
  Tokens name_tokens;
  base::SplitStringDontTrim(name, '.', &name_tokens);
  return name_tokens;
}

// We accept the inverse case for www for historical reasons.
bool SSLErrorClassification::IsWWWSubDomainMatch() const {
  std::string host_name = request_url_.host();
  if (IsHostNameKnownTLD(host_name)) {
    std::vector<std::string> dns_names;
    cert_.GetDNSNames(&dns_names);
    bool result = false;
    // Need to account for all possible domains given in the SSL certificate.
    for (size_t i = 0; i < dns_names.size(); ++i) {
      if (dns_names[i].empty() || dns_names[i].find('\0') != std::string::npos
          || dns_names[i].length() == host_name.length()
          || !(IsHostNameKnownTLD(dns_names[i]))) {
        result = result || false;
      } else if (dns_names[i].length() > host_name.length()) {
        result = result ||
            net::StripWWW(base::ASCIIToUTF16(dns_names[i])) ==
            base::ASCIIToUTF16(host_name);
      } else {
        result = result ||
            net::StripWWW(base::ASCIIToUTF16(host_name)) ==
            base::ASCIIToUTF16(dns_names[i]);
      }
    }
    return result;
  }
  return false;
}

bool SSLErrorClassification::NameUnderAnyNames(
    const Tokens& child,
    const std::vector<Tokens>& potential_parents) const {
  bool result = false;
  // Need to account for all the possible domains given in the SSL certificate.
  for (size_t i = 0; i < potential_parents.size(); ++i) {
    if (potential_parents[i].empty() ||
        potential_parents[i].size() >= child.size()) {
      result = result || false;
    } else {
      size_t domain_diff = FindSubDomainDifference(child,
                                                   potential_parents[i]);
      if (domain_diff == 1 &&  child[0] != "www")
        result = result || true;
    }
  }
  return result;
}

bool SSLErrorClassification::AnyNamesUnderName(
    const std::vector<Tokens>& potential_children,
    const Tokens& parent) const {
  bool result = false;
  // Need to account for all the possible domains given in the SSL certificate.
  for (size_t i = 0; i < potential_children.size(); ++i) {
    if (potential_children[i].empty() ||
        potential_children[i].size() <= parent.size()) {
      result = result || false;
    } else {
      size_t domain_diff = FindSubDomainDifference(potential_children[i],
                                                   parent);
      if (domain_diff == 1 &&  potential_children[i][0] != "www")
        result = result || true;
    }
  }
  return result;
}

bool SSLErrorClassification::IsSubDomainOutsideWildcard(
    const Tokens& host_name_tokens) const {
  std::string host_name = request_url_.host();
  std::vector<std::string> dns_names;
  cert_.GetDNSNames(&dns_names);
  bool result = false;

  // This method requires that the host name be longer than the dns name on
  // the certificate.
  for (size_t i = 0; i < dns_names.size(); ++i) {
    const std::string& name = dns_names[i];
    if (name.length() < 2 || name.length() >= host_name.length() ||
        name.find('\0') != std::string::npos ||
        !IsHostNameKnownTLD(name)
        || name[0] != '*' || name[1] != '.') {
      continue;
    }

    // Move past the "*.".
    std::string extracted_dns_name = name.substr(2);
    if (FindSubDomainDifference(
        host_name_tokens, Tokenize(extracted_dns_name)) == 2) {
      return true;
    }
  }
  return result;
}

bool SSLErrorClassification::IsCertLikelyFromMultiTenantHosting() const {
  std::string host_name = request_url_.host();
  std::vector<std::string> dns_names;
  std::vector<std::string> dns_names_domain;
  cert_.GetDNSNames(&dns_names);
  size_t dns_names_size = dns_names.size();

  // If there is only 1 DNS name then it is definitely not a shared certificate.
  if (dns_names_size == 0 || dns_names_size == 1)
    return false;

  // Check to see if all the domains in the SAN field in the SSL certificate are
  // the same or not.
  for (size_t i = 0; i < dns_names_size; ++i) {
    dns_names_domain.push_back(
        net::registry_controlled_domains::
        GetDomainAndRegistry(
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
  static const int kMinimumEditDsitance = 5;
  for (size_t i = 0; i < dns_names_size; ++i) {
    for (size_t j = i + 1; j < dns_names_size; ++j) {
      int edit_distance = GetLevensteinDistance(dns_names[i], dns_names[j]);
      if (edit_distance < kMinimumEditDsitance)
        return false;
    }
  }
  return true;
}
