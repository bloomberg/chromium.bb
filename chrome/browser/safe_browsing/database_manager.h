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

#include "base/memory/ref_counted.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "content/public/common/resource_type.h"
#include "url/gurl.h"

// Interface to either the locally-managed or a remotely-managed database.
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
                                        const std::string& metadata) {}

    // Called when the result of checking a download URL is known.
    virtual void OnCheckDownloadUrlResult(const std::vector<GURL>& url_chain,
                                          SBThreatType threat_type) {}

    // Called when the result of checking a set of extensions is known.
    virtual void OnCheckExtensionsResult(
        const std::set<std::string>& threats) {}
  };


  // Returns true if URL-checking is supported on this build+device.
  // If false, calls to CheckBrowseUrl may dcheck-fail.
  virtual bool IsSupported() const = 0;

  // Returns true if checks are never done synchronously, and therefore
  // always have some latency.
  virtual bool ChecksAreAlwaysAsync() const = 0;

  // Returns true if this resource type should be checked.
  virtual bool CanCheckResourceType(
      content::ResourceType resource_type) const = 0;

  // Returns true if the url's scheme can be checked.
  virtual bool CanCheckUrl(const GURL& url) const = 0;

  // Returns whether download protection is enabled.
  virtual bool download_protection_enabled() const = 0;

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

  // Check if the CSD malware IP matching kill switch is turned on.
  virtual bool IsMalwareKillSwitchOn() = 0;

  // Check if the CSD whitelist kill switch is turned on.
  virtual bool IsCsdWhitelistKillSwitchOn() = 0;

  // Called on the IO thread to cancel a pending check if the result is no
  // longer needed.  Also called after the result has been handled.
  virtual void CancelCheck(Client* client) = 0;

  // Called to initialize objects that are used on the io_thread.  This may be
  // called multiple times during the life of the DatabaseManager. Must be
  // called on IO thread.
  virtual void StartOnIOThread() = 0;

  // Called to stop or shutdown operations on the io_thread. This may be called
  // multiple times during the life of the DatabaseManager. Must be called
  // on IO thread. If shutdown is true, the manager is disabled permanently.
  virtual void StopOnIOThread(bool shutdown) = 0;

 protected:
  virtual ~SafeBrowsingDatabaseManager() {}

  friend class base::RefCountedThreadSafe<SafeBrowsingDatabaseManager>;
};  // class SafeBrowsingDatabaseManager

#endif  // CHROME_BROWSER_SAFE_BROWSING_DATABASE_MANAGER_H_
