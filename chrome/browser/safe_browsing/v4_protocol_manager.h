// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_V4_PROTOCOL_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_V4_PROTOCOL_MANAGER_H_

// A class that implements Chrome's interface with the SafeBrowsing V4 protocol.
//
// The V4ProtocolManager handles formatting and making requests of, and handling
// responses from, Google's SafeBrowsing servers.

#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/safe_browsing/protocol_manager_helper.h"
#include "components/safe_browsing_db/safebrowsing.pb.h"
#include "components/safe_browsing_db/util.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

namespace safe_browsing {

class V4ProtocolManagerFactory;

class V4ProtocolManager : public net::URLFetcherDelegate,
                          public base::NonThreadSafe {
 public:
  // FullHashCallback is invoked when GetFullHashes completes.
  // Parameters:
  //   - The vector of full hash results. If empty, indicates that there
  //     were no matches, and that the resource is safe.
  //   - The negative cache duration of the result.
  typedef base::Callback<void(const std::vector<SBFullHashResult>&,
                              const base::TimeDelta&)>
      FullHashCallback;

  ~V4ProtocolManager() override;

  // Makes the passed |factory| the factory used to instantiate
  // a V4ProtocolManager. Useful for tests.
  static void RegisterFactory(V4ProtocolManagerFactory* factory) {
    factory_ = factory;
  }

  // Create an instance of the safe browsing v4 protocol manager.
  static V4ProtocolManager* Create(
      net::URLRequestContextGetter* request_context_getter,
      const SafeBrowsingProtocolConfig& config);

  // net::URLFetcherDelegate interface.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Retrieve the full hash for a set of prefixes, and invoke the callback
  // argument when the results are retrieved. The callback may be invoked
  // synchronously.
  virtual void GetFullHashes(const std::vector<SBPrefix>& prefixes,
                             const std::vector<PlatformType>& platforms,
                             ThreatType threat_type,
                             FullHashCallback callback);

  // Retrieve the full hash and API metadata for a set of prefixes, and invoke
  // the callback argument when the results are retrieved. The callback may be
  // invoked synchronously.
  virtual void GetFullHashesWithApis(const std::vector<SBPrefix>& prefixes,
                                     FullHashCallback callback);

  // Enumerate failures for histogramming purposes.  DO NOT CHANGE THE
  // ORDERING OF THESE VALUES.
  // TODO(kcarattini): Use a custom v4 histogram set.
  enum ResultType {
    // 200 response code means that the server recognized the hash
    // prefix, while 204 is an empty response indicating that the
    // server did not recognize it.
    GET_HASH_STATUS_200,
    GET_HASH_STATUS_204,

    // Subset of successful responses which returned no full hashes.
    // This includes the STATUS_204 case, and the *_ERROR cases.
    GET_HASH_FULL_HASH_EMPTY,

    // Subset of successful responses for which one or more of the
    // full hashes matched (should lead to an interstitial).
    GET_HASH_FULL_HASH_HIT,

    // Subset of successful responses which weren't empty and have no
    // matches.  It means that there was a prefix collision which was
    // cleared up by the full hashes.
    GET_HASH_FULL_HASH_MISS,

    // Subset of successful responses where the response body wasn't parsable.
    GET_HASH_PARSE_ERROR,

    // Gethash request failed (network error).
    GET_HASH_NETWORK_ERROR,

    // Gethash request returned HTTP result code other than 200 or 204.
    GET_HASH_HTTP_ERROR,

    // Gethash attempted during error backoff, no request sent.
    GET_HASH_BACKOFF_ERROR,

    // Gethash attempted before min wait duration elapsed, no request sent.
    GET_HASH_MIN_WAIT_DURATION_ERROR,

    // Memory space for histograms is determined by the max.  ALWAYS
    // ADD NEW VALUES BEFORE THIS ONE.
    GET_HASH_RESULT_MAX
  };

  // Record a GetHash result.
  static void RecordGetHashResult(ResultType result_type);

  // Record HTTP response code when there's no error in fetching an HTTP
  // request, and the error code, when there is.
  // |metric_name| is the name of the UMA metric to record the response code or
  // error code against, |status| represents the status of the HTTP request, and
  // |response code| represents the HTTP response code received from the server.
  static void RecordHttpResponseOrErrorCode(const char* metric_name,
                                            const net::URLRequestStatus& status,
                                            int response_code);

 protected:
  // Constructs a V4ProtocolManager that issues
  // network requests using |request_context_getter|.
  V4ProtocolManager(net::URLRequestContextGetter* request_context_getter,
                    const SafeBrowsingProtocolConfig& config);

 private:
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4ProtocolManagerTest, TestGetHashUrl);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4ProtocolManagerTest,
                           TestGetHashRequest);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4ProtocolManagerTest,
                           TestParseHashResponse);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4ProtocolManagerTest,
                           TestParseHashResponseWrongThreatEntryType);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4ProtocolManagerTest,
                           TestParseHashResponseSocialEngineeringThreatType);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4ProtocolManagerTest,
                           TestParseHashResponseNonPermissionMetadata);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4ProtocolManagerTest,
                           TestParseHashResponseInconsistentThreatTypes);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4ProtocolManagerTest,
                           TestGetHashBackOffTimes);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4ProtocolManagerTest,
                           TestGetHashErrorHandlingOK);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4ProtocolManagerTest,
                           TestGetHashErrorHandlingNetwork);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4ProtocolManagerTest,
                           TestGetHashErrorHandlingResponseCode);
  friend class V4ProtocolManagerFactoryImpl;

  // Generates GetHashWithApis Pver4 request URL for retrieving full hashes.
  // |request_base64| is the serialized FindFullHashesRequest protocol buffer
  // encoded in base 64.
  GURL GetHashUrl(const std::string& request_base64) const;

  // Fills a FindFullHashesRequest protocol buffer for a request.
  // Returns the serialized and base 64 encoded request as a string.
  std::string GetHashRequest(const std::vector<SBPrefix>& prefixes,
                             const std::vector<PlatformType>& platforms,
                             ThreatType threat_type);

  // Parses a FindFullHashesResponse protocol buffer and fills the results in
  // |full_hashes| and |negative_cache_duration|. |data| is a serialized
  // FindFullHashes protocol buffer. |negative_cache_duration| is the duration
  // to cache the response for entities that did not match the threat list.
  // Returns true if parsing is successful, false otherwise.
  bool ParseHashResponse(const std::string& data_base64,
                         std::vector<SBFullHashResult>* full_hashes,
                         base::TimeDelta* negative_cache_duration);

  // Worker function for calculating the GetHash backoff times.
  // |multiplier| is doubled for each consecutive error after the
  // first, and |error_count| is incremented with each call.
  static base::TimeDelta GetNextBackOffInterval(size_t* error_count,
                                                size_t* multiplier);

  // Resets the gethash error counter and multiplier.
  void ResetGetHashErrors();

  // Updates internal state for each GetHash response error, assuming that
  // the current time is |now|.
  void HandleGetHashError(const base::Time& now);

 private:
  // Map of GetHash requests to parameters which created it.
  typedef base::hash_map<const net::URLFetcher*, FullHashCallback> HashRequests;

  // The factory that controls the creation of V4ProtocolManager.
  // This is used by tests.
  static V4ProtocolManagerFactory* factory_;

  // Current active request (in case we need to cancel) for updates or chunks
  // from the SafeBrowsing service. We can only have one of these outstanding
  // at any given time unlike GetHash requests, which are tracked separately.
  scoped_ptr<net::URLFetcher> request_;

  // The number of HTTP response errors since the the last successful HTTP
  // response, used for request backoff timing.
  size_t gethash_error_count_;

  // Multiplier for the backoff error after the second.
  size_t gethash_back_off_mult_;

  HashRequests hash_requests_;

  // For v4, the next gethash time is set to the backoff time is the last
  // response was an error, or the minimum wait time if the last response was
  // successful.
  base::Time next_gethash_time_;

  // Current product version sent in each request.
  std::string version_;

  // The safe browsing client name sent in each request.
  std::string client_name_;

  // The context we use to issue network requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // ID for URLFetchers for testing.
  int url_fetcher_id_;

  DISALLOW_COPY_AND_ASSIGN(V4ProtocolManager);
};

// Interface of a factory to create V4ProtocolManager.  Useful for tests.
class V4ProtocolManagerFactory {
 public:
  V4ProtocolManagerFactory() {}
  virtual ~V4ProtocolManagerFactory() {}
  virtual V4ProtocolManager* CreateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const SafeBrowsingProtocolConfig& config) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(V4ProtocolManagerFactory);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_V4_PROTOCOL_MANAGER_H_
