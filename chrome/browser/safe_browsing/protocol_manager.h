// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_PROTOCOL_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_PROTOCOL_MANAGER_H_
#pragma once

// A class that implements Chrome's interface with the SafeBrowsing protocol.
// The SafeBrowsingProtocolManager handles formatting and making requests of,
// and handling responses from, Google's SafeBrowsing servers. This class uses
// The SafeBrowsingProtocolParser class to do the actual parsing.

#include <deque>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/safe_browsing/chunk_range.h"
#include "chrome/browser/safe_browsing/protocol_parser.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/common/net/url_fetcher.h"

class URLRequestStatus;

#if defined(COMPILER_GCC)
// Allows us to use URLFetchers in a hash_map with gcc (MSVC is okay without
// specifying this).
namespace __gnu_cxx {
template<>
struct hash<const URLFetcher*> {
  size_t operator()(const URLFetcher* fetcher) const {
    return reinterpret_cast<size_t>(fetcher);
  }
};
}
#endif

class SafeBrowsingProtocolManager : public URLFetcher::Delegate {
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingProtocolManagerTest, TestBackOffTimes);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingProtocolManagerTest, TestChunkStrings);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingProtocolManagerTest, TestGetHashUrl);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingProtocolManagerTest,
                           TestGetHashBackOffTimes);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingProtocolManagerTest, TestMacKeyUrl);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingProtocolManagerTest,
                           TestMalwareReportUrl);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingProtocolManagerTest, TestNextChunkUrl);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingProtocolManagerTest, TestUpdateUrl);
  friend class SafeBrowsingServiceTest;

 public:
  // Constructs a SafeBrowsingProtocolManager for |sb_service| that issues
  // network requests using |request_context_getter|. When |disable_auto_update|
  // is true, protocol manager won't schedule next update until
  // ForceScheduleNextUpdate is called.
  SafeBrowsingProtocolManager(SafeBrowsingService* sb_service,
                              const std::string& client_name,
                              const std::string& client_key,
                              const std::string& wrapped_key,
                              URLRequestContextGetter* request_context_getter,
                              const std::string& info_url_prefix,
                              const std::string& mackey_url_prefix,
                              bool disable_auto_update);
  virtual ~SafeBrowsingProtocolManager();

  // Sets up the update schedule and internal state for making periodic requests
  // of the SafeBrowsing service.
  void Initialize();

  // URLFetcher::Delegate interface.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // API used by the SafeBrowsingService for issuing queries. When the results
  // are available, SafeBrowsingService::HandleGetHashResults is called.
  void GetFullHash(SafeBrowsingService::SafeBrowsingCheck* check,
                   const std::vector<SBPrefix>& prefixes);

  // Forces the start of next update after |next_update_msec| in msec.
  void ForceScheduleNextUpdate(int next_update_msec);

  bool is_initial_request() const { return initial_request_; }

  // Scheduled update callback.
  void GetNextUpdate();

  // Called by the SafeBrowsingService when our request for a list of all chunks
  // for each list is done.  If database_error is true, that means the protocol
  // manager shouldn't fetch updates since they can't be written to disk.  It
  // should try again later to open the database.
  void OnGetChunksComplete(const std::vector<SBListChunkRanges>& list,
                           bool database_error);

  // Called after the chunks that were parsed were inserted in the database.
  void OnChunkInserted();

  // The last time we received an update.
  base::Time last_update() const { return last_update_; }

  // Reports a malware resource to the SafeBrowsing service.
  void ReportMalware(const GURL& malware_url,
                     const GURL& page_url,
                     const GURL& referrer_url,
                     bool is_subresource);

  // Setter for additional_query_. To make sure the additional_query_ won't
  // be changed in the middle of an update, caller (e.g.: SafeBrowsingService)
  // should call this after callbacks triggered in UpdateFinished() or before
  // IssueUpdateRequest().
  void set_additional_query(const std::string& query) {
    additional_query_ = query;
  }
  const std::string& additional_query() const {
    return additional_query_;
  }

 private:
  // Internal API for fetching information from the SafeBrowsing servers. The
  // GetHash requests are higher priority since they can block user requests
  // so are handled separately.
  enum SafeBrowsingRequestType {
    NO_REQUEST = 0,     // No requests in progress
    UPDATE_REQUEST,     // Request for redirect URLs
    CHUNK_REQUEST,      // Request for a specific chunk
    GETKEY_REQUEST      // Update the client's MAC key
  };

  // Composes a URL using |prefix|, |method| (e.g.: gethash, download,
  // newkey, report), |client_name| and |version|. When not empty,
  // |additional_query| is appended to the URL with an additional "&"
  // in the front.
  static std::string ComposeUrl(const std::string& prefix,
                                const std::string& method,
                                const std::string& client_name,
                                const std::string& version,
                                const std::string& additional_query);

  // Generates Update URL for querying about the latest set of chunk updates.
  // Append "wrkey=xxx" to the URL when |use_mac| is true.
  GURL UpdateUrl(bool use_mac) const;
  // Generates GetHash request URL for retrieving full hashes.
  // Append "wrkey=xxx" to the URL when |use_mac| is true.
  GURL GetHashUrl(bool use_mac) const;
  // Generates new MAC client key request URL.
  GURL MacKeyUrl() const;
  // Generates URL for reporting malware pages.
  GURL MalwareReportUrl(const GURL& malware_url, const GURL& page_url,
                        const GURL& referrer_url, bool is_subresource) const;
  // Composes a ChunkUrl based on input string.
  GURL NextChunkUrl(const std::string& input) const;

  // Returns the time (in milliseconds) for the next update request. If
  // 'back_off' is true, the time returned will increment an error count and
  // return the appriate next time (see ScheduleNextUpdate below).
  int GetNextUpdateTime(bool back_off);

  // Worker function for calculating GetHash and Update backoff times (in
  // seconds). 'Multiplier' is doubled for each consecutive error between the
  // 2nd and 5th, and 'error_count' is incremented with each call.
  int GetNextBackOffTime(int* error_count, int* multiplier);

  // Manages our update with the next allowable update time. If 'back_off_' is
  // true, we must decrease the frequency of requests of the SafeBrowsing
  // service according to section 5 of the protocol specification.
  // When disable_auto_update_ is set, ScheduleNextUpdate will do nothing.
  // ForceScheduleNextUpdate has to be called to trigger the update.
  void ScheduleNextUpdate(bool back_off);

  // Sends a request for a list of chunks we should download to the SafeBrowsing
  // servers. In order to format this request, we need to send all the chunk
  // numbers for each list that we have to the server. Getting the chunk numbers
  // requires a database query (run on the database thread), and the request
  // is sent upon completion of that query in OnGetChunksComplete.
  void IssueUpdateRequest();

  // Sends a request for a chunk to the SafeBrowsing servers.
  void IssueChunkRequest();

  // Gets a key from the SafeBrowsing servers for use with MAC. This should only
  // be called once per client unless the server directly tells us to update.
  void IssueKeyRequest();

  // Formats a string returned from the database into:
  //   "list_name;a:<add_chunk_ranges>:s:<sub_chunk_ranges>:mac\n"
  static std::string FormatList(const SBListChunkRanges& list, bool use_mac);

  // Runs the protocol parser on received data and update the
  // SafeBrowsingService with the new content. Returns 'true' on successful
  // parse, 'false' on error.
  bool HandleServiceResponse(const GURL& url, const char* data, int length);

  // If the SafeBrowsing service wants us to re-key, we clear our key state and
  // issue the request.
  void HandleReKey();

  // Updates internal state for each GetHash response error, assuming that the
  // current time is |now|.
  void HandleGetHashError(const base::Time& now);

  // Helper function for update completion.
  void UpdateFinished(bool success);

  // A callback that runs if we timeout waiting for a response to an update
  // request. We use this to properly set our update state.
  void UpdateResponseTimeout();

 private:
  // Main SafeBrowsing interface object.
  SafeBrowsingService* sb_service_;

  // Current active request (in case we need to cancel) for updates or chunks
  // from the SafeBrowsing service. We can only have one of these outstanding
  // at any given time unlike GetHash requests, which are tracked separately.
  scoped_ptr<URLFetcher> request_;

  // The kind of request that is currently in progress.
  SafeBrowsingRequestType request_type_;

  // The number of HTTP response errors, used for request backoff timing.
  int update_error_count_;
  int gethash_error_count_;

  // Multipliers which double (max == 8) for each error after the second.
  int update_back_off_mult_;
  int gethash_back_off_mult_;

  // Multiplier between 0 and 1 to spread clients over an interval.
  float back_off_fuzz_;

  // The list for which we are make a request.
  std::string list_name_;

  // For managing the next earliest time to query the SafeBrowsing servers for
  // updates.
  int next_update_sec_;
  base::OneShotTimer<SafeBrowsingProtocolManager> update_timer_;

  // All chunk requests that need to be made, along with their MAC.
  std::deque<ChunkUrl> chunk_request_urls_;

  // Map of GetHash requests.
  typedef base::hash_map<const URLFetcher*,
                         SafeBrowsingService::SafeBrowsingCheck*> HashRequests;
  HashRequests hash_requests_;

  // The next scheduled update has special behavior for the first 2 requests.
  enum UpdateRequestState {
    FIRST_REQUEST = 0,
    SECOND_REQUEST,
    NORMAL_REQUEST
  };
  UpdateRequestState update_state_;

  // We'll attempt to get keys once per browser session if we don't already have
  // them. They are not essential to operation, but provide a layer of
  // verification.
  bool initial_request_;

  // True if the service has been given an add/sub chunk but it hasn't been
  // added to the database yet.
  bool chunk_pending_to_write_;

  // The keys used for MAC. Empty keys mean we aren't using MAC.
  std::string client_key_;
  std::string wrapped_key_;

  // The last time we successfully received an update.
  base::Time last_update_;

  // While in GetHash backoff, we can't make another GetHash until this time.
  base::Time next_gethash_time_;

  // Current product version sent in each request.
  std::string version_;

  // Used for measuring chunk request latency.
  base::Time chunk_request_start_;

  // Tracks the size of each update (in bytes).
  int update_size_;

  // Track outstanding malware report fetchers for clean up.
  std::set<const URLFetcher*> malware_reports_;

  // The safe browsing client name sent in each request.
  std::string client_name_;

  // A string that is appended to the end of URLs for download, gethash,
  // newkey, malware report and chunk update requests.
  std::string additional_query_;

  // The context we use to issue network requests.
  scoped_refptr<URLRequestContextGetter> request_context_getter_;

  // URL prefix where browser fetches safebrowsing chunk updates, hashes, and
  // reports malware.
  std::string info_url_prefix_;

  // URL prefix where browser fetches MAC client key.
  std::string mackey_url_prefix_;

  // When true, protocol manager will not start an update unless
  // ForceScheduleNextUpdate() is called. This is set for testing purpose.
  bool disable_auto_update_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingProtocolManager);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_PROTOCOL_MANAGER_H_
