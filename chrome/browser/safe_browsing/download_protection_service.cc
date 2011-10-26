// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection_service.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/net/http_return.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "content/browser/browser_thread.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace safe_browsing {

const char DownloadProtectionService::kDownloadRequestUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/download";

DownloadProtectionService::DownloadInfo::DownloadInfo()
    : total_bytes(0), user_initiated(false) {}

DownloadProtectionService::DownloadInfo::~DownloadInfo() {}

DownloadProtectionService::DownloadProtectionService(
    SafeBrowsingService* sb_service,
    net::URLRequestContextGetter* request_context_getter)
    : sb_service_(sb_service),
      request_context_getter_(request_context_getter),
      enabled_(false) {}

DownloadProtectionService::~DownloadProtectionService() {
  STLDeleteContainerPairFirstPointers(download_requests_.begin(),
                                      download_requests_.end());
  download_requests_.clear();
}

void DownloadProtectionService::SetEnabled(bool enabled) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&DownloadProtectionService::SetEnabledOnIOThread,
                 this, enabled));
}

void DownloadProtectionService::SetEnabledOnIOThread(bool enabled) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (enabled == enabled_) {
    return;
  }
  enabled_ = enabled;
  if (!enabled_) {
    for (DownloadRequests::iterator it = download_requests_.begin();
         it != download_requests_.end(); ++it) {
      it->second.Run(SAFE);
    }
    STLDeleteContainerPairFirstPointers(download_requests_.begin(),
                                        download_requests_.end());
    download_requests_.clear();
  }
}

void DownloadProtectionService::OnURLFetchComplete(
    const content::URLFetcher* source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  scoped_ptr<const content::URLFetcher> s(source);
  if (download_requests_.find(source) != download_requests_.end()) {
    CheckDownloadCallback callback = download_requests_[source];
    download_requests_.erase(source);
    if (!enabled_) {
      // SafeBrowsing got disabled.  We can't do anything.  Note: the request
      // object will be deleted.
      RecordStats(REASON_SB_DISABLED);
      return;
    }
    DownloadCheckResultReason reason = REASON_MAX;
    reason = REASON_SERVER_PING_FAILED;
    if (source->GetStatus().is_success() &&
        RC_REQUEST_OK == source->GetResponseCode()) {
      std::string data;
      source->GetResponseAsString(&data);
      if (data.size() > 0) {
        // For now no matter what we'll always say the download is safe.
        // TODO(noelutz): Parse the response body to see exactly what's going
        // on.
        reason = REASON_INVALID_RESPONSE_PROTO;
      }
    }
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DownloadProtectionService::EndCheckClientDownload,
                   this, SAFE, reason, callback));
  } else {
    NOTREACHED();
  }
}

bool DownloadProtectionService::CheckClientDownload(
    const DownloadInfo& info,
    const CheckDownloadCallback& callback) {
  // TODO(noelutz): implement some cache to make sure we don't issue the same
  // request over and over again if a user downloads the same binary multiple
  // times.
  if (info.download_url_chain.empty()) {
    RecordStats(REASON_INVALID_URL);
    return true;
  }
  const GURL& final_url = info.download_url_chain.back();
  if (!final_url.is_valid() || final_url.is_empty() ||
      !final_url.SchemeIs("http")) {
    RecordStats(REASON_INVALID_URL);
    return true;  // For now we only support HTTP download URLs.
  }
  // TODO(noelutz): DownloadInfo should also contain the IP address of every
  // URL in the redirect chain.  We also should check whether the download URL
  // is hosted on the internal network.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&DownloadProtectionService::StartCheckClientDownload,
                 this, info, callback));
  return false;
}

void DownloadProtectionService::StartCheckClientDownload(
    const DownloadInfo& info,
    const CheckDownloadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!enabled_ || !sb_service_.get()) {
    // This is a hard fail.  We won't even call the callback in this case.
    RecordStats(REASON_SB_DISABLED);
    return;
  }
  DownloadCheckResultReason reason = REASON_MAX;
  for (size_t i = 0; i < info.download_url_chain.size(); ++i) {
    if (sb_service_->MatchDownloadWhitelistUrl(info.download_url_chain[i])) {
      reason = REASON_WHITELISTED_URL;
      break;
    }
  }
  if (sb_service_->MatchDownloadWhitelistUrl(info.referrer_url)) {
    reason = REASON_WHITELISTED_REFERRER;
  }
  // TODO(noelutz): check signature and CA against whitelist.

  ClientDownloadRequest request;
  request.set_url(info.download_url_chain.back().spec());
  request.mutable_digests()->set_sha256(info.sha256_hash);
  request.set_length(info.total_bytes);
  for (size_t i = 0; i < info.download_url_chain.size(); ++i) {
    ClientDownloadRequest::Resource* resource = request.add_resources();
    resource->set_url(info.download_url_chain[i].spec());
    if (i == info.download_url_chain.size() - 1) {
      // The last URL in the chain is the download URL.
      resource->set_type(ClientDownloadRequest::DOWNLOAD_URL);
      resource->set_referrer(info.referrer_url.spec());
    } else {
      resource->set_type(ClientDownloadRequest::DOWNLOAD_REDIRECT);
    }
    // TODO(noelutz): fill out the remote IP addresses.
  }
  request.set_user_initiated(info.user_initiated);
  std::string request_data;
  if (!request.SerializeToString(&request_data)) {
    reason = REASON_INVALID_REQUEST_PROTO;
  }

  if (reason != REASON_MAX) {
    // We stop here because the download is considered safe.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&DownloadProtectionService::EndCheckClientDownload,
                   this, SAFE, reason, callback));
    return;
  }

  content::URLFetcher* fetcher = content::URLFetcher::Create(
      0 /* ID used for testing */, GURL(kDownloadRequestUrl),
      content::URLFetcher::POST, this);
  download_requests_[fetcher] = callback;
  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  fetcher->SetRequestContext(request_context_getter_.get());
  fetcher->SetUploadData("application/octet-stream", request_data);
  fetcher->Start();
}

void DownloadProtectionService::EndCheckClientDownload(
    DownloadCheckResult result,
    DownloadCheckResultReason reason,
    const CheckDownloadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RecordStats(reason);
  callback.Run(result);
}

void DownloadProtectionService::RecordStats(DownloadCheckResultReason reason) {
  UMA_HISTOGRAM_ENUMERATION("SBClientDownload.CheckDownloadStats",
                            reason,
                            REASON_MAX);
}
}  // namespace safe_browsing
