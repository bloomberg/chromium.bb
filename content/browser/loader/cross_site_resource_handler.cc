// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/cross_site_resource_handler.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/cross_site_request_manager.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/renderer_host/cross_site_transferring_request.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/url_constants.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "webkit/browser/appcache/appcache_interceptor.h"

namespace content {

namespace {

bool leak_requests_for_testing_ = false;

// The parameters to OnCrossSiteResponseHelper exceed the number of arguments
// base::Bind supports.
struct CrossSiteResponseParams {
  CrossSiteResponseParams(
      int render_view_id,
      const GlobalRequestID& global_request_id,
      bool is_transfer,
      const std::vector<GURL>& transfer_url_chain,
      const Referrer& referrer,
      PageTransition page_transition,
      int64 frame_id,
      bool should_replace_current_entry)
      : render_view_id(render_view_id),
        global_request_id(global_request_id),
        is_transfer(is_transfer),
        transfer_url_chain(transfer_url_chain),
        referrer(referrer),
        page_transition(page_transition),
        frame_id(frame_id),
        should_replace_current_entry(should_replace_current_entry) {
  }

  int render_view_id;
  GlobalRequestID global_request_id;
  bool is_transfer;
  std::vector<GURL> transfer_url_chain;
  Referrer referrer;
  PageTransition page_transition;
  int64 frame_id;
  bool should_replace_current_entry;
};

void OnCrossSiteResponseHelper(const CrossSiteResponseParams& params) {
  scoped_ptr<CrossSiteTransferringRequest> cross_site_transferring_request;
  if (params.is_transfer) {
    cross_site_transferring_request.reset(new CrossSiteTransferringRequest(
        params.global_request_id));
  }

  RenderViewHostImpl* rvh =
      RenderViewHostImpl::FromID(params.global_request_id.child_id,
                                 params.render_view_id);
  if (rvh) {
    rvh->OnCrossSiteResponse(
        params.global_request_id, cross_site_transferring_request.Pass(),
        params.transfer_url_chain, params.referrer,
        params.page_transition, params.frame_id,
        params.should_replace_current_entry);
  } else if (leak_requests_for_testing_ && cross_site_transferring_request) {
    // Some unit tests expect requests to be leaked in this case, so they can
    // pass them along manually.
    cross_site_transferring_request->ReleaseRequest();
  }
}

}  // namespace

CrossSiteResourceHandler::CrossSiteResourceHandler(
    scoped_ptr<ResourceHandler> next_handler,
    net::URLRequest* request)
    : LayeredResourceHandler(request, next_handler.Pass()),
      has_started_response_(false),
      in_cross_site_transition_(false),
      completed_during_transition_(false),
      did_defer_(false) {
}

CrossSiteResourceHandler::~CrossSiteResourceHandler() {
  // Cleanup back-pointer stored on the request info.
  GetRequestInfo()->set_cross_site_handler(NULL);
}

bool CrossSiteResourceHandler::OnRequestRedirected(
    int request_id,
    const GURL& new_url,
    ResourceResponse* response,
    bool* defer) {
  // We should not have started the transition before being redirected.
  DCHECK(!in_cross_site_transition_);
  return next_handler_->OnRequestRedirected(
      request_id, new_url, response, defer);
}

bool CrossSiteResourceHandler::OnResponseStarted(
    int request_id,
    ResourceResponse* response,
    bool* defer) {
  // At this point, we know that the response is safe to send back to the
  // renderer: it is not a download, and it has passed the SSL and safe
  // browsing checks.
  // We should not have already started the transition before now.
  DCHECK(!in_cross_site_transition_);
  has_started_response_ = true;

  ResourceRequestInfoImpl* info = GetRequestInfo();

  // We will need to swap processes if either (1) a redirect that requires a
  // transfer occurred before we got here, or (2) a pending cross-site request
  // was already in progress.  Note that a swap may no longer be needed if we
  // transferred back into the original process due to a redirect.
  bool should_transfer =
      GetContentClient()->browser()->ShouldSwapProcessesForRedirect(
          info->GetContext(), request()->original_url(), request()->url());

  // When the --site-per-process flag is passed, we transfer processes for
  // cross-site subframe navigations.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSitePerProcess)) {
    GURL referrer(request()->referrer());
    // We skip this for WebUI processes for now, since pages like the NTP host
    // cross-site WebUI iframes but don't have referrers.
    bool is_webui_process = ChildProcessSecurityPolicyImpl::GetInstance()->
        HasWebUIBindings(info->GetChildID());

    // TODO(creis): This shouldn't rely on the referrer to determine the parent
    // frame's URL.  This also doesn't work for hosted apps, due to passing NULL
    // to IsSameWebSite.  It should be possible to always send the navigation to
    // the UI thread to make a policy decision, which could let us eliminate the
    // renderer-side check in RenderViewImpl::decidePolicyForNavigation as well.
    if (info->GetResourceType() == ResourceType::SUB_FRAME &&
        !is_webui_process &&
        !SiteInstance::IsSameWebSite(NULL, request()->url(), referrer)) {
      should_transfer = true;
    }
  }

  bool swap_needed = should_transfer ||
      CrossSiteRequestManager::GetInstance()->
          HasPendingCrossSiteRequest(info->GetChildID(), info->GetRouteID());

  // If this is a download, just pass the response through without doing a
  // cross-site check.  The renderer will see it is a download and abort the
  // request.
  //
  // Similarly, HTTP 204 (No Content) responses leave us showing the previous
  // page.  We should allow the navigation to finish without running the unload
  // handler or swapping in the pending RenderViewHost.
  //
  // In both cases, any pending RenderViewHost (if one was created for this
  // navigation) will stick around until the next cross-site navigation, since
  // we are unable to tell when to destroy it.
  // See RenderFrameHostManager::RendererAbortedProvisionalLoad.
  //
  // TODO(davidben): Unify IsDownload() and is_stream(). Several places need to
  // check for both and remembering about streams is error-prone.
  if (!swap_needed || info->IsDownload() || info->is_stream() ||
      (response->head.headers.get() &&
       response->head.headers->response_code() == 204)) {
    return next_handler_->OnResponseStarted(request_id, response, defer);
  }

  // Now that we know a swap is needed and we have something to commit, we
  // pause to let the UI thread run the unload handler of the previous page
  // and set up a transfer if needed.
  StartCrossSiteTransition(request_id, response, should_transfer);

  // Defer loading until after the onunload event handler has run.
  *defer = true;
  OnDidDefer();
  return true;
}

bool CrossSiteResourceHandler::OnReadCompleted(int request_id,
                                               int bytes_read,
                                               bool* defer) {
  CHECK(!in_cross_site_transition_);
  return next_handler_->OnReadCompleted(request_id, bytes_read, defer);
}

void CrossSiteResourceHandler::OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& security_info,
    bool* defer) {
  if (!in_cross_site_transition_) {
    ResourceRequestInfoImpl* info = GetRequestInfo();
    // If we've already completed the transition, or we're canceling the
    // request, or an error occurred with no cross-process navigation in
    // progress, then we should just pass this through.
    if (has_started_response_ ||
        status.status() != net::URLRequestStatus::FAILED ||
        !CrossSiteRequestManager::GetInstance()->HasPendingCrossSiteRequest(
            info->GetChildID(), info->GetRouteID())) {
      next_handler_->OnResponseCompleted(request_id, status,
                                         security_info, defer);
      return;
    }

    // An error occurred. We should wait now for the cross-process transition,
    // so that the error message (e.g., 404) can be displayed to the user.
    // Also continue with the logic below to remember that we completed
    // during the cross-site transition.
    StartCrossSiteTransition(request_id, NULL, false);
  }

  // We have to buffer the call until after the transition completes.
  completed_during_transition_ = true;
  completed_status_ = status;
  completed_security_info_ = security_info;

  // Defer to tell RDH not to notify the world or clean up the pending request.
  // We will do so in ResumeResponse.
  *defer = true;
  OnDidDefer();
}

// We can now send the response to the new renderer, which will cause
// WebContentsImpl to swap in the new renderer and destroy the old one.
void CrossSiteResourceHandler::ResumeResponse() {
  DCHECK(request());
  DCHECK(in_cross_site_transition_);
  in_cross_site_transition_ = false;
  ResourceRequestInfoImpl* info = GetRequestInfo();

  if (has_started_response_) {
    // Send OnResponseStarted to the new renderer.
    DCHECK(response_);
    bool defer = false;
    if (!next_handler_->OnResponseStarted(info->GetRequestID(), response_.get(),
                                          &defer)) {
      controller()->Cancel();
    } else if (!defer) {
      // Unpause the request to resume reading.  Any further reads will be
      // directed toward the new renderer.
      ResumeIfDeferred();
    }
  }

  // Remove ourselves from the ExtraRequestInfo.
  info->set_cross_site_handler(NULL);

  // If the response completed during the transition, notify the next
  // event handler.
  if (completed_during_transition_) {
    bool defer = false;
    next_handler_->OnResponseCompleted(info->GetRequestID(),
                                       completed_status_,
                                       completed_security_info_,
                                       &defer);
    if (!defer)
      ResumeIfDeferred();
  }
}

// static
void CrossSiteResourceHandler::SetLeakRequestsForTesting(
    bool leak_requests_for_testing) {
  leak_requests_for_testing_ = leak_requests_for_testing;
}

// Prepare to render the cross-site response in a new RenderViewHost, by
// telling the old RenderViewHost to run its onunload handler.
void CrossSiteResourceHandler::StartCrossSiteTransition(
    int request_id,
    ResourceResponse* response,
    bool should_transfer) {
  in_cross_site_transition_ = true;
  response_ = response;

  // Store this handler on the ExtraRequestInfo, so that RDH can call our
  // ResumeResponse method when we are ready to resume.
  ResourceRequestInfoImpl* info = GetRequestInfo();
  info->set_cross_site_handler(this);

  DCHECK_EQ(request_id, info->GetRequestID());
  GlobalRequestID global_id(info->GetChildID(), info->GetRequestID());

  // Tell the contents responsible for this request that a cross-site response
  // is starting, so that it can tell its old renderer to run its onunload
  // handler now.  We will wait until the unload is finished and (if a transfer
  // is needed) for the new renderer's request to arrive.
  // The |transfer_url_chain| contains any redirect URLs that have already
  // occurred, plus the destination URL at the end.
  std::vector<GURL> transfer_url_chain;
  Referrer referrer;
  int frame_id = -1;
  if (should_transfer) {
    transfer_url_chain = request()->url_chain();
    referrer = Referrer(GURL(request()->referrer()), info->GetReferrerPolicy());
    frame_id = info->GetFrameID();

    appcache::AppCacheInterceptor::PrepareForCrossSiteTransfer(
        request(), global_id.child_id);
    ResourceDispatcherHostImpl::Get()->MarkAsTransferredNavigation(global_id);
  }
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &OnCrossSiteResponseHelper,
          CrossSiteResponseParams(info->GetRouteID(),
                                  global_id,
                                  should_transfer,
                                  transfer_url_chain,
                                  referrer,
                                  info->GetPageTransition(),
                                  frame_id,
                                  info->should_replace_current_entry())));
}

void CrossSiteResourceHandler::ResumeIfDeferred() {
  if (did_defer_) {
    request()->LogUnblocked();
    did_defer_ = false;
    controller()->Resume();
  }
}

void CrossSiteResourceHandler::OnDidDefer() {
  did_defer_ = true;
  request()->LogBlockedBy("CrossSiteResourceHandler");
}

}  // namespace content
