// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_PING_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_PING_MANAGER_H_

// A class that reports safebrowsing statistics to Google's SafeBrowsing
// servers.
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/safe_browsing/protocol_manager_helper.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net


class SafeBrowsingPingManager : public net::URLFetcherDelegate {
 public:
  virtual ~SafeBrowsingPingManager();

  // Create an instance of the safe browsing ping manager.
  static SafeBrowsingPingManager* Create(
      net::URLRequestContextGetter* request_context_getter,
      const SafeBrowsingProtocolConfig& config);

  // net::URLFetcherDelegate interface.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // For UMA users we report to Google when a SafeBrowsing interstitial is shown
  // to the user.  |threat_type| should be one of the types known by
  // SafeBrowsingHitUrl.
  void ReportSafeBrowsingHit(const GURL& malicious_url,
                             const GURL& page_url,
                             const GURL& referrer_url,
                             bool is_subresource,
                             SBThreatType threat_type,
                             const std::string& post_data);

  // Users can opt-in on the SafeBrowsing interstitial to send detailed
  // malware reports. |report| is the serialized report.
  void ReportMalwareDetails(const std::string& report);

 private:
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingPingManagerTest,
                           TestSafeBrowsingHitUrl);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingPingManagerTest,
                           TestMalwareDetailsUrl);

  typedef std::set<const net::URLFetcher*> Reports;

  // Constructs a SafeBrowsingPingManager that issues network requests
  // using |request_context_getter|.
  SafeBrowsingPingManager(
      net::URLRequestContextGetter* request_context_getter,
      const SafeBrowsingProtocolConfig& config);

  // Generates URL for reporting safe browsing hits for UMA users.
  GURL SafeBrowsingHitUrl(
      const GURL& malicious_url, const GURL& page_url, const GURL& referrer_url,
      bool is_subresource,
      SBThreatType threat_type) const;
  // Generates URL for reporting malware details for users who opt-in.
  GURL MalwareDetailsUrl() const;

  // Current product version sent in each request.
  std::string version_;

  // The safe browsing client name sent in each request.
  std::string client_name_;

  // The context we use to issue network requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // URL prefix where browser reports hits to the safebrowsing list and
  // sends detaild malware reports for UMA users.
  std::string url_prefix_;

  // Track outstanding SafeBrowsing report fetchers for clean up.
  // We add both "hit" and "detail" fetchers in this set.
  Reports safebrowsing_reports_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingPingManager);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_PING_MANAGER_H_
