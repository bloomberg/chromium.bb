// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_interception/intercept_navigation_resource_throttle.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using content::PageTransition;
using content::Referrer;
using content::RenderViewHost;
using content::ResourceRequestInfo;

namespace components {

namespace {

struct ShouldIgnoreCallbackParams {
  int render_process_id;
  int render_view_id;
  GURL url;
  Referrer referrer;
  bool has_user_gesture;
  bool is_post;
  PageTransition transition_type;
};

void CheckIfShouldIgnoreNavigationOnUIThread(
    const ShouldIgnoreCallbackParams& params,
    InterceptNavigationResourceThrottle::CheckOnUIThreadCallback
    should_ignore_callback,
    base::Callback<void(bool)> callback) {

  bool should_ignore_navigation = false;
  RenderViewHost* rvh =
      RenderViewHost::FromID(params.render_process_id, params.render_view_id);

  if (rvh) {
    GURL validated_url(params.url);
    RenderViewHost::FilterURL(rvh->GetProcess(), false, &validated_url);

    should_ignore_navigation = should_ignore_callback.Run(
        rvh,
        validated_url,
        params.referrer,
        params.is_post,
        params.has_user_gesture,
        params.transition_type);
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
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

InterceptNavigationResourceThrottle::~InterceptNavigationResourceThrottle() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void InterceptNavigationResourceThrottle::WillStartRequest(bool* defer) {
  *defer = CheckIfShouldIgnoreNavigation(request_->url());
}

void InterceptNavigationResourceThrottle::WillRedirectRequest(
    const GURL& new_url,
    bool* defer) {
  *defer = CheckIfShouldIgnoreNavigation(new_url);
}

bool InterceptNavigationResourceThrottle::CheckIfShouldIgnoreNavigation(
    const GURL& url) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request_);
  if (!info)
    return false;

  int render_process_id, render_view_id;
  if (!info->GetAssociatedRenderView(&render_process_id, &render_view_id))
    return false;

  ShouldIgnoreCallbackParams params;
  params.render_process_id = render_process_id;
  params.render_view_id = render_view_id;
  params.url = url;
  params.referrer = Referrer(GURL(request_->referrer()),
                             info->GetReferrerPolicy());
  params.has_user_gesture = info->HasUserGesture();
  params.is_post = request_->method() == "POST";
  params.transition_type = info->GetPageTransition();

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &CheckIfShouldIgnoreNavigationOnUIThread,
          params,
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

}  // namespace components
