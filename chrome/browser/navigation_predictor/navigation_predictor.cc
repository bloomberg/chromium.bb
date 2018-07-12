// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "url/gurl.h"

NavigationPredictor::NavigationPredictor(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {
  DCHECK(render_frame_host_);
}
NavigationPredictor::~NavigationPredictor() = default;

void NavigationPredictor::Create(
    blink::mojom::AnchorElementMetricsHostRequest request,
    content::RenderFrameHost* render_frame_host) {
  // Only valid for the main frame.
  if (render_frame_host->GetParent())
    return;

  mojo::MakeStrongBinding(
      std::make_unique<NavigationPredictor>(render_frame_host),
      std::move(request));
}

bool NavigationPredictor::IsValidMetricFromRenderer(
    const blink::mojom::AnchorElementMetrics& metric) const {
  return metric.target_url.SchemeIsHTTPOrHTTPS();
}

SiteEngagementService* NavigationPredictor::GetEngagementService() const {
  Profile* profile = Profile::FromBrowserContext(
      render_frame_host_->GetSiteInstance()->GetBrowserContext());
  return SiteEngagementService::Get(profile);
}

void NavigationPredictor::UpdateAnchorElementMetrics(
    blink::mojom::AnchorElementMetricsPtr metrics) {
  // TODO(chelu): https://crbug.com/850624/. Use |metrics| to aggregate metrics
  // extracted from the browser process. Analyze and use them to take some
  // actions accordingly.
  if (!IsValidMetricFromRenderer(*metrics)) {
    mojo::ReportBadMessage("Bad anchor element metrics.");
    return;
  }

  SiteEngagementService* engagement_service = GetEngagementService();
  DCHECK(engagement_service);

  double target_score = engagement_service->GetScore(metrics->target_url);

  UMA_HISTOGRAM_COUNTS_100("AnchorElementMetrics.Clicked.HrefEngagementScore2",
                           static_cast<int>(target_score));

  if (target_score > 0) {
    UMA_HISTOGRAM_COUNTS_100(
        "AnchorElementMetrics.Clicked.HrefEngagementScorePositive",
        static_cast<int>(target_score));
  }
}
