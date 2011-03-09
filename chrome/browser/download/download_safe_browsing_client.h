// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SAFE_BROWSING_CLIENT_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SAFE_BROWSING_CLIENT_H_
#pragma once

#include "base/callback.h"
#include "base/ref_counted.h"
#include "base/time.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"

struct DownloadCreateInfo;

// This is a helper class used by DownloadManager to check a download URL with
// SafeBrowsingService. The client is refcounted and will be  released once
// there is no reference to it.
// Usage:
// {
//    scoped_refptr<DownloadSBClient> client_ = new DownloadSBClient(...);
//    client_->CheckDownloadUrl(..., NewCallback(this,
//                              &DownloadManager::UrlCallBack));
//    or
//    client_->CheckDownloadHash(..., NewCallback(this,
//                               &DownloadManager::HashCallBack));
// }
// DownloadManager::UrlCallBack(...) or HashCallCall {
//    // After this, the |client_| is gone.
// }
class DownloadSBClient
    : public SafeBrowsingService::Client,
      public base::RefCountedThreadSafe<DownloadSBClient> {
 public:
  typedef Callback2<DownloadCreateInfo*, bool>::Type UrlDoneCallback;
  typedef Callback2<int32, bool>::Type HashDoneCallback;

  DownloadSBClient(int32 download_id,
                   const GURL& download_url,
                   const GURL& page_url,
                   const GURL& referrer_url);

  // Call safebrowsing service to verifiy the download.
  // For each DownloadSBClient instance, either CheckDownloadUrl or
  // CheckDownloadHash can be called, and be called only once.
  // DownloadSBClient instance.
  void CheckDownloadUrl(DownloadCreateInfo* info, UrlDoneCallback* callback);
  void CheckDownloadHash(const std::string& hash, HashDoneCallback* callback);

 private:
  // Call SafeBrowsingService on IO thread to verify the download URL or
  // hash of downloaded file.
  void CheckDownloadUrlOnIOThread(const GURL& url);
  void CheckDownloadHashOnIOThread(const std::string& hash);

  // Callback interfaces for SafeBrowsingService::Client.
  virtual void OnDownloadUrlCheckResult(
      const GURL& url, SafeBrowsingService::UrlCheckResult result);
  virtual void OnDownloadHashCheckResult(
      const std::string& hash, SafeBrowsingService::UrlCheckResult result);

  // Enumerate for histogramming purposes.
  // DO NOT CHANGE THE ORDERING OF THESE VALUES (different histogram data will
  // be mixed together based on their values).
  enum SBStatsType {
    DOWNLOAD_URL_CHECKS_TOTAL,
    DOWNLOAD_URL_CHECKS_CANCELED,
    DOWNLOAD_URL_CHECKS_MALWARE,

    DOWNLOAD_HASH_CHECKS_TOTAL,
    DOWNLOAD_HASH_CHECKS_MALWARE,

    // Memory space for histograms is determined by the max.
    // ALWAYS ADD NEW VALUES BEFORE THIS ONE.
    DOWNLOAD_CHECKS_MAX
  };

  friend class base::RefCountedThreadSafe<DownloadSBClient>;
  virtual ~DownloadSBClient();

  // Call DownloadManager on UI thread for download URL or hash check.
  void SafeBrowsingCheckUrlDone(SafeBrowsingService::UrlCheckResult result);
  void SafeBrowsingCheckHashDone(SafeBrowsingService::UrlCheckResult result);

  // Report malware hits to safebrowsing service.
  void ReportMalware(SafeBrowsingService::UrlCheckResult result);

  // Update the UMA stats.
  void UpdateDownloadCheckStats(SBStatsType stat_type);

  scoped_ptr<UrlDoneCallback> url_done_callback_;
  scoped_ptr<HashDoneCallback> hash_done_callback_;

  // Not owned by this class.
  DownloadCreateInfo* info_;

  int32 download_id_;
  scoped_refptr<SafeBrowsingService> sb_service_;

  // These URLs are used to report malware to safe browsing service.
  GURL download_url_;
  GURL page_url_;
  GURL referrer_url_;

  // When a safebrowsing check starts, for stats purpose.
  base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(DownloadSBClient);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SAFE_BROWSING_CLIENT_H_
