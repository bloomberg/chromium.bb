// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "content/browser/renderer_host/cross_site_resource_handler.h"

#include "base/logging.h"
#include "content/browser/renderer_host/global_request_id.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_notification_task.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"

CrossSiteResourceHandler::CrossSiteResourceHandler(
    ResourceHandler* handler,
    int render_process_host_id,
    int render_view_id,
    ResourceDispatcherHost* resource_dispatcher_host)
    : next_handler_(handler),
      render_process_host_id_(render_process_host_id),
      render_view_id_(render_view_id),
      has_started_response_(false),
      in_cross_site_transition_(false),
      request_id_(-1),
      completed_during_transition_(false),
      completed_status_(),
      response_(NULL),
      rdh_(resource_dispatcher_host) {}

bool CrossSiteResourceHandler::OnUploadProgress(int request_id,
                                                uint64 position,
                                                uint64 size) {
  return next_handler_->OnUploadProgress(request_id, position, size);
}

bool CrossSiteResourceHandler::OnRequestRedirected(
    int request_id,
    const GURL& new_url,
    content::ResourceResponse* response,
    bool* defer) {
  // We should not have started the transition before being redirected.
  DCHECK(!in_cross_site_transition_);
  return next_handler_->OnRequestRedirected(
      request_id, new_url, response, defer);
}

bool CrossSiteResourceHandler::OnResponseStarted(
    int request_id,
    content::ResourceResponse* response) {
  // At this point, we know that the response is safe to send back to the
  // renderer: it is not a download, and it has passed the SSL and safe
  // browsing checks.
  // We should not have already started the transition before now.
  DCHECK(!in_cross_site_transition_);
  has_started_response_ = true;

  // Look up the request and associated info.
  GlobalRequestID global_id(render_process_host_id_, request_id);
  net::URLRequest* request = rdh_->GetURLRequest(global_id);
  if (!request) {
    DLOG(WARNING) << "Request wasn't found";
    return false;
  }
  ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);

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
      (response->headers && response->headers->response_code() == 204)) {
    return next_handler_->OnResponseStarted(request_id, response);
  }

  // Tell the renderer to run the onunload event handler, and wait for the
  // reply.
  StartCrossSiteTransition(request_id, response, global_id);
  return true;
}

bool CrossSiteResourceHandler::OnWillStart(int request_id,
                                           const GURL& url,
                                           bool* defer) {
  return next_handler_->OnWillStart(request_id, url, defer);
}

bool CrossSiteResourceHandler::OnWillRead(int request_id, net::IOBuffer** buf,
                                          int* buf_size, int min_size) {
  return next_handler_->OnWillRead(request_id, buf, buf_size, min_size);
}

bool CrossSiteResourceHandler::OnReadCompleted(int request_id,
                                               int* bytes_read) {
  if (!in_cross_site_transition_) {
    return next_handler_->OnReadCompleted(request_id, bytes_read);
  }
  return true;
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
    } else {
      // An error occured, we should wait now for the cross-site transition,
      // so that the error message (e.g., 404) can be displayed to the user.
      // Also continue with the logic below to remember that we completed
      // during the cross-site transition.
      GlobalRequestID global_id(render_process_host_id_, request_id);
      StartCrossSiteTransition(request_id, NULL, global_id);
    }
  }

  // We have to buffer the call until after the transition completes.
  completed_during_transition_ = true;
  completed_status_ = status;
  completed_security_info_ = security_info;

  // Return false to tell RDH not to notify the world or clean up the
  // pending request.  We will do so in ResumeResponse.
  return false;
}

void CrossSiteResourceHandler::OnRequestClosed() {
  next_handler_->OnRequestClosed();
}

// We can now send the response to the new renderer, which will cause
// TabContents to swap in the new renderer and destroy the old one.
void CrossSiteResourceHandler::ResumeResponse() {
  DCHECK(request_id_ != -1);
  DCHECK(in_cross_site_transition_);
  in_cross_site_transition_ = false;

  // Find the request for this response.
  GlobalRequestID global_id(render_process_host_id_, request_id_);
  net::URLRequest* request = rdh_->GetURLRequest(global_id);
  if (!request) {
    DLOG(WARNING) << "Resuming a request that wasn't found";
    return;
  }

  if (has_started_response_) {
    // Send OnResponseStarted to the new renderer.
    DCHECK(response_);
    next_handler_->OnResponseStarted(request_id_, response_);

    // Unpause the request to resume reading.  Any further reads will be
    // directed toward the new renderer.
    rdh_->PauseRequest(render_process_host_id_, request_id_, false);
  }

  // Remove ourselves from the ExtraRequestInfo.
  ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);
  info->set_cross_site_handler(NULL);

  // If the response completed during the transition, notify the next
  // event handler.
  if (completed_during_transition_) {
    next_handler_->OnResponseCompleted(request_id_, completed_status_,
                                       completed_security_info_);
    rdh_->RemovePendingRequest(render_process_host_id_, request_id_);
  }
}

CrossSiteResourceHandler::~CrossSiteResourceHandler() {}

// Prepare to render the cross-site response in a new RenderViewHost, by
// telling the old RenderViewHost to run its onunload handler.
void CrossSiteResourceHandler::StartCrossSiteTransition(
    int request_id,
    content::ResourceResponse* response,
    const GlobalRequestID& global_id) {
  in_cross_site_transition_ = true;
  request_id_ = request_id;
  response_ = response;

  // Store this handler on the ExtraRequestInfo, so that RDH can call our
  // ResumeResponse method when the close ACK is received.
  net::URLRequest* request = rdh_->GetURLRequest(global_id);
  if (!request) {
    DLOG(WARNING) << "Cross site response for a request that wasn't found";
    return;
  }
  ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);
  info->set_cross_site_handler(this);

  if (has_started_response_) {
    // Pause the request until the old renderer is finished and the new
    // renderer is ready.
    rdh_->PauseRequest(render_process_host_id_, request_id, true);
  }
  // If our OnResponseStarted wasn't called, then we're being called by
  // OnResponseCompleted after a failure.  We don't need to pause, because
  // there will be no reads.

  // Tell the tab responsible for this request that a cross-site response is
  // starting, so that it can tell its old renderer to run its onunload
  // handler now.  We will wait to hear the corresponding ClosePage_ACK.
  CallRenderViewHostRendererManagementDelegate(
      render_process_host_id_, render_view_id_,
      &RenderViewHostDelegate::RendererManagement::OnCrossSiteResponse,
      render_process_host_id_, request_id);

  // TODO(creis): If the above call should fail, then we need to notify the IO
  // thread to proceed anyway, using ResourceDispatcherHost::OnClosePageACK.
}
