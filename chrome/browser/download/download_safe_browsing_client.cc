// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "chrome/browser/download/download_safe_browsing_client.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/history/download_create_info.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"

// TODO(lzheng): Get rid of the AddRef and Release after
// SafeBrowsingService::Client is changed to RefCountedThreadSafe<>.

DownloadSBClient::DownloadSBClient(int32 download_id,
                                   const std::vector<GURL>& url_chain,
                                   const GURL& referrer_url)
  : info_(NULL),
    download_id_(download_id),
    url_chain_(url_chain),
    referrer_url_(referrer_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!url_chain.empty());
  ResourceDispatcherHost* rdh = g_browser_process->resource_dispatcher_host();
  if (rdh)
    sb_service_ = rdh->safe_browsing_service();
}

DownloadSBClient::~DownloadSBClient() {}

void DownloadSBClient::CheckDownloadUrl(DownloadCreateInfo* info,
                                        UrlDoneCallback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // It is not allowed to call this method twice.
  CHECK(!url_done_callback_.get() && !hash_done_callback_.get());
  CHECK(callback);
  CHECK(info);

  info_ = info;
  start_time_ = base::TimeTicks::Now();
  url_done_callback_.reset(callback);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &DownloadSBClient::CheckDownloadUrlOnIOThread,
                        info->url_chain));
}

void DownloadSBClient::CheckDownloadHash(const std::string& hash,
                                         HashDoneCallback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // It is not allowed to call this method twice.
  CHECK(!url_done_callback_.get() && !hash_done_callback_.get());
  CHECK(callback);

  start_time_ = base::TimeTicks::Now();
  hash_done_callback_.reset(callback);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &DownloadSBClient::CheckDownloadHashOnIOThread,
                        hash));
}

void DownloadSBClient::CheckDownloadUrlOnIOThread(
    const std::vector<GURL>& url_chain) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Will be released in OnDownloadUrlCheckResult.
  AddRef();
  if (sb_service_.get() && !sb_service_->CheckDownloadUrl(url_chain, this)) {
    // Wait for SafeBrowsingService to call back OnDownloadUrlCheckResult.
    return;
  }
  OnDownloadUrlCheckResult(url_chain, SafeBrowsingService::SAFE);
}

// The callback interface for SafeBrowsingService::Client.
// Called when the result of checking a download URL is known.
void DownloadSBClient::OnDownloadUrlCheckResult(
    const std::vector<GURL>& url_chain,
    SafeBrowsingService::UrlCheckResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &DownloadSBClient::SafeBrowsingCheckUrlDone,
                        result));
  Release();
}

void DownloadSBClient::CheckDownloadHashOnIOThread(const std::string& hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Will be released in OnDownloadUrlCheckResult.
  AddRef();
  if (sb_service_.get() && !sb_service_->CheckDownloadHash(hash, this)) {
    // Wait for SafeBrowsingService to call back OnDownloadUrlCheckResult.
    return;
  }
  OnDownloadHashCheckResult(hash, SafeBrowsingService::SAFE);
}

// The callback interface for SafeBrowsingService::Client.
// Called when the result of checking a download URL is known.
void DownloadSBClient::OnDownloadHashCheckResult(
    const std::string& hash, SafeBrowsingService::UrlCheckResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &DownloadSBClient::SafeBrowsingCheckHashDone,
                        result));
  Release();
}

void DownloadSBClient::SafeBrowsingCheckUrlDone(
    SafeBrowsingService::UrlCheckResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "SafeBrowsingCheckUrlDone with result: " << result;

  bool is_dangerous = result != SafeBrowsingService::SAFE;
  url_done_callback_->Run(info_, is_dangerous);

  if (sb_service_.get() && sb_service_->download_protection_enabled()) {
    UMA_HISTOGRAM_TIMES("SB2.DownloadUrlCheckDuration",
                        base::TimeTicks::Now() - start_time_);
    UpdateDownloadCheckStats(DOWNLOAD_URL_CHECKS_TOTAL);
    if (is_dangerous) {
      UpdateDownloadCheckStats(DOWNLOAD_URL_CHECKS_MALWARE);
      ReportMalware(result);
    }
  }
}

void DownloadSBClient::SafeBrowsingCheckHashDone(
    SafeBrowsingService::UrlCheckResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "SafeBrowsingCheckHashDone with result: " << result;

  bool is_dangerous = result != SafeBrowsingService::SAFE;
  hash_done_callback_->Run(download_id_, is_dangerous);

  if (sb_service_.get() && sb_service_->download_protection_enabled()) {
    UMA_HISTOGRAM_TIMES("SB2.DownloadHashCheckDuration",
                        base::TimeTicks::Now() - start_time_);
    UpdateDownloadCheckStats(DOWNLOAD_HASH_CHECKS_TOTAL);
    if (is_dangerous) {
      UpdateDownloadCheckStats(DOWNLOAD_HASH_CHECKS_MALWARE);
      ReportMalware(result);
    }
  }
}

void DownloadSBClient::ReportMalware(
    SafeBrowsingService::UrlCheckResult result) {
  std::string post_data;
  for (size_t i = 0; i < url_chain_.size(); ++i)
    post_data += url_chain_[i].spec() + "\n";

  sb_service_->ReportSafeBrowsingHit(url_chain_.back(),  // malicious_url
                                     url_chain_.front(), // page_url
                                     referrer_url_,
                                     true,
                                     result,
                                     post_data);
}

void DownloadSBClient::UpdateDownloadCheckStats(SBStatsType stat_type) {
  UMA_HISTOGRAM_ENUMERATION("SB2.DownloadChecks",
                            stat_type,
                            DOWNLOAD_CHECKS_MAX);
}
