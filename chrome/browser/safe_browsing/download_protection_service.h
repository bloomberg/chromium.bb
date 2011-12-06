// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper class which handles communication with the SafeBrowsing servers for
// improved binary download protection.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_SERVICE_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "googleurl/src/gurl.h"

class DownloadItem;
class SafeBrowsingService;

namespace net {
class URLRequestContextGetter;
class X509Certificate;
}  // namespace net

namespace safe_browsing {
class SignatureUtil;

// This class provides an asynchronous API to check whether a particular
// client download is malicious or not.
class DownloadProtectionService {
 public:
  // TODO(noelutz): we're missing some fields here: server IPs,
  // tab URL redirect chain, ...
  struct DownloadInfo {
    FilePath local_file;  // Where the download is currently stored.
    FilePath target_file;  // Where the download will eventually be stored.
    std::vector<GURL> download_url_chain;
    GURL referrer_url;
    std::string sha256_hash;
    int64 total_bytes;
    bool user_initiated;
    std::string remote_address;
    DownloadInfo();
    ~DownloadInfo();
    std::string DebugString() const;
    // Creates a DownloadInfo from a DownloadItem object.
    static DownloadInfo FromDownloadItem(const DownloadItem& item);
  };

  enum DownloadCheckResult {
    SAFE,
    DANGEROUS,
    // In the future we may introduce a third category which corresponds to
    // suspicious downloads that are not known to be malicious.
  };

  // Callback type which is invoked once the download request is done.
  typedef base::Callback<void(DownloadCheckResult)> CheckDownloadCallback;

  // Creates a download service.  The service is initially disabled.  You need
  // to call SetEnabled() to start it.  |sb_service| owns this object; we
  // keep a reference to |request_context_getter|.
  DownloadProtectionService(
      SafeBrowsingService* sb_service,
      net::URLRequestContextGetter* request_context_getter);

  virtual ~DownloadProtectionService();

  // Checks whether the given client download is likely to be malicious or not.
  // The result is delivered asynchronously via the given callback.  This
  // method must be called on the UI thread, and the callback will also be
  // invoked on the UI thread.  This method must be called once the download
  // is finished and written to disk.
  virtual void CheckClientDownload(const DownloadInfo& info,
                                   const CheckDownloadCallback& callback);

  // Checks whether any of the URLs in the redirect chain of the
  // download match the SafeBrowsing bad binary URL list.  The result is
  // delivered asynchronously via the given callback.  This method must be
  // called on the UI thread, and the callback will also be invoked on the UI
  // thread.  Pre-condition: !info.download_url_chain.empty().
  virtual void CheckDownloadUrl(const DownloadInfo& info,
                                const CheckDownloadCallback& callback);

  // Returns true iff the given filename has an extension that is supported
  // by this service.  In other words: this method returns true if it can
  // protect users against files with that particular extension.
  bool IsSupportedFileType(const FilePath& filename) const;

  // Enables or disables the service.  This is usually called by the
  // SafeBrowsingService, which tracks whether any profile uses these services
  // at all.  Disabling causes any pending and future requests to have their
  // callbacks called with "SAFE" results.
  void SetEnabled(bool enabled);

  bool enabled() const {
    return enabled_;
  }

  // Returns the timeout that is used by CheckClientDownload().
  int64 download_request_timeout_ms() const {
    return download_request_timeout_ms_;
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
    REASON_NOT_BINARY_FILE,
    REASON_REQUEST_CANCELED,
    REASON_DOWNLOAD_DANGEROUS,
    REASON_DOWNLOAD_SAFE,
    REASON_EMPTY_URL_CHAIN,
    REASON_HTTPS_URL,
    REASON_PING_DISABLED,
    REASON_TRUSTED_EXECUTABLE,
    REASON_OS_NOT_SUPPORTED,
    REASON_MAX  // Always add new values before this one.
  };

 private:
  class CheckClientDownloadRequest;  // Per-request state
  friend class DownloadProtectionServiceTest;
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           CheckClientDownloadValidateRequest);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           CheckClientDownloadSuccess);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           CheckClientDownloadFetchFailed);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           TestDownloadRequestTimeout);
  static const char kDownloadRequestUrl[];

  // Cancels all requests in |download_requests_|, and empties it, releasing
  // the references to the requests.
  void CancelPendingRequests();

  // Called by a CheckClientDownloadRequest instance when it finishes, to
  // remove it from |download_requests_|.
  void RequestFinished(CheckClientDownloadRequest* request);

  static void FillDownloadInfo(const DownloadItem& item,
                               DownloadInfo* download_info);

  // Given a certificate and its immediate issuer certificate, generates the
  // list of strings that need to be checked against the download whitelist to
  // determine whether the certificate is whitelisted.
  static void GetCertificateWhitelistStrings(
      const net::X509Certificate& certificate,
      const net::X509Certificate& issuer,
      std::vector<std::string>* whitelist_strings);

  // This pointer may be NULL if SafeBrowsing is disabled. The
  // SafeBrowsingService owns us, so we don't need to hold a reference to it.
  SafeBrowsingService* sb_service_;

  // The context we use to issue network requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // Map of client download request to the corresponding callback that
  // has to be invoked when the request is done.  This map contains all
  // pending server requests.
  std::set<scoped_refptr<CheckClientDownloadRequest> > download_requests_;

  // Keeps track of the state of the service.
  bool enabled_;

  // SignatureUtil object, may be overridden for testing.
  scoped_refptr<SignatureUtil> signature_util_;

  int64 download_request_timeout_ms_;

  DISALLOW_COPY_AND_ASSIGN(DownloadProtectionService);
};
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_SERVICE_H_
