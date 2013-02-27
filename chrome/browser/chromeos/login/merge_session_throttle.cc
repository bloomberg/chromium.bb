// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/merge_session_throttle.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/google/google_util.h"
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

using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;

namespace {

void ShowDeleayedLoadingPage(
    int render_process_id,
    int render_view_id,
    const GURL& url,
    const chromeos::MergeSessionLoadPage::CompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Check again on UI thread and proceed if it's connected.
  if (chromeos::UserManager::Get()->GetMergeSessionState() !=
          chromeos::UserManager::MERGE_STATUS_IN_PROCESS) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, callback);
  } else {
    RenderViewHost* render_view_host =
        RenderViewHost::FromID(render_process_id, render_view_id);
    WebContents* web_contents = render_view_host ?
        WebContents::FromRenderViewHost(render_view_host) : NULL;
    // There is a chance that the tab closed after we decided to show
    // the offline page on the IO thread and before we actually show the
    // offline page here on the UI thread.
    if (web_contents)
      (new chromeos::MergeSessionLoadPage(web_contents, url, callback))->Show();
  }
}

}  // namespace

MergeSessionThrottle::MergeSessionThrottle(int render_process_id,
                                           int render_view_id,
                                           net::URLRequest* request)
    : render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      request_(request) {
}

MergeSessionThrottle::~MergeSessionThrottle() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void MergeSessionThrottle::WillStartRequest(bool* defer) {
  if (!ShouldShowMergeSessionPage(request_->url()))
    return;

  DVLOG(1) << "WillStartRequest: url=" << request_->url();
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &ShowDeleayedLoadingPage,
          render_process_id_,
          render_view_id_,
          request_->url(),
          base::Bind(
              &MergeSessionThrottle::OnBlockingPageComplete,
              AsWeakPtr())));
  *defer = true;
}

void MergeSessionThrottle::OnBlockingPageComplete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  controller()->Resume();
}

bool MergeSessionThrottle::ShouldShowMergeSessionPage(const GURL& url) const {
  // If we are loading google properties while merge session is in progress,
  // we will show delayed loading page instead.
  return !net::NetworkChangeNotifier::IsOffline() &&
         chromeos::UserManager::Get()->GetMergeSessionState() ==
             chromeos::UserManager::MERGE_STATUS_IN_PROCESS &&
         google_util::IsGoogleHostname(url.host(),
                                       google_util::ALLOW_SUBDOMAIN);
}
