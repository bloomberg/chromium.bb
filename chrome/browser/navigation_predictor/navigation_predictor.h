// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_

#include "base/macros.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom.h"

namespace content {
class RenderFrameHost;
}

class GURL;
class Profile;

// This class gathers metrics of anchor elements from both renderer process
// and browser process. Then it uses these metrics to make predictions on what
// are the most likely anchor elements that the user will click.
class NavigationPredictor : public blink::mojom::AnchorElementMetricsHost {
 public:
  explicit NavigationPredictor(Profile* profile);
  ~NavigationPredictor() override;

  static void Create(mojo::InterfaceRequest<AnchorElementMetricsHost> request,
                     content::RenderFrameHost* render_frame_host);

  // blink::mojom::AnchorElementMetricsHost implementation.
  void UpdateAnchorElementMetrics(
      blink::mojom::AnchorElementMetricsPtr metrics) override;

 private:
  // Get site engagement score from SiteEngagementService.
  base::Optional<double> GetEngagementScore(const GURL& url) const;

  // Browser profile used to retrieve site engagement score.
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(NavigationPredictor);
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_
