// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_safe_browsing_resource_throttle.h"

#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/browser/aw_safe_browsing_ui_manager.h"
#include "android_webview/browser/aw_safe_browsing_whitelist_manager.h"
#include "base/macros.h"
#include "components/safe_browsing/base_resource_throttle.h"
#include "components/safe_browsing_db/database_manager.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/resource_type.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"

namespace android_webview {

// static
const void* const AwSafeBrowsingResourceThrottle::kUserDataKey =
    &AwSafeBrowsingResourceThrottle::kUserDataKey;

// static
AwSafeBrowsingResourceThrottle* AwSafeBrowsingResourceThrottle::MaybeCreate(
    net::URLRequest* request,
    content::ResourceType resource_type,
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
    AwSafeBrowsingWhitelistManager* whitelist_manager) {
  if (database_manager->IsSupported()) {
    return new AwSafeBrowsingResourceThrottle(request, resource_type,
                                              database_manager, ui_manager,
                                              whitelist_manager);
  }
  return nullptr;
}

AwSafeBrowsingResourceThrottle::AwSafeBrowsingResourceThrottle(
    net::URLRequest* request,
    content::ResourceType resource_type,
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<AwSafeBrowsingUIManager> ui_manager,
    AwSafeBrowsingWhitelistManager* whitelist_manager)
    : safe_browsing::BaseResourceThrottle(
          request,
          resource_type,
          safe_browsing::CreateSBThreatTypeSet(
              {safe_browsing::SB_THREAT_TYPE_URL_MALWARE,
               safe_browsing::SB_THREAT_TYPE_URL_PHISHING}),
          database_manager,
          ui_manager),
      request_(request),
      whitelist_manager_(whitelist_manager) {}

AwSafeBrowsingResourceThrottle::~AwSafeBrowsingResourceThrottle() {}

bool AwSafeBrowsingResourceThrottle::CheckUrl(const GURL& gurl) {
  if (whitelist_manager_->IsURLWhitelisted(gurl)) {
    return true;
  }
  return BaseResourceThrottle::CheckUrl(gurl);
}

void AwSafeBrowsingResourceThrottle::StartDisplayingBlockingPageHelper(
    security_interstitials::UnsafeResource resource) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&AwSafeBrowsingResourceThrottle::StartApplicationResponse,
                 AsWeakPtr(), ui_manager(), resource,
                 AwWebResourceRequest(*request_)));
}

// static
void AwSafeBrowsingResourceThrottle::StartApplicationResponse(
    const base::WeakPtr<BaseResourceThrottle>& throttle,
    scoped_refptr<safe_browsing::BaseUIManager> ui_manager,
    const security_interstitials::UnsafeResource& resource,
    const AwWebResourceRequest& request) {
  content::WebContents* web_contents = resource.web_contents_getter.Run();
  AwContentsClientBridge* client =
      AwContentsClientBridge::FromWebContents(web_contents);

  if (client) {
    base::Callback<void(SafeBrowsingAction, bool)> callback =
        base::Bind(&AwSafeBrowsingResourceThrottle::DoApplicationResponse,
                   throttle, ui_manager, resource);

    client->OnSafeBrowsingHit(request, resource.threat_type, callback);
  }
}

// static
void AwSafeBrowsingResourceThrottle::DoApplicationResponse(
    const base::WeakPtr<BaseResourceThrottle>& throttle,
    scoped_refptr<safe_browsing::BaseUIManager> ui_manager,
    const security_interstitials::UnsafeResource& resource,
    SafeBrowsingAction action,
    bool reporting) {
  if (!reporting) {
    AwSafeBrowsingUIManager* aw_ui_manager =
        static_cast<AwSafeBrowsingUIManager*>(ui_manager.get());
    aw_ui_manager->SetExtendedReportingAllowed(false);
  }
  // TODO(ntfschr): fully handle reporting once we add support (crbug/688629)
  bool proceed;
  switch (action) {
    case SafeBrowsingAction::SHOW_INTERSTITIAL:
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::Bind(&BaseResourceThrottle::StartDisplayingBlockingPage,
                     throttle, ui_manager, resource));
      return;
    case SafeBrowsingAction::PROCEED:
      proceed = true;
      break;
    case SafeBrowsingAction::BACK_TO_SAFETY:
      proceed = false;
      break;
    default:
      NOTREACHED();
  }

  content::WebContents* web_contents = resource.web_contents_getter.Run();
  content::NavigationEntry* entry = resource.GetNavigationEntryForResource();
  GURL main_frame_url = entry ? entry->GetURL() : GURL();

  // Navigate back for back-to-safety on subresources
  if (!proceed && resource.is_subframe) {
    if (web_contents->GetController().CanGoBack()) {
      web_contents->GetController().GoBack();
    } else {
      web_contents->GetController().LoadURL(
          ui_manager->default_safe_page(), content::Referrer(),
          ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
    }
  }

  ui_manager->OnBlockingPageDone(
      std::vector<security_interstitials::UnsafeResource>{resource}, proceed,
      web_contents, main_frame_url);
}

void AwSafeBrowsingResourceThrottle::CancelResourceLoad() {
  request_->SetUserData(kUserDataKey,
                        base::MakeUnique<base::SupportsUserData::Data>());
  Cancel();
}

}  // namespace android_webview
