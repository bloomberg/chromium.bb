// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_MEDIA_ENGAGEMENT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_MEDIA_ENGAGEMENT_UI_H_

#include "base/macros.h"
#include "chrome/browser/media/media_engagement_score_details.mojom.h"
#include "chrome/browser/ui/webui/mojo_web_ui_controller.h"

// The UI for chrome://media-engagement/.
class MediaEngagementUI
    : public MojoWebUIController<
          media::mojom::MediaEngagementScoreDetailsProvider> {
 public:
  explicit MediaEngagementUI(content::WebUI* web_ui);
  ~MediaEngagementUI() override;

 private:
  // MojoWebUIController overrides:
  void BindUIHandler(const service_manager::BindSourceInfo& source_info,
                     media::mojom::MediaEngagementScoreDetailsProviderRequest
                         request) override;

  std::unique_ptr<media::mojom::MediaEngagementScoreDetailsProvider>
      ui_handler_;

  DISALLOW_COPY_AND_ASSIGN(MediaEngagementUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_MEDIA_ENGAGEMENT_UI_H_
