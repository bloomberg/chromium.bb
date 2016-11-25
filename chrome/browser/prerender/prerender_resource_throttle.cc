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
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
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
  PrerenderThrottleInfo() : manager_(nullptr) {}

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
      prerender_throttle_info_(new PrerenderThrottleInfo()) {}

PrerenderResourceThrottle::~PrerenderResourceThrottle() {}

void PrerenderResourceThrottle::WillStartRequest(bool* defer) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  *defer = true;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PrerenderResourceThrottle::WillStartRequestOnUI, AsWeakPtr(),
                 request_->method(), info->GetResourceType(),
                 info->GetChildID(), info->GetRenderFrameID(), request_->url(),
                 prerender_throttle_info_));
}

void PrerenderResourceThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  *defer = true;
  std::string header;
  request_->GetResponseHeaderByName(kFollowOnlyWhenPrerenderShown, &header);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PrerenderResourceThrottle::WillRedirectRequestOnUI,
                 AsWeakPtr(), header, info->GetResourceType(), info->IsAsync(),
                 IsNoStoreResponse(*request_), info->GetChildID(),
                 info->GetRenderFrameID(), redirect_info.new_url));
}

void PrerenderResourceThrottle::WillProcessResponse(bool* defer) {
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
                 info->GetChildID(), info->GetRenderFrameID(),
                 prerender_throttle_info_));
}

const char* PrerenderResourceThrottle::GetNameForLogging() const {
  return "PrerenderResourceThrottle";
}

void PrerenderResourceThrottle::Resume() {
  controller()->Resume();
}

void PrerenderResourceThrottle::Cancel() {
  controller()->Cancel();
}

// static
void PrerenderResourceThrottle::WillStartRequestOnUI(
    const base::WeakPtr<PrerenderResourceThrottle>& throttle,
    const std::string& method,
    ResourceType resource_type,
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    scoped_refptr<PrerenderThrottleInfo> prerender_throttle_info) {
  bool cancel = false;
  PrerenderContents* prerender_contents =
      PrerenderContentsFromRenderFrame(render_process_id, render_frame_id);
  if (prerender_contents) {
    DCHECK(prerender_throttle_info);
    prerender_throttle_info->Set(prerender_contents->prerender_mode(),
                                 prerender_contents->origin(),
                                 prerender_contents->prerender_manager());

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
    } else if (!PrerenderManager::DoesSubresourceURLHaveValidScheme(url)) {
      prerender_contents->Destroy(FINAL_STATUS_UNSUPPORTED_SCHEME);
      ReportUnsupportedPrerenderScheme(url);
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
                        : &PrerenderResourceThrottle::Resume,
                 throttle));
}

// static
void PrerenderResourceThrottle::WillRedirectRequestOnUI(
    const base::WeakPtr<PrerenderResourceThrottle>& throttle,
    const std::string& follow_only_when_prerender_shown_header,
    ResourceType resource_type,
    bool async,
    bool is_no_store,
    int render_process_id,
    int render_frame_id,
    const GURL& new_url) {
  bool cancel = false;
  PrerenderContents* prerender_contents =
      PrerenderContentsFromRenderFrame(render_process_id, render_frame_id);
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
                        : &PrerenderResourceThrottle::Resume,
                 throttle));
}

// static
void PrerenderResourceThrottle::WillProcessResponseOnUI(
    bool is_main_resource,
    bool is_no_store,
    int redirect_count,
    int render_process_id,
    int render_frame_id,
    scoped_refptr<PrerenderThrottleInfo> prerender_throttle_info) {
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
PrerenderContents* PrerenderResourceThrottle::PrerenderContentsFromRenderFrame(
    int render_process_id, int render_frame_id) {
  if (g_prerender_contents_for_testing)
    return g_prerender_contents_for_testing;
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      render_process_id, render_frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  return PrerenderContents::FromWebContents(web_contents);
}

}  // namespace prerender
