// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_TEST_DATABASE_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_DB_TEST_DATABASE_MANAGER_H_

#include <set>
#include <string>
#include <vector>

#include "components/safe_browsing_db/database_manager.h"

namespace net {
class URLRequestContextGetter;
}

namespace safe_browsing {

struct V4ProtocolConfig;

// This is a non-pure-virtual implementation of the SafeBrowsingDatabaseManager
// interface.  It's used in tests by overriding only the functions that get
// called, and it'll complain if you call one that isn't overriden.
class TestSafeBrowsingDatabaseManager
    : public SafeBrowsingDatabaseManager {
 public:
  // SafeBrowsingDatabaseManager implementation:
  bool IsSupported() const override;
  safe_browsing::ThreatSource GetThreatSource() const override;
  bool ChecksAreAlwaysAsync() const override;
  bool CanCheckResourceType(content::ResourceType resource_type) const override;
  bool CanCheckUrl(const GURL& url) const override;
  bool IsDownloadProtectionEnabled() const override;
  bool CheckBrowseUrl(const GURL& url, Client* client) override;
  bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                        Client* client) override;
  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override;
  bool CheckResourceUrl(const GURL& url, Client* client) override;
  bool MatchCsdWhitelistUrl(const GURL& url) override;
  bool MatchMalwareIP(const std::string& ip_address) override;
  bool MatchDownloadWhitelistUrl(const GURL& url) override;
  bool MatchDownloadWhitelistString(const std::string& str) override;
  bool MatchInclusionWhitelistUrl(const GURL& url) override;
  bool MatchModuleWhitelistString(const std::string& str) override;
  bool IsMalwareKillSwitchOn() override;
  bool IsCsdWhitelistKillSwitchOn() override;
  void CancelCheck(Client* client) override;
  void CheckApiBlacklistUrl(const GURL& url, Client* client) override;
  void StartOnIOThread(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config) override;
  void StopOnIOThread(bool shutdown) override;

 protected:
  ~TestSafeBrowsingDatabaseManager() override {};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_TEST_DATABASE_MANAGER_H_
