// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom.h"

namespace content {
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
  // blink::mojom::AnchorElementMetricsHost:
  void UpdateAnchorElementMetrics(
      blink::mojom::AnchorElementMetricsPtr metrics) override;

  // Returns true if the anchor element metric from the renderer process is
  // valid.
  bool IsValidMetricFromRenderer(
      const blink::mojom::AnchorElementMetrics& metric) const;

  // Returns site engagement service, which can be used to get site engagement
  // score. Return value is guaranteed to be non-null.
  SiteEngagementService* GetEngagementService() const;

  // |render_frame_host_| is the host associated with the render frame. It is
  // used to retrieve metrics at the browser side.
  content::RenderFrameHost* const render_frame_host_;

  DISALLOW_COPY_AND_ASSIGN(NavigationPredictor);
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_
