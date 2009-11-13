// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Safe Browsing service is responsible for downloading anti-phishing and
// anti-malware tables and checking urls against them.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_

#include <deque>
#include <set>
#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "base/time.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/resource_type.h"

class BloomFilter;
class PrefService;
class SafeBrowsingBlockingPage;
class SafeBrowsingDatabase;
class SafeBrowsingProtocolManager;
class URLRequestContextGetter;

namespace base {
class Thread;
}

// Construction needs to happen on the main thread.
class SafeBrowsingService
    : public base::RefCountedThreadSafe<SafeBrowsingService> {
 public:
  // Users of this service implement this interface to be notified
  // asynchronously of the result.
  enum UrlCheckResult {
    URL_SAFE,
    URL_PHISHING,
    URL_MALWARE,
  };

  class Client {
   public:
    virtual ~Client() {}

    // Called when the result of checking a URL is known.
    virtual void OnUrlCheckResult(const GURL& url, UrlCheckResult result) = 0;

    // Called when the user has made a decision about how to handle the
    // SafeBrowsing interstitial page.
    virtual void OnBlockingPageComplete(bool proceed) = 0;
  };

  // Structure used to pass parameters between the IO and UI thread when
  // interacting with the blocking page.
  struct UnsafeResource {
    GURL url;
    ResourceType::Type resource_type;
    UrlCheckResult threat_type;
    Client* client;
    int render_process_host_id;
    int render_view_id;
  };

  // Bundle of SafeBrowsing state for one URL check.
  struct SafeBrowsingCheck {
    GURL url;
    Client* client;
    bool need_get_hash;
    base::Time start;  // Time that check was sent to SB service.
    UrlCheckResult result;
    std::vector<SBPrefix> prefix_hits;
    std::vector<SBFullHashResult> full_hits;
  };

  // Creates the safe browsing service.  Need to initialize before using.
  SafeBrowsingService();

  // Called on the UI thread to initialize the service.
  void Initialize();

  // Called on the main thread to let us know that the io_thread is going away.
  void ShutDown();

  // Returns true if the url's scheme can be checked.
  bool CanCheckUrl(const GURL& url) const;

  // Called on the IO thread to check if the given url is safe or not.  If we
  // can synchronously determine that the url is safe, CheckUrl returns true.
  // Otherwise it returns false, and "client" is called asynchronously with the
  // result when it is ready.
  bool CheckUrl(const GURL& url, Client* client);

  // Called on the IO thread to cancel a pending check if the result is no
  // longer needed.
  void CancelCheck(Client* client);

  // Called on the IO thread to display an interstitial page.
  void DisplayBlockingPage(const GURL& url,
                           ResourceType::Type resource_type,
                           UrlCheckResult result,
                           Client* client,
                           int render_process_host_id,
                           int render_view_id);

  // Called on the IO thread when the SafeBrowsingProtocolManager has received
  // the full hash results for prefix hits detected in the database.
  void HandleGetHashResults(
      SafeBrowsingCheck* check,
      const std::vector<SBFullHashResult>& full_hashes,
      bool can_cache);

  // Called on the IO thread.
  void HandleChunk(const std::string& list, std::deque<SBChunk>* chunks);
  void HandleChunkDelete(std::vector<SBChunkDelete>* chunk_deletes);

  // Update management.  Called on the IO thread.
  void UpdateStarted();
  void UpdateFinished(bool update_succeeded);

  // The blocking page on the UI thread has completed.
  void OnBlockingPageDone(const std::vector<UnsafeResource>& resources,
                          bool proceed);

  // Called on the UI thread when the SafeBrowsingProtocolManager has received
  // updated MAC keys.
  void OnNewMacKeys(const std::string& client_key,
                    const std::string& wrapped_key);

  // Notification on the UI thread from the advanced options UI.
  void OnEnable(bool enabled);

  bool enabled() const { return enabled_; }

  // Preference handling.
  static void RegisterPrefs(PrefService* prefs);

  // Called on the IO thread to reset the database.
  void ResetDatabase();

  // Log the user perceived delay caused by SafeBrowsing. This delay is the time
  // delta starting from when we would have started reading data from the
  // network, and ending when the SafeBrowsing check completes indicating that
  // the current page is 'safe'.
  void LogPauseDelay(base::TimeDelta time);

 private:
  typedef std::set<SafeBrowsingCheck*> CurrentChecks;
  typedef std::vector<SafeBrowsingCheck*> GetHashRequestors;
  typedef base::hash_map<SBPrefix, GetHashRequestors> GetHashRequests;

  // Used for whitelisting a render view when the user ignores our warning.
  struct WhiteListedEntry {
    int render_process_host_id;
    int render_view_id;
    std::string domain;
    UrlCheckResult result;
  };

  // Clients that we've queued up for checking later once the database is ready.
  struct QueuedCheck {
    Client* client;
    GURL url;
    base::Time start;
  };

  friend class base::RefCountedThreadSafe<SafeBrowsingService>;

  ~SafeBrowsingService();

  // Called to initialize objects that are used on the io_thread.
  void OnIOInitialize(const std::string& client_key,
                      const std::string& wrapped_key,
                      URLRequestContextGetter* request_context_getter);

  // Called to initialize objects that are used on the db_thread.
  void OnDBInitialize();

  // Called to shutdown operations on the io_thread.
  void OnIOShutdown();

  // Should only be called on db thread as SafeBrowsingDatabase is not
  // threadsafe.
  SafeBrowsingDatabase* GetDatabase();

  // Called on the IO thread with the check result.
  void OnCheckDone(SafeBrowsingCheck* info);

  // Called on the database thread to retrieve chunks.
  void GetAllChunksFromDatabase();

  // Called on the IO thread with the results of all chunks.
  void OnGetAllChunksFromDatabase(const std::vector<SBListChunkRanges>& lists,
                                  bool database_error);

  // Called on the db thread when a chunk insertion is complete.
  void ChunkInserted();

  // Called on the IO thread after the database reports that it added a chunk.
  void OnChunkInserted();

  // Notification that the database is done loading its bloom filter.
  void DatabaseLoadComplete();

  // Called on the database thread to add/remove chunks and host keys.
  // Callee will free the data when it's done.
  void HandleChunkForDatabase(const std::string& list,
                              std::deque<SBChunk>* chunks);

  void DeleteChunks(std::vector<SBChunkDelete>* chunk_deletes);

  static UrlCheckResult GetResultFromListname(const std::string& list_name);

  void NotifyClientBlockingComplete(Client* client, bool proceed);

  void DatabaseUpdateFinished(bool update_succeeded);

  // Start up SafeBrowsing objects. This can be called at browser start, or when
  // the user checks the "Enable SafeBrowsing" option in the Advanced options
  // UI.
  void Start();

  // Runs on the db thread to reset the database. We assume that resetting the
  // database is a synchronous operation.
  void OnResetDatabase();

  // Runs on the io thread when the reset is complete.
  void OnResetComplete();

  // Store in-memory the GetHash response. Runs on the database thread.
  void CacheHashResults(const std::vector<SBPrefix>& prefixes,
                        const std::vector<SBFullHashResult>& full_hashes);

  // Internal worker function for processing full hashes.
  void OnHandleGetHashResults(SafeBrowsingCheck* check,
                              const std::vector<SBFullHashResult>& full_hashes);

  void HandleOneCheck(SafeBrowsingCheck* check,
                      const std::vector<SBFullHashResult>& full_hashes);

  // Invoked on the UI thread to show the blocking page.
  void DoDisplayBlockingPage(const UnsafeResource& resource);

  // Report any pages that contain malware sub-resources to the SafeBrowsing
  // service.
  void ReportMalware(const GURL& malware_url,
                     const GURL& page_url,
                     const GURL& referrer_url);

  // During a reset or the initial load we may have to queue checks until the
  // database is ready. This method is run once the database has loaded (or if
  // we shut down SafeBrowsing before the database has finished loading).
  void RunQueuedClients();

  CurrentChecks checks_;

  // Used for issuing only one GetHash request for a given prefix.
  GetHashRequests gethash_requests_;

  // The sqlite database.  We don't use a scoped_ptr because it needs to be
  // destructed on a different thread than this object.
  SafeBrowsingDatabase* database_;

  // Handles interaction with SafeBrowsing servers.
  SafeBrowsingProtocolManager* protocol_manager_;

  std::vector<WhiteListedEntry> white_listed_entries_;

  // Whether the service is running. 'enabled_' is used by SafeBrowsingService
  // on the IO thread during normal operations.
  bool enabled_;

  // The SafeBrowsing thread that runs database operations.
  scoped_ptr<base::Thread> safe_browsing_thread_;

  // Indicates if we are in the process of resetting the database.
  bool resetting_;

  // Indicates if the database has finished initialization.
  bool database_loaded_;

  // Indicates if we're currently in an update cycle.
  bool update_in_progress_;

  std::deque<QueuedCheck> queued_checks_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingService);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
