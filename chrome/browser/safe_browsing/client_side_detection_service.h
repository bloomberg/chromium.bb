// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper class which handles communication with the SafeBrowsing backends for
// client-side phishing detection.  This class is used to fetch the client-side
// model and send a file descriptor that points to the model file to all
// renderers.  This class is also used to send a ping back to Google to verify
// if a particular site is really phishing or not.
//
// This class is not thread-safe and expects all calls to be made on the UI
// thread.  We also expect that the calling thread runs a message loop and that
// there is a FILE thread running to execute asynchronous file operations.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_H_
#pragma once

#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/task.h"
#include "base/time.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

class RenderProcessHost;

namespace base {
class TimeDelta;
}

namespace net {
class URLRequestContextGetter;
class URLRequestStatus;
}  // namespace net

namespace safe_browsing {
class ClientPhishingRequest;

class ClientSideDetectionService : public URLFetcher::Delegate,
                                   public NotificationObserver {
 public:
  typedef Callback2<GURL /* phishing URL */, bool /* is phishing */>::Type
      ClientReportPhishingRequestCallback;

  virtual ~ClientSideDetectionService();

  // Creates a client-side detection service and starts fetching the client-side
  // detection model if necessary.  The model will be stored in |model_path|.
  // The caller takes ownership of the object.  This function may return NULL.
  static ClientSideDetectionService* Create(
      const FilePath& model_path,
      net::URLRequestContextGetter* request_context_getter);

  // From the URLFetcher::Delegate interface.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const net::ResponseCookies& cookies,
                                  const std::string& data);

  // NotificationObserver overrides:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Sends a request to the SafeBrowsing servers with the ClientPhishingRequest.
  // The URL scheme of the |url()| in the request should be HTTP.  This method
  // takes ownership of the |verdict| as well as the |callback| and calls the
  // the callback once the result has come back from the server or if an error
  // occurs during the fetch.  If an error occurs the phishing verdict will
  // always be false.  The callback is always called after
  // SendClientReportPhishingRequest() returns and on the same thread as
  // SendClientReportPhishingRequest() was called.
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

  // Returns true and sets is_phishing if url is in the cache and valid.
  virtual bool GetValidCachedResult(const GURL& url, bool* is_phishing);

  // Returns true if the url is in the cache.
  virtual bool IsInCache(const GURL& url);

  // Returns true if we have sent more than kMaxReportsPerInterval in the last
  // kReportsInterval.
  virtual bool OverReportLimit();

 protected:
  // Use Create() method to create an instance of this object.
  ClientSideDetectionService(
      const FilePath& model_path,
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
    MODEL_FILE_UTIL_PROXY_ERROR,
    MODEL_CREATE_TMP_FILE_FAILED,
    MODEL_WRITE_TMP_FILE_FAILED,
    MODEL_CLOSE_TMP_FILE_FAILED,
    MODEL_REOPEN_TMP_FILE_FAILED,
    MODEL_MOVE_TMP_FILE_ERROR,
    MODEL_STATUS_MAX  // Always add new values before this one.
  };

  // Starts fetching the model from the network or the cache.  This method
  // is called periodically to check whether a new client model is available
  // for download.
  void StartFetchModel();

  // This method is called when we're done fetching the model either because
  // we hit an error somewhere or because we're actually done writing the model
  // to the temporary file.
  virtual void EndFetchModel(ClientModelStatus status);  // Virtual for testing.

 private:
  friend class ClientSideDetectionServiceTest;
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest, FetchModelTest);

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

  static const char kClientReportPhishingUrl[];
  static const char kClientModelUrl[];
  static const size_t kMaxModelSizeBytes;
  static const int kMaxReportsPerInterval;
  static const int kClientModelFetchIntervalMs;
  static const int kInitialClientModelFetchDelayMs;
  static const base::TimeDelta kReportsInterval;
  static const base::TimeDelta kNegativeCacheInterval;
  static const base::TimeDelta kPositiveCacheInterval;

  // Callback that is invoked once the attempt to create the temporary model
  // file on disk is done.  If the file was created successfully we
  // start writing the model to disk (asynchronously).  Otherwise, we
  // give up and send an invalid platform file to all the pending callbacks.
  void CreateTmpModelFileDone(base::PlatformFileError error_code,
                              base::PassPlatformFile file,
                              FilePath tmp_model_path);

  // Callback is invoked once we're done writing the model file to disk.
  // If everything went well then |tmp_model_file_| is a valid platform file.
  // If an error occurs we give up and send an invalid platform file to all the
  // pending callbacks.
  void WriteTmpModelFileDone(base::PlatformFileError error_code,
                             int bytes_written);

  // Called when we're done closing the writable model file and we're
  // ready to re-open the temporary model file in read-only mode.
  void CloseTmpModelFileDone(base::PlatformFileError error_code);

  // Reopen the temporary model file in read-only mode to make sure we don't
  // pass a writable file descritor to the renderer.
  void ReOpenTmpModelFileDone(base::PlatformFileError error_code,
                              base::PassPlatformFile file,
                              bool created);

  // Callback is invoked when the temporary model file was moved to its final
  // destination where the model is stored on disk.
  void MoveTmpModelFileDone(base::PlatformFileError error_code);

  // Helper function which closes the given model file if necessary.
  void CloseModelFile(base::PlatformFile* file);

  // Starts sending the request to the client-side detection frontends.
  // This method takes ownership of both pointers.
  void StartClientReportPhishingRequest(
      ClientPhishingRequest* verdict,
      ClientReportPhishingRequestCallback* callback);

  // Called by OnURLFetchComplete to handle the response from fetching the
  // model.
  void HandleModelResponse(const URLFetcher* source,
                           const GURL& url,
                           const net::URLRequestStatus& status,
                           int response_code,
                           const net::ResponseCookies& cookies,
                           const std::string& data);

  // Called by OnURLFetchComplete to handle the server response from
  // sending the client-side phishing request.
  void HandlePhishingVerdict(const URLFetcher* source,
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
  void SendModelToProcess(RenderProcessHost* process);

  // Same as above but sends the model to all rendereres.
  void SendModelToRenderers();

  FilePath model_path_;
  base::PlatformFile model_file_;
  int model_version_;
  scoped_ptr<URLFetcher> model_fetcher_;

  FilePath tmp_model_path_;
  base::PlatformFile tmp_model_file_;
  int tmp_model_version_;
  std::string tmp_model_string_;
  scoped_ptr<base::TimeDelta> tmp_model_max_age_;

  // Map of client report phishing request to the corresponding callback that
  // has to be invoked when the request is done.
  struct ClientReportInfo;
  std::map<const URLFetcher*, ClientReportInfo*> client_phishing_reports_;

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
  ScopedRunnableMethodFactory<ClientSideDetectionService> method_factory_;

  // The client-side detection service object (this) might go away before some
  // of the callbacks are done (e.g., asynchronous file operations).  The
  // callback factory will revoke all pending callbacks if this goes away to
  // avoid a crash.
  base::ScopedCallbackFactory<ClientSideDetectionService> callback_factory_;

  // The context we use to issue network requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // The network blocks that we consider private IP address ranges.
  std::vector<AddressRange> private_networks_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ClientSideDetectionService);
};

}  // namepsace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_H_
