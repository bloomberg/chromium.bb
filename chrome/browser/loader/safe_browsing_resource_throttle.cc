// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/safe_browsing_resource_throttle.h"

#include <iterator>
#include <utility>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing/base_ui_manager.h"
#include "components/safe_browsing_db/util.h"
#include "components/safe_browsing_db/v4_feature_list.h"
#include "components/safe_browsing_db/v4_local_database_manager.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"

using safe_browsing::BaseUIManager;

namespace {

// Destroys the prerender contents associated with the web_contents, if any.
void DestroyPrerenderContents(
    const content::ResourceRequestInfo::WebContentsGetter&
        web_contents_getter) {
  content::WebContents* web_contents = web_contents_getter.Run();
  if (web_contents) {
    prerender::PrerenderContents* prerender_contents =
        prerender::PrerenderContents::FromWebContents(web_contents);
    if (prerender_contents)
      prerender_contents->Destroy(prerender::FINAL_STATUS_SAFE_BROWSING);
  }
}

}  // namespace

// static
SafeBrowsingResourceThrottle* SafeBrowsingResourceThrottle::MaybeCreate(
    net::URLRequest* request,
    content::ResourceType resource_type,
    safe_browsing::SafeBrowsingService* sb_service) {
  if (sb_service->database_manager()->IsSupported()) {
    return new SafeBrowsingResourceThrottle(request, resource_type, sb_service);
  }
  return nullptr;
}

SafeBrowsingResourceThrottle::SafeBrowsingResourceThrottle(
    const net::URLRequest* request,
    content::ResourceType resource_type,
    safe_browsing::SafeBrowsingService* sb_service)
    : safe_browsing::BaseResourceThrottle(request,
                                          resource_type,
                                          sb_service->database_manager(),
                                          sb_service->ui_manager()) {}

SafeBrowsingResourceThrottle::~SafeBrowsingResourceThrottle() {}

const char* SafeBrowsingResourceThrottle::GetNameForLogging() const {
  return "SafeBrowsingResourceThrottle";
}

void SafeBrowsingResourceThrottle::MaybeDestroyPrerenderContents(
    const content::ResourceRequestInfo* info) {
  // Destroy the prefetch with FINAL_STATUS_SAFEBROSWING.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&DestroyPrerenderContents,
                 info->GetWebContentsGetterForRequest()));
}

void SafeBrowsingResourceThrottle::StartDisplayingBlockingPageHelper(
    security_interstitials::UnsafeResource resource) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&SafeBrowsingResourceThrottle::StartDisplayingBlockingPage,
                 AsWeakPtr(), ui_manager(), resource));
}

// Static
void SafeBrowsingResourceThrottle::StartDisplayingBlockingPage(
    const base::WeakPtr<safe_browsing::BaseResourceThrottle>& throttle,
    scoped_refptr<BaseUIManager> ui_manager,
    const security_interstitials::UnsafeResource& resource) {
  content::WebContents* web_contents = resource.web_contents_getter.Run();
  if (web_contents) {
    BaseResourceThrottle::NotifySubresourceFilterOfBlockedResource(resource);
    prerender::PrerenderContents* prerender_contents =
        prerender::PrerenderContents::FromWebContents(web_contents);
    if (prerender_contents) {
      prerender_contents->Destroy(prerender::FINAL_STATUS_SAFE_BROWSING);
    } else {
      ui_manager->DisplayBlockingPage(resource);
      return;
    }
  }

  // Tab is gone or it's being prerendered.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingResourceThrottle::Cancel, throttle));
}
