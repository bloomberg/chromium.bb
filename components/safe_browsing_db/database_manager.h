// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Safe Browsing service is responsible for downloading anti-phishing and
// anti-malware tables and checking urls against them.

#ifndef COMPONENTS_SAFE_BROWSING_DB_DATABASE_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_DB_DATABASE_MANAGER_H_

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/safe_browsing_db/hit_report.h"
#include "components/safe_browsing_db/util.h"
#include "content/public/common/resource_type.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace safe_browsing {

struct V4ProtocolConfig;
class V4GetHashProtocolManager;

// Base class to either the locally-managed or a remotely-managed database.
class SafeBrowsingDatabaseManager
    : public base::RefCountedThreadSafe<SafeBrowsingDatabaseManager> {
 public:
  // Callers requesting a result should derive from this class.
  // The destructor should call db_manager->CancelCheck(client) if a
  // request is still pending.
  class Client {
   public:
    virtual ~Client() {}

    // Called when the result of checking a browse URL is known.
    virtual void OnCheckBrowseUrlResult(const GURL& url,
                                        SBThreatType threat_type,
                                        const ThreatMetadata& metadata) {}

    // Called when the result of checking a download URL is known.
    virtual void OnCheckDownloadUrlResult(const std::vector<GURL>& url_chain,
                                          SBThreatType threat_type) {}

    // Called when the result of checking a set of extensions is known.
    virtual void OnCheckExtensionsResult(
        const std::set<std::string>& threats) {}

    // Called when the result of checking the API blacklist is known.
    // TODO(kcarattini): Consider if we need |url| passed here, remove if not.
    virtual void OnCheckApiBlacklistUrlResult(const GURL& url,
                                              const ThreatMetadata& metadata) {}

    // Called when the result of checking the resource blacklist is known.
    virtual void OnCheckResourceUrlResult(const GURL& url,
                                          SBThreatType threat_type,
                                          const std::string& threat_hash) {}
  };

  // Returns true if URL-checking is supported on this build+device.
  // If false, calls to CheckBrowseUrl may dcheck-fail.
  virtual bool IsSupported() const = 0;

  // Returns the ThreatSource for this implementation.
  virtual ThreatSource GetThreatSource() const = 0;

  // Returns true if checks are never done synchronously, and therefore
  // always have some latency.
  virtual bool ChecksAreAlwaysAsync() const = 0;

  // Returns true if this resource type should be checked.
  virtual bool CanCheckResourceType(
      content::ResourceType resource_type) const = 0;

  // Returns true if the url's scheme can be checked.
  virtual bool CanCheckUrl(const GURL& url) const = 0;

  // Returns whether download protection is enabled.
  virtual bool IsDownloadProtectionEnabled() const = 0;

  // Called on the IO thread to check if the given url is safe or not.  If we
  // can synchronously determine that the url is safe, CheckUrl returns true.
  // Otherwise it returns false, and "client" is called asynchronously with the
  // result when it is ready.
  virtual bool CheckBrowseUrl(const GURL& url, Client* client) = 0;

  // Check if the prefix for |url| is in safebrowsing download add lists.
  // Result will be passed to callback in |client|.
  virtual bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                                Client* client) = 0;

  // Check which prefixes in |extension_ids| are in the safebrowsing blacklist.
  // Returns true if not, false if further checks need to be made in which case
  // the result will be passed to |client|.
  virtual bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                                 Client* client) = 0;

  // Check if |url| is in the resources blacklist. Returns true if not, false
  // if further checks need to be made in which case the result will be passed
  // to callback in |client|.
  virtual bool CheckResourceUrl(const GURL& url, Client* client) = 0;

  // Check if the |url| matches any of the full-length hashes from the client-
  // side phishing detection whitelist.  Returns true if there was a match and
  // false otherwise.  To make sure we are conservative we will return true if
  // an error occurs.  This method must be called on the IO thread.
  virtual bool MatchCsdWhitelistUrl(const GURL& url) = 0;

  // Check if the given IP address (either IPv4 or IPv6) matches the malware
  // IP blacklist.
  virtual bool MatchMalwareIP(const std::string& ip_address) = 0;

  // Check if the |url| matches any of the full-length hashes from the download
  // whitelist.  Returns true if there was a match and false otherwise. To make
  // sure we are conservative we will return true if an error occurs.  This
  // method must be called on the IO thread.
  virtual bool MatchDownloadWhitelistUrl(const GURL& url) = 0;

  // Check if |str| matches any of the full-length hashes from the download
  // whitelist.  Returns true if there was a match and false otherwise. To make
  // sure we are conservative we will return true if an error occurs.  This
  // method must be called on the IO thread.
  virtual bool MatchDownloadWhitelistString(const std::string& str) = 0;

  // Check if the |url| matches any of the full-length hashes from the off-
  // domain inclusion whitelist. Returns true if there was a match and false
  // otherwise. To make sure we are conservative, we will return true if an
  // error occurs.  This method must be called on the IO thread.
  virtual bool MatchInclusionWhitelistUrl(const GURL& url) = 0;

  // Check if |str|, a lowercase DLL file name, matches any of the full-length
  // hashes from the module whitelist.  Returns true if there was a match and
  // false otherwise.  To make sure we are conservative we will return true if
  // an error occurs.  This method must be called on the IO thread.
  virtual bool MatchModuleWhitelistString(const std::string& str) = 0;

  // Check if the CSD malware IP matching kill switch is turned on.
  virtual bool IsMalwareKillSwitchOn() = 0;

  // Check if the CSD whitelist kill switch is turned on.
  virtual bool IsCsdWhitelistKillSwitchOn() = 0;

  // Called on the IO thread to cancel a pending check if the result is no
  // longer needed.  Also called after the result has been handled. Api checks
  // are handled separately. To cancel an API check use CancelApiCheck.
  virtual void CancelCheck(Client* client) = 0;

  // Called on the IO thread to cancel a pending API check if the result is no
  // longer needed. Returns true if the client was found and the check
  // successfully cancelled.
  virtual bool CancelApiCheck(Client* client);

  // Called on the IO thread to check if the given url has blacklisted APIs.
  // "client" is called asynchronously with the result when it is ready. Callers
  // should wait for results before calling this method a second time with the
  // same client. This method has the same implementation for both the local and
  // remote database managers since it pings Safe Browsing servers directly
  // without accessing the database at all.  Returns true if we can
  // synchronously determine that the url is safe. Otherwise it returns false,
  // and "client" is called asynchronously with the result when it is ready.
  virtual bool CheckApiBlacklistUrl(const GURL& url, Client* client);

  // Called to initialize objects that are used on the io_thread, such as the
  // v4 protocol manager.  This may be called multiple times during the life of
  // the DatabaseManager. Must be called on IO thread.
  virtual void StartOnIOThread(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config);

  // Called to stop or shutdown operations on the io_thread.
  virtual void StopOnIOThread(bool shutdown);

 protected:
  // Bundled client info for an API abuse hash prefix check.
  class SafeBrowsingApiCheck {
   public:
    SafeBrowsingApiCheck(const GURL& url,
                         const std::vector<SBFullHash>& full_hashes,
                         Client* client);
    ~SafeBrowsingApiCheck();

    const GURL& url() {return url_;}
    std::vector<SBFullHash>& full_hashes() {return full_hashes_;}
    SafeBrowsingDatabaseManager::Client* client() {return client_;}

   private:
    GURL url_;
    std::vector<SBFullHash> full_hashes_;
    // Not owned.
    SafeBrowsingDatabaseManager::Client* client_;

    DISALLOW_COPY_AND_ASSIGN(SafeBrowsingApiCheck);
  };

  SafeBrowsingDatabaseManager();

  virtual ~SafeBrowsingDatabaseManager();

  friend class base::RefCountedThreadSafe<SafeBrowsingDatabaseManager>;

  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseManagerTest,
                           CheckApiBlacklistUrlPrefixes);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseManagerTest,
                           HandleGetHashesWithApisResults);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseManagerTest,
                           HandleGetHashesWithApisResultsNoMatch);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseManagerTest,
                           HandleGetHashesWithApisResultsMatches);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseManagerTest,
                           CancelApiCheck);

  typedef std::set<SafeBrowsingApiCheck*> CurrentApiChecks;

  // In-progress checks. This set owns the SafeBrowsingApiCheck pointers and is
  // responsible for deleting them when removing from the set.
  CurrentApiChecks api_checks_;

  // Called on the IO thread wheh the SafeBrowsingProtocolManager has received
  // the full hash and api results for prefixes of the |url| argument in
  // CheckApiBlacklistUrl.
  virtual void HandleGetHashesWithApisResults(
      SafeBrowsingApiCheck* check,
      const std::vector<SBFullHashResult>& full_hash_results,
      const base::TimeDelta& negative_cache_duration);

  // Created and destroyed via StartOnIOThread/StopOnIOThread.
  V4GetHashProtocolManager* v4_get_hash_protocol_manager_;

 private:
  // Returns an iterator to the pending API check with the given |client|.
  CurrentApiChecks::iterator FindClientApiCheck(Client* client);
};  // class SafeBrowsingDatabaseManager

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_DATABASE_MANAGER_H_
