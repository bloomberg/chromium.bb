// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/safe_browsing/ui_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/debug/leak_tracker.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/safe_browsing/metadata.pb.h"
#include "ios/chrome/browser/safe_browsing/ping_manager.h"
#include "ios/chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "ios/chrome/browser/safe_browsing/safe_browsing_service.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_thread.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

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

web::NavigationItem* GetActiveItemForNavigationManager(
    web::NavigationManager* navigation_manager) {
  web::NavigationItem* active_item = navigation_manager->GetTransientItem();
  if (!active_item)
    active_item = navigation_manager->GetPendingItem();
  if (!active_item)
    active_item = navigation_manager->GetLastCommittedItem();
  return active_item;
}

}  // namespace

namespace safe_browsing {

// SafeBrowsingUIManager::UnsafeResource ---------------------------------------

SafeBrowsingUIManager::UnsafeResource::UnsafeResource()
    : is_subresource(false),
      threat_type(SB_THREAT_TYPE_SAFE),
      threat_source(safe_browsing::ThreatSource::UNKNOWN) {}

SafeBrowsingUIManager::UnsafeResource::~UnsafeResource() {}

bool SafeBrowsingUIManager::UnsafeResource::IsMainPageLoadBlocked() const {
  // Subresource hits cannot happen until after main page load is committed.
  if (is_subresource)
    return false;

  // Client-side phishing detection interstitials never block the main frame
  // load, since they happen after the page is finished loading.
  if (threat_type == SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL ||
      threat_type == SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL) {
    return false;
  }

  return true;
}

// SafeBrowsingUIManager -------------------------------------------------------

SafeBrowsingUIManager::SafeBrowsingUIManager(
    const scoped_refptr<SafeBrowsingService>& service)
    : sb_service_(service) {}

SafeBrowsingUIManager::~SafeBrowsingUIManager() {}

void SafeBrowsingUIManager::StopOnIOThread(bool shutdown) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);

  if (shutdown)
    sb_service_ = nullptr;
}

void SafeBrowsingUIManager::LogPauseDelay(base::TimeDelta time) {
  UMA_HISTOGRAM_LONG_TIMES("SB2.Delay", time);
}

void SafeBrowsingUIManager::OnBlockingPageDone(
    const std::vector<UnsafeResource>& resources,
    bool proceed) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  for (const auto& resource : resources) {
    if (!resource.callback.is_null()) {
      DCHECK(resource.callback_thread);
      resource.callback_thread->PostTask(
          FROM_HERE, base::Bind(resource.callback, proceed));
    }

    if (proceed)
      AddToWhitelist(resource);
  }
}

void SafeBrowsingUIManager::DisplayBlockingPage(
    const UnsafeResource& resource) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  if (resource.is_subresource && !resource.is_subframe) {
    // Sites tagged as serving Unwanted Software should only show a warning for
    // main-frame or sub-frame resource. Similar warning restrictions should be
    // applied to malware sites tagged as "landing sites" (see "Types of
    // Malware sites" under
    // https://developers.google.com/safe-browsing/developers_guide_v3#UserWarnings).
    MalwarePatternType proto;
    if (resource.threat_type == SB_THREAT_TYPE_URL_UNWANTED ||
        (resource.threat_type == SB_THREAT_TYPE_URL_MALWARE &&
         !resource.threat_metadata.empty() &&
         proto.ParseFromString(resource.threat_metadata) &&
         proto.pattern_type() == MalwarePatternType::LANDING)) {
      if (!resource.callback.is_null()) {
        DCHECK(resource.callback_thread);
        resource.callback_thread->PostTask(FROM_HERE,
                                           base::Bind(resource.callback, true));
      }

      return;
    }
  }

  // The tab might have been closed. If it was closed, just act as if "Don't
  // Proceed" had been chosen.
  web::WebState* web_state = resource.weak_web_state.get();
  if (!web_state) {
    std::vector<UnsafeResource> resources;
    resources.push_back(resource);
    OnBlockingPageDone(resources, false);
    return;
  }

  // Check if the user has already ignored a SB warning for the same WebState
  // and top-level domain.
  if (IsWhitelisted(resource)) {
    if (!resource.callback.is_null()) {
      DCHECK(resource.callback_thread);
      resource.callback_thread->PostTask(FROM_HERE,
                                         base::Bind(resource.callback, true));
    }
    return;
  }

  if (resource.threat_type != SB_THREAT_TYPE_SAFE) {
    HitReport hit_report;
    hit_report.malicious_url = resource.url;
    hit_report.page_url = web_state->GetVisibleURL();
    hit_report.is_subresource = resource.is_subresource;
    hit_report.threat_type = resource.threat_type;
    hit_report.threat_source = resource.threat_source;

    web::NavigationItem* item =
        GetActiveItemForNavigationManager(web_state->GetNavigationManager());
    if (item) {
      hit_report.referrer_url = item->GetReferrer().url;
    }

    // When the malicious url is on the main frame, and resource.original_url
    // is not the same as the resource.url, that means we have a redirect from
    // resource.original_url to resource.url.
    // Also, at this point, page_url points to the _previous_ page that we
    // were on. We replace page_url with resource.original_url and referrer
    // with page_url.
    if (!resource.is_subresource && !resource.original_url.is_empty() &&
        resource.original_url != resource.url) {
      hit_report.referrer_url = hit_report.page_url;
      hit_report.page_url = resource.original_url;
    }

    ios::ChromeBrowserState* browser_state =
        ios::ChromeBrowserState::FromBrowserState(web_state->GetBrowserState());
    hit_report.is_extended_reporting =
        browser_state &&
        browser_state->GetPrefs()->GetBoolean(
            prefs::kSafeBrowsingExtendedReportingEnabled);
    hit_report.is_metrics_reporting_active =
        safe_browsing::IsMetricsReportingActive();

    MaybeReportSafeBrowsingHit(hit_report);
  }

  if (resource.threat_type != SB_THREAT_TYPE_SAFE) {
    FOR_EACH_OBSERVER(Observer, observer_list_, OnSafeBrowsingHit(resource));
  }
  SafeBrowsingBlockingPage::ShowBlockingPage(web_state, this, resource);
}

// A safebrowsing hit is sent after a blocking page for malware/phishing
// or after the warning dialog for download urls, only for
// UMA || extended_reporting users.
void SafeBrowsingUIManager::MaybeReportSafeBrowsingHit(
    const HitReport& hit_report) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);

  // Decide if we should send this report.
  if (hit_report.is_metrics_reporting_active ||
      hit_report.is_extended_reporting) {
    web::WebThread::PostTask(
        web::WebThread::IO, FROM_HERE,
        base::Bind(&SafeBrowsingUIManager::ReportSafeBrowsingHitOnIOThread,
                   this, hit_report));
  }
}

void SafeBrowsingUIManager::ReportSafeBrowsingHitOnIOThread(
    const HitReport& hit_report) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);

  // The service may delete the ping manager (i.e. when user disabling service,
  // etc). This happens on the IO thread.
  if (!sb_service_ || !sb_service_->ping_manager())
    return;

  DVLOG(1) << "ReportSafeBrowsingHit: " << hit_report.malicious_url << " "
           << hit_report.page_url << " " << hit_report.referrer_url << " "
           << hit_report.is_subresource << " " << hit_report.threat_type;
  sb_service_->ping_manager()->ReportSafeBrowsingHit(hit_report);
}

void SafeBrowsingUIManager::ReportInvalidCertificateChain(
    const std::string& serialized_report,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  web::WebThread::PostTaskAndReply(
      web::WebThread::IO, FROM_HERE,
      base::Bind(
          &SafeBrowsingUIManager::ReportInvalidCertificateChainOnIOThread, this,
          serialized_report),
      callback);
}

void SafeBrowsingUIManager::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  observer_list_.AddObserver(observer);
}

void SafeBrowsingUIManager::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  observer_list_.RemoveObserver(observer);
}

void SafeBrowsingUIManager::ReportInvalidCertificateChainOnIOThread(
    const std::string& serialized_report) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);

  // The service may delete the ping manager (i.e. when user disabling service,
  // etc). This happens on the IO thread.
  if (!sb_service_ || !sb_service_->ping_manager())
    return;

  sb_service_->ping_manager()->ReportInvalidCertificateChain(serialized_report);
}

// If the user had opted-in to send ThreatDetails, this gets called
// when the report is ready.
void SafeBrowsingUIManager::SendSerializedThreatDetails(
    const std::string& serialized) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);

  // The service may delete the ping manager (i.e. when user disabling service,
  // etc). This happens on the IO thread.
  if (sb_service_.get() == nullptr || sb_service_->ping_manager() == nullptr)
    return;

  if (!serialized.empty()) {
    DVLOG(1) << "Sending serialized threat details.";
    sb_service_->ping_manager()->ReportThreatDetails(serialized);
  }
}

// Whitelist this domain in the current WebState. Either add the
// domain to an existing WhitelistUrlSet, or create a new WhitelistUrlSet.
void SafeBrowsingUIManager::AddToWhitelist(const UnsafeResource& resource) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  DCHECK(resource.weak_web_state.get());

  web::WebState* web_state = resource.weak_web_state.get();
  WhitelistUrlSet* site_list =
      static_cast<WhitelistUrlSet*>(web_state->GetUserData(kWhitelistKey));
  if (!site_list) {
    site_list = new WhitelistUrlSet;
    web_state->SetUserData(kWhitelistKey, site_list);
  }

  GURL whitelisted_url(resource.is_subresource ? web_state->GetVisibleURL()
                                               : resource.url);
  site_list->Insert(whitelisted_url);
}

// Check if the user has already ignored a SB warning for this WebState and
// top-level domain.
bool SafeBrowsingUIManager::IsWhitelisted(const UnsafeResource& resource) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  DCHECK(resource.weak_web_state.get());

  web::WebState* web_state = resource.weak_web_state.get();
  GURL maybe_whitelisted_url(
      resource.is_subresource ? web_state->GetVisibleURL() : resource.url);
  WhitelistUrlSet* site_list =
      static_cast<WhitelistUrlSet*>(web_state->GetUserData(kWhitelistKey));
  if (!site_list)
    return false;
  return site_list->Contains(maybe_whitelisted_url);
}

}  // namespace safe_browsing
