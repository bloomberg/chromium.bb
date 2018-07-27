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

struct NavigationPredictor::MetricsFromBrowser {
  MetricsFromBrowser(const GURL& source_url,
                     const GURL& target_url,
                     const SiteEngagementService* engagement_service)
      : source_engagement_score(engagement_service->GetScore(source_url)),
        target_engagement_score(engagement_service->GetScore(target_url)) {
    DCHECK(source_engagement_score >= 0 &&
           source_engagement_score <= engagement_service->GetMaxPoints());
    DCHECK(target_engagement_score >= 0 &&
           target_engagement_score <= engagement_service->GetMaxPoints());
  }

  // The site engagement score of the url of the root document, and the target
  // url (href) of the anchor element. The scores are retrieved from the site
  // engagement service. The range of the scores are between 0 and
  // SiteEngagementScore::kMaxPoints (both inclusive).
  const double source_engagement_score;
  const double target_engagement_score;
};

NavigationPredictor::NavigationPredictor(
    content::RenderFrameHost* render_frame_host)
    : browser_context_(
          render_frame_host->GetSiteInstance()->GetBrowserContext()) {
  DCHECK(browser_context_);
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
  return metric.target_url.SchemeIsHTTPOrHTTPS() &&
         metric.source_url.SchemeIsHTTPOrHTTPS();
}

SiteEngagementService* NavigationPredictor::GetEngagementService() const {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  return SiteEngagementService::Get(profile);
}

void NavigationPredictor::ReportAnchorElementMetricsOnClick(
    blink::mojom::AnchorElementMetricsPtr metrics) {
  // TODO(chelu): https://crbug.com/850624/. Use |metrics| to aggregate metrics
  // extracted from the browser process. Analyze and use them to take some
  // actions accordingly.
  if (!IsValidMetricFromRenderer(*metrics)) {
    mojo::ReportBadMessage("Bad anchor element metrics: onClick.");
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

void NavigationPredictor::ReportAnchorElementMetricsOnLoad(
    std::vector<blink::mojom::AnchorElementMetricsPtr> metrics) {
  if (metrics.empty()) {
    mojo::ReportBadMessage("Bad anchor element metrics: empty.");
    return;
  }

  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Visible.NumberOfAnchorElements", metrics.size());

  SiteEngagementService* engagement_service = GetEngagementService();
  DCHECK(engagement_service);

  // TODO(chelu): https://crbug.com/850624/. Merge anchor elements that point to
  // the same target. e.g, image and accompanying headline text may point to the
  // same target url.

  std::vector<double> scores;
  for (const auto& metric : metrics) {
    if (!IsValidMetricFromRenderer(*metric)) {
      mojo::ReportBadMessage("Bad anchor element metrics: onLoad.");
      return;
    }

    RecordMetricsOnLoad(*metric);

    MetricsFromBrowser metrics_browser(metric->source_url, metric->target_url,
                                       engagement_service);
    double score = GetAnchorElementScore(*metric, metrics_browser);
    scores.push_back(score);
  }

  MaybeTakeActionOnLoad(scores);
}

double NavigationPredictor::GetAnchorElementScore(
    const blink::mojom::AnchorElementMetrics& metrics_renderer,
    const MetricsFromBrowser& metrics_browser) const {
  // TODO(chelu): https://crbug.com/850624/. Experiment with other heuristic
  // algorithms for computing the anchor elements score.
  return metrics_renderer.ratio_visible_area *
         metrics_browser.target_engagement_score;
}

void NavigationPredictor::MaybeTakeActionOnLoad(
    const std::vector<double>& scores) const {
  // TODO(chelu): https://crbug.com/850624/. Given the calculated navigation
  // scores, this function decides which action to take, or decides not to do
  // anything. Example actions including preresolve, preload, prerendering, etc.
  // Other information (e.g., target urls) should also be passed in.

  double highest_score = 0;
  for (double score : scores)
    highest_score = std::max(highest_score, score);

  UMA_HISTOGRAM_COUNTS_100(
      "AnchorElementMetrics.Visible.HighestNavigationScore",
      static_cast<int>(highest_score));
}

void NavigationPredictor::RecordMetricsOnLoad(
    const blink::mojom::AnchorElementMetrics& metric) const {
  UMA_HISTOGRAM_PERCENTAGE("AnchorElementMetrics.Visible.RatioArea",
                           static_cast<int>(metric.ratio_area * 100));

  UMA_HISTOGRAM_PERCENTAGE("AnchorElementMetrics.Visible.RatioVisibleArea",
                           static_cast<int>(metric.ratio_visible_area * 100));

  UMA_HISTOGRAM_PERCENTAGE(
      "AnchorElementMetrics.Visible.RatioDistanceTopToVisibleTop",
      static_cast<int>(
          std::min(metric.ratio_distance_top_to_visible_top, 1.0f) * 100));

  UMA_HISTOGRAM_PERCENTAGE(
      "AnchorElementMetrics.Visible.RatioDistanceCenterToVisibleTop",
      static_cast<int>(
          std::min(metric.ratio_distance_center_to_visible_top, 1.0f) * 100));

  UMA_HISTOGRAM_COUNTS_10000(
      "AnchorElementMetrics.Visible.RatioDistanceRootTop",
      static_cast<int>(std::min(metric.ratio_distance_root_top, 100.0f) * 100));

  UMA_HISTOGRAM_COUNTS_10000(
      "AnchorElementMetrics.Visible.RatioDistanceRootBottom",
      static_cast<int>(std::min(metric.ratio_distance_root_bottom, 100.0f) *
                       100));

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Visible.IsInIFrame",
                        metric.is_in_iframe);

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Visible.ContainsImage",
                        metric.contains_image);

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Visible.IsSameHost",
                        metric.is_same_host);

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Visible.IsUrlIncrementedByOne",
                        metric.is_url_incremented_by_one);
}
