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
//    client_->CheckDownloadUrl(NewCallback(this, &DownloadManager::CallBack));
// }
// DownloadManager::CallBack(...) {
//    // After this, the |client_| is gone.
// }
class DownloadSBClient
    : public SafeBrowsingService::Client,
      public base::RefCountedThreadSafe<DownloadSBClient> {
 public:
  typedef Callback2<DownloadCreateInfo*, bool>::Type DoneCallback;

  explicit DownloadSBClient(DownloadCreateInfo* info);

  // Note: This method can only be called once per DownloadSBClient instance.
  void CheckDownloadUrl(DoneCallback* callback);

 protected:
  // Call SafeBrowsingService on IO thread to verify the download URL.
  void CheckDownloadUrlOnIOThread();

  // The callback interface for SafeBrowsingService::Client.
  // Called when the result of checking a download URL is known.
  virtual void OnDownloadUrlCheckResult(
      const GURL& url, SafeBrowsingService::UrlCheckResult result);

 private:
  // Enumerate for histogramming purposes.
  // DO NOT CHANGE THE ORDERING OF THESE VALUES (different histogram data will
  // be mixed together based on their values).
  enum SBStatsType {
    DOWNLOAD_URL_CHECKS_TOTAL,
    DOWNLOAD_URL_CHECKS_CANCELED,
    DOWNLOAD_URL_CHECKS_MALWARE,

    // Memory space for histograms is determined by the max.
    // ALWAYS ADD NEW VALUES BEFORE THIS ONE.
    DOWNLOAD_URL_CHECKS_MAX
  };

  friend class base::RefCountedThreadSafe<DownloadSBClient>;
  virtual ~DownloadSBClient();

  // Call DownloadManager on UI thread.
  void SafeBrowsingCheckUrlDone(SafeBrowsingService::UrlCheckResult result);

  // Update the UMA stats.
  void UpdateDownloadUrlCheckStats(SBStatsType stat_type);

  scoped_ptr<DoneCallback> done_callback_;

  // Not owned by this class.
  DownloadCreateInfo* info_;
  scoped_refptr<SafeBrowsingService> sb_service_;

  // When a safebrowsing URL check starts, for stats purpose.
  base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(DownloadSBClient);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SAFE_BROWSING_CLIENT_H_
