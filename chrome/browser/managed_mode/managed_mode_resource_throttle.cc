// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_mode_resource_throttle.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/managed_mode/managed_mode_interstitial.h"
#include "chrome/browser/managed_mode/managed_mode_navigation_observer.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_controller.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;

namespace {

// Uniquely identifies a tab, used to set a temporary exception on it.
struct WebContentsId {
  WebContentsId(int render_process_host_id, int render_view_id)
      : render_process_host_id(render_process_host_id),
        render_view_id(render_view_id) {}

  bool operator<(const WebContentsId& key) const {
    if (render_process_host_id != key.render_process_host_id)
      return render_process_host_id < key.render_process_host_id;
    return render_view_id < key.render_view_id;
  }

  int render_process_host_id;
  int render_view_id;
};

// This map contains <render_process_host_id_, render_view_id> pairs mapped
// to a host string which identifies individual tabs. If a host is present for
// a specific pair then the user clicked preview, is navigating around and has
// not clicked one of the options on the infobar.
typedef std::map<WebContentsId, std::string> PreviewMap;
base::LazyInstance<PreviewMap> g_in_preview_mode = LAZY_INSTANCE_INITIALIZER;

}

ManagedModeResourceThrottle::ManagedModeResourceThrottle(
    const net::URLRequest* request,
    int render_process_host_id,
    int render_view_id,
    bool is_main_frame,
    const ManagedModeURLFilter* url_filter)
    : weak_ptr_factory_(this),
      request_(request),
      render_process_host_id_(render_process_host_id),
      render_view_id_(render_view_id),
      is_main_frame_(is_main_frame),
      temporarily_allowed_(false),
      url_filter_(url_filter) {}

ManagedModeResourceThrottle::~ManagedModeResourceThrottle() {}

// static
void ManagedModeResourceThrottle::AddTemporaryException(
    int render_process_host_id,
    int render_view_id,
    const GURL& url) {
  WebContentsId web_contents_id(render_process_host_id, render_view_id);
  g_in_preview_mode.Get()[web_contents_id] = url.host();
}

// static
void ManagedModeResourceThrottle::RemoveTemporaryException(
      int render_process_host_id,
      int render_view_id) {
  WebContentsId web_contents_id(render_process_host_id, render_view_id);
  g_in_preview_mode.Get().erase(web_contents_id);
}

void ManagedModeResourceThrottle::ShowInterstitialIfNeeded(bool is_redirect,
                                                           const GURL& url,
                                                           bool* defer) {
  // Only treat main frame requests for now (ignoring subresources).
  if (!is_main_frame_)
    return;

  if (url_filter_->GetFilteringBehaviorForURL(url) !=
      ManagedModeURLFilter::BLOCK) {
    return;
  }

  // Do not show interstitial for redirects in preview mode and URLs which have
  // the same hostname as the one on which the user clicked "Preview" on.
  PreviewMap* preview_map = g_in_preview_mode.Pointer();
  if (temporarily_allowed_) {
    DCHECK(is_redirect);
    return;
  }

  WebContentsId web_contents_id(render_process_host_id_, render_view_id_);
  PreviewMap::iterator it = preview_map->find(web_contents_id);
  if (it != preview_map->end() && url.host() == it->second) {
    temporarily_allowed_ = true;
    RemoveTemporaryException(render_process_host_id_, render_view_id_);
    return;
  }

  *defer = true;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&ManagedModeNavigationObserver::OnRequestBlocked,
                 render_process_host_id_, render_view_id_, url,
                 base::Bind(&ManagedModeResourceThrottle::OnInterstitialResult,
                            weak_ptr_factory_.GetWeakPtr())));
}

void ManagedModeResourceThrottle::WillStartRequest(bool* defer) {
  ShowInterstitialIfNeeded(false, request_->url(), defer);
}

void ManagedModeResourceThrottle::WillRedirectRequest(const GURL& new_url,
                                                      bool* defer) {
  ShowInterstitialIfNeeded(true, new_url, defer);
}

void ManagedModeResourceThrottle::OnInterstitialResult(bool continue_request) {
  if (continue_request) {
    temporarily_allowed_ = true;
    controller()->Resume();
  } else {
    controller()->Cancel();
  }
}
