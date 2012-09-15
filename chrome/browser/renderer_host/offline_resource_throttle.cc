// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/offline_resource_throttle.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/offline/offline_load_page.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "webkit/appcache/appcache_service.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;

namespace {

void ShowOfflinePage(
    int render_process_id,
    int render_view_id,
    const GURL& url,
    const chromeos::OfflineLoadPage::CompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Check again on UI thread and proceed if it's connected.
  if (!net::NetworkChangeNotifier::IsOffline()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, base::Bind(callback, true));
  } else {
    RenderViewHost* render_view_host =
        RenderViewHost::FromID(render_process_id, render_view_id);
    WebContents* web_contents = render_view_host ?
        WebContents::FromRenderViewHost(render_view_host) : NULL;
    // There is a chance that the tab closed after we decided to show
    // the offline page on the IO thread and before we actually show the
    // offline page here on the UI thread.
    if (web_contents)
      (new chromeos::OfflineLoadPage(web_contents, url, callback))->Show();
  }
}

}  // namespace

OfflineResourceThrottle::OfflineResourceThrottle(
    int render_process_id,
    int render_view_id,
    net::URLRequest* request,
    appcache::AppCacheService* appcache_service)
    : render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      request_(request),
      appcache_service_(appcache_service) {
  DCHECK(appcache_service);
}

OfflineResourceThrottle::~OfflineResourceThrottle() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!appcache_completion_callback_.IsCancelled())
    appcache_completion_callback_.Cancel();
}

void OfflineResourceThrottle::WillStartRequest(bool* defer) {
  if (!ShouldShowOfflinePage(request_->url()))
    return;

  DVLOG(1) << "WillStartRequest: this=" << this << ", url=" << request_->url();

  const GURL* url = &(request_->url());
  const GURL* first_party = &(request_->first_party_for_cookies());

  // Anticipate a client-side HSTS based redirect from HTTP to HTTPS, and
  // ask the appcache about the HTTPS url instead of the HTTP url.
  GURL redirect_url;
  if (request_->GetHSTSRedirect(&redirect_url)) {
    if (url->GetOrigin() == first_party->GetOrigin())
      first_party = &redirect_url;
    url = &redirect_url;
  }

  DCHECK(appcache_completion_callback_.IsCancelled());

  appcache_completion_callback_.Reset(
      base::Bind(&OfflineResourceThrottle::OnCanHandleOfflineComplete,
                 AsWeakPtr()));
  appcache_service_->CanHandleMainResourceOffline(
      *url, *first_party,
      appcache_completion_callback_.callback());

  *defer = true;
}

void OfflineResourceThrottle::OnBlockingPageComplete(bool proceed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (proceed) {
    controller()->Resume();
  } else {
    controller()->Cancel();
  }
}

bool OfflineResourceThrottle::IsRemote(const GURL& url) const {
  return !net::IsLocalhost(url.host()) &&
    (url.SchemeIs(chrome::kFtpScheme) ||
     url.SchemeIs(chrome::kHttpScheme) ||
     url.SchemeIs(chrome::kHttpsScheme));
}

bool OfflineResourceThrottle::ShouldShowOfflinePage(const GURL& url) const {
  // If the network is disconnected while loading other resources, we'll simply
  // show broken link/images.
  return IsRemote(url) && net::NetworkChangeNotifier::IsOffline();
}

void OfflineResourceThrottle::OnCanHandleOfflineComplete(int rv) {
  appcache_completion_callback_.Cancel();

  if (rv == net::OK) {
    controller()->Resume();
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(
            &ShowOfflinePage,
            render_process_id_,
            render_view_id_,
            request_->url(),
            base::Bind(
                &OfflineResourceThrottle::OnBlockingPageComplete,
                AsWeakPtr())));
  }
}
