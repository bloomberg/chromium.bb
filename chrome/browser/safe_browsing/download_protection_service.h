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
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
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

class Profile;

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

  // A type of callback run on the main thread when a PPAPI
  // ClientDownloadRequest has been formed for a download.
  typedef base::Callback<void(const ClientDownloadRequest*)>
      PPAPIDownloadRequestCallback;

  // A list of PPAPI ClientDownloadRequest callbacks.
  typedef base::CallbackList<void(const ClientDownloadRequest*)>
      PPAPIDownloadRequestCallbackList;

  // A subscription to a registered PPAPI ClientDownloadRequest callback.
  typedef std::unique_ptr<PPAPIDownloadRequestCallbackList::Subscription>
      PPAPIDownloadRequestSubscription;

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
  virtual void CheckClientDownload(
      content::DownloadItem* item,
      const CheckDownloadCallback& callback);

  // Checks whether any of the URLs in the redirect chain of the
  // download match the SafeBrowsing bad binary URL list.  The result is
  // delivered asynchronously via the given callback.  This method must be
  // called on the UI thread, and the callback will also be invoked on the UI
  // thread.  Pre-condition: !info.download_url_chain.empty().
  virtual void CheckDownloadUrl(content::DownloadItem* item,
                                const CheckDownloadCallback& callback);

  // Returns true iff the download specified by |info| should be scanned by
  // CheckClientDownload() for malicious content.
  virtual bool IsSupportedDownload(
      const content::DownloadItem& item,
      const base::FilePath& target_path) const;

  virtual void CheckPPAPIDownloadRequest(
      const GURL& requestor_url,
      const GURL& initiating_frame_url,
      content::WebContents* web_contents,
      const base::FilePath& default_file_path,
      const std::vector<base::FilePath::StringType>& alternate_extensions,
      Profile* profile,
      const CheckDownloadCallback& callback);

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

  // Registers a callback that will be run when a PPAPI ClientDownloadRequest
  // has been formed.
  PPAPIDownloadRequestSubscription RegisterPPAPIDownloadRequestCallback(
      const PPAPIDownloadRequestCallback& callback);

  double whitelist_sample_rate() const {
    return whitelist_sample_rate_;
  }

  scoped_refptr<SafeBrowsingNavigationObserverManager>
  navigation_observer_manager() {
    return navigation_observer_manager_;
  }

  static void SetDownloadPingToken(content::DownloadItem* item,
                                   const std::string& token);

  static std::string GetDownloadPingToken(const content::DownloadItem* item);

 protected:
  // Enum to keep track why a particular download verdict was chosen.
  // Used for UMA metrics. Do not reorder.
  enum DownloadCheckResultReason {
    REASON_INVALID_URL = 0,
    REASON_SB_DISABLED = 1,
    REASON_WHITELISTED_URL = 2,
    REASON_WHITELISTED_REFERRER = 3,
    REASON_INVALID_REQUEST_PROTO = 4,
    REASON_SERVER_PING_FAILED = 5,
    REASON_INVALID_RESPONSE_PROTO = 6,
    REASON_NOT_BINARY_FILE = 7,
    REASON_REQUEST_CANCELED = 8,
    REASON_DOWNLOAD_DANGEROUS = 9,
    REASON_DOWNLOAD_SAFE = 10,
    REASON_EMPTY_URL_CHAIN = 11,
    DEPRECATED_REASON_HTTPS_URL = 12,
    REASON_PING_DISABLED = 13,
    REASON_TRUSTED_EXECUTABLE = 14,
    REASON_OS_NOT_SUPPORTED = 15,
    REASON_DOWNLOAD_UNCOMMON = 16,
    REASON_DOWNLOAD_NOT_SUPPORTED = 17,
    REASON_INVALID_RESPONSE_VERDICT = 18,
    REASON_ARCHIVE_WITHOUT_BINARIES = 19,
    REASON_DOWNLOAD_DANGEROUS_HOST = 20,
    REASON_DOWNLOAD_POTENTIALLY_UNWANTED = 21,
    REASON_UNSUPPORTED_URL_SCHEME = 22,
    REASON_MANUAL_BLACKLIST = 23,
    REASON_LOCAL_FILE = 24,
    REASON_REMOTE_FILE = 25,
    REASON_SAMPLED_UNSUPPORTED_FILE = 26,
    REASON_VERDICT_UNKNOWN = 27,
    REASON_MAX  // Always add new values before this one.
  };

 private:
  class CheckClientDownloadRequest;
  class PPAPIDownloadRequest;
  friend class DownloadUrlSBClient;
  friend class DownloadProtectionServiceTest;
  friend class DownloadDangerPromptTest;

  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           CheckClientDownloadWhitelistedUrlWithoutSampling);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           CheckClientDownloadWhitelistedUrlWithSampling);
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
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           PPAPIDownloadRequest_InvalidResponse);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           PPAPIDownloadRequest_Timeout);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceFlagTest,
                           CheckClientDownloadOverridenByFlag);

  static const char kDownloadRequestUrl[];

  static const void* const kDownloadPingTokenKey;

  // Helper class for easy setting and getting token string.
  class DownloadPingToken : public base::SupportsUserData::Data {
   public:
    explicit DownloadPingToken(const std::string& token)
       : token_string_(token) {}

    std::string token_string() {
      return token_string_;
    }
   private:
    std::string token_string_;

    DISALLOW_COPY_AND_ASSIGN(DownloadPingToken);
  };

  // Cancels all requests in |download_requests_|, and empties it, releasing
  // the references to the requests.
  void CancelPendingRequests();

  // Called by a CheckClientDownloadRequest instance when it finishes, to
  // remove it from |download_requests_|.
  void RequestFinished(CheckClientDownloadRequest* request);

  void PPAPIDownloadCheckRequestFinished(PPAPIDownloadRequest* request);

  // Given a certificate and its immediate issuer certificate, generates the
  // list of strings that need to be checked against the download whitelist to
  // determine whether the certificate is whitelisted.
  static void GetCertificateWhitelistStrings(
      const net::X509Certificate& certificate,
      const net::X509Certificate& issuer,
      std::vector<std::string>* whitelist_strings);

  // Returns the URL that will be used for download requests.
  static GURL GetDownloadRequestUrl();

  // If kDownloadAttribution feature is enabled, identify referrer chain info of
  // a download. This function also records UMA stats of download attribution
  // result.
  std::unique_ptr<ReferrerChain> IdentifyReferrerChain(
    const GURL& download_url,
    content::WebContents* web_contents);

  // If kDownloadAttribution feature is enabled, identify referrer chain of the
  // PPAPI download based on the frame URL where the download is initiated.
  // Then add referrer chain info to ClientDownloadRequest proto. This function
  // also records UMA stats of download attribution result.
  void AddReferrerChainToPPAPIClientDownloadRequest(
      const GURL& initiating_frame_url,
      const GURL& initiating_main_frame_url,
      int tab_id,
      bool has_user_gesture,
      ClientDownloadRequest* out_request);

  // These pointers may be NULL if SafeBrowsing is disabled.
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<SafeBrowsingNavigationObserverManager>
  navigation_observer_manager_;

  // The context we use to issue network requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // Set of pending server requests for DownloadManager mediated downloads.
  std::set<scoped_refptr<CheckClientDownloadRequest>> download_requests_;

  // Set of pending server requests for PPAPI mediated downloads. Using a map
  // because heterogeneous lookups aren't available yet in std::unordered_map.
  std::unordered_map<PPAPIDownloadRequest*,
                     std::unique_ptr<PPAPIDownloadRequest>>
      ppapi_download_requests_;

  // Keeps track of the state of the service.
  bool enabled_;

  // BinaryFeatureExtractor object, may be overridden for testing.
  scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor_;

  int64_t download_request_timeout_ms_;

  std::unique_ptr<DownloadFeedbackService> feedback_service_;

  // A list of callbacks to be run on the main thread when a
  // ClientDownloadRequest has been formed.
  ClientDownloadRequestCallbackList client_download_request_callbacks_;

  // A list of callbacks to be run on the main thread when a
  // PPAPIDownloadRequest has been formed.
  PPAPIDownloadRequestCallbackList ppapi_download_request_callbacks_;

  // List of 8-byte hashes that are blacklisted manually by flag.
  // Normally empty.
  std::set<std::string> manual_blacklist_hashes_;

  // Rate of whitelisted downloads we sample to send out download ping.
  double whitelist_sample_rate_;

  DISALLOW_COPY_AND_ASSIGN(DownloadProtectionService);
};
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_SERVICE_H_
