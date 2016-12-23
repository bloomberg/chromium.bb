// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/engagement/site_engagement_ui.h"

#include <cmath>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace {

// Implementation of mojom::SiteEngagementUIHandler that gets information from
// the
// SiteEngagementService to provide data for the WebUI.
class SiteEngagementUIHandlerImpl : public mojom::SiteEngagementUIHandler {
 public:
  // SiteEngagementUIHandlerImpl is deleted when the supplied pipe is destroyed.
  SiteEngagementUIHandlerImpl(
      Profile* profile,
      mojo::InterfaceRequest<mojom::SiteEngagementUIHandler> request)
      : profile_(profile), binding_(this, std::move(request)) {
    DCHECK(profile_);
  }

  ~SiteEngagementUIHandlerImpl() override {}

  // mojom::SiteEngagementUIHandler overrides:
  void GetSiteEngagementInfo(
      const GetSiteEngagementInfoCallback& callback) override {
    SiteEngagementService* service = SiteEngagementService::Get(profile_);
    std::map<GURL, double> score_map = service->GetScoreMap();

    std::vector<mojom::SiteEngagementInfoPtr> engagement_info;
    engagement_info.reserve(score_map.size());
    for (const auto& info : score_map) {
      mojom::SiteEngagementInfoPtr origin_info(
          mojom::SiteEngagementInfo::New());
      origin_info->origin = info.first;
      origin_info->score = info.second;
      engagement_info.push_back(std::move(origin_info));
    }

    callback.Run(std::move(engagement_info));
  }

  void SetSiteEngagementScoreForOrigin(const GURL& origin,
                                       double score) override {
    if (!origin.is_valid() || score < 0 ||
        score > SiteEngagementService::GetMaxPoints() || std::isnan(score)) {
      return;
    }

    SiteEngagementService* service = SiteEngagementService::Get(profile_);
    service->ResetScoreForURL(origin, score);
  }

 private:
  // The Profile* handed to us in our constructor.
  Profile* profile_;

  mojo::Binding<mojom::SiteEngagementUIHandler> binding_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementUIHandlerImpl);
};

}  // namespace

SiteEngagementUI::SiteEngagementUI(content::WebUI* web_ui)
    : MojoWebUIController<mojom::SiteEngagementUIHandler>(web_ui) {
  // Set up the chrome://site-engagement/ source.
  std::unique_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUISiteEngagementHost));
  source->AddResourcePath("site_engagement.js", IDR_SITE_ENGAGEMENT_JS);
  source->AddResourcePath(
      "chrome/browser/ui/webui/engagement/site_engagement.mojom",
      IDR_SITE_ENGAGEMENT_MOJO_JS);
  source->AddResourcePath("url/mojo/url.mojom", IDR_URL_MOJO_JS);
  source->SetDefaultResource(IDR_SITE_ENGAGEMENT_HTML);
  source->UseGzip(std::unordered_set<std::string>());
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source.release());
}

SiteEngagementUI::~SiteEngagementUI() {}

void SiteEngagementUI::BindUIHandler(
    mojo::InterfaceRequest<mojom::SiteEngagementUIHandler> request) {
  ui_handler_.reset(new SiteEngagementUIHandlerImpl(
      Profile::FromWebUI(web_ui()), std::move(request)));
}
