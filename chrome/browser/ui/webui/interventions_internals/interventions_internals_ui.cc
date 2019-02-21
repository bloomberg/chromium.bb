// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/interventions_internals/interventions_internals_ui.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/previews/content/previews_ui_service.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

content::WebUIDataSource* GetSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIInterventionsInternalsHost);
  source->AddResourcePath("index.js", IDR_INTERVENTIONS_INTERNALS_INDEX_JS);
  source->AddResourcePath(
      "chrome/browser/ui/webui/interventions_internals/"
      "interventions_internals.mojom-lite.js",
      IDR_INTERVENTIONS_INTERNALS_MOJOM_LITE_JS);
  source->AddResourcePath("url/mojom/url.mojom-lite.js", IDR_URL_MOJOM_LITE_JS);
  source->SetDefaultResource(IDR_INTERVENTIONS_INTERNALS_INDEX_HTML);
  source->UseGzip();
  return source;
}

content::WebUIDataSource* GetUnsupportedSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIInterventionsInternalsHost);
  source->SetDefaultResource(IDR_INTERVENTIONS_INTERNALS_UNSUPPORTED_PAGE_HTML);
  source->UseGzip();
  return source;
}

}  // namespace

InterventionsInternalsUI::InterventionsInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui), previews_ui_service_(nullptr) {
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
  previews_ui_service_ = previews_service->previews_ui_service();
  AddHandlerToRegistry(base::BindRepeating(
      &InterventionsInternalsUI::BindInterventionsInternalsPageHandler,
      base::Unretained(this)));
}

InterventionsInternalsUI::~InterventionsInternalsUI() {}

void InterventionsInternalsUI::BindInterventionsInternalsPageHandler(
    mojom::InterventionsInternalsPageHandlerRequest request) {
  DCHECK(previews_ui_service_);
  page_handler_ = std::make_unique<InterventionsInternalsPageHandler>(
      std::move(request), previews_ui_service_, nullptr);
}
