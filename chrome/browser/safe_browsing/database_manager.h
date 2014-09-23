// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Safe Browsing service is responsible for downloading anti-phishing and
// anti-malware tables and checking urls against them.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DATABASE_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_DATABASE_MANAGER_H_

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "url/gurl.h"

class SafeBrowsingService;
class SafeBrowsingDatabase;

namespace base {
class Thread;
}

namespace net {
class URLRequestContext;
class URLRequestContextGetter;
}

namespace safe_browsing {
class ClientSideDetectionService;
class DownloadProtectionService;
}

// Construction needs to happen on the main thread.
class SafeBrowsingDatabaseManager
    : public base::RefCountedThreadSafe<SafeBrowsingDatabaseManager>,
      public SafeBrowsingProtocolManagerDelegate {
 public:
  class Client;

  // Bundle of SafeBrowsing state while performing a URL or hash prefix check.
  struct SafeBrowsingCheck {
    // |check_type| should correspond to the type of item that is being
    // checked, either a URL or a binary hash/URL. We store this for two
    // purposes: to know which of Client's methods to call when a result is
    // known, and for logging purposes. It *isn't* used to predict the response
    // list type, that is information that the server gives us.
    SafeBrowsingCheck(const std::vector<GURL>& urls,
                      const std::vector<SBFullHash>& full_hashes,
                      Client* client,
                      safe_browsing_util::ListType check_type,
                      const std::vector<SBThreatType>& expected_threats);
    ~SafeBrowsingCheck();

    // Either |urls| or |full_hashes| is used to lookup database. |*_results|
    // are parallel vectors containing the results. They are initialized to
    // contain SB_THREAT_TYPE_SAFE.
    std::vector<GURL> urls;
    std::vector<SBThreatType> url_results;
    std::vector<std::string> url_metadata;
    std::vector<SBFullHash> full_hashes;
    std::vector<SBThreatType> full_hash_results;

    Client* client;
    bool need_get_hash;
    base::TimeTicks start;  // When check was sent to SB service.
    safe_browsing_util::ListType check_type;  // See comment in constructor.
    std::vector<SBThreatType> expected_threats;
    std::vector<SBPrefix> prefix_hits;
    std::vector<SBFullHashResult> cache_hits;

    // Vends weak pointers for TimeoutCallback().  If the response is
    // received before the timeout fires, factory is destructed and
    // the timeout won't be fired.
    // TODO(lzheng): We should consider to use this time out check
    // for browsing too (instead of implementin in
    // safe_browsing_resource_handler.cc).
    scoped_ptr<base::WeakPtrFactory<
        SafeBrowsingDatabaseManager> > timeout_factory_;

   private:
    DISALLOW_COPY_AND_ASSIGN(SafeBrowsingCheck);
  };

  class Client {
   public:
    void OnSafeBrowsingResult(const SafeBrowsingCheck& check);

   protected:
    virtual ~Client() {}

    // Called when the result of checking a browse URL is known.
    virtual void OnCheckBrowseUrlResult(const GURL& url,
                                        SBThreatType threat_type,
                                        const std::string& metadata) {}

    // Called when the result of checking a download URL is known.
    virtual void OnCheckDownloadUrlResult(const std::vector<GURL>& url_chain,
                                          SBThreatType threat_type) {}

    // Called when the result of checking a set of extensions is known.
    virtual void OnCheckExtensionsResult(
        const std::set<std::string>& threats) {}
  };

  // Creates the safe browsing service.  Need to initialize before using.
  explicit SafeBrowsingDatabaseManager(
      const scoped_refptr<SafeBrowsingService>& service);

  // Returns true if the url's scheme can be checked.
  bool CanCheckUrl(const GURL& url) const;

  // Returns whether download protection is enabled.
  bool download_protection_enabled() const {
    return enable_download_protection_;
  }

  // Called on the IO thread to check if the given url is safe or not.  If we
  // can synchronously determine that the url is safe, CheckUrl returns true.
  // Otherwise it returns false, and "client" is called asynchronously with the
  // result when it is ready.
  virtual bool CheckBrowseUrl(const GURL& url, Client* client);

  // Check if the prefix for |url| is in safebrowsing download add lists.
  // Result will be passed to callback in |client|.
  virtual bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                                Client* client);

  // Check which prefixes in |extension_ids| are in the safebrowsing blacklist.
  // Returns true if not, false if further checks need to be made in which case
  // the result will be passed to |client|.
  virtual bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                                 Client* client);

  // Check if the given url is on the side-effect free whitelist.
  // Can be called on any thread. Returns false if the check cannot be performed
  // (e.g. because we are disabled or because of an invalid scheme in the URL).
  // Otherwise, returns true if the URL is on the whitelist based on matching
  // the hash prefix only (so there may be false positives).
  virtual bool CheckSideEffectFreeWhitelistUrl(const GURL& url);

  // Check if the |url| matches any of the full-length hashes from the
  // client-side phishing detection whitelist.  Returns true if there was a
  // match and false otherwise.  To make sure we are conservative we will return
  // true if an error occurs. This method is expected to be called on the IO
  // thread.
  virtual bool MatchCsdWhitelistUrl(const GURL& url);

  // Check if the given IP address (either IPv4 or IPv6) matches the malware
  // IP blacklist.
  virtual bool MatchMalwareIP(const std::string& ip_address);

  // Check if the |url| matches any of the full-length hashes from the
  // download whitelist.  Returns true if there was a match and false otherwise.
  // To make sure we are conservative we will return true if an error occurs.
  // This method is expected to be called on the IO thread.
  virtual bool MatchDownloadWhitelistUrl(const GURL& url);

  // Check if |str| matches any of the full-length hashes from the download
  // whitelist.  Returns true if there was a match and false otherwise.
  // To make sure we are conservative we will return true if an error occurs.
  // This method is expected to be called on the IO thread.
  virtual bool MatchDownloadWhitelistString(const std::string& str);

  // Check if the CSD malware IP matching kill switch is turned on.
  virtual bool IsMalwareKillSwitchOn();

  // Check if the CSD whitelist kill switch is turned on.
  virtual bool IsCsdWhitelistKillSwitchOn();

  // Called on the IO thread to cancel a pending check if the result is no
  // longer needed.
  void CancelCheck(Client* client);

  // Called on the IO thread when the SafeBrowsingProtocolManager has received
  // the full hash results for prefix hits detected in the database.
  void HandleGetHashResults(SafeBrowsingCheck* check,
                            const std::vector<SBFullHashResult>& full_hashes,
                            const base::TimeDelta& cache_lifetime);

  // Log the user perceived delay caused by SafeBrowsing. This delay is the time
  // delta starting from when we would have started reading data from the
  // network, and ending when the SafeBrowsing check completes indicating that
  // the current page is 'safe'.
  void LogPauseDelay(base::TimeDelta time);

  // Called to initialize objects that are used on the io_thread.  This may be
  // called multiple times during the life of the DatabaseManager. Should be
  // called on IO thread.
  void StartOnIOThread();

  // Called to stop or shutdown operations on the io_thread. This may be called
  // multiple times during the life of the DatabaseManager. Should be called
  // on IO thread. If shutdown is true, the manager is disabled permanently.
  void StopOnIOThread(bool shutdown);

 protected:
  virtual ~SafeBrowsingDatabaseManager();

  // protected for tests.
  void NotifyDatabaseUpdateFinished(bool update_succeeded);

 private:
  friend class base::RefCountedThreadSafe<SafeBrowsingDatabaseManager>;
  friend class SafeBrowsingServerTest;
  friend class SafeBrowsingServiceTest;
  friend class SafeBrowsingServiceTestHelper;
  friend class SafeBrowsingDatabaseManagerTest;
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseManagerTest, GetUrlThreatType);

  typedef std::set<SafeBrowsingCheck*> CurrentChecks;
  typedef std::vector<SafeBrowsingCheck*> GetHashRequestors;
  typedef base::hash_map<SBPrefix, GetHashRequestors> GetHashRequests;

  // Clients that we've queued up for checking later once the database is ready.
  struct QueuedCheck {
    QueuedCheck(const safe_browsing_util::ListType check_type,
                Client* client,
                const GURL& url,
                const std::vector<SBThreatType>& expected_threats,
                const base::TimeTicks& start);
    ~QueuedCheck();
    safe_browsing_util::ListType check_type;
    Client* client;
    GURL url;
    std::vector<SBThreatType> expected_threats;
    base::TimeTicks start;  // When check was queued.
  };

  // Return the threat type from the first result in |full_hashes| which matches
  // |hash|, or SAFE if none match.
  static SBThreatType GetHashThreatType(
      const SBFullHash& hash,
      const std::vector<SBFullHashResult>& full_hashes);

  // Given a URL, compare all the possible host + path full hashes to the set of
  // provided full hashes.  Returns the threat type of the matching result from
  // |full_hashes|, or SAFE if none match.
  static SBThreatType GetUrlThreatType(
      const GURL& url,
      const std::vector<SBFullHashResult>& full_hashes,
      size_t* index);

  // Called to stop operations on the io_thread. This may be called multiple
  // times during the life of the DatabaseManager. Should be called on IO
  // thread.
  void DoStopOnIOThread();

  // Returns whether |database_| exists and is accessible.
  bool DatabaseAvailable() const;

  // Called on the IO thread.  If the database does not exist, queues up a call
  // on the db thread to create it.  Returns whether the database is available.
  //
  // Note that this is only needed outside the db thread, since functions on the
  // db thread can call GetDatabase() directly.
  bool MakeDatabaseAvailable();

  // Should only be called on db thread as SafeBrowsingDatabase is not
  // threadsafe.
  SafeBrowsingDatabase* GetDatabase();

  // Called on the IO thread with the check result.
  void OnCheckDone(SafeBrowsingCheck* info);

  // Called on the database thread to retrieve chunks.
  void GetAllChunksFromDatabase(GetChunksCallback callback);

  // Called on the IO thread with the results of all chunks.
  void OnGetAllChunksFromDatabase(const std::vector<SBListChunkRanges>& lists,
                                  bool database_error,
                                  GetChunksCallback callback);

  // Called on the IO thread after the database reports that it added a chunk.
  void OnAddChunksComplete(AddChunksCallback callback);

  // Notification that the database is done loading its bloom filter.  We may
  // have had to queue checks until the database is ready, and if so, this
  // checks them.
  void DatabaseLoadComplete();

  // Called on the database thread to add/remove chunks and host keys.
  void AddDatabaseChunks(const std::string& list,
                         scoped_ptr<ScopedVector<SBChunkData> > chunks,
                         AddChunksCallback callback);

  void DeleteDatabaseChunks(
      scoped_ptr<std::vector<SBChunkDelete> > chunk_deletes);

  void NotifyClientBlockingComplete(Client* client, bool proceed);

  void DatabaseUpdateFinished(bool update_succeeded);

  // Called on the db thread to close the database.  See CloseDatabase().
  void OnCloseDatabase();

  // Runs on the db thread to reset the database. We assume that resetting the
  // database is a synchronous operation.
  void OnResetDatabase();

  // Internal worker function for processing full hashes.
  void OnHandleGetHashResults(SafeBrowsingCheck* check,
                              const std::vector<SBFullHashResult>& full_hashes);

  // Run one check against |full_hashes|.  Returns |true| if the check
  // finds a match in |full_hashes|.
  bool HandleOneCheck(SafeBrowsingCheck* check,
                      const std::vector<SBFullHashResult>& full_hashes);

  // Invoked by CheckDownloadUrl. It checks the download URL on
  // safe_browsing_thread_.
  void CheckDownloadUrlOnSBThread(SafeBrowsingCheck* check);

  // The callback function when a safebrowsing check is timed out. Client will
  // be notified that the safebrowsing check is SAFE when this happens.
  void TimeoutCallback(SafeBrowsingCheck* check);

  // Calls the Client's callback on IO thread after CheckDownloadUrl finishes.
  void CheckDownloadUrlDone(SafeBrowsingCheck* check);

  // Checks all extension ID hashes on safe_browsing_thread_.
  void CheckExtensionIDsOnSBThread(SafeBrowsingCheck* check);

  // Helper function that calls safe browsing client and cleans up |checks_|.
  void SafeBrowsingCheckDone(SafeBrowsingCheck* check);

  // Helper function to set |check| with default values and start a safe
  // browsing check with timeout of |timeout|. |task| will be called on
  // success, otherwise TimeoutCallback will be called.
  void StartSafeBrowsingCheck(SafeBrowsingCheck* check,
                              const base::Closure& task);

  // SafeBrowsingProtocolManageDelegate override
  virtual void ResetDatabase() OVERRIDE;
  virtual void UpdateStarted() OVERRIDE;
  virtual void UpdateFinished(bool success) OVERRIDE;
  virtual void GetChunks(GetChunksCallback callback) OVERRIDE;
  virtual void AddChunks(const std::string& list,
                         scoped_ptr<ScopedVector<SBChunkData> > chunks,
                         AddChunksCallback callback) OVERRIDE;
  virtual void DeleteChunks(
      scoped_ptr<std::vector<SBChunkDelete> > chunk_deletes) OVERRIDE;

  scoped_refptr<SafeBrowsingService> sb_service_;

  CurrentChecks checks_;

  // Used for issuing only one GetHash request for a given prefix.
  GetHashRequests gethash_requests_;

  // The persistent database.  We don't use a scoped_ptr because it
  // needs to be destroyed on a different thread than this object.
  SafeBrowsingDatabase* database_;

  // Lock used to prevent possible data races due to compiler optimizations.
  mutable base::Lock database_lock_;

  // Whether the service is running. 'enabled_' is used by the
  // SafeBrowsingDatabaseManager on the IO thread during normal operations.
  bool enabled_;

  // Indicate if download_protection is enabled by command switch
  // so we allow this feature to be exersized.
  bool enable_download_protection_;

  // Indicate if client-side phishing detection whitelist should be enabled
  // or not.
  bool enable_csd_whitelist_;

  // Indicate if the download whitelist should be enabled or not.
  bool enable_download_whitelist_;

  // Indicate if the extension blacklist should be enabled.
  bool enable_extension_blacklist_;

  // Indicate if the side effect free whitelist should be enabled.
  bool enable_side_effect_free_whitelist_;

  // Indicate if the csd malware IP blacklist should be enabled.
  bool enable_ip_blacklist_;

  // The SafeBrowsing thread that runs database operations.
  //
  // Note: Functions that run on this thread should run synchronously and return
  // to the IO thread, not post additional tasks back to this thread, lest we
  // cause a race condition at shutdown time that leads to a database leak.
  scoped_ptr<base::Thread> safe_browsing_thread_;

  // Indicates if we're currently in an update cycle.
  bool update_in_progress_;

  // When true, newly fetched chunks may not in the database yet since the
  // database is still updating.
  bool database_update_in_progress_;

  // Indicates if we're in the midst of trying to close the database.  If this
  // is true, nothing on the IO thread should access the database.
  bool closing_database_;

  std::deque<QueuedCheck> queued_checks_;

  // Timeout to use for safe browsing checks.
  base::TimeDelta check_timeout_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingDatabaseManager);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_DATABASE_MANAGER_H_
