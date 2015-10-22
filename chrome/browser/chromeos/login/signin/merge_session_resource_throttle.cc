// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/merge_session_resource_throttle.h"

#include "chrome/browser/chromeos/login/signin/merge_session_throttling_utils.h"
#include "chrome/browser/chromeos/login/signin/merge_session_xhr_request_waiter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;

namespace {

void DelayXHRLoadOnUIThread(
    int render_process_id,
    int render_view_id,
    const GURL& url,
    const merge_session_throttling_utils::CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool should_delay_request = false;
  RenderViewHost* render_view_host =
      RenderViewHost::FromID(render_process_id, render_view_id);
  WebContents* web_contents = nullptr;
  if (render_view_host) {
    web_contents = WebContents::FromRenderViewHost(render_view_host);
    if (web_contents)
      should_delay_request =
          merge_session_throttling_utils::ShouldDelayRequest(web_contents);
  }
  if (should_delay_request) {
    DVLOG(1) << "Creating XHR waiter for " << url.spec();
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());

    // The MergeSessionXHRRequestWaiter will delete itself once it is done
    // blocking the request.
    (new chromeos::MergeSessionXHRRequestWaiter(profile, callback))
        ->StartWaiting();
  } else {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, callback);
  }
}

}  // namespace

MergeSessionResourceThrottle::MergeSessionResourceThrottle(
    net::URLRequest* request)
    : request_(request), weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

MergeSessionResourceThrottle::~MergeSessionResourceThrottle() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void MergeSessionResourceThrottle::WillStartRequest(bool* defer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!merge_session_throttling_utils::ShouldDelayUrl(request_->url()))
    return;

  DVLOG(1) << "WillStartRequest: defer " << request_->url();
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &DelayXHRLoadOnUIThread, info->GetChildID(), info->GetRouteID(),
          request_->url(),
          base::Bind(&MergeSessionResourceThrottle::OnBlockingPageComplete,
                     weak_factory_.GetWeakPtr())));
  *defer = true;
}

const char* MergeSessionResourceThrottle::GetNameForLogging() const {
  return "MergeSessionResourceThrottle";
}

void MergeSessionResourceThrottle::OnBlockingPageComplete() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  controller()->Resume();
}
