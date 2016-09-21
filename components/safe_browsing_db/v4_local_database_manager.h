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
#include "components/safe_browsing_db/v4_database.h"
#include "components/safe_browsing_db/v4_get_hash_protocol_manager.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "components/safe_browsing_db/v4_update_protocol_manager.h"
#include "url/gurl.h"

using content::ResourceType;

namespace safe_browsing {

typedef unsigned ThreatSeverity;

// Manages the local, on-disk database of updates downloaded from the
// SafeBrowsing service and interfaces with the protocol manager.
class V4LocalDatabaseManager : public SafeBrowsingDatabaseManager {
 public:
  enum class ClientCallbackType {
    // This represents the case when we're trying to determine if a URL is
    // unsafe from the following perspectives: Malware, Phishing, UwS.
    CHECK_BROWSE_URL = 0,

    // This should always be the last value.
    CHECK_MAX
  };

  // The information we need to return the response to the SafeBrowsing client
  // that asked for the safety reputation of a URL if we can't determine that
  // synchronously.
  // TODO(vakh): In its current form, it only includes information for
  // |CheckBrowseUrl| method. Extend it to serve other methods on |client|.
  struct PendingCheck {
    PendingCheck(Client* client,
                 ClientCallbackType client_callback_type,
                 const GURL& url);

    ~PendingCheck();

    // The SafeBrowsing client that's waiting for the safe/unsafe verdict.
    Client* client;

    // Determines which funtion from the |client| needs to be called once we
    // know whether the URL in |url| is safe or unsafe.
    ClientCallbackType client_callback_type;

    // The URL that is being checked for being unsafe.
    GURL url;

    // The metadata associated with the full hash of the severest match found
    // for that URL.
    ThreatMetadata url_metadata;

    // The threat verdict for the URL being checked.
    SBThreatType result_threat_type;
  };

  // Construct V4LocalDatabaseManager.
  // Must be initialized by calling StartOnIOThread() before using.
  V4LocalDatabaseManager(const base::FilePath& base_path);

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
  void StartOnIOThread(net::URLRequestContextGetter* request_context_getter,
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
  bool MatchModuleWhitelistString(const std::string& str) override;
  bool CheckResourceUrl(const GURL& url, Client* client) override;
  bool IsMalwareKillSwitchOn() override;
  bool IsCsdWhitelistKillSwitchOn() override;

 protected:
  std::unordered_set<ListIdentifier> GetStoresForFullHashRequests() override;

 private:
  friend class V4LocalDatabaseManagerTest;
  void SetTaskRunnerForTest(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
    task_runner_ = task_runner;
  }
  FRIEND_TEST_ALL_PREFIXES(V4LocalDatabaseManagerTest,
                           TestGetSeverestThreatTypeAndMetadata);

  // The set of clients awaiting a full hash response. It is used for tracking
  // which clients have cancelled their outstanding request.
  typedef std::unordered_set<Client*> PendingClients;

  ~V4LocalDatabaseManager() override;

  // The callback called each time the protocol manager downloads updates
  // successfully.
  void UpdateRequestCompleted(
      std::unique_ptr<ParsedServerResponse> parsed_server_response);

  void SetupUpdateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config);

  void SetupDatabase();

  // Called when the |v4_get_hash_protocol_manager_| has the full hash response
  // avaialble for the URL that we requested. It determines the severest
  // threat type and responds to the |client| with that information.
  void OnFullHashResponse(std::unique_ptr<PendingCheck> pending_check,
                          const std::vector<FullHashInfo>& full_hash_infos);

  // Called when all the stores managed by the database have been read from
  // disk after startup and the database is ready for use.
  void DatabaseReady(std::unique_ptr<V4Database> v4_database);

  // Called when the database has been updated and schedules the next update.
  void DatabaseUpdated();

  // Calls the appopriate method on the |client| object, based on the contents
  // of |pending_check|.
  void RespondToClient(std::unique_ptr<PendingCheck> pending_check);

  // Finds the most severe |SBThreatType| and the corresponding |metadata| from
  // |full_hash_infos|.
  static void GetSeverestThreatTypeAndMetadata(
      SBThreatType* result_threat_type,
      ThreatMetadata* metadata,
      const std::vector<FullHashInfo>& full_hash_infos);

  // The base directory under which to create the files that contain hashes.
  const base::FilePath base_path_;

  // Whether the service is running.
  bool enabled_;

  // The set of clients that are waiting for a full hash response from the
  // SafeBrowsing service.
  PendingClients pending_clients_;

  // The list of stores to manage (for hash prefixes and full hashes), along
  // with the corresponding filename on disk for each of them.
  StoreIdAndFileNames store_id_file_names_;

  // The protocol manager that downloads the hash prefix updates.
  std::unique_ptr<V4UpdateProtocolManager> v4_update_protocol_manager_;

  // The database that manages the stores containing the hash prefix updates.
  // All writes to this variable must happen on the IO thread only.
  std::unique_ptr<V4Database> v4_database_;

  // Called when the V4Database has finished applying the latest update and is
  // ready to process next update.
  DatabaseUpdatedCallback db_updated_callback_;

  // The sequenced task runner for running safe browsing database operations.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  friend class base::RefCountedThreadSafe<V4LocalDatabaseManager>;
  DISALLOW_COPY_AND_ASSIGN(V4LocalDatabaseManager);
};  // class V4LocalDatabaseManager

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_LOCAL_DATABASE_MANAGER_H_
