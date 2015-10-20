// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TEST_DATABASE_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_TEST_DATABASE_MANAGER_H_

#include <set>
#include <string>
#include <vector>

#include "chrome/browser/safe_browsing/database_manager.h"

// This is a non-pure-virtual implementation of the SafeBrowsingDatabaseManager
// interface.  It's used in tests by overriding only the functions that get
// called, and it'll complain if you call one that isn't overriden.
class TestSafeBrowsingDatabaseManager
    : public SafeBrowsingDatabaseManager {
 public:
  // SafeBrowsingDatabaseManager implementation:
  bool IsSupported() const override;
  bool ChecksAreAlwaysAsync() const override;
  bool CanCheckResourceType(content::ResourceType resource_type) const override;
  bool CanCheckUrl(const GURL& url) const override;
  bool download_protection_enabled() const override;
  bool CheckBrowseUrl(const GURL& url, Client* client) override;
  bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                        Client* client) override;
  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override;
  bool MatchCsdWhitelistUrl(const GURL& url) override;
  bool MatchMalwareIP(const std::string& ip_address) override;
  bool MatchDownloadWhitelistUrl(const GURL& url) override;
  bool MatchDownloadWhitelistString(const std::string& str) override;
  bool MatchInclusionWhitelistUrl(const GURL& url) override;
  bool IsMalwareKillSwitchOn() override;
  bool IsCsdWhitelistKillSwitchOn() override;
  void CancelCheck(Client* client) override;
  void StartOnIOThread() override;
  void StopOnIOThread(bool shutdown) override;

 protected:
  ~TestSafeBrowsingDatabaseManager() override {};
};
#endif  // CHROME_BROWSER_SAFE_BROWSING_TEST_DATABASE_MANAGER_H_
