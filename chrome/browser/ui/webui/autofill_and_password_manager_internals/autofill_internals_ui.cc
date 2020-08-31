// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/autofill_internals_ui.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/internals_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/browser/autofill_log_router_factory.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

AutofillInternalsUI::AutofillInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile,
                                autofill::CreateInternalsHTMLSource(
                                    chrome::kChromeUIAutofillInternalsHost));
  web_ui->AddMessageHandler(std::make_unique<autofill::InternalsUIHandler>(
      "setUpAutofillInternals",
      base::BindRepeating(
          &autofill::AutofillLogRouterFactory::GetForBrowserContext)));
}

AutofillInternalsUI::~AutofillInternalsUI() = default;
