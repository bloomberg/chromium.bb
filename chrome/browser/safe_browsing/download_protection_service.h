// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper class which handles communication with the SafeBrowsing servers for
// improved binary download protection.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_SERVICE_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

namespace net {
class URLRequestContextGetter;
class URLRequestStatus;
}  // namespace net
class SafeBrowsingService;

namespace safe_browsing {

// This class provides an asynchronous API to check whether a particular
// client download is malicious or not.
class DownloadProtectionService
    : public base::RefCountedThreadSafe<DownloadProtectionService>,
      public content::URLFetcherDelegate {
 public:
  // TODO(noelutz): we're missing some fields here: filename to get
  // the signature, server IPs, tab URL redirect chain, ...
  struct DownloadInfo {
    std::vector<GURL> download_url_chain;
    GURL referrer_url;
    std::string sha256_hash;
    int64 total_bytes;
    bool user_initiated;
    DownloadInfo();
    ~DownloadInfo();
  };

  enum DownloadCheckResult {
    SAFE,
    MALICIOUS,
    // In the future we may introduce a third category which corresponds to
    // suspicious downloads that are not known to be malicious.
  };

  // Callback type which is invoked once the download request is done.
  typedef base::Callback<void(DownloadCheckResult)> CheckDownloadCallback;

  // Creates a download service.  The service is initially disabled.  You need
  // to call SetEnabled() to start it.  We keep scoped references to both of
  // these objects.
  DownloadProtectionService(
      SafeBrowsingService* sb_service,
      net::URLRequestContextGetter* request_context_getter);

  // From the content::URLFetcherDelegate interface.
  virtual void OnURLFetchComplete(const URLFetcher* source) OVERRIDE;

  // Checks whether the given client download is likely to be
  // malicious or not.  If this method returns true it means the
  // download is safe or we're unable to tell whether it is safe or
  // not.  In this case the callback will never be called.  If this
  // method returns false we will asynchronously check whether the
  // download is safe and call the callback when we have the response.
  // This method should be called on the UI thread.  The callback will
  // be called on the UI thread and will always be called asynchronously.
  virtual bool CheckClientDownload(const DownloadInfo& info,
                                   const CheckDownloadCallback& callback);

  // Enables or disables the service.  This is usually called by the
  // SafeBrowsingService, which tracks whether any profile uses these services
  // at all.  Disabling cancels any pending requests; existing requests will
  // have their callbacks called with "SAFE" results.  Note: SetEnabled() is
  // asynchronous because this method is called on the UI thread but most
  // everything else happens on the IO thread.
  void SetEnabled(bool enabled);

  bool enabled() const {
    return enabled_;
  }

 protected:
  // Enum to keep track why a particular download verdict was chosen.
  // This is used to keep some stats around.
  enum DownloadCheckResultReason {
    REASON_INVALID_URL,
    REASON_SB_DISABLED,
    REASON_WHITELISTED_URL,
    REASON_WHITELISTED_REFERRER,
    REASON_INVALID_REQUEST_PROTO,
    REASON_SERVER_PING_FAILED,
    REASON_INVALID_RESPONSE_PROTO,
    REASON_MAX  // Always add new values before this one.
  };

  virtual ~DownloadProtectionService();

 private:
  friend class base::RefCountedThreadSafe<DownloadProtectionService>;
  friend class DownloadProtectionServiceTest;
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           CheckClientDownloadValidateRequest);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           CheckClientDownloadSuccess);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           CheckClientDownloadFetchFailed);

  static const char kDownloadRequestUrl[];

  // Same as above but this method is called on the IO thread after we have
  // done some basic checks to see whether the download is definitely not
  // safe.
  void StartCheckClientDownload(const DownloadInfo& info,
                                const CheckDownloadCallback& callback);

  // This function must run on the UI thread and will invoke the callback
  // with the given result.
  void EndCheckClientDownload(DownloadCheckResult result,
                              DownloadCheckResultReason reason,
                              const CheckDownloadCallback& callback);

  void RecordStats(DownloadCheckResultReason reason);

  // SetEnabled(bool) calls this method on the IO thread.
  void SetEnabledOnIOThread(bool enableed);

  // This pointer may be NULL if SafeBrowsing is disabled.
  scoped_refptr<SafeBrowsingService> sb_service_;

  // The context we use to issue network requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // Map of client download request to the corresponding callback that
  // has to be invoked when the request is done.  This map contains all
  // pending server requests.
  std::map<const URLFetcher*, CheckDownloadCallback> download_requests_;

  // Keeps track of the state of the service.
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(DownloadProtectionService);
};
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_SERVICE_H_
