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
#include "chrome/common/chrome_switches.h"
#include "content/browser/browser_thread.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"

// TODO(lzheng): Get rid of the AddRef and Release after
// SafeBrowsingService::Client is changed to RefCountedThreadSafe<>.

DownloadSBClient::DownloadSBClient(DownloadCreateInfo* info) : info_(info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(info_);
  ResourceDispatcherHost* rdh = g_browser_process->resource_dispatcher_host();
  if (rdh)
    sb_service_ = rdh->safe_browsing_service();
}

DownloadSBClient::~DownloadSBClient() {}

void DownloadSBClient::CheckDownloadUrl(DoneCallback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // It is not allowed to call this method twice.
  CHECK(!done_callback_.get());
  CHECK(callback);

  start_time_ = base::TimeTicks::Now();
  done_callback_.reset(callback);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &DownloadSBClient::CheckDownloadUrlOnIOThread));
  UpdateDownloadUrlCheckStats(DOWNLOAD_URL_CHECKS_TOTAL);
}

void DownloadSBClient::CheckDownloadUrlOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Will be released in OnDownloadUrlCheckResult.
  AddRef();
  if (sb_service_.get() && !sb_service_->CheckDownloadUrl(info_->url, this)) {
    // Wait for SafeBrowsingService to call back OnDownloadUrlCheckResult.
    return;
  }
  OnDownloadUrlCheckResult(info_->url, SafeBrowsingService::SAFE);
}

// The callback interface for SafeBrowsingService::Client.
// Called when the result of checking a download URL is known.
void DownloadSBClient::OnDownloadUrlCheckResult(
    const GURL& url, SafeBrowsingService::UrlCheckResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &DownloadSBClient::SafeBrowsingCheckUrlDone,
                        result));
  Release();
}

void DownloadSBClient::SafeBrowsingCheckUrlDone(
    SafeBrowsingService::UrlCheckResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "SafeBrowsingCheckUrlDone with result: " << result;

  bool is_dangerous = result != SafeBrowsingService::SAFE;
  CommandLine* cmdline = CommandLine::ForCurrentProcess();
  if (!cmdline->HasSwitch(switches::kSbEnableDownloadWarningUI)) {
    // Always ignore the safebrowsing result without the flag.
    done_callback_->Run(info_, false);
  } else {
    done_callback_->Run(info_, is_dangerous);
  }

  UMA_HISTOGRAM_TIMES("SB2.DownloadUrlCheckDuration",
                      base::TimeTicks::Now() - start_time_);
  if (is_dangerous) {
    UpdateDownloadUrlCheckStats(DOWNLOAD_URL_CHECKS_MALWARE);

    // TODO(lzheng): we need to collect page_url and referrer_url to report.
    sb_service_->ReportSafeBrowsingHit(info_->url,
                                       info_->url /* page_url */,
                                       info_->url /* referrer_url */,
                                       true,
                                       result);
  }
}

void DownloadSBClient::UpdateDownloadUrlCheckStats(SBStatsType stat_type) {
  UMA_HISTOGRAM_ENUMERATION("SB2.DownloadUrlChecks",
                            stat_type,
                            DOWNLOAD_URL_CHECKS_MAX);
}
