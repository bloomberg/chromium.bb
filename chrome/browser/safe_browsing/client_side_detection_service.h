// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper class which handles communication with the SafeBrowsing backends for
// client-side phishing detection.  This class is used to fetch the client-side
// model and send it to all renderers.  This class is also used to send a ping
// back to Google to verify if a particular site is really phishing or not.
//
// This class is not thread-safe and expects all calls to be made on the UI
// thread.  We also expect that the calling thread runs a message loop.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_H_
#pragma once

#include <map>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_old.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "base/time.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

class SafeBrowsingService;

namespace base {
class TimeDelta;
}

namespace content {
class RenderProcessHost;
}

namespace net {
class URLRequestContextGetter;
class URLRequestStatus;
typedef std::vector<std::string> ResponseCookies;
}  // namespace net

namespace safe_browsing {
class ClientPhishingRequest;
class ClientPhishingResponse;
class ClientSideModel;

class ClientSideDetectionService : public content::URLFetcherDelegate,
                                   public content::NotificationObserver {
 public:
  typedef Callback2<GURL /* phishing URL */, bool /* is phishing */>::Type
      ClientReportPhishingRequestCallback;

  virtual ~ClientSideDetectionService();

  // Creates a client-side detection service.  The service is initially
  // disabled, use SetEnabledAndRefreshState() to start it.  The caller takes
  // ownership of the object.  This function may return NULL.
  static ClientSideDetectionService* Create(
      net::URLRequestContextGetter* request_context_getter);

  // Enables or disables the service, and refreshes the state of all renderers.
  // This is usually called by the SafeBrowsingService, which tracks whether
  // any profile uses these services at all.  Disabling cancels any pending
  // requests; existing ClientSideDetectionHosts will have their callbacks
  // called with "false" verdicts.  Enabling starts downloading the model after
  // a delay.  In all cases, each render process is updated to match the state
  // of the SafeBrowsing preference for that profile.
  void SetEnabledAndRefreshState(bool enabled);

  bool enabled() const {
    return enabled_;
  }

  // From the content::URLFetcherDelegate interface.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Sends a request to the SafeBrowsing servers with the ClientPhishingRequest.
  // The URL scheme of the |url()| in the request should be HTTP.  This method
  // takes ownership of the |verdict| as well as the |callback| and calls the
  // the callback once the result has come back from the server or if an error
  // occurs during the fetch.  If the service is disabled or an error occurs
  // the phishing verdict will always be false.  The callback is always called
  // after SendClientReportPhishingRequest() returns and on the same thread as
  // SendClientReportPhishingRequest() was called.  You may set |callback| to
  // NULL if you don't care about the server verdict.
  virtual void SendClientReportPhishingRequest(
      ClientPhishingRequest* verdict,
      ClientReportPhishingRequestCallback* callback);

  // Returns true if the given IP address string falls within a private
  // (unroutable) network block.  Pages which are hosted on these IP addresses
  // are exempt from client-side phishing detection.  This is called by the
  // ClientSideDetectionHost prior to sending the renderer a
  // SafeBrowsingMsg_StartPhishingDetection IPC.
  //
  // ip_address should be a dotted IPv4 address, or an unbracketed IPv6
  // address.
  virtual bool IsPrivateIPAddress(const std::string& ip_address) const;

  // Returns true if the given IP address is on the list of known bad IPs.
  // ip_address should be a dotted IPv4 address, or an unbracketed IPv6
  // address.
  virtual bool IsBadIpAddress(const std::string& ip_address) const;

  // Returns true and sets is_phishing if url is in the cache and valid.
  virtual bool GetValidCachedResult(const GURL& url, bool* is_phishing);

  // Returns true if the url is in the cache.
  virtual bool IsInCache(const GURL& url);

  // Returns true if we have sent more than kMaxReportsPerInterval in the last
  // kReportsInterval.
  virtual bool OverReportLimit();

 protected:
  // Use Create() method to create an instance of this object.
  explicit ClientSideDetectionService(
      net::URLRequestContextGetter* request_context_getter);

  // Enum used to keep stats about why we fail to get the client model.
  enum ClientModelStatus {
    MODEL_SUCCESS,
    MODEL_NOT_CHANGED,
    MODEL_FETCH_FAILED,
    MODEL_EMPTY,
    MODEL_TOO_LARGE,
    MODEL_PARSE_ERROR,
    MODEL_MISSING_FIELDS,
    MODEL_INVALID_VERSION_NUMBER,
    MODEL_BAD_HASH_IDS,
    MODEL_STATUS_MAX  // Always add new values before this one.
  };

  // Starts fetching the model from the network or the cache.  This method
  // is called periodically to check whether a new client model is available
  // for download.
  void StartFetchModel();

  // Schedules the next fetch of the model.
  virtual void ScheduleFetchModel(int64 delay_ms);  // Virtual for testing.

  // This method is called when we're done fetching the model either because
  // we hit an error somewhere or because we're actually done fetch and
  // validating the model.
  virtual void EndFetchModel(ClientModelStatus status);  // Virtual for testing.

 private:
  friend class ClientSideDetectionServiceTest;
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest, FetchModelTest);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest, SetBadSubnets);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest,
                           SetEnabledAndRefreshState);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest, IsBadIpAddress);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest,
                           IsFalsePositiveResponse);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest,
                           ModelHasValidHashIds);

  // CacheState holds all information necessary to respond to a caller without
  // actually making a HTTP request.
  struct CacheState {
    bool is_phishing;
    base::Time timestamp;

    CacheState(bool phish, base::Time time);
  };
  typedef std::map<GURL, linked_ptr<CacheState> > PhishingCache;

  // A tuple of (IP address block, prefix size) representing a private
  // IP address range.
  typedef std::pair<net::IPAddressNumber, size_t> AddressRange;

  // Maps a IPv6 subnet mask to a set of hashed IPv6 subnets.  The IPv6
  // subnets are in network order and hashed with sha256.
  typedef std::map<std::string /* subnet mask */,
                   std::set<std::string /* hashed subnet */> > BadSubnetMap;

  static const char kClientReportPhishingUrl[];
  static const char kClientModelUrl[];
  static const size_t kMaxModelSizeBytes;
  static const int kMaxReportsPerInterval;
  static const int kClientModelFetchIntervalMs;
  static const int kInitialClientModelFetchDelayMs;
  static const base::TimeDelta kReportsInterval;
  static const base::TimeDelta kNegativeCacheInterval;
  static const base::TimeDelta kPositiveCacheInterval;

  // Starts sending the request to the client-side detection frontends.
  // This method takes ownership of both pointers.
  void StartClientReportPhishingRequest(
      ClientPhishingRequest* verdict,
      ClientReportPhishingRequestCallback* callback);

  // Called by OnURLFetchComplete to handle the response from fetching the
  // model.
  void HandleModelResponse(const content::URLFetcher* source,
                           const GURL& url,
                           const net::URLRequestStatus& status,
                           int response_code,
                           const net::ResponseCookies& cookies,
                           const std::string& data);

  // Called by OnURLFetchComplete to handle the server response from
  // sending the client-side phishing request.
  void HandlePhishingVerdict(const content::URLFetcher* source,
                             const GURL& url,
                             const net::URLRequestStatus& status,
                             int response_code,
                             const net::ResponseCookies& cookies,
                             const std::string& data);

  // Invalidate cache results which are no longer useful.
  void UpdateCache();

  // Get the number of phishing reports that we have sent over kReportsInterval
  int GetNumReports();

  // Initializes the |private_networks_| vector with the network blocks
  // that we consider non-public IP addresses.  Returns true on success.
  bool InitializePrivateNetworks();

  // Send the model to the given renderer.
  void SendModelToProcess(content::RenderProcessHost* process);

  // Same as above but sends the model to all rendereres.
  void SendModelToRenderers();

  // Reads the bad subnets from the client model and inserts them into
  // |bad_subnets| for faster lookups.  This method is static to simplify
  // testing.
  static void SetBadSubnets(const ClientSideModel& model,
                            BadSubnetMap* bad_subnets);


  // Returns true iff all the hash id's in the client-side model point to
  // valid hashes in the model.
  static bool ModelHasValidHashIds(const ClientSideModel& model);

  // Returns true iff the response is phishing (phishy() is true) and if the
  // given URL matches one of the whitelisted expressions in the given
  // ClientPhishingResponse.
  static bool IsFalsePositiveResponse(const GURL& url,
                                      const ClientPhishingResponse& response);

  // Whether the service is running or not.  When the service is not running,
  // it won't download the model nor report detected phishing URLs.
  bool enabled_;

  std::string model_str_;
  scoped_ptr<ClientSideModel> model_;
  scoped_ptr<base::TimeDelta> model_max_age_;
  scoped_ptr<content::URLFetcher> model_fetcher_;

  // Map of client report phishing request to the corresponding callback that
  // has to be invoked when the request is done.
  struct ClientReportInfo;
  std::map<const content::URLFetcher*, ClientReportInfo*>
      client_phishing_reports_;

  // Cache of completed requests. Used to satisfy requests for the same urls
  // as long as the next request falls within our caching window (which is
  // determined by kNegativeCacheInterval and kPositiveCacheInterval). The
  // size of this cache is limited by kMaxReportsPerDay *
  // ceil(InDays(max(kNegativeCacheInterval, kPositiveCacheInterval))).
  // TODO(gcasto): Serialize this so that it doesn't reset on browser restart.
  PhishingCache cache_;

  // Timestamp of when we sent a phishing request. Used to limit the number
  // of phishing requests that we send in a day.
  // TODO(gcasto): Serialize this so that it doesn't reset on browser restart.
  std::queue<base::Time> phishing_report_times_;

  // Used to asynchronously call the callbacks for
  // SendClientReportPhishingRequest.
  base::WeakPtrFactory<ClientSideDetectionService> weak_factory_;

  // The context we use to issue network requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // The network blocks that we consider private IP address ranges.
  std::vector<AddressRange> private_networks_;

  // Map of bad subnets which are copied from the client model and put into
  // this map to speed up lookups.
  BadSubnetMap bad_subnets_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ClientSideDetectionService);
};
}  // namepsace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_H_
