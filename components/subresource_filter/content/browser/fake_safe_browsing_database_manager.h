// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_FAKE_SAFE_BROWSING_DATABASE_MANAGER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_FAKE_SAFE_BROWSING_DATABASE_MANAGER_H_

#include <map>
#include <set>

#include "base/macros.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "content/public/common/resource_type.h"

class GURL;

// Database manager that allows any URL to be configured as blacklisted for
// testing.
class FakeSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  FakeSafeBrowsingDatabaseManager();

  void AddBlacklistedUrl(const GURL& url,
                         safe_browsing::SBThreatType threat_type);
  void RemoveBlacklistedUrl(const GURL& url);

  void SimulateTimeout();

 protected:
  ~FakeSafeBrowsingDatabaseManager() override;

  // safe_browsing::TestSafeBrowsingDatabaseManager:
  bool CheckUrlForSubresourceFilter(const GURL& url, Client* client) override;
  bool CheckResourceUrl(const GURL& url, Client* client) override;
  bool IsSupported() const override;
  void CancelCheck(Client* client) override;
  bool ChecksAreAlwaysAsync() const override;
  bool CanCheckResourceType(
      content::ResourceType /* resource_type */) const override;
  safe_browsing::ThreatSource GetThreatSource() const override;
  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override;

 private:
  void OnCheckUrlForSubresourceFilterComplete(Client* client, const GURL& url);

  std::map<GURL, safe_browsing::SBThreatType> url_to_threat_type_;
  std::set<Client*> checks_;
  bool simulate_timeout_;

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingDatabaseManager);
};

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_FAKE_SAFE_BROWSING_DATABASE_MANAGER_H_
