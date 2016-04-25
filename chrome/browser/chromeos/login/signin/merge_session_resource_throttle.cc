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
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    const GURL& url,
    const merge_session_throttling_utils::CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebContents* web_contents = web_contents_getter.Run();
  if (web_contents &&
      merge_session_throttling_utils::ShouldDelayRequestForWebContents(
          web_contents)) {
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
          &DelayXHRLoadOnUIThread, info->GetWebContentsGetterForRequest(),
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
