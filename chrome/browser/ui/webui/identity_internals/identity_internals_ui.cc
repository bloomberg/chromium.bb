// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/identity_internals/identity_internals_ui.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/identity_internals/identity_internals_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"


IdentityInternalsUI::IdentityInternalsUI(content::WebUI* web_ui)
    : MojoWebUIController(web_ui) {
  // chrome://identity-internals source.
  content::WebUIDataSource* html_source =
    content::WebUIDataSource::Create(chrome::kChromeUIIdentityInternalsHost);
  html_source->SetUseJsonJSFormatV2();

  // Localized strings
  html_source->AddLocalizedString("tokenCacheHeader",
      IDS_IDENTITY_INTERNALS_TOKEN_CACHE_TEXT);
  html_source->AddLocalizedString("accessToken",
      IDS_IDENTITY_INTERNALS_ACCESS_TOKEN);
  html_source->AddLocalizedString("extensionName",
      IDS_IDENTITY_INTERNALS_EXTENSION_NAME);
  html_source->AddLocalizedString("extensionId",
      IDS_IDENTITY_INTERNALS_EXTENSION_ID);
  html_source->AddLocalizedString("tokenStatus",
      IDS_IDENTITY_INTERNALS_TOKEN_STATUS);
  html_source->AddLocalizedString("expirationTime",
      IDS_IDENTITY_INTERNALS_EXPIRATION_TIME);
  html_source->AddLocalizedString("scopes",
      IDS_IDENTITY_INTERNALS_SCOPES);
  html_source->AddLocalizedString("revoke",
      IDS_IDENTITY_INTERNALS_REVOKE);
  html_source->SetJsonPath("strings.js");

  // Required resources
  html_source->AddResourcePath("identity_internals.css",
      IDR_IDENTITY_INTERNALS_CSS);
  html_source->AddResourcePath("identity_internals.js",
      IDR_IDENTITY_INTERNALS_JS);
  html_source->SetDefaultResource(IDR_IDENTITY_INTERNALS_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);

  AddMojoResourcePath(
      "chrome/browser/ui/webui/identity_internals/identity_internals.mojom",
      IDR_IDENTITY_INTERNALS_MOJO_JS);
}

IdentityInternalsUI::~IdentityInternalsUI() {}

scoped_ptr<MojoWebUIHandler> IdentityInternalsUI::CreateUIHandler(
    mojo::ScopedMessagePipeHandle handle_to_page) {
  return scoped_ptr<MojoWebUIHandler>(mojo::BindToPipe(
      new IdentityInternalsUIHandler(Profile::FromWebUI(web_ui())),
      handle_to_page.Pass()));
}
