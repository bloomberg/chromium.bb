// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/intercept_navigation_resource_throttle.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/common/referrer.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using content::Referrer;
using content::RenderViewHost;
using content::ResourceRequestInfo;

namespace {

void CheckIfShouldIgnoreNavigationOnUIThread(
    int render_process_id,
    int render_view_id,
    const GURL& url,
    const Referrer& referrer,
    bool is_content_initiated,
    InterceptNavigationResourceThrottle::CheckOnUIThreadCallback
        should_ignore_callback,
    base::Callback<void(bool)> callback) {

  RenderViewHost* rvh =
      RenderViewHost::FromID(render_process_id, render_view_id);

  GURL validated_url(url);
  RenderViewHost::FilterURL(
      rvh->GetProcess()->GetID(),
      false,
      &validated_url);

  bool should_ignore_navigation = false;
  should_ignore_navigation = should_ignore_callback.Run(
      rvh, validated_url, referrer, is_content_initiated);

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

  // This class should only be instantiated for top level frame requests.
  DCHECK(info->IsMainFrame());

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &CheckIfShouldIgnoreNavigationOnUIThread,
          render_process_id,
          render_view_id,
          url,
          Referrer(GURL(request_->referrer()), info->GetReferrerPolicy()),
          info->HasUserGesture(),
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
