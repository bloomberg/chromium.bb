// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the SafeBrowsingDatabaseManager that sends URLs
// via IPC to a database that chromium doesn't manage locally.

#ifndef COMPONENTS_SAFE_BROWSING_DB_REMOTE_DATABASE_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_DB_REMOTE_DATABASE_MANAGER_H_

#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/safe_browsing_db/database_manager.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}

namespace safe_browsing {

struct V4ProtocolConfig;

// An implementation that proxies requests to a service outside of Chromium.
// Does not manage a local database.
class RemoteSafeBrowsingDatabaseManager : public SafeBrowsingDatabaseManager {
 public:
  // Construct RemoteSafeBrowsingDatabaseManager.
  // Must be initialized by calling StartOnIOThread() before using.
  RemoteSafeBrowsingDatabaseManager();

  //
  // SafeBrowsingDatabaseManager implementation
  //

  void CancelCheck(Client* client) override;
  bool CanCheckResourceType(content::ResourceType resource_type) const override;
  bool CanCheckSubresourceFilter() const override;
  bool CanCheckUrl(const GURL& url) const override;
  bool ChecksAreAlwaysAsync() const override;
  bool CheckBrowseUrl(const GURL& url,
                      const SBThreatTypeSet& threat_types,
                      Client* client) override;
  bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                        Client* client) override;
  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override;
  AsyncMatch CheckCsdWhitelistUrl(const GURL& url, Client* client) override;
  bool CheckResourceUrl(const GURL& url, Client* client) override;
  bool CheckUrlForSubresourceFilter(const GURL& url, Client* client) override;
  bool MatchCsdWhitelistUrl(const GURL& url) override;
  bool MatchDownloadWhitelistString(const std::string& str) override;
  bool MatchDownloadWhitelistUrl(const GURL& url) override;
  bool MatchMalwareIP(const std::string& ip_address) override;
  bool MatchModuleWhitelistString(const std::string& str) override;
  safe_browsing::ThreatSource GetThreatSource() const override;
  bool IsCsdWhitelistKillSwitchOn() override;
  bool IsDownloadProtectionEnabled() const override;
  bool IsMalwareKillSwitchOn() override;
  bool IsSupported() const override;
  void StartOnIOThread(net::URLRequestContextGetter* request_context_getter,
                       const V4ProtocolConfig& config) override;
  void StopOnIOThread(bool shutdown) override;

  //
  // RemoteSafeBrowsingDatabaseManager implementation
  //

 private:
  ~RemoteSafeBrowsingDatabaseManager() override;
  class ClientRequest;  // Per-request tracker.

  // Requests currently outstanding.  This owns the ptrs.
  std::vector<ClientRequest*> current_requests_;

  base::flat_set<content::ResourceType> resource_types_to_check_;

  friend class base::RefCountedThreadSafe<RemoteSafeBrowsingDatabaseManager>;
  DISALLOW_COPY_AND_ASSIGN(RemoteSafeBrowsingDatabaseManager);
};  // class RemoteSafeBrowsingDatabaseManager

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_REMOTE_DATABASE_MANAGER_H_
