// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/interventions_internals/interventions_internals_ui.h"

#include <string>

#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/previews/core/previews_ui_service.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

content::WebUIDataSource* GetSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIInterventionsInternalsHost);
  source->AddResourcePath("index.js", IDR_INTERVENTIONS_INTERNALS_INDEX_JS);
  source->AddResourcePath(
      "chrome/browser/ui/webui/interventions_internals/"
      "interventions_internals.mojom.js",
      IDR_INTERVENTIONS_INTERNALS_MOJO_INDEX_JS);
  source->AddResourcePath("url/mojo/url.mojom.js", IDR_URL_MOJO_JS);
  source->SetDefaultResource(IDR_INTERVENTIONS_INTERNALS_INDEX_HTML);
  source->UseGzip(std::vector<std::string>());
  return source;
}

content::WebUIDataSource* GetUnsupportedSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIInterventionsInternalsHost);
  source->SetDefaultResource(IDR_INTERVENTIONS_INTERNALS_UNSUPPORTED_PAGE_HTML);
  source->UseGzip(std::vector<std::string>());
  return source;
}

}  // namespace

InterventionsInternalsUI::InterventionsInternalsUI(content::WebUI* web_ui)
    : MojoWebUIController(web_ui), logger_(nullptr) {
  // Set up the chrome://interventions-internals/ source.
  Profile* profile = Profile::FromWebUI(web_ui);

  PreviewsService* previews_service =
      PreviewsServiceFactory::GetForProfile(profile);
  if (!previews_service) {
    // In Guest Mode or Incognito Mode.
    content::WebUIDataSource::Add(profile, GetUnsupportedSource());
    return;
  }

  content::WebUIDataSource::Add(profile, GetSource());
  logger_ = previews_service->previews_ui_service()->previews_logger();
}

InterventionsInternalsUI::~InterventionsInternalsUI() {}

void InterventionsInternalsUI::BindUIHandler(
    mojom::InterventionsInternalsPageHandlerRequest request) {
  DCHECK(logger_);
  page_handler_.reset(
      new InterventionsInternalsPageHandler(std::move(request), logger_));
}
