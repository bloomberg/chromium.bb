// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/ui_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/debug/leak_tracker.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/malware_details.h"
#include "chrome/browser/safe_browsing/metadata.pb.h"
#include "chrome/browser/safe_browsing/ping_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/url_constants.h"
#include "components/metrics/metrics_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::WebContents;

struct SafeBrowsingUIManager::WhiteListedEntry {
  int render_process_host_id;
  int render_view_id;
  std::string domain;
  SBThreatType threat_type;
};

SafeBrowsingUIManager::UnsafeResource::UnsafeResource()
    : is_subresource(false),
      threat_type(SB_THREAT_TYPE_SAFE),
      render_process_host_id(-1),
      render_view_id(-1) {
}

SafeBrowsingUIManager::UnsafeResource::~UnsafeResource() { }

SafeBrowsingUIManager::SafeBrowsingUIManager(
    const scoped_refptr<SafeBrowsingService>& service)
    : sb_service_(service) {
}

SafeBrowsingUIManager::~SafeBrowsingUIManager() { }

void SafeBrowsingUIManager::StopOnIOThread(bool shutdown) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (shutdown)
    sb_service_ = NULL;
}

void SafeBrowsingUIManager::LogPauseDelay(base::TimeDelta time) {
  UMA_HISTOGRAM_LONG_TIMES("SB2.Delay", time);
}

// Only report SafeBrowsing related stats when UMA is enabled. User must also
// ensure that safe browsing is enabled from the calling profile.
bool SafeBrowsingUIManager::CanReportStats() const {
  const metrics::MetricsService* metrics = g_browser_process->metrics_service();
  return metrics && metrics->reporting_active();
}

void SafeBrowsingUIManager::OnBlockingPageDone(
    const std::vector<UnsafeResource>& resources,
    bool proceed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  for (std::vector<UnsafeResource>::const_iterator iter = resources.begin();
       iter != resources.end(); ++iter) {
    const UnsafeResource& resource = *iter;
    if (!resource.callback.is_null())
      resource.callback.Run(proceed);

    if (proceed) {
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&SafeBrowsingUIManager::UpdateWhitelist, this, resource));
    }
  }
}

void SafeBrowsingUIManager::DisplayBlockingPage(
    const UnsafeResource& resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!resource.threat_metadata.empty() &&
      resource.threat_type == SB_THREAT_TYPE_URL_MALWARE) {
    safe_browsing::MalwarePatternType proto;
    // Malware sites tagged as "landing site" should only show a warning for a
    // main-frame or sub-frame resource. (See "Types of Malware sites" under
    // https://developers.google.com/safe-browsing/developers_guide_v3#UserWarnings)
    if (proto.ParseFromString(resource.threat_metadata) &&
        proto.pattern_type() == safe_browsing::MalwarePatternType::LANDING &&
        resource.is_subresource && !resource.is_subframe) {
      if (!resource.callback.is_null()) {
        BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE, base::Bind(resource.callback, true));
      }
      return;
    }
  }

  // Indicate to interested observers that the resource in question matched the
  // SB filters. If the resource is already whitelisted, OnSafeBrowsingHit
  // won't be called.
  if (resource.threat_type != SB_THREAT_TYPE_SAFE) {
    FOR_EACH_OBSERVER(Observer, observer_list_, OnSafeBrowsingMatch(resource));
  }

  // Check if the user has already ignored our warning for this render_view
  // and domain.
  if (IsWhitelisted(resource)) {
    if (!resource.callback.is_null()) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE, base::Bind(resource.callback, true));
    }
    return;
  }

  // The tab might have been closed.
  WebContents* web_contents =
      tab_util::GetWebContentsByID(resource.render_process_host_id,
                                   resource.render_view_id);

  if (!web_contents) {
    // The tab is gone and we did not have a chance at showing the interstitial.
    // Just act as if "Don't Proceed" were chosen.
    std::vector<UnsafeResource> resources;
    resources.push_back(resource);
    BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingUIManager::OnBlockingPageDone,
                 this, resources, false));
    return;
  }

  if (resource.threat_type != SB_THREAT_TYPE_SAFE &&
      CanReportStats()) {
    GURL page_url = web_contents->GetURL();
    GURL referrer_url;
    NavigationEntry* entry = web_contents->GetController().GetActiveEntry();
    if (entry)
      referrer_url = entry->GetReferrer().url;

    // When the malicious url is on the main frame, and resource.original_url
    // is not the same as the resource.url, that means we have a redirect from
    // resource.original_url to resource.url.
    // Also, at this point, page_url points to the _previous_ page that we
    // were on. We replace page_url with resource.original_url and referrer
    // with page_url.
    if (!resource.is_subresource &&
        !resource.original_url.is_empty() &&
        resource.original_url != resource.url) {
      referrer_url = page_url;
      page_url = resource.original_url;
    }
    ReportSafeBrowsingHit(resource.url, page_url, referrer_url,
                          resource.is_subresource, resource.threat_type,
                          std::string() /* post_data */);
  }
  if (resource.threat_type != SB_THREAT_TYPE_SAFE) {
    FOR_EACH_OBSERVER(Observer, observer_list_, OnSafeBrowsingHit(resource));
  }
  SafeBrowsingBlockingPage::ShowBlockingPage(this, resource);
}

// A safebrowsing hit is sent after a blocking page for malware/phishing
// or after the warning dialog for download urls, only for UMA users.
void SafeBrowsingUIManager::ReportSafeBrowsingHit(
    const GURL& malicious_url,
    const GURL& page_url,
    const GURL& referrer_url,
    bool is_subresource,
    SBThreatType threat_type,
    const std::string& post_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!CanReportStats())
    return;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingUIManager::ReportSafeBrowsingHitOnIOThread, this,
                 malicious_url, page_url, referrer_url, is_subresource,
                 threat_type, post_data));
}

void SafeBrowsingUIManager::AddObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observer_list_.AddObserver(observer);
}

void SafeBrowsingUIManager::RemoveObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observer_list_.RemoveObserver(observer);
}

void SafeBrowsingUIManager::ReportSafeBrowsingHitOnIOThread(
    const GURL& malicious_url,
    const GURL& page_url,
    const GURL& referrer_url,
    bool is_subresource,
    SBThreatType threat_type,
    const std::string& post_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // The service may delete the ping manager (i.e. when user disabling service,
  // etc). This happens on the IO thread.
  if (sb_service_.get() == NULL || sb_service_->ping_manager() == NULL)
    return;

  DVLOG(1) << "ReportSafeBrowsingHit: " << malicious_url << " " << page_url
           << " " << referrer_url << " " << is_subresource << " "
           << threat_type;
  sb_service_->ping_manager()->ReportSafeBrowsingHit(
      malicious_url, page_url,
      referrer_url, is_subresource,
      threat_type, post_data);
}

// If the user had opted-in to send MalwareDetails, this gets called
// when the report is ready.
void SafeBrowsingUIManager::SendSerializedMalwareDetails(
    const std::string& serialized) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // The service may delete the ping manager (i.e. when user disabling service,
  // etc). This happens on the IO thread.
  if (sb_service_.get() == NULL || sb_service_->ping_manager() == NULL)
    return;

  if (!serialized.empty()) {
    DVLOG(1) << "Sending serialized malware details.";
    sb_service_->ping_manager()->ReportMalwareDetails(serialized);
  }
}

void SafeBrowsingUIManager::UpdateWhitelist(const UnsafeResource& resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Whitelist this domain and warning type for the given tab.
  WhiteListedEntry entry;
  entry.render_process_host_id = resource.render_process_host_id;
  entry.render_view_id = resource.render_view_id;
  entry.domain = net::registry_controlled_domains::GetDomainAndRegistry(
      resource.url,
      net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  entry.threat_type = resource.threat_type;
  white_listed_entries_.push_back(entry);
}

bool SafeBrowsingUIManager::IsWhitelisted(const UnsafeResource& resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Check if the user has already ignored our warning for this render_view
  // and domain.
  for (size_t i = 0; i < white_listed_entries_.size(); ++i) {
    const WhiteListedEntry& entry = white_listed_entries_[i];
    if (entry.render_process_host_id == resource.render_process_host_id &&
        entry.render_view_id == resource.render_view_id &&
        // Threat type must be the same or they can either be client-side
        // phishing/malware URL or a SafeBrowsing phishing/malware URL.
        // If we show one type of phishing/malware warning we don't want to show
        // a second phishing/malware warning.
        (entry.threat_type == resource.threat_type ||
         (entry.threat_type == SB_THREAT_TYPE_URL_PHISHING &&
          resource.threat_type == SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL) ||
         (entry.threat_type == SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL &&
          resource.threat_type == SB_THREAT_TYPE_URL_PHISHING) ||
         (entry.threat_type == SB_THREAT_TYPE_URL_MALWARE &&
          resource.threat_type == SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL) ||
         (entry.threat_type == SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL &&
          resource.threat_type == SB_THREAT_TYPE_URL_MALWARE))) {
      return entry.domain ==
          net::registry_controlled_domains::GetDomainAndRegistry(
              resource.url,
              net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
    }
  }
  return false;
}

