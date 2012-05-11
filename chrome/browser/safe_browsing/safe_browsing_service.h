// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Safe Browsing service is responsible for downloading anti-phishing and
// anti-malware tables and checking urls against them.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
#pragma once

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_helpers.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"

class MalwareDetails;
class PrefChangeRegistrar;
class PrefService;
class SafeBrowsingDatabase;
class SafeBrowsingProtocolManager;
class SafeBrowsingServiceFactory;
class SafeBrowsingURLRequestContextGetter;

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
class SafeBrowsingService
    : public base::RefCountedThreadSafe<
          SafeBrowsingService, content::BrowserThread::DeleteOnUIThread>,
      public content::NotificationObserver {
 public:
  class Client;
  // Users of this service implement this interface to be notified
  // asynchronously of the result.
  enum UrlCheckResult {
    SAFE,
    URL_PHISHING,
    URL_MALWARE,
    BINARY_MALWARE_URL,  // Binary url leads to a malware.
    BINARY_MALWARE_HASH,  // Binary hash indicates this is a malware.

    // Url detected by the client-side phishing model.  Note that unlike the
    // above values, this does not correspond to a downloaded list.
    CLIENT_SIDE_PHISHING_URL,
  };

  // Passed a boolean indicating whether or not it is OK to proceed with
  // loading an URL.
  typedef base::Callback<void(bool /*proceed*/)> UrlCheckCallback;

  // Structure used to pass parameters between the IO and UI thread when
  // interacting with the blocking page.
  struct UnsafeResource {
    UnsafeResource();
    ~UnsafeResource();

    GURL url;
    GURL original_url;
    std::vector<GURL> redirect_urls;
    bool is_subresource;
    UrlCheckResult threat_type;
    UrlCheckCallback callback;
    int render_process_host_id;
    int render_view_id;
  };

  // Bundle of SafeBrowsing state for one URL or hash prefix check.
  struct SafeBrowsingCheck {
    SafeBrowsingCheck();
    ~SafeBrowsingCheck();

    // Either |urls| or |prefix| is used to lookup database.
    std::vector<GURL> urls;
    scoped_ptr<SBFullHash> full_hash;

    Client* client;
    bool need_get_hash;
    base::TimeTicks start;  // When check was sent to SB service.
    UrlCheckResult result;
    bool is_download;  // If this check for download url or hash.
    std::vector<SBPrefix> prefix_hits;
    std::vector<SBFullHashResult> full_hits;

    // Vends weak pointers for TimeoutCallback().  If the response is
    // received before the timeout fires, factory is destructed and
    // the timeout won't be fired.
    // TODO(lzheng): We should consider to use this time out check
    // for browsing too (instead of implementin in
    // safe_browsing_resource_handler.cc).
    scoped_ptr<base::WeakPtrFactory<SafeBrowsingService> > timeout_factory_;

   private:
    DISALLOW_COPY_AND_ASSIGN(SafeBrowsingCheck);
  };

  // Observer class can be used to get notified when a SafeBrowsing hit
  // was found.
  class Observer {
   public:
    // The |resource| must not be accessed after OnSafeBrowsingHit returns.
    // This method will be called on the UI thread.
    virtual void OnSafeBrowsingHit(const UnsafeResource& resource) = 0;

   protected:
    Observer() {}
    virtual ~Observer() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  class Client {
   public:
    virtual ~Client() {}

    void OnSafeBrowsingResult(const SafeBrowsingCheck& check);

   protected:
    // Called when the result of checking a browse URL is known.
    virtual void OnBrowseUrlCheckResult(const GURL& url,
                                        UrlCheckResult result) {}

    // Called when the result of checking a download URL is known.
    virtual void OnDownloadUrlCheckResult(const std::vector<GURL>& url_chain,
                                          UrlCheckResult result) {}

    // Called when the result of checking a download binary hash is known.
    virtual void OnDownloadHashCheckResult(const std::string& hash,
                                           UrlCheckResult result) {}
  };

  // Makes the passed |factory| the factory used to instanciate
  // a SafeBrowsingService. Useful for tests.
  static void RegisterFactory(SafeBrowsingServiceFactory* factory) {
    factory_ = factory;
  }

  // Create an instance of the safe browsing service.
  static SafeBrowsingService* CreateSafeBrowsingService();

  // Called on the UI thread to initialize the service.
  void Initialize();

  // Called on the main thread to let us know that the io_thread is going away.
  void ShutDown();

  // Returns true if the url's scheme can be checked.
  bool CanCheckUrl(const GURL& url) const;

  // Called on UI thread to decide if safe browsing related stats
  // could be reported.
  virtual bool CanReportStats() const;

  // Called on UI thread to decide if the download file's sha256 hash
  // should be calculated for safebrowsing.
  bool DownloadBinHashNeeded() const;

  // Called on the IO thread to check if the given url is safe or not.  If we
  // can synchronously determine that the url is safe, CheckUrl returns true.
  // Otherwise it returns false, and "client" is called asynchronously with the
  // result when it is ready.
  virtual bool CheckBrowseUrl(const GURL& url, Client* client);

  // Check if the prefix for |url| is in safebrowsing download add lists.
  // Result will be passed to callback in |client|.
  virtual bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                                Client* client);

  // Check if the prefix for |full_hash| is in safebrowsing binhash add lists.
  // Result will be passed to callback in |client|.
  virtual bool CheckDownloadHash(const std::string& full_hash, Client* client);

  // Check if the |url| matches any of the full-length hashes from the
  // client-side phishing detection whitelist.  Returns true if there was a
  // match and false otherwise.  To make sure we are conservative we will return
  // true if an error occurs. This method is expected to be called on the IO
  // thread.
  virtual bool MatchCsdWhitelistUrl(const GURL& url);

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

  // Called on the IO thread to cancel a pending check if the result is no
  // longer needed.
  void CancelCheck(Client* client);

  // Called on the IO thread to display an interstitial page.
  // |url| is the url of the resource that matches a safe browsing list.
  // If the request contained a chain of redirects, |url| is the last url
  // in the chain, and |original_url| is the first one (the root of the
  // chain). Otherwise, |original_url| = |url|.
  void DisplayBlockingPage(const GURL& url,
                           const GURL& original_url,
                           const std::vector<GURL>& redirect_urls,
                           bool is_subresource,
                           UrlCheckResult result,
                           const UrlCheckCallback& callback,
                           int render_process_host_id,
                           int render_view_id);

  // Same as above but gets invoked on the UI thread.
  virtual void DoDisplayBlockingPage(const UnsafeResource& resource);

  // Returns true if we already displayed an interstitial for that resource.
  // Called on the UI thread.
  bool IsWhitelisted(const UnsafeResource& resource);

  // Called on the IO thread when the SafeBrowsingProtocolManager has received
  // the full hash results for prefix hits detected in the database.
  void HandleGetHashResults(
      SafeBrowsingCheck* check,
      const std::vector<SBFullHashResult>& full_hashes,
      bool can_cache);

  // Called on the IO thread.
  void HandleChunk(const std::string& list, SBChunkList* chunks);
  void HandleChunkDelete(std::vector<SBChunkDelete>* chunk_deletes);

  // Update management.  Called on the IO thread.
  void UpdateStarted();
  void UpdateFinished(bool update_succeeded);
  // Whether there is an update in progress. Called on the IO thread.
  bool IsUpdateInProgress() const;

  // The blocking page on the UI thread has completed.
  void OnBlockingPageDone(const std::vector<UnsafeResource>& resources,
                          bool proceed);

  bool enabled() const { return enabled_; }

  bool download_protection_enabled() const {
    return enabled_ && enable_download_protection_;
  }

  safe_browsing::ClientSideDetectionService*
      safe_browsing_detection_service() const {
    return csd_service_.get();
  }

  // The DownloadProtectionService is not valid after the SafeBrowsingService
  // is destroyed.
  safe_browsing::DownloadProtectionService*
      download_protection_service() const {
    return download_service_.get();
  }

  net::URLRequestContextGetter* url_request_context();

  // Called on the IO thread to reset the database.
  void ResetDatabase();

  // Called on the IO thread to release memory.
  void PurgeMemory();

  // Log the user perceived delay caused by SafeBrowsing. This delay is the time
  // delta starting from when we would have started reading data from the
  // network, and ending when the SafeBrowsing check completes indicating that
  // the current page is 'safe'.
  void LogPauseDelay(base::TimeDelta time);

  // Called on the IO thread by the MalwareDetails with the serialized
  // protocol buffer, so the service can send it over.
  virtual void SendSerializedMalwareDetails(const std::string& serialized);

  // Report hits to the unsafe contents (malware, phishing, unsafe download URL)
  // to the server. Can only be called on UI thread.  If |post_data| is
  // non-empty, the request will be sent as a POST instead of a GET.
  virtual void ReportSafeBrowsingHit(const GURL& malicious_url,
                                     const GURL& page_url,
                                     const GURL& referrer_url,
                                     bool is_subresource,
                                     UrlCheckResult threat_type,
                                     const std::string& post_data);

  // Add and remove observers.  These methods must be invoked on the UI thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* remove);

 protected:
  // Creates the safe browsing service.  Need to initialize before using.
  SafeBrowsingService();

  virtual ~SafeBrowsingService();

 private:
  friend class SafeBrowsingServiceFactoryImpl;

  typedef std::set<SafeBrowsingCheck*> CurrentChecks;
  typedef std::vector<SafeBrowsingCheck*> GetHashRequestors;
  typedef base::hash_map<SBPrefix, GetHashRequestors> GetHashRequests;

  // Used for whitelisting a render view when the user ignores our warning.
  struct WhiteListedEntry;

  // Clients that we've queued up for checking later once the database is ready.
  struct QueuedCheck {
    Client* client;
    GURL url;
    base::TimeTicks start;  // When check was queued.
  };

  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<SafeBrowsingService>;
  friend class SafeBrowsingServiceTest;
  friend class SafeBrowsingURLRequestContextGetter;

  void InitURLRequestContextOnIOThread(
      net::URLRequestContextGetter* system_url_request_context_getter);

  void DestroyURLRequestContextOnIOThread();

  // Called to initialize objects that are used on the io_thread.  This may be
  // called multiple times during the life of the SafeBrowsingService.
  void StartOnIOThread();

  // Called to shutdown operations on the io_thread. This may be called multiple
  // times during the life of the SafeBrowsingService.
  void StopOnIOThread();

  // Returns whether |database_| exists and is accessible.
  bool DatabaseAvailable() const;

  // Called on the IO thread.  If the database does not exist, queues up a call
  // on the db thread to create it.  Returns whether the database is available.
  //
  // Note that this is only needed outside the db thread, since functions on the
  // db thread can call GetDatabase() directly.
  bool MakeDatabaseAvailable();

  // Called on the IO thread to try to close the database, freeing the memory
  // associated with it.  The database will be automatically reopened as needed.
  //
  // NOTE: Actual database closure is asynchronous, and until it happens, the IO
  // thread is not allowed to access it; may not actually trigger a close if one
  // is already pending or doing so would cause problems.
  void CloseDatabase();

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

  // Called on the IO thread after the database reports that it added a chunk.
  void OnChunkInserted();

  // Notification that the database is done loading its bloom filter.  We may
  // have had to queue checks until the database is ready, and if so, this
  // checks them.
  void DatabaseLoadComplete();

  // Called on the database thread to add/remove chunks and host keys.
  // Callee will free the data when it's done.
  void HandleChunkForDatabase(const std::string& list,
                              SBChunkList* chunks);

  void DeleteChunks(std::vector<SBChunkDelete>* chunk_deletes);

  static UrlCheckResult GetResultFromListname(const std::string& list_name);

  void NotifyClientBlockingComplete(Client* client, bool proceed);

  void DatabaseUpdateFinished(bool update_succeeded);

  // Start up SafeBrowsing objects. This can be called at browser start, or when
  // the user checks the "Enable SafeBrowsing" option in the Advanced options
  // UI.
  void Start();

  // Stops the SafeBrowsingService. This can be called when the safe browsing
  // preference is disabled.
  void Stop();

  // Called on the db thread to close the database.  See CloseDatabase().
  void OnCloseDatabase();

  // Runs on the db thread to reset the database. We assume that resetting the
  // database is a synchronous operation.
  void OnResetDatabase();

  // Store in-memory the GetHash response. Runs on the database thread.
  void CacheHashResults(const std::vector<SBPrefix>& prefixes,
                        const std::vector<SBFullHashResult>& full_hashes);

  // Internal worker function for processing full hashes.
  void OnHandleGetHashResults(SafeBrowsingCheck* check,
                              const std::vector<SBFullHashResult>& full_hashes);

  // Run one check against |full_hashes|.  Returns |true| if the check
  // finds a match in |full_hashes|.
  bool HandleOneCheck(SafeBrowsingCheck* check,
                      const std::vector<SBFullHashResult>& full_hashes);

  // Call protocol manager on IO thread to report hits of unsafe contents.
  void ReportSafeBrowsingHitOnIOThread(const GURL& malicious_url,
                                       const GURL& page_url,
                                       const GURL& referrer_url,
                                       bool is_subresource,
                                       UrlCheckResult threat_type,
                                       const std::string& post_data);

  // Checks the download hash on safe_browsing_thread_.
  void CheckDownloadHashOnSBThread(SafeBrowsingCheck* check);

  // Invoked by CheckDownloadUrl. It checks the download URL on
  // safe_browsing_thread_.
  void CheckDownloadUrlOnSBThread(SafeBrowsingCheck* check);

  // The callback function when a safebrowsing check is timed out. Client will
  // be notified that the safebrowsing check is SAFE when this happens.
  void TimeoutCallback(SafeBrowsingCheck* check);

  // Calls the Client's callback on IO thread after CheckDownloadUrl finishes.
  void CheckDownloadUrlDone(SafeBrowsingCheck* check);

  // Calls the Client's callback on IO thread after CheckDownloadHash finishes.
  void CheckDownloadHashDone(SafeBrowsingCheck* check);

  // Helper function that calls safe browsing client and cleans up |checks_|.
  void SafeBrowsingCheckDone(SafeBrowsingCheck* check);

  // Helper function to set |check| with default values and start a safe
  // browsing check with timeout of |timeout_ms|. |task| will be called upon
  // success, otherwise TimeoutCallback will be called.
  void StartDownloadCheck(SafeBrowsingCheck* check,
                          Client* client,
                          const base::Closure& task,
                          int64 timeout_ms);

  // Adds the given entry to the whitelist.  Called on the UI thread.
  void UpdateWhitelist(const UnsafeResource& resource);

  // content::NotificationObserver override
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Starts following the safe browsing preference on |pref_service|.
  void AddPrefService(PrefService* pref_service);

  // Stop following the safe browsing preference on |pref_service|.
  void RemovePrefService(PrefService* pref_service);

  // Checks if any profile is currently using the safe browsing service, and
  // starts or stops the service accordingly.
  void RefreshState();

  // The factory used to instanciate a SafeBrowsingService object.
  // Useful for tests, so they can provide their own implementation of
  // SafeBrowsingService.
  static SafeBrowsingServiceFactory* factory_;

  CurrentChecks checks_;

  // Used for issuing only one GetHash request for a given prefix.
  GetHashRequests gethash_requests_;

  // The persistent database.  We don't use a scoped_ptr because it
  // needs to be destructed on a different thread than this object.
  SafeBrowsingDatabase* database_;

  // Lock used to prevent possible data races due to compiler optimizations.
  mutable base::Lock database_lock_;

  // The SafeBrowsingURLRequestContextGetter used to access
  // |url_request_context_|.
  scoped_refptr<net::URLRequestContextGetter>
      url_request_context_getter_;

  // The SafeBrowsingURLRequestContext.
  scoped_ptr<net::URLRequestContext> url_request_context_;

  // Handles interaction with SafeBrowsing servers.
  SafeBrowsingProtocolManager* protocol_manager_;

  // Only access this whitelist from the UI thread.
  std::vector<WhiteListedEntry> white_listed_entries_;

  // Whether the service is running. 'enabled_' is used by SafeBrowsingService
  // on the IO thread during normal operations.
  bool enabled_;

  // Indicate if download_protection is enabled by command switch
  // so we allow this feature to be exersized.
  bool enable_download_protection_;

  // Indicate if client-side phishing detection whitelist should be enabled
  // or not.
  bool enable_csd_whitelist_;

  // Indicate if the download whitelist should be enabled or not.
  bool enable_download_whitelist_;

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

  // When download url check takes this long, client's callback will be called
  // without waiting for the result.
  int64 download_urlcheck_timeout_ms_;

  // Similar to |download_urlcheck_timeout_ms_|, but for download hash checks.
  int64 download_hashcheck_timeout_ms_;

  ObserverList<Observer> observer_list_;

  // Used to track purge memory notifications. Lives on the IO thread.
  scoped_ptr<content::NotificationRegistrar> registrar_;

  // Tracks existing PrefServices, and the safe browsing preference on each.
  // This is used to determine if any profile is currently using the safe
  // browsing service, and to start it up or shut it down accordingly.
  std::map<PrefService*, PrefChangeRegistrar*> prefs_map_;

  // Used to track creation and destruction of profiles on the UI thread.
  content::NotificationRegistrar prefs_registrar_;

  // The ClientSideDetectionService is managed by the SafeBrowsingService,
  // since its running state and lifecycle depends on SafeBrowsingService's.
  scoped_ptr<safe_browsing::ClientSideDetectionService> csd_service_;

  // The DownloadProtectionService is managed by the SafeBrowsingService,
  // since its running state and lifecycle depends on SafeBrowsingService's.
  scoped_ptr<safe_browsing::DownloadProtectionService> download_service_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingService);
};

// Factory for creating SafeBrowsingService.  Useful for tests.
class SafeBrowsingServiceFactory {
 public:
  SafeBrowsingServiceFactory() { }
  virtual ~SafeBrowsingServiceFactory() { }
  virtual SafeBrowsingService* CreateSafeBrowsingService() = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceFactory);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
