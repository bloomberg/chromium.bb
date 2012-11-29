// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_mode_resource_throttle.h"

#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/managed_mode/managed_mode_interstitial.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "content/public/browser/resource_controller.h"
#include "net/url_request/url_request.h"

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
      url_filter_(ManagedMode::GetURLFilterForIOThread()) {}

ManagedModeResourceThrottle::~ManagedModeResourceThrottle() {}

void ManagedModeResourceThrottle::WillStartRequest(bool* defer) {
  if (url_filter_->GetFilteringBehaviorForURL(request_->url()) !=
      ManagedModeURLFilter::BLOCK) {
    return;
  }

  if (is_main_frame_) {
    *defer = true;
    ManagedModeInterstitial::ShowInterstitial(
        render_process_host_id_, render_view_id_, request_->url(),
        base::Bind(&ManagedModeResourceThrottle::OnInterstitialResult,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void ManagedModeResourceThrottle::OnInterstitialResult(bool continue_request) {
  if (continue_request) {
    controller()->Resume();
  } else {
    controller()->Cancel();
  }
}
