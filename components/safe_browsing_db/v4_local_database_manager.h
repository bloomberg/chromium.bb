// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_LOCAL_DATABASE_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_LOCAL_DATABASE_MANAGER_H_

// A class that provides the interface between the SafeBrowsing protocol manager
// and database that holds the downloaded updates.

#include <memory>

#include "components/safe_browsing_db/database_manager.h"
#include "components/safe_browsing_db/hit_report.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "components/safe_browsing_db/v4_update_protocol_manager.h"
#include "url/gurl.h"

using content::ResourceType;

namespace safe_browsing {

// Manages the local, on-disk database of updates downloaded from the
// SafeBrowsing service and interfaces with the protocol manager.
class V4LocalDatabaseManager : public SafeBrowsingDatabaseManager {
 public:
  // Construct V4LocalDatabaseManager.
  // Must be initialized by calling StartOnIOThread() before using.
  V4LocalDatabaseManager();

  //
  // SafeBrowsingDatabaseManager implementation
  //

  bool IsSupported() const override;
  safe_browsing::ThreatSource GetThreatSource() const override;
  bool ChecksAreAlwaysAsync() const override;
  bool CanCheckResourceType(content::ResourceType resource_type) const override;
  bool CanCheckUrl(const GURL& url) const override;
  bool IsDownloadProtectionEnabled() const override;
  bool CheckBrowseUrl(const GURL& url, Client* client) override;
  void CancelCheck(Client* client) override;
  void StartOnIOThread(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config) override;
  void StopOnIOThread(bool shutdown) override;
  bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                        Client* client) override;
  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override;
  bool MatchCsdWhitelistUrl(const GURL& url) override;
  bool MatchMalwareIP(const std::string& ip_address) override;
  bool MatchDownloadWhitelistUrl(const GURL& url) override;
  bool MatchDownloadWhitelistString(const std::string& str) override;
  bool MatchInclusionWhitelistUrl(const GURL& url) override;
  bool MatchModuleWhitelistString(const std::string& str) override;
  bool CheckResourceUrl(const GURL& url, Client* client) override;
  bool IsMalwareKillSwitchOn() override;
  bool IsCsdWhitelistKillSwitchOn() override;

 private:
  ~V4LocalDatabaseManager() override;

  // The callback called each time the protocol manager downloads updates
  // successfully.
  void UpdateRequestCompleted(const std::vector<ListUpdateResponse>& responses);

  bool enabled_;

  // Stores the current status of the lists to download from the SafeBrowsing
  // servers.
  base::hash_map<UpdateListIdentifier, std::string> current_list_states_;

  // The protocol manager that downloads the hash prefix updates.
  std::unique_ptr<V4UpdateProtocolManager> v4_update_protocol_manager_;

  friend class base::RefCountedThreadSafe<V4LocalDatabaseManager>;
  DISALLOW_COPY_AND_ASSIGN(V4LocalDatabaseManager);
};  // class V4LocalDatabaseManager

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_LOCAL_DATABASE_MANAGER_H_
