// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_ERROR_CLASSIFICATION_H_
#define CHROME_BROWSER_SSL_SSL_ERROR_CLASSIFICATION_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/cert/x509_certificate.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// This class classifies characteristics of SSL errors, including information
// about captive portal detection.
//
// This class should only be used on the UI thread because its
// implementation uses captive_portal::CaptivePortalService which can only be
// accessed on the UI thread.
class SSLErrorClassification : public content::NotificationObserver {
 public:
  SSLErrorClassification(content::WebContents* web_contents,
                         const base::Time& current_time,
                         const GURL& url,
                         int cert_error,
                         const net::X509Certificate& cert);
  virtual ~SSLErrorClassification();

  // Returns true if the system time is in the past.
  static bool IsUserClockInThePast(const base::Time& time_now);

  // Returns true if the system time is too far in the future or the user is
  // using a version of Chrome which is more than 1 year old.
  static bool IsUserClockInTheFuture(const base::Time& time_now);

  // Returns true if the Windows platform is likely to not have SHA-256 support.
  // On other platforms, returns false always.
  static bool MaybeWindowsLacksSHA256Support();

  // A function which calculates the severity score when the ssl error is
  // |CERT_DATE_INVALID|. The calculated score is between 0.0 and 1.0, higher
  // being more severe, indicating how severe the certificate's
  // date invalid error is.
  void InvalidDateSeverityScore();

  // A function which calculates the severity score when the ssl error is
  // |CERT_COMMON_NAME_INVALID|. The calculated score is between 0.0 and 1.0,
  // higher being more severe, indicating how severe the certificate's common
  // name invalid error is.
  void InvalidCommonNameSeverityScore();

  void RecordUMAStatistics(bool overridable) const;
  void RecordCaptivePortalUMAStatistics(bool overridable) const;
  base::TimeDelta TimePassedSinceExpiry() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SSLErrorClassificationTest, TestDateInvalidScore);
  FRIEND_TEST_ALL_PREFIXES(SSLErrorClassificationTest, TestNameMismatch);
  FRIEND_TEST_ALL_PREFIXES(SSLErrorClassificationTest,
                           TestHostNameHasKnownTLD);

  typedef std::vector<std::string> Tokens;

  // Returns true if the hostname has a known Top Level Domain.
  static bool IsHostNameKnownTLD(const std::string& host_name);

  // Returns true if the site's hostname differs from one of the DNS
  // names in the certificate (CN or SANs) only by the presence or
  // absence of the single-label prefix "www". E.g.:
  //
  //     www.example.com ~ example.com -> true
  //     example.com ~ www.example.com -> true
  //     www.food.example.com ~ example.com -> false
  //     mail.example.com ~ example.com -> false
  bool IsWWWSubDomainMatch() const;

  // Returns true if |child| is a subdomain of any of the |potential_parents|.
  bool NameUnderAnyNames(const Tokens& child,
                        const std::vector<Tokens>& potential_parents) const;

  // Returns true if any of the |potential_children| is a subdomain of the
  // |parent|. The inverse case should be treated carefully as this is most
  // likely a MITM attack. We don't want foo.appspot.com to be able to MITM for
  // appspot.com.
  bool AnyNamesUnderName(const std::vector<Tokens>& potential_children,
                        const Tokens& parent) const;

  // Returns true if |hostname| is too broad for the scope of a wildcard
  // certificate. E.g.:
  //
  //     a.b.example.com ~ *.example.com --> true
  //     b.example.com ~ *.example.com --> false
  bool IsSubDomainOutsideWildcard(const Tokens& hostname) const;

  // Returns true if the certificate is a shared certificate. Note - This
  // function should be used with caution (only for UMA histogram) as an
  // attacker could easily get a certificate with more than 5 names in the SAN
  // fields.
  bool IsCertLikelyFromMultiTenantHosting() const;

  static std::vector<Tokens> GetTokenizedDNSNames(
      const std::vector<std::string>& dns_names);

  // If |potential_subdomain| is a subdomain of |parent|, returns the
  // number of DNS labels by which |potential_subdomain| is under
  // |parent|. Otherwise, returns 0.
  //
  // For example,
  //
  //   FindSubDomainDifference(Tokenize("a.b.example.com"),
  //                           Tokenize("example.com"))
  // --> 2.
  size_t FindSubDomainDifference(const Tokens& potential_subdomain,
                                 const Tokens& parent) const;

  static Tokens Tokenize(const std::string& name);

  float CalculateScoreTimePassedSinceExpiry() const;
  float CalculateScoreEnvironments() const;

  // content::NotificationObserver:
  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE;

  content::WebContents* web_contents_;
  // This stores the current time.
  base::Time current_time_;
  const GURL& request_url_;
  int cert_error_;
  // This stores the certificate.
  const net::X509Certificate& cert_;
  // Is captive portal detection enabled?
  bool captive_portal_detection_enabled_;
  // Did the probe complete before the interstitial was closed?
  bool captive_portal_probe_completed_;
  // Did the captive portal probe receive an error or get a non-HTTP response?
  bool captive_portal_no_response_;
  // Was a captive portal detected?
  bool captive_portal_detected_;

  content::NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_SSL_SSL_ERROR_CLASSIFICATION_H_
