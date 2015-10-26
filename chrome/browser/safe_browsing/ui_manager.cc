// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/ui_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/debug/leak_tracker.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/malware_details.h"
#include "chrome/browser/safe_browsing/metadata.pb.h"
#include "chrome/browser/safe_browsing/ping_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::WebContents;

namespace {

const void* const kWhitelistKey = &kWhitelistKey;

class WhitelistUrlSet : public base::SupportsUserData::Data {
 public:
  WhitelistUrlSet() {}

  bool Contains(const GURL url) {
    auto iter = set_.find(url.GetWithEmptyPath());
    return iter != set_.end();
  }

  void Insert(const GURL url) { set_.insert(url.GetWithEmptyPath()); }

 private:
  std::set<GURL> set_;

  DISALLOW_COPY_AND_ASSIGN(WhitelistUrlSet);
};

}  // namespace

// SafeBrowsingUIManager::UnsafeResource ---------------------------------------

SafeBrowsingUIManager::UnsafeResource::UnsafeResource()
    : is_subresource(false),
      threat_type(SB_THREAT_TYPE_SAFE),
      render_process_host_id(-1),
      render_view_id(-1),
      threat_source(FROM_UNKNOWN) {
}

SafeBrowsingUIManager::UnsafeResource::~UnsafeResource() { }

// SafeBrowsingUIManager -------------------------------------------------------

SafeBrowsingUIManager::SafeBrowsingUIManager(
    const scoped_refptr<SafeBrowsingService>& service)
    : sb_service_(service) {}

SafeBrowsingUIManager::~SafeBrowsingUIManager() {}

void SafeBrowsingUIManager::StopOnIOThread(bool shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (shutdown)
    sb_service_ = NULL;
}

void SafeBrowsingUIManager::LogPauseDelay(base::TimeDelta time) {
  UMA_HISTOGRAM_LONG_TIMES("SB2.Delay", time);
}


void SafeBrowsingUIManager::OnBlockingPageDone(
    const std::vector<UnsafeResource>& resources,
    bool proceed) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (std::vector<UnsafeResource>::const_iterator iter = resources.begin();
       iter != resources.end(); ++iter) {
    const UnsafeResource& resource = *iter;
    if (!resource.callback.is_null())
      resource.callback.Run(proceed);

    if (proceed) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&SafeBrowsingUIManager::AddToWhitelist, this, resource));
    }
  }
}

void SafeBrowsingUIManager::DisplayBlockingPage(
    const UnsafeResource& resource) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (resource.is_subresource && !resource.is_subframe) {
    // Sites tagged as serving Unwanted Software should only show a warning for
    // main-frame or sub-frame resource. Similar warning restrictions should be
    // applied to malware sites tagged as "landing sites" (see "Types of
    // Malware sites" under
    // https://developers.google.com/safe-browsing/developers_guide_v3#UserWarnings).
    safe_browsing::MalwarePatternType proto;
    if (resource.threat_type == SB_THREAT_TYPE_URL_UNWANTED ||
        (resource.threat_type == SB_THREAT_TYPE_URL_MALWARE &&
         !resource.threat_metadata.empty() &&
         proto.ParseFromString(resource.threat_metadata) &&
         proto.pattern_type() == safe_browsing::MalwarePatternType::LANDING)) {
      if (!resource.callback.is_null()) {
        BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE, base::Bind(resource.callback, true));
      }
      return;
    }
  }

  // Indicate to interested observers that the resource in question matched the
  // SB filters.
  if (resource.threat_type != SB_THREAT_TYPE_SAFE) {
    FOR_EACH_OBSERVER(Observer, observer_list_, OnSafeBrowsingMatch(resource));
  }

  // The tab might have been closed. If it was closed, just act as if "Don't
  // Proceed" had been chosen.
  WebContents* web_contents =
      tab_util::GetWebContentsByID(resource.render_process_host_id,
                                   resource.render_view_id);
  if (!web_contents) {
    std::vector<UnsafeResource> resources;
    resources.push_back(resource);
    BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingUIManager::OnBlockingPageDone,
                 this, resources, false));
    return;
  }

  // Check if the user has already ignored a SB warning for the same WebContents
  // and top-level domain.
  if (IsWhitelisted(resource)) {
    if (!resource.callback.is_null()) {
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                              base::Bind(resource.callback, true));
    }
    return;
  }

  if (resource.threat_type != SB_THREAT_TYPE_SAFE) {
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

    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    const bool is_extended_reporting =
        profile &&
        profile->GetPrefs()->GetBoolean(
            prefs::kSafeBrowsingExtendedReportingEnabled);

    MaybeReportSafeBrowsingHit(resource.url, page_url, referrer_url,
                               resource.is_subresource, resource.threat_type,
                               std::string(), /* post_data */
                               is_extended_reporting);
  }

  if (resource.threat_type != SB_THREAT_TYPE_SAFE) {
    FOR_EACH_OBSERVER(Observer, observer_list_, OnSafeBrowsingHit(resource));
  }
  SafeBrowsingBlockingPage::ShowBlockingPage(this, resource);
}

// A safebrowsing hit is sent after a blocking page for malware/phishing
// or after the warning dialog for download urls, only for
// UMA || extended_reporting users.
void SafeBrowsingUIManager::MaybeReportSafeBrowsingHit(
    const GURL& malicious_url,
    const GURL& page_url,
    const GURL& referrer_url,
    bool is_subresource,
    SBThreatType threat_type,
    const std::string& post_data,
    bool is_extended_reporting) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Decide if we should send this report.
  const metrics::MetricsService* metrics = g_browser_process->metrics_service();
  const bool metrics_active = metrics && metrics->reporting_active();
  if (metrics_active || is_extended_reporting) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SafeBrowsingUIManager::ReportSafeBrowsingHitOnIOThread,
                   this, malicious_url, page_url, referrer_url, is_subresource,
                   threat_type, post_data, is_extended_reporting));
  }
}

void SafeBrowsingUIManager::ReportInvalidCertificateChain(
    const std::string& serialized_report,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &SafeBrowsingUIManager::ReportInvalidCertificateChainOnIOThread, this,
          serialized_report),
      callback);
}

void SafeBrowsingUIManager::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_list_.AddObserver(observer);
}

void SafeBrowsingUIManager::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_list_.RemoveObserver(observer);
}

void SafeBrowsingUIManager::ReportSafeBrowsingHitOnIOThread(
    const GURL& malicious_url,
    const GURL& page_url,
    const GURL& referrer_url,
    bool is_subresource,
    SBThreatType threat_type,
    const std::string& post_data,
    bool is_extended_reporting) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The service may delete the ping manager (i.e. when user disabling service,
  // etc). This happens on the IO thread.
  if (sb_service_.get() == NULL || sb_service_->ping_manager() == NULL)
    return;

  DVLOG(1) << "ReportSafeBrowsingHit: " << malicious_url << " " << page_url
           << " " << referrer_url << " " << is_subresource << " "
           << threat_type;
  sb_service_->ping_manager()->ReportSafeBrowsingHit(
      malicious_url, page_url, referrer_url, is_subresource, threat_type,
      post_data, is_extended_reporting);
}

void SafeBrowsingUIManager::ReportInvalidCertificateChainOnIOThread(
    const std::string& serialized_report) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The service may delete the ping manager (i.e. when user disabling service,
  // etc). This happens on the IO thread.
  if (!sb_service_ || !sb_service_->ping_manager())
    return;

  sb_service_->ping_manager()->ReportInvalidCertificateChain(serialized_report);
}

// If the user had opted-in to send MalwareDetails, this gets called
// when the report is ready.
void SafeBrowsingUIManager::SendSerializedMalwareDetails(
    const std::string& serialized) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The service may delete the ping manager (i.e. when user disabling service,
  // etc). This happens on the IO thread.
  if (sb_service_.get() == NULL || sb_service_->ping_manager() == NULL)
    return;

  if (!serialized.empty()) {
    DVLOG(1) << "Sending serialized malware details.";
    sb_service_->ping_manager()->ReportMalwareDetails(serialized);
  }
}

// Whitelist this domain in the current WebContents. Either add the
// domain to an existing WhitelistUrlSet, or create a new WhitelistUrlSet.
void SafeBrowsingUIManager::AddToWhitelist(const UnsafeResource& resource) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebContents* web_contents = tab_util::GetWebContentsByID(
      resource.render_process_host_id, resource.render_view_id);

  WhitelistUrlSet* site_list =
      static_cast<WhitelistUrlSet*>(web_contents->GetUserData(kWhitelistKey));
  if (!site_list) {
    site_list = new WhitelistUrlSet;
    web_contents->SetUserData(kWhitelistKey, site_list);
  }

  GURL whitelisted_url(resource.is_subresource ? web_contents->GetVisibleURL()
                                               : resource.url);
  site_list->Insert(whitelisted_url);
}

// Check if the user has already ignored a SB warning for this WebContents and
// top-level domain.
bool SafeBrowsingUIManager::IsWhitelisted(const UnsafeResource& resource) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebContents* web_contents = tab_util::GetWebContentsByID(
      resource.render_process_host_id, resource.render_view_id);

  GURL maybe_whitelisted_url(
      resource.is_subresource ? web_contents->GetVisibleURL() : resource.url);
  WhitelistUrlSet* site_list =
      static_cast<WhitelistUrlSet*>(web_contents->GetUserData(kWhitelistKey));
  if (!site_list)
    return false;
  return site_list->Contains(maybe_whitelisted_url);
}
