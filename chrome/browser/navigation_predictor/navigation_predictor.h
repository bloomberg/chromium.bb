// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_

#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}

class SiteEngagementService;

// This class gathers metrics of anchor elements from both renderer process
// and browser process. Then it uses these metrics to make predictions on what
// are the most likely anchor elements that the user will click.
class NavigationPredictor : public blink::mojom::AnchorElementMetricsHost {
 public:
  // |render_frame_host| is the host associated with the render frame. It is
  // used to retrieve metrics at the browser side.
  explicit NavigationPredictor(content::RenderFrameHost* render_frame_host);
  ~NavigationPredictor() override;

  // Create and bind NavigationPredictor.
  static void Create(mojo::InterfaceRequest<AnchorElementMetricsHost> request,
                     content::RenderFrameHost* render_frame_host);

 private:
  // Struct holding features of an anchor element, extracted from the browser.
  struct MetricsFromBrowser;

  // blink::mojom::AnchorElementMetricsHost:
  void ReportAnchorElementMetricsOnClick(
      blink::mojom::AnchorElementMetricsPtr metrics) override;
  void ReportAnchorElementMetricsOnLoad(
      std::vector<blink::mojom::AnchorElementMetricsPtr> metrics) override;

  // Returns true if the anchor element metric from the renderer process is
  // valid.
  bool IsValidMetricFromRenderer(
      const blink::mojom::AnchorElementMetrics& metric) const;

  // Returns site engagement service, which can be used to get site engagement
  // score. Return value is guaranteed to be non-null.
  SiteEngagementService* GetEngagementService() const;

  // Merge anchor element metrics that have the same target url (href).
  void MergeMetricsSameTargetUrl(
      std::vector<blink::mojom::AnchorElementMetricsPtr>* metrics) const;

  // Given metrics of an anchor element from both renderer and browser process,
  // returns navigation score.
  double GetAnchorElementScore(
      const blink::mojom::AnchorElementMetrics& metrics_renderer,
      const MetricsFromBrowser& metrics_browser) const;

  // Given a vector of navigation scores, decide what action to take, or decide
  // not to do anything. Example actions including preresolve, preload,
  // prerendering, etc.
  void MaybeTakeActionOnLoad(const std::vector<double>& scores) const;

  // Record anchor element metrics on page load.
  void RecordMetricsOnLoad(
      const blink::mojom::AnchorElementMetrics& metric) const;

  // Used to get keyed services.
  content::BrowserContext* const browser_context_;

  DISALLOW_COPY_AND_ASSIGN(NavigationPredictor);
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_
