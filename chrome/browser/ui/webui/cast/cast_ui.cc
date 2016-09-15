// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cast/cast_ui.h"

#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

CastUI::CastUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Retrieve the ID of the component extension.
  // TODO(crbug.com/597778): remove reference to MediaRouterMojoImpl.
  auto* router = static_cast<media_router::MediaRouterMojoImpl*>(
      media_router::MediaRouterFactory::GetApiForBrowserContext(
          web_ui->GetWebContents()->GetBrowserContext()));
  std::string extension_id = router->media_route_provider_extension_id();

  // Set up the chrome://cast data source and add required resources.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUICastHost);

  html_source->AddResourcePath("cast.css", IDR_CAST_CSS);
  html_source->AddResourcePath("cast.js", IDR_CAST_JS);
  html_source->AddResourcePath("cast_favicon.ico", IDR_CAST_FAVICON);
  html_source->AddString("extensionId", extension_id);
  html_source->SetJsonPath("strings.js");
  html_source->SetDefaultResource(IDR_CAST_HTML);
  html_source->OverrideContentSecurityPolicyObjectSrc("object-src * chrome:;");

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);
}

CastUI::~CastUI() {
}
