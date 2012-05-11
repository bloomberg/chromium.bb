// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/safe_browsing/chunk_range.h"
#include "chrome/browser/safe_browsing/protocol_parser.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "content/public/common/url_fetcher_delegate.h"

#if defined(COMPILER_GCC)
// Allows us to use URLFetchers in a hash_map with gcc (MSVC is okay without
// specifying this).
namespace __gnu_cxx {
template<>
struct hash<const net::URLFetcher*> {
  size_t operator()(const net::URLFetcher* fetcher) const {
    return reinterpret_cast<size_t>(fetcher);
  }
};
}
#endif

class SafeBrowsingProtocolManager;
// Interface of a factory to create ProtocolManager.  Useful for tests.
class SBProtocolManagerFactory {
 public:
  SBProtocolManagerFactory() {}
  virtual ~SBProtocolManagerFactory() {}
  virtual SafeBrowsingProtocolManager* CreateProtocolManager(
      SafeBrowsingService* sb_service,
      const std::string& client_name,
      net::URLRequestContextGetter* request_context_getter,
      const std::string& url_prefix,
      bool disable_auto_update) = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(SBProtocolManagerFactory);
};

class SafeBrowsingProtocolManager : public content::URLFetcherDelegate {
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingProtocolManagerTest, TestBackOffTimes);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingProtocolManagerTest, TestChunkStrings);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingProtocolManagerTest, TestGetHashUrl);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingProtocolManagerTest,
                           TestGetHashBackOffTimes);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingProtocolManagerTest,
                           TestSafeBrowsingHitUrl);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingProtocolManagerTest,
                           TestMalwareDetailsUrl);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingProtocolManagerTest, TestNextChunkUrl);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingProtocolManagerTest, TestUpdateUrl);
  friend class SafeBrowsingServiceTest;

 public:
  virtual ~SafeBrowsingProtocolManager();

  // Makes the passed |factory| the factory used to instantiate
  // a SafeBrowsingService. Useful for tests.
  static void RegisterFactory(SBProtocolManagerFactory* factory) {
    factory_ = factory;
  }

  // Create an instance of the safe browsing service.
  static SafeBrowsingProtocolManager* Create(
      SafeBrowsingService* sb_service,
      const std::string& client_name,
      net::URLRequestContextGetter* request_context_getter,
      const std::string& url_prefix,
      bool disable_auto_update);

  // Sets up the update schedule and internal state for making periodic requests
  // of the SafeBrowsing service.
  virtual void Initialize();

  // content::URLFetcherDelegate interface.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // API used by the SafeBrowsingService for issuing queries. When the results
  // are available, SafeBrowsingService::HandleGetHashResults is called.
  virtual void GetFullHash(SafeBrowsingService::SafeBrowsingCheck* check,
                           const std::vector<SBPrefix>& prefixes);

  // Forces the start of next update after |next_update_msec| in msec.
  void ForceScheduleNextUpdate(int next_update_msec);

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

  // For UMA users we report to Google when a SafeBrowsing interstitial is shown
  // to the user.  |threat_type| should be one of the types known by
  // SafeBrowsingHitUrl.
  void ReportSafeBrowsingHit(const GURL& malicious_url,
                             const GURL& page_url,
                             const GURL& referrer_url,
                             bool is_subresource,
                             SafeBrowsingService::UrlCheckResult threat_type,
                             const std::string& post_data);

  // Users can opt-in on the SafeBrowsing interstitial to send detailed
  // malware reports. |report| is the serialized report.
  void ReportMalwareDetails(const std::string& report);

  // The last time we received an update.
  base::Time last_update() const { return last_update_; }

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

  // Enumerate failures for histogramming purposes.  DO NOT CHANGE THE
  // ORDERING OF THESE VALUES.
  enum ResultType {
    // 200 response code means that the server recognized the hash
    // prefix, while 204 is an empty response indicating that the
    // server did not recognize it.
    GET_HASH_STATUS_200,
    GET_HASH_STATUS_204,

    // Subset of successful responses which returned no full hashes.
    // This includes the 204 case, and also 200 responses for stale
    // prefixes (deleted at the server but yet deleted on the client).
    GET_HASH_FULL_HASH_EMPTY,

    // Subset of successful responses for which one or more of the
    // full hashes matched (should lead to an interstitial).
    GET_HASH_FULL_HASH_HIT,

    // Subset of successful responses which weren't empty and have no
    // matches.  It means that there was a prefix collision which was
    // cleared up by the full hashes.
    GET_HASH_FULL_HASH_MISS,

    // Memory space for histograms is determined by the max.  ALWAYS
    // ADD NEW VALUES BEFORE THIS ONE.
    GET_HASH_RESULT_MAX
  };

  // Record a GetHash result. |is_download| indicates if the get
  // hash is triggered by download related lookup.
  static void RecordGetHashResult(bool is_download,
                                  ResultType result_type);

 protected:
  // Constructs a SafeBrowsingProtocolManager for |sb_service| that issues
  // network requests using |request_context_getter|. When |disable_auto_update|
  // is true, protocol manager won't schedule next update until
  // ForceScheduleNextUpdate is called.
  SafeBrowsingProtocolManager(
      SafeBrowsingService* sb_service,
      const std::string& client_name,
      net::URLRequestContextGetter* request_context_getter,
      const std::string& url_prefix,
      bool disable_auto_update);

 private:
  friend class SBProtocolManagerFactoryImpl;

  // Internal API for fetching information from the SafeBrowsing servers. The
  // GetHash requests are higher priority since they can block user requests
  // so are handled separately.
  enum SafeBrowsingRequestType {
    NO_REQUEST = 0,     // No requests in progress
    UPDATE_REQUEST,     // Request for redirect URLs
    CHUNK_REQUEST,      // Request for a specific chunk
  };

  // Composes a URL using |prefix|, |method| (e.g.: gethash, download, report).
  // |client_name| and |version|. When not empty, |additional_query| is
  // appended to the URL with an additional "&" in the front.
  static std::string ComposeUrl(const std::string& prefix,
                                const std::string& method,
                                const std::string& client_name,
                                const std::string& version,
                                const std::string& additional_query);

  // Generates Update URL for querying about the latest set of chunk updates.
  GURL UpdateUrl() const;
  // Generates GetHash request URL for retrieving full hashes.
  GURL GetHashUrl() const;
  // Generates URL for reporting safe browsing hits for UMA users.
  GURL SafeBrowsingHitUrl(
      const GURL& malicious_url, const GURL& page_url, const GURL& referrer_url,
      bool is_subresource,
      SafeBrowsingService::UrlCheckResult threat_type) const;
  // Generates URL for reporting malware details for users who opt-in.
  GURL MalwareDetailsUrl() const;

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

  // Formats a string returned from the database into:
  //   "list_name;a:<add_chunk_ranges>:s:<sub_chunk_ranges>\n"
  static std::string FormatList(const SBListChunkRanges& list);

  // Runs the protocol parser on received data and update the
  // SafeBrowsingService with the new content. Returns 'true' on successful
  // parse, 'false' on error.
  bool HandleServiceResponse(const GURL& url, const char* data, int length);

  // Updates internal state for each GetHash response error, assuming that the
  // current time is |now|.
  void HandleGetHashError(const base::Time& now);

  // Helper function for update completion.
  void UpdateFinished(bool success);

  // A callback that runs if we timeout waiting for a response to an update
  // request. We use this to properly set our update state.
  void UpdateResponseTimeout();

 private:
  // The factory that controls the creation of SafeBrowsingProtocolManager.
  // This is used by tests.
  static SBProtocolManagerFactory* factory_;

  // Main SafeBrowsing interface object.
  SafeBrowsingService* sb_service_;

  // Current active request (in case we need to cancel) for updates or chunks
  // from the SafeBrowsing service. We can only have one of these outstanding
  // at any given time unlike GetHash requests, which are tracked separately.
  scoped_ptr<content::URLFetcher> request_;

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

  // All chunk requests that need to be made.
  std::deque<ChunkUrl> chunk_request_urls_;

  // Map of GetHash requests.
  typedef base::hash_map<const net::URLFetcher*,
                         SafeBrowsingService::SafeBrowsingCheck*> HashRequests;
  HashRequests hash_requests_;

  // The next scheduled update has special behavior for the first 2 requests.
  enum UpdateRequestState {
    FIRST_REQUEST = 0,
    SECOND_REQUEST,
    NORMAL_REQUEST
  };
  UpdateRequestState update_state_;

  // True if the service has been given an add/sub chunk but it hasn't been
  // added to the database yet.
  bool chunk_pending_to_write_;

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

  // Track outstanding SafeBrowsing report fetchers for clean up.
  // We add both "hit" and "detail" fetchers in this set.
  std::set<const net::URLFetcher*> safebrowsing_reports_;

  // The safe browsing client name sent in each request.
  std::string client_name_;

  // A string that is appended to the end of URLs for download, gethash,
  // safebrowsing hits and chunk update requests.
  std::string additional_query_;

  // The context we use to issue network requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // URL prefix where browser fetches safebrowsing chunk updates, hashes, and
  // reports hits to the safebrowsing list and sends detaild malware reports
  // for UMA users.
  std::string url_prefix_;

  // When true, protocol manager will not start an update unless
  // ForceScheduleNextUpdate() is called. This is set for testing purpose.
  bool disable_auto_update_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingProtocolManager);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_PROTOCOL_MANAGER_H_
