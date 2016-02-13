// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/engagement/site_engagement_ui.h"

#include <utility>

#include "base/macros.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace {

// Implementation of SiteEngagementUIHandler that gets information from the
// SiteEngagementService to provide data for the WebUI.
class SiteEngagementUIHandlerImpl : public SiteEngagementUIHandler {
 public:
  // SiteEngagementUIHandlerImpl is deleted when the supplied pipe is destroyed.
  SiteEngagementUIHandlerImpl(
      Profile* profile,
      mojo::InterfaceRequest<SiteEngagementUIHandler> request)
      : profile_(profile), binding_(this, std::move(request)) {
    DCHECK(profile_);
  }

  ~SiteEngagementUIHandlerImpl() override {}

  // SiteEngagementUIHandler overrides:
  void GetSiteEngagementInfo(
      const GetSiteEngagementInfoCallback& callback) override {
    mojo::Array<SiteEngagementInfoPtr> engagement_info;

    SiteEngagementService* service = SiteEngagementService::Get(profile_);

    for (const std::pair<GURL, double>& info : service->GetScoreMap()) {
      SiteEngagementInfoPtr origin_info(SiteEngagementInfo::New());
      origin_info->origin = mojo::String::From(info.first);
      origin_info->score = info.second;
      engagement_info.push_back(std::move(origin_info));
    }

    callback.Run(std::move(engagement_info));
  }

  void SetSiteEngagementScoreForOrigin(const mojo::String& origin,
                                       double score) override {
    GURL origin_gurl(origin.get());
    if (!origin_gurl.is_valid() || score < 0 ||
        score > SiteEngagementScore::kMaxPoints) {
      return;
    }

    SiteEngagementService* service = SiteEngagementService::Get(profile_);
    service->ResetScoreForURL(origin_gurl, score);
  }

 private:
  // The Profile* handed to us in our constructor.
  Profile* profile_;

  mojo::Binding<SiteEngagementUIHandler> binding_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementUIHandlerImpl);
};

}  // namespace

SiteEngagementUI::SiteEngagementUI(content::WebUI* web_ui)
    : MojoWebUIController<SiteEngagementUIHandler>(web_ui) {
  // Set up the chrome://site-engagement/ source.
  scoped_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUISiteEngagementHost));
  source->AddResourcePath("engagement_table.html",
                          IDR_SITE_ENGAGEMENT_ENGAGEMENT_TABLE_HTML);
  source->AddResourcePath("engagement_table.css",
                          IDR_SITE_ENGAGEMENT_ENGAGEMENT_TABLE_CSS);
  source->AddResourcePath("engagement_table.js",
                          IDR_SITE_ENGAGEMENT_ENGAGEMENT_TABLE_JS);
  source->AddResourcePath("site_engagement.js", IDR_SITE_ENGAGEMENT_JS);
  source->AddResourcePath(
      "chrome/browser/ui/webui/engagement/site_engagement.mojom",
      IDR_SITE_ENGAGEMENT_MOJO_JS);
  source->SetDefaultResource(IDR_SITE_ENGAGEMENT_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source.release());
}

SiteEngagementUI::~SiteEngagementUI() {}

void SiteEngagementUI::BindUIHandler(
    mojo::InterfaceRequest<SiteEngagementUIHandler> request) {
  ui_handler_.reset(new SiteEngagementUIHandlerImpl(
      Profile::FromWebUI(web_ui()), std::move(request)));
}
