// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/cross_site_resource_handler.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/common/resource_response.h"
#include "net/http/http_response_headers.h"

namespace content {

namespace {

void OnCrossSiteResponseHelper(int render_process_id,
                               int render_view_id,
                               int request_id) {
  RenderViewHostImpl* rvh = RenderViewHostImpl::FromID(render_process_id,
                                                       render_view_id);
  if (rvh && rvh->GetDelegate()->GetRendererManagementDelegate()) {
    rvh->GetDelegate()->GetRendererManagementDelegate()->OnCrossSiteResponse(
        render_process_id, request_id);
  }
}

}  // namespace

CrossSiteResourceHandler::CrossSiteResourceHandler(
    scoped_ptr<ResourceHandler> next_handler,
    int render_process_host_id,
    int render_view_id,
    net::URLRequest* request)
    : LayeredResourceHandler(next_handler.Pass()),
      render_process_host_id_(render_process_host_id),
      render_view_id_(render_view_id),
      request_(request),
      has_started_response_(false),
      in_cross_site_transition_(false),
      request_id_(-1),
      completed_during_transition_(false),
      did_defer_(false),
      completed_status_(),
      response_(NULL) {
}

CrossSiteResourceHandler::~CrossSiteResourceHandler() {
  // Cleanup back-pointer stored on the request info.
  ResourceRequestInfoImpl::ForRequest(request_)->set_cross_site_handler(NULL);
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

  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(request_);

  // If this is a download, just pass the response through without doing a
  // cross-site check.  The renderer will see it is a download and abort the
  // request.
  //
  // Similarly, HTTP 204 (No Content) responses leave us showing the previous
  // page.  We should allow the navigation to finish without running the unload
  // handler or swapping in the pending RenderViewHost.
  //
  // In both cases, the pending RenderViewHost will stick around until the next
  // cross-site navigation, since we are unable to tell when to destroy it.
  // See RenderViewHostManager::RendererAbortedProvisionalLoad.
  if (info->is_download() ||
      (response->head.headers &&
       response->head.headers->response_code() == 204)) {
    return next_handler_->OnResponseStarted(request_id, response, defer);
  }

  // Tell the renderer to run the onunload event handler.
  StartCrossSiteTransition(request_id, response);

  // Defer loading until after the onunload event handler has run.
  did_defer_ = *defer = true;
  return true;
}

bool CrossSiteResourceHandler::OnReadCompleted(int request_id,
                                               int bytes_read,
                                               bool* defer) {
  CHECK(!in_cross_site_transition_);
  return next_handler_->OnReadCompleted(request_id, bytes_read, defer);
}

bool CrossSiteResourceHandler::OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& security_info) {
  if (!in_cross_site_transition_) {
    if (has_started_response_ ||
        status.status() != net::URLRequestStatus::FAILED) {
      // We've already completed the transition or we're canceling the request,
      // so just pass it through.
      return next_handler_->OnResponseCompleted(request_id, status,
                                                security_info);
    }

    // An error occured, we should wait now for the cross-site transition,
    // so that the error message (e.g., 404) can be displayed to the user.
    // Also continue with the logic below to remember that we completed
    // during the cross-site transition.
    StartCrossSiteTransition(request_id, NULL);
  }

  // We have to buffer the call until after the transition completes.
  completed_during_transition_ = true;
  completed_status_ = status;
  completed_security_info_ = security_info;

  // Return false to tell RDH not to notify the world or clean up the
  // pending request.  We will do so in ResumeResponse.
  did_defer_ = true;
  return false;
}

// We can now send the response to the new renderer, which will cause
// WebContentsImpl to swap in the new renderer and destroy the old one.
void CrossSiteResourceHandler::ResumeResponse() {
  DCHECK(request_id_ != -1);
  DCHECK(in_cross_site_transition_);
  in_cross_site_transition_ = false;

  if (has_started_response_) {
    // Send OnResponseStarted to the new renderer.
    DCHECK(response_);
    bool defer = false;
    if (!next_handler_->OnResponseStarted(request_id_, response_, &defer)) {
      controller()->Cancel();
    } else if (!defer) {
      // Unpause the request to resume reading.  Any further reads will be
      // directed toward the new renderer.
      ResumeIfDeferred();
    }
  }

  // Remove ourselves from the ExtraRequestInfo.
  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request_);
  info->set_cross_site_handler(NULL);

  // If the response completed during the transition, notify the next
  // event handler.
  if (completed_during_transition_) {
    if (next_handler_->OnResponseCompleted(request_id_, completed_status_,
                                           completed_security_info_)) {
      ResumeIfDeferred();
    }
  }
}

// Prepare to render the cross-site response in a new RenderViewHost, by
// telling the old RenderViewHost to run its onunload handler.
void CrossSiteResourceHandler::StartCrossSiteTransition(
    int request_id,
    ResourceResponse* response) {
  in_cross_site_transition_ = true;
  request_id_ = request_id;
  response_ = response;

  // Store this handler on the ExtraRequestInfo, so that RDH can call our
  // ResumeResponse method when the close ACK is received.
  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request_);
  info->set_cross_site_handler(this);

  // Tell the contents responsible for this request that a cross-site response
  // is starting, so that it can tell its old renderer to run its onunload
  // handler now.  We will wait to hear the corresponding ClosePage_ACK.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &OnCrossSiteResponseHelper,
          render_process_host_id_,
          render_view_id_,
          request_id));

  // TODO(creis): If the above call should fail, then we need to notify the IO
  // thread to proceed anyway, using ResourceDispatcherHost::OnClosePageACK.
}

void CrossSiteResourceHandler::ResumeIfDeferred() {
  if (did_defer_) {
    did_defer_ = false;
    controller()->Resume();
  }
}

}  // namespace content
