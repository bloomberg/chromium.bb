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
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "url/gurl.h"

NavigationPredictor::NavigationPredictor(Profile* profile) : profile_(profile) {
  DCHECK(profile_);
}
NavigationPredictor::~NavigationPredictor() = default;

void NavigationPredictor::Create(
    blink::mojom::AnchorElementMetricsHostRequest request,
    content::RenderFrameHost* render_frame_host) {
  // Only valid for the main frame.
  if (render_frame_host->GetParent())
    return;

  Profile* profile = Profile::FromBrowserContext(
      render_frame_host->GetSiteInstance()->GetBrowserContext());

  mojo::MakeStrongBinding(std::make_unique<NavigationPredictor>(profile),
                          std::move(request));
}

base::Optional<double> NavigationPredictor::GetEngagementScore(
    const GURL& url) const {
  DCHECK(url.SchemeIsHTTPOrHTTPS());

  SiteEngagementService* service = SiteEngagementService::Get(profile_);
  return service ? base::make_optional(service->GetScore(url)) : base::nullopt;
}

void NavigationPredictor::UpdateAnchorElementMetrics(
    blink::mojom::AnchorElementMetricsPtr metrics) {
  // TODO(chelu): https://crbug.com/850624/. Use |metrics| to aggregate metrics
  // extracted from the browser process. Analyze and use them to take some
  // actions accordingly.
  auto target_score = GetEngagementScore(metrics->target_url);
  if (target_score.has_value()) {
    UMA_HISTOGRAM_COUNTS_100(
        "AnchorElementMetrics.Clicked.HrefEngagementScore2",
        static_cast<int>(target_score.value()));
  }
}
