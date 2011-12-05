// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/transfer_navigation_resource_handler.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "content/public/common/referrer.h"

namespace {

// This code is borrowed from ChromeContentRendererClient. We want to mimic
// the logic used by the renderer.
// TODO(mpcomplete): move to common and share with the renderer logic.
// http://crbug.com/79520
const Extension* GetNonBookmarkAppExtension(
    const ExtensionSet& extensions, const GURL& url) {
  // Exclude bookmark apps, which do not use the app process model.
  const Extension* extension = extensions.GetByURL(ExtensionURLInfo(url));
  if (extension && extension->from_bookmark())
    extension = NULL;
  return extension;
}

bool CrossesExtensionExtents(
    const ExtensionSet& extensions,
    const GURL& old_url,
    const GURL& new_url) {
  const Extension* new_url_extension = GetNonBookmarkAppExtension(extensions,
                                                                  new_url);
  const Extension* old_url_extension = GetNonBookmarkAppExtension(extensions,
                                                                  old_url);

  // TODO(creis): Temporary workaround for crbug.com/59285: Only return true if
  // we would enter an extension app's extent from a non-app, or if we leave an
  // extension with no web extent.  We avoid swapping processes to exit a hosted
  // app for now, since we do not yet support postMessage calls from outside the
  // app back into it (e.g., as in Facebook OAuth 2.0).
  bool old_url_is_hosted_app = old_url_extension &&
      !old_url_extension->web_extent().is_empty();
  if (old_url_is_hosted_app)
    return false;

  return old_url_extension != new_url_extension;
}

void RequestTransferURLOnUIThread(int render_process_id,
                                  int render_view_id,
                                  const GURL& new_url,
                                  const content::Referrer& referrer,
                                  WindowOpenDisposition window_open_disposition,
                                  int64 frame_id,
                                  const GlobalRequestID& request_id) {
  RenderViewHost* rvh = RenderViewHost::FromID(render_process_id,
                                               render_view_id);
  if (!rvh)
    return;

  RenderViewHostDelegate* delegate = rvh->delegate();
  if (!delegate)
    return;

  delegate->RequestTransferURL(
      new_url, referrer,
      window_open_disposition, frame_id, request_id);
}

}  // namespace

TransferNavigationResourceHandler::TransferNavigationResourceHandler(
    ResourceHandler* handler,
    ResourceDispatcherHost* resource_dispatcher_host,
    net::URLRequest* request)
    : next_handler_(handler),
      rdh_(resource_dispatcher_host),
      request_(request) {
}

TransferNavigationResourceHandler::~TransferNavigationResourceHandler() {
}

bool TransferNavigationResourceHandler::OnUploadProgress(int request_id,
                                                         uint64 position,
                                                         uint64 size) {
  return next_handler_->OnUploadProgress(request_id, position, size);
}

bool TransferNavigationResourceHandler::OnRequestRedirected(
    int request_id,
    const GURL& new_url,
    content::ResourceResponse* response,
    bool* defer) {

  ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request_);

  // If a toplevel request is redirecting across extension extents, we want to
  // switch processes. We do this by deferring the redirect and resuming the
  // request once the navigation controller properly assigns the right process
  // to host the new URL.
  // TODO(mpcomplete): handle for cases other than extensions (e.g. WebUI).
  const content::ResourceContext& resource_context = *info->context();
  ProfileIOData* io_data =
      reinterpret_cast<ProfileIOData*>(resource_context.GetUserData(NULL));

  if (info->resource_type() == ResourceType::MAIN_FRAME &&
      CrossesExtensionExtents(io_data->GetExtensionInfoMap()->extensions(),
                              request_->url(), new_url)) {
    int render_process_id, render_view_id;
    if (ResourceDispatcherHost::RenderViewForRequest(
            request_, &render_process_id, &render_view_id)) {

      GlobalRequestID global_id(info->child_id(), info->request_id());
      rdh_->MarkAsTransferredNavigation(global_id, request_);

      content::BrowserThread::PostTask(
          content::BrowserThread::UI,
          FROM_HERE,
          base::Bind(&RequestTransferURLOnUIThread,
              render_process_id, render_view_id,
              new_url,
              content::Referrer(GURL(request_->referrer()),
                                info->referrer_policy()),
              CURRENT_TAB, info->frame_id(), global_id));

      *defer = true;
      return true;
    }
  }

  return next_handler_->OnRequestRedirected(
      request_id, new_url, response, defer);
}

bool TransferNavigationResourceHandler::OnResponseStarted(
    int request_id, content::ResourceResponse* response) {
  return next_handler_->OnResponseStarted(request_id, response);
}

bool TransferNavigationResourceHandler::OnWillStart(int request_id,
                                                    const GURL& url,
                                                    bool* defer) {
  return next_handler_->OnWillStart(request_id, url, defer);
}

bool TransferNavigationResourceHandler::OnWillRead(int request_id,
                                                   net::IOBuffer** buf,
                                                   int* buf_size,
                                                   int min_size) {
  return next_handler_->OnWillRead(request_id, buf, buf_size, min_size);
}

bool TransferNavigationResourceHandler::OnReadCompleted(int request_id,
                                                        int* bytes_read) {
  return next_handler_->OnReadCompleted(request_id, bytes_read);
}

bool TransferNavigationResourceHandler::OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& security_info) {
  return next_handler_->OnResponseCompleted(request_id, status, security_info);
}

void TransferNavigationResourceHandler::OnRequestClosed() {
  next_handler_->OnRequestClosed();
}
