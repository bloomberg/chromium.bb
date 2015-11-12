// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SSL_ERRORS_ERROR_CLASSIFICATION_H_
#define COMPONENTS_SSL_ERRORS_ERROR_CLASSIFICATION_H_

#include <string>
#include <vector>

namespace base {
class Time;
}

class GURL;

namespace net {
class X509Certificate;
}

namespace ssl_errors {

typedef std::vector<std::string> HostnameTokens;

// Methods for identifying specific error causes. ------------------------------

// Returns true if the system time is in the past.
bool IsUserClockInThePast(const base::Time& time_now);

// Returns true if the system time is too far in the future or the user is
// using a version of Chrome which is more than 1 year old.
bool IsUserClockInTheFuture(const base::Time& time_now);

// Returns true if |hostname| is too broad for the scope of a wildcard
// certificate. E.g.:
//     a.b.example.com ~ *.example.com --> true
//     b.example.com ~ *.example.com --> false
bool IsSubDomainOutsideWildcard(const GURL& request_url,
                                const net::X509Certificate& cert);

// Returns true if the certificate is a shared certificate. Note - This
// function should be used with caution (only for UMA histogram) as an
// attacker could easily get a certificate with more than 5 names in the SAN
// fields.
bool IsCertLikelyFromMultiTenantHosting(const GURL& request_url,
                                        const net::X509Certificate& cert);

// Returns true if the hostname in |request_url_| has the same domain
// (effective TLD + 1 label) as at least one of the subject
// alternative names in |cert_|.
bool IsCertLikelyFromSameDomain(const GURL& request_url,
                                const net::X509Certificate& cert);

// Returns true if the site's hostname differs from one of the DNS
// names in the certificate (CN or SANs) only by the presence or
// absence of the single-label prefix "www". E.g.: (The first domain
// is hostname and the second domain is a DNS name in the certificate)
//     www.example.com ~ example.com -> true
//     example.com ~ www.example.com -> true
//     www.food.example.com ~ example.com -> false
//     mail.example.com ~ example.com -> false
bool GetWWWSubDomainMatch(const GURL& request_url,
                          const std::vector<std::string>& dns_names,
                          std::string* www_match_host_name);

// Method for recording results. -----------------------------------------------

void RecordUMAStatistics(bool overridable,
                         const base::Time& current_time,
                         const GURL& request_url,
                         int cert_error,
                         const net::X509Certificate& cert);

// Helper methods for classification. ------------------------------------------

// Tokenize DNS names and hostnames.
HostnameTokens Tokenize(const std::string& name);

// Sets a clock for browser tests that check the build time. Used by
// IsUserClockInThePast and IsUserClockInTheFuture.
void SetBuildTimeForTesting(const base::Time& testing_time);

// Returns true if the hostname has a known Top Level Domain.
bool IsHostNameKnownTLD(const std::string& host_name);

// Returns true if any one of the following conditions hold:
// 1.|hostname| is an IP Address in an IANA-reserved range.
// 2.|hostname| is a not-yet-assigned by ICANN gTLD.
// 3.|hostname| is a dotless domain.
bool IsHostnameNonUniqueOrDotless(const std::string& hostname);

// Returns true if |child| is a subdomain of any of the |potential_parents|.
bool NameUnderAnyNames(const HostnameTokens& child,
                       const std::vector<HostnameTokens>& potential_parents);

// Returns true if any of the |potential_children| is a subdomain of the
// |parent|. The inverse case should be treated carefully as this is most
// likely a MITM attack. We don't want foo.appspot.com to be able to MITM for
// appspot.com.
bool AnyNamesUnderName(const std::vector<HostnameTokens>& potential_children,
                       const HostnameTokens& parent);

}  // namespace ssl_errors

#endif  // COMPONENTS_SSL_ERRORS_ERROR_CLASSIFICATION_H_
