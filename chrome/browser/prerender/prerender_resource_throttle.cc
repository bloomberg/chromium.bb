// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_resource_throttle.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::ResourceRequestInfo;
using content::ResourceType;

namespace prerender {

namespace {
static const char kFollowOnlyWhenPrerenderShown[] =
    "follow-only-when-prerender-shown";

PrerenderContents* g_prerender_contents_for_testing;

// Returns true if the response has a "no-store" cache control header.
bool IsNoStoreResponse(const net::URLRequest& request) {
  const net::HttpResponseInfo& response_info = request.response_info();
  return response_info.headers.get() &&
         response_info.headers->HasHeaderValue("cache-control", "no-store");
}

}  // namespace

// Used to pass information between different UI thread tasks of the same
// throttle. This is reference counted as the throttler may be destroyed before
// the UI thread task has a chance to run.
//
// This class is created on the IO thread, and destroyed on the UI thread. Its
// members should only be accessed on the UI thread.
class PrerenderThrottleInfo
    : public base::RefCountedThreadSafe<PrerenderThrottleInfo,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  PrerenderThrottleInfo()
      : mode_(NO_PRERENDER), origin_(ORIGIN_NONE), manager_(nullptr) {}

  void Set(PrerenderMode mode, Origin origin, PrerenderManager* manager) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    mode_ = mode;
    origin_ = origin;
    manager_ = manager->AsWeakPtr();
  }

  PrerenderMode mode() const { return mode_; }
  Origin origin() const { return origin_; }
  base::WeakPtr<PrerenderManager> manager() const {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return manager_;
  }

 private:
  friend class base::RefCountedThreadSafe<PrerenderThrottleInfo>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<PrerenderThrottleInfo>;
  ~PrerenderThrottleInfo() {}

  PrerenderMode mode_;
  Origin origin_;
  base::WeakPtr<PrerenderManager> manager_;
};

void PrerenderResourceThrottle::OverridePrerenderContentsForTesting(
    PrerenderContents* contents) {
  g_prerender_contents_for_testing = contents;
}

PrerenderResourceThrottle::PrerenderResourceThrottle(net::URLRequest* request)
    : request_(request),
      load_flags_(net::LOAD_NORMAL),
      prerender_throttle_info_(new PrerenderThrottleInfo()) {}

PrerenderResourceThrottle::~PrerenderResourceThrottle() {}

void PrerenderResourceThrottle::WillStartRequest(bool* defer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  *defer = true;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PrerenderResourceThrottle::WillStartRequestOnUI, AsWeakPtr(),
                 request_->method(), info->GetResourceType(), request_->url(),
                 info->GetWebContentsGetterForRequest(),
                 prerender_throttle_info_));
}

void PrerenderResourceThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  *defer = true;
  std::string header;
  request_->GetResponseHeaderByName(kFollowOnlyWhenPrerenderShown, &header);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PrerenderResourceThrottle::WillRedirectRequestOnUI,
                 AsWeakPtr(), header, info->GetResourceType(), info->IsAsync(),
                 IsNoStoreResponse(*request_), redirect_info.new_url,
                 info->GetWebContentsGetterForRequest()));
}

void PrerenderResourceThrottle::WillProcessResponse(bool* defer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  if (!info)
    return;

  DCHECK_GT(request_->url_chain().size(), 0u);
  int redirect_count =
      base::saturated_cast<int>(request_->url_chain().size()) - 1;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PrerenderResourceThrottle::WillProcessResponseOnUI,
                 content::IsResourceTypeFrame(info->GetResourceType()),
                 IsNoStoreResponse(*request_), redirect_count,
                 prerender_throttle_info_));
}

const char* PrerenderResourceThrottle::GetNameForLogging() const {
  return "PrerenderResourceThrottle";
}

void PrerenderResourceThrottle::ResumeHandler() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  request_->SetLoadFlags(request_->load_flags() | load_flags_);
  Resume();
}

// static
void PrerenderResourceThrottle::WillStartRequestOnUI(
    const base::WeakPtr<PrerenderResourceThrottle>& throttle,
    const std::string& method,
    ResourceType resource_type,
    const GURL& url,
    const ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    scoped_refptr<PrerenderThrottleInfo> prerender_throttle_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool cancel = false;
  PrerenderContents* prerender_contents =
      PrerenderContentsFromGetter(web_contents_getter);
  if (prerender_contents) {
    DCHECK(prerender_throttle_info);
    prerender_throttle_info->Set(prerender_contents->prerender_mode(),
                                 prerender_contents->origin(),
                                 prerender_contents->prerender_manager());
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&PrerenderResourceThrottle::SetPrerenderMode, throttle,
                   prerender_contents->prerender_mode()));

    // Abort any prerenders that spawn requests that use unsupported HTTP
    // methods or schemes.
    if (!prerender_contents->IsValidHttpMethod(method)) {
      // If this is a full prerender, cancel the prerender in response to
      // invalid requests.  For prefetches, cancel invalid requests but keep the
      // prefetch going, unless it's the main frame that's invalid.
      if (prerender_contents->prerender_mode() == FULL_PRERENDER ||
          resource_type == content::RESOURCE_TYPE_MAIN_FRAME) {
        prerender_contents->Destroy(FINAL_STATUS_INVALID_HTTP_METHOD);
      }
      cancel = true;
    } else if (!PrerenderManager::DoesSubresourceURLHaveValidScheme(url) &&
               resource_type != content::RESOURCE_TYPE_MAIN_FRAME) {
      // For ORIGIN_OFFLINE, simply cancel this invalid request and continue.
      // For other origins, fail the prerender here.
      if (prerender_contents->origin() != ORIGIN_OFFLINE) {
        // Destroying the prerender for unsupported scheme only for non-main
        // resource to allow chrome://crash to actually crash in the
        // *RendererCrash tests instead of being intercepted here. The
        // unsupported
        // scheme for the main resource is checked in WillRedirectRequestOnUI()
        // and PrerenderContents::CheckURL(). See http://crbug.com/673771.
        prerender_contents->Destroy(FINAL_STATUS_UNSUPPORTED_SCHEME);
        ReportUnsupportedPrerenderScheme(url);
      }
      cancel = true;
#if defined(OS_ANDROID)
    } else if (resource_type == content::RESOURCE_TYPE_FAVICON) {
      // Delay icon fetching until the contents are getting swapped in
      // to conserve network usage in mobile devices.
      prerender_contents->AddResourceThrottle(throttle);
      return;
#endif
    }
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(cancel ? &PrerenderResourceThrottle::Cancel
                        : &PrerenderResourceThrottle::ResumeHandler,
                 throttle));
}

// static
void PrerenderResourceThrottle::WillRedirectRequestOnUI(
    const base::WeakPtr<PrerenderResourceThrottle>& throttle,
    const std::string& follow_only_when_prerender_shown_header,
    ResourceType resource_type,
    bool async,
    bool is_no_store,
    const GURL& new_url,
    const ResourceRequestInfo::WebContentsGetter& web_contents_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool cancel = false;
  PrerenderContents* prerender_contents =
      PrerenderContentsFromGetter(web_contents_getter);
  if (prerender_contents) {
    prerender_contents->prerender_manager()->RecordPrefetchResponseReceived(
        prerender_contents->origin(),
        content::IsResourceTypeFrame(resource_type), true /* is_redirect */,
        is_no_store);
    // Abort any prerenders with requests which redirect to invalid schemes.
    if (!PrerenderManager::DoesURLHaveValidScheme(new_url)) {
      prerender_contents->Destroy(FINAL_STATUS_UNSUPPORTED_SCHEME);
      ReportUnsupportedPrerenderScheme(new_url);
      cancel = true;
    } else if (follow_only_when_prerender_shown_header == "1" &&
               resource_type != content::RESOURCE_TYPE_MAIN_FRAME) {
      // Only defer redirects with the Follow-Only-When-Prerender-Shown
      // header. Do not defer redirects on main frame loads.
      if (!async) {
        // Cancel on deferred synchronous requests. Those will
        // indefinitely hang up a renderer process.
        prerender_contents->Destroy(FINAL_STATUS_BAD_DEFERRED_REDIRECT);
        cancel = true;
      } else {
        // Defer the redirect until the prerender is used or canceled.
        prerender_contents->AddResourceThrottle(throttle);
        return;
      }
    }
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(cancel ? &PrerenderResourceThrottle::Cancel
                        : &PrerenderResourceThrottle::ResumeHandler,
                 throttle));
}

// static
void PrerenderResourceThrottle::WillProcessResponseOnUI(
    bool is_main_resource,
    bool is_no_store,
    int redirect_count,
    scoped_refptr<PrerenderThrottleInfo> prerender_throttle_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(prerender_throttle_info);
  if (!prerender_throttle_info->manager())
    return;

  if (prerender_throttle_info->mode() != PREFETCH_ONLY)
    return;

  prerender_throttle_info->manager()->RecordPrefetchResponseReceived(
      prerender_throttle_info->origin(), is_main_resource,
      false /* is_redirect */, is_no_store);
  prerender_throttle_info->manager()->RecordPrefetchRedirectCount(
      prerender_throttle_info->origin(), is_main_resource, redirect_count);
}

// static
PrerenderContents* PrerenderResourceThrottle::PrerenderContentsFromGetter(
    const ResourceRequestInfo::WebContentsGetter& web_contents_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_prerender_contents_for_testing)
    return g_prerender_contents_for_testing;
  return PrerenderContents::FromWebContents(web_contents_getter.Run());
}

void PrerenderResourceThrottle::SetPrerenderMode(PrerenderMode mode) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  load_flags_ = (mode == PREFETCH_ONLY) ? net::LOAD_PREFETCH : net::LOAD_NORMAL;
}

}  // namespace prerender
