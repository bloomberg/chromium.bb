// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_interception/intercept_navigation_resource_throttle.h"

#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "ui/base/page_transition_types.h"

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using ui::PageTransition;
using content::Referrer;
using content::RenderProcessHost;
using content::ResourceRequestInfo;

namespace navigation_interception {

namespace {

void CheckIfShouldIgnoreNavigationOnUIThread(
    int render_process_id,
    int render_frame_id,
    const NavigationParams& navigation_params,
    InterceptNavigationResourceThrottle::CheckOnUIThreadCallback
    should_ignore_callback,
    base::Callback<void(bool)> callback) {
  bool should_ignore_navigation = false;
  RenderProcessHost* rph = RenderProcessHost::FromID(render_process_id);
  if (rph) {
    NavigationParams validated_params(navigation_params);
    rph->FilterURL(false, &validated_params.url());

    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromID(render_process_id, render_frame_id);
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);

    if (web_contents) {
      should_ignore_navigation = should_ignore_callback.Run(web_contents,
                                                            validated_params);
    }
  }

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, should_ignore_navigation));
}

} // namespace

InterceptNavigationResourceThrottle::InterceptNavigationResourceThrottle(
    net::URLRequest* request,
    CheckOnUIThreadCallback should_ignore_callback)
    : request_(request),
      should_ignore_callback_(should_ignore_callback),
      weak_ptr_factory_(this) {
}

InterceptNavigationResourceThrottle::~InterceptNavigationResourceThrottle() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void InterceptNavigationResourceThrottle::WillStartRequest(bool* defer) {
  *defer =
      CheckIfShouldIgnoreNavigation(request_->url(), request_->method(), false);
}

void InterceptNavigationResourceThrottle::WillRedirectRequest(
    const GURL& new_url,
    bool* defer) {
  *defer =
      CheckIfShouldIgnoreNavigation(new_url, GetMethodAfterRedirect(), true);
}

const char* InterceptNavigationResourceThrottle::GetNameForLogging() const {
  return "InterceptNavigationResourceThrottle";
}

std::string InterceptNavigationResourceThrottle::GetMethodAfterRedirect() {
  net::HttpResponseHeaders* headers = request_->response_headers();
  if (!headers)
    return request_->method();
  // TODO(davidben): Plumb net::RedirectInfo through content::ResourceThrottle
  // and unexpose net::URLRequest::ComputeMethodForRedirect.
  return net::URLRequest::ComputeMethodForRedirect(
             request_->method(), headers->response_code());
}

bool InterceptNavigationResourceThrottle::CheckIfShouldIgnoreNavigation(
    const GURL& url,
    const std::string& method,
    bool is_redirect) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request_);
  if (!info)
    return false;

  int render_process_id, render_frame_id;
  if (!info->GetAssociatedRenderFrame(&render_process_id, &render_frame_id))
    return false;

  NavigationParams navigation_params(url,
                                     Referrer(GURL(request_->referrer()),
                                              info->GetReferrerPolicy()),
                                     info->HasUserGesture(),
                                     method == "POST",
                                     info->GetPageTransition(),
                                     is_redirect);

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &CheckIfShouldIgnoreNavigationOnUIThread,
          render_process_id,
          render_frame_id,
          navigation_params,
          should_ignore_callback_,
          base::Bind(
              &InterceptNavigationResourceThrottle::OnResultObtained,
              weak_ptr_factory_.GetWeakPtr())));

  // Defer request while we wait for the UI thread to check if the navigation
  // should be ignored.
  return true;
}

void InterceptNavigationResourceThrottle::OnResultObtained(
    bool should_ignore_navigation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (should_ignore_navigation) {
    controller()->CancelAndIgnore();
  } else {
    controller()->Resume();
  }
}

}  // namespace navigation_interception
