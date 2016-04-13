// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper class which handles communication with the SafeBrowsing servers for
// improved binary download protection.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "components/safe_browsing_db/database_manager.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"


namespace content {
class DownloadItem;
class PageNavigator;
}

namespace net {
class X509Certificate;
}  // namespace net

namespace safe_browsing {
class BinaryFeatureExtractor;
class ClientDownloadRequest;
class DownloadFeedbackService;

// This class provides an asynchronous API to check whether a particular
// client download is malicious or not.
class DownloadProtectionService {
 public:
  enum DownloadCheckResult {
    UNKNOWN,
    SAFE,
    DANGEROUS,
    UNCOMMON,
    DANGEROUS_HOST,
    POTENTIALLY_UNWANTED
  };

  // Callback type which is invoked once the download request is done.
  typedef base::Callback<void(DownloadCheckResult)> CheckDownloadCallback;

  // A type of callback run on the main thread when a ClientDownloadRequest has
  // been formed for a download, or when one has not been formed for a supported
  // download.
  typedef base::Callback<void(content::DownloadItem*,
                              const ClientDownloadRequest*)>
      ClientDownloadRequestCallback;

  // A list of ClientDownloadRequest callbacks.
  typedef base::CallbackList<void(content::DownloadItem*,
                                  const ClientDownloadRequest*)>
      ClientDownloadRequestCallbackList;

  // A subscription to a registered ClientDownloadRequest callback.
  typedef std::unique_ptr<ClientDownloadRequestCallbackList::Subscription>
      ClientDownloadRequestSubscription;

  // Creates a download service.  The service is initially disabled.  You need
  // to call SetEnabled() to start it.  |sb_service| owns this object.
  explicit DownloadProtectionService(SafeBrowsingService* sb_service);

  virtual ~DownloadProtectionService();

  // Parse a flag of blacklisted sha256 hashes to check at each download.
  // This is used for testing, to hunt for safe-browsing by-pass bugs.
  virtual void ParseManualBlacklistFlag();

  // Return true if this hash value is blacklisted via flag (for testing).
  virtual bool IsHashManuallyBlacklisted(const std::string& sha256_hash) const;

  // Checks whether the given client download is likely to be malicious or not.
  // The result is delivered asynchronously via the given callback.  This
  // method must be called on the UI thread, and the callback will also be
  // invoked on the UI thread.  This method must be called once the download
  // is finished and written to disk.
  virtual void CheckClientDownload(content::DownloadItem* item,
                                   const CheckDownloadCallback& callback);

  // Checks whether any of the URLs in the redirect chain of the
  // download match the SafeBrowsing bad binary URL list.  The result is
  // delivered asynchronously via the given callback.  This method must be
  // called on the UI thread, and the callback will also be invoked on the UI
  // thread.  Pre-condition: !info.download_url_chain.empty().
  virtual void CheckDownloadUrl(const content::DownloadItem& item,
                                const CheckDownloadCallback& callback);

  // Returns true iff the download specified by |info| should be scanned by
  // CheckClientDownload() for malicious content.
  virtual bool IsSupportedDownload(const content::DownloadItem& item,
                                   const base::FilePath& target_path) const;

  // Display more information to the user regarding the download specified by
  // |info|. This method is invoked when the user requests more information
  // about a download that was marked as malicious.
  void ShowDetailsForDownload(const content::DownloadItem& item,
                              content::PageNavigator* navigator);

  // Enables or disables the service.  This is usually called by the
  // SafeBrowsingService, which tracks whether any profile uses these services
  // at all.  Disabling causes any pending and future requests to have their
  // callbacks called with "UNKNOWN" results.
  void SetEnabled(bool enabled);

  bool enabled() const {
    return enabled_;
  }

  // Returns the timeout that is used by CheckClientDownload().
  int64_t download_request_timeout_ms() const {
    return download_request_timeout_ms_;
  }

  DownloadFeedbackService* feedback_service() {
    return feedback_service_.get();
  }

  // Registers a callback that will be run when a ClientDownloadRequest has
  // been formed.
  ClientDownloadRequestSubscription RegisterClientDownloadRequestCallback(
      const ClientDownloadRequestCallback& callback);

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
    REASON_NOT_BINARY_FILE,
    REASON_REQUEST_CANCELED,
    REASON_DOWNLOAD_DANGEROUS,
    REASON_DOWNLOAD_SAFE,
    REASON_EMPTY_URL_CHAIN,
    DEPRECATED_REASON_HTTPS_URL,
    REASON_PING_DISABLED,
    REASON_TRUSTED_EXECUTABLE,
    REASON_OS_NOT_SUPPORTED,
    REASON_DOWNLOAD_UNCOMMON,
    REASON_DOWNLOAD_NOT_SUPPORTED,
    REASON_INVALID_RESPONSE_VERDICT,
    REASON_ARCHIVE_WITHOUT_BINARIES,
    REASON_DOWNLOAD_DANGEROUS_HOST,
    REASON_DOWNLOAD_POTENTIALLY_UNWANTED,
    REASON_UNSUPPORTED_URL_SCHEME,
    REASON_MANUAL_BLACKLIST,
    REASON_MAX  // Always add new values before this one.
  };

 private:
  class CheckClientDownloadRequest;  // Per-request state
  friend class DownloadProtectionServiceTest;

  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           CheckClientDownloadWhitelistedUrl);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           CheckClientDownloadValidateRequest);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           CheckClientDownloadSuccess);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           CheckClientDownloadHTTPS);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           CheckClientDownloadBlob);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           CheckClientDownloadData);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           CheckClientDownloadZip);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           CheckClientDownloadFetchFailed);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           TestDownloadRequestTimeout);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           CheckClientCrxDownloadSuccess);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceFlagTest,
                           CheckClientDownloadOverridenByFlag);

  static const char kDownloadRequestUrl[];

  // Cancels all requests in |download_requests_|, and empties it, releasing
  // the references to the requests.
  void CancelPendingRequests();

  // Called by a CheckClientDownloadRequest instance when it finishes, to
  // remove it from |download_requests_|.
  void RequestFinished(CheckClientDownloadRequest* request);

  // Given a certificate and its immediate issuer certificate, generates the
  // list of strings that need to be checked against the download whitelist to
  // determine whether the certificate is whitelisted.
  static void GetCertificateWhitelistStrings(
      const net::X509Certificate& certificate,
      const net::X509Certificate& issuer,
      std::vector<std::string>* whitelist_strings);

  // Returns the URL that will be used for download requests.
  static GURL GetDownloadRequestUrl();

  // These pointers may be NULL if SafeBrowsing is disabled.
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  // The context we use to issue network requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // Map of client download request to the corresponding callback that
  // has to be invoked when the request is done.  This map contains all
  // pending server requests.
  std::set<scoped_refptr<CheckClientDownloadRequest> > download_requests_;

  // Keeps track of the state of the service.
  bool enabled_;

  // BinaryFeatureExtractor object, may be overridden for testing.
  scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor_;

  int64_t download_request_timeout_ms_;

  std::unique_ptr<DownloadFeedbackService> feedback_service_;

  // A list of callbacks to be run on the main thread when a
  // ClientDownloadRequest has been formed.
  ClientDownloadRequestCallbackList client_download_request_callbacks_;

  // List of 8-byte hashes that are blacklisted manually by flag.
  // Normally empty.
  std::set<std::string> manual_blacklist_hashes_;

  DISALLOW_COPY_AND_ASSIGN(DownloadProtectionService);
};
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_SERVICE_H_
