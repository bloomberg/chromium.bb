// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor.h"

#include "base/metrics/histogram_macros.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

NavigationPredictor::NavigationPredictor() {}
NavigationPredictor::~NavigationPredictor() = default;

void NavigationPredictor::Create(
    blink::mojom::AnchorElementMetricsHostRequest request,
    content::RenderFrameHost* render_frame_host) {
  // Only valid for the main frame.
  if (render_frame_host->GetParent())
    return;

  mojo::MakeStrongBinding(std::make_unique<NavigationPredictor>(),
                          std::move(request));
}

void NavigationPredictor::UpdateAnchorElementMetrics(
    blink::mojom::AnchorElementMetricsPtr metrics) {
  // TODO(chelu): https://crbug.com/850624/. Use |metrics| to aggregate metrics
  // extracted from the browser process. Analyze and use them to take some
  // actions accordingly.
  UMA_HISTOGRAM_COUNTS_100("AnchorElementMetrics.Clicked.HrefEngagementScore",
                           0);
}
