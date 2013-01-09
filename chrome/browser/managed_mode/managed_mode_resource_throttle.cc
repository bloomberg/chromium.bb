// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_mode_resource_throttle.h"

#include "base/lazy_instance.h"
#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/managed_mode/managed_mode_interstitial.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "content/public/browser/resource_controller.h"
#include "net/url_request/url_request.h"

namespace {

// This map contains <render_process_host_id_, render_view_id> pairs mapped
// to hostnames which identify individual tabs. If a hostname
// is present for a specific pair then the user clicked preview, is
// navigating around and has not clicked one of the options on the infobar.
typedef std::map<std::pair<int, int>, std::string> PreviewMap;
base::LazyInstance<PreviewMap> g_in_preview_mode = LAZY_INSTANCE_INITIALIZER;

}

ManagedModeResourceThrottle::ManagedModeResourceThrottle(
    const net::URLRequest* request,
    int render_process_host_id,
    int render_view_id,
    bool is_main_frame)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      request_(request),
      render_process_host_id_(render_process_host_id),
      render_view_id_(render_view_id),
      is_main_frame_(is_main_frame),
      temporarily_allowed_(false),
      url_filter_(ManagedMode::GetURLFilterForIOThread()) {}

ManagedModeResourceThrottle::~ManagedModeResourceThrottle() {}

// static
void ManagedModeResourceThrottle::AddTemporaryException(
    int render_process_host_id,
    int render_view_id,
    const GURL& url) {
  g_in_preview_mode.Get()[std::make_pair(render_process_host_id,
                                         render_view_id)] = url.host();
}

// static
void ManagedModeResourceThrottle::RemoveTemporaryException(
      int render_process_host_id,
      int render_view_id) {
  g_in_preview_mode.Get().erase(std::make_pair(render_process_host_id,
                                               render_view_id));
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

  PreviewMap::iterator it = preview_map->find(
      std::make_pair(render_process_host_id_, render_view_id_));
  if (it != preview_map->end() && url.host() == it->second)
    return;

  *defer = true;
  ManagedModeInterstitial::ShowInterstitial(
      render_process_host_id_, render_view_id_, url,
      base::Bind(&ManagedModeResourceThrottle::OnInterstitialResult,
                 weak_ptr_factory_.GetWeakPtr()));
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
