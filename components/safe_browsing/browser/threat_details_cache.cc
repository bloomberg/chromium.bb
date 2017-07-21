// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the ThreatDetails class.

#include "components/safe_browsing/browser/threat_details.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/md5.h"
#include "base/strings/string_util.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/safe_browsing/browser/threat_details_cache.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;

// Only send small files for now, a better strategy would use the size
// of the whole report and the user's bandwidth.
static const uint32_t kMaxBodySizeBytes = 1024;

namespace safe_browsing {

ThreatDetailsCacheCollector::ThreatDetailsCacheCollector()
    : resources_(NULL), result_(NULL), has_started_(false) {}

void ThreatDetailsCacheCollector::StartCacheCollection(
    net::URLRequestContextGetter* request_context_getter,
    ResourceMap* resources,
    bool* result,
    const base::Closure& callback) {
  // Start the data collection from the HTTP cache. We use a URLFetcher
  // and set the right flags so we only hit the cache.
  DVLOG(1) << "Getting cache data for all urls...";
  request_context_getter_ = request_context_getter;
  resources_ = resources;
  resources_it_ = resources_->begin();
  result_ = result;
  callback_ = callback;
  has_started_ = true;

  // Post a task in the message loop, so the callers don't need to
  // check if we call their callback immediately.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ThreatDetailsCacheCollector::OpenEntry, this));
}

bool ThreatDetailsCacheCollector::HasStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return has_started_;
}

ThreatDetailsCacheCollector::~ThreatDetailsCacheCollector() {}

// Fetch a URL and advance to the next one when done.
void ThreatDetailsCacheCollector::OpenEntry() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "OpenEntry";

  if (resources_it_ == resources_->end()) {
    AllDone(true);
    return;
  }

  if (!request_context_getter_.get()) {
    DVLOG(1) << "Missing request context getter";
    AllDone(false);
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("safe_browsing_cache_collector", R"(
        semantics {
          sender: "Threat Details Cache Collector"
          description:
            "This request fetches different items from safe browsing cache "
            "and DOES NOT make an actual network request."
          trigger:
            "When safe browsing extended report is collecting data."
          data:
            "None"
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature by stopping sending "
            "security incident reports to Google via disabling 'Automatically "
            "report details of possible security incidents to Google.' in "
            "Chrome's settings under Advanced Settings, Privacy. The feature "
            "is disabled by default."
          chrome_policy {
            SafeBrowsingExtendedReportingOptInAllowed {
              policy_options {mode: MANDATORY}
              SafeBrowsingExtendedReportingOptInAllowed: false
            }
          }
        })");

  current_fetch_ =
      net::URLFetcher::Create(GURL(resources_it_->first), net::URLFetcher::GET,
                              this, traffic_annotation);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      current_fetch_.get(),
      data_use_measurement::DataUseUserData::SAFE_BROWSING);
  current_fetch_->SetRequestContext(request_context_getter_.get());
  // Only from cache, and don't save cookies.
  current_fetch_->SetLoadFlags(net::LOAD_ONLY_FROM_CACHE |
                               net::LOAD_SKIP_CACHE_VALIDATION |
                               net::LOAD_DO_NOT_SAVE_COOKIES);
  current_fetch_->SetAutomaticallyRetryOn5xx(false);  // No retries.
  current_fetch_->Start();  // OnURLFetchComplete will be called when done.
}

ClientSafeBrowsingReportRequest::Resource*
ThreatDetailsCacheCollector::GetResource(const GURL& url) {
  ResourceMap::iterator it = resources_->find(url.spec());
  if (it != resources_->end()) {
    return it->second.get();
  }
  return NULL;
}

void ThreatDetailsCacheCollector::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DVLOG(1) << "OnUrlFetchComplete";
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(current_fetch_.get());
  if (source->GetStatus().status() != net::URLRequestStatus::SUCCESS &&
      source->GetStatus().error() == net::ERR_CACHE_MISS) {
    // Cache miss, skip this resource.
    DVLOG(1) << "Cache miss for url: " << source->GetURL();
    AdvanceEntry();
    return;
  }

  if (source->GetStatus().status() != net::URLRequestStatus::SUCCESS) {
    // Some other error occurred, e.g. the request could have been cancelled.
    DVLOG(1) << "Unsuccessful fetch: " << source->GetURL();
    AdvanceEntry();
    return;
  }

  // Set the response headers and body to the right resource, which
  // might not be the same as the one we asked for.
  // For redirects, resources_it_->first != url.spec().
  ClientSafeBrowsingReportRequest::Resource* resource =
      GetResource(source->GetURL());
  if (!resource) {
    DVLOG(1) << "Cannot find resource for url:" << source->GetURL();
    AdvanceEntry();
    return;
  }

  ReadResponse(resource, source);
  std::string data;
  source->GetResponseAsString(&data);
  ReadData(resource, data);
  AdvanceEntry();
}

void ThreatDetailsCacheCollector::ReadResponse(
    ClientSafeBrowsingReportRequest::Resource* pb_resource,
    const net::URLFetcher* source) {
  DVLOG(1) << "ReadResponse";
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::HttpResponseHeaders* headers = source->GetResponseHeaders();
  if (!headers) {
    DVLOG(1) << "Missing response headers.";
    return;
  }

  ClientSafeBrowsingReportRequest::HTTPResponse* pb_response =
      pb_resource->mutable_response();
  pb_response->mutable_firstline()->set_code(headers->response_code());
  size_t iter = 0;
  std::string name, value;
  while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
    ClientSafeBrowsingReportRequest::HTTPHeader* pb_header =
        pb_response->add_headers();
    pb_header->set_name(name);
    // Strip any Set-Cookie headers.
    if (base::LowerCaseEqualsASCII(name, "set-cookie")) {
      pb_header->set_value("");
    } else {
      pb_header->set_value(value);
    }
  }

  if (!source->WasFetchedViaProxy()) {
    pb_response->set_remote_ip(source->GetSocketAddress().ToString());
  }
}

void ThreatDetailsCacheCollector::ReadData(
    ClientSafeBrowsingReportRequest::Resource* pb_resource,
    const std::string& data) {
  DVLOG(1) << "ReadData";
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ClientSafeBrowsingReportRequest::HTTPResponse* pb_response =
      pb_resource->mutable_response();
  if (data.size() <= kMaxBodySizeBytes) {  // Only send small bodies for now.
    pb_response->set_body(data);
  }
  pb_response->set_bodylength(data.size());
  pb_response->set_bodydigest(base::MD5String(data));
}

void ThreatDetailsCacheCollector::AdvanceEntry() {
  DVLOG(1) << "AdvanceEntry";
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Advance to the next resource.
  ++resources_it_;
  current_fetch_.reset(NULL);

  // Create a task so we don't take over the IO thread for too long.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ThreatDetailsCacheCollector::OpenEntry, this));
}

void ThreatDetailsCacheCollector::AllDone(bool success) {
  DVLOG(1) << "AllDone";
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  *result_ = success;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, callback_);
  callback_.Reset();
}

}  // namespace safe_browsing
