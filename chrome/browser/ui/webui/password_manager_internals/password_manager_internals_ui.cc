// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager_internals/password_manager_internals_ui.h"

#include <algorithm>
#include <set>

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/password_manager/content/browser/password_manager_internals_service_factory.h"
#include "components/password_manager/core/browser/password_manager_internals_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/password_manager_internals_resources.h"
#include "net/base/escape.h"

using password_manager::PasswordManagerInternalsService;
using password_manager::PasswordManagerInternalsServiceFactory;

namespace {

content::WebUIDataSource* CreatePasswordManagerInternalsHTMLSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIPasswordManagerInternalsHost);

  source->AddResourcePath(
      "password_manager_internals.js",
      IDR_PASSWORD_MANAGER_INTERNALS_PASSWORD_MANAGER_INTERNALS_JS);
  source->SetDefaultResource(
      IDR_PASSWORD_MANAGER_INTERNALS_PASSWORD_MANAGER_INTERNALS_HTML);
  return source;
}

}  // namespace

PasswordManagerInternalsUI::PasswordManagerInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      WebContentsObserver(web_ui->GetWebContents()),
      registered_with_logging_service_(false) {
  // Set up the chrome://password-manager-internals/ source.
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui),
                                CreatePasswordManagerInternalsHTMLSource());
}

PasswordManagerInternalsUI::~PasswordManagerInternalsUI() {
  UnregisterFromLoggingService();
}

void PasswordManagerInternalsUI::DidStartLoading(
    content::RenderViewHost* /* render_view_host */) {
  UnregisterFromLoggingService();
}

void PasswordManagerInternalsUI::DidStopLoading(
    content::RenderViewHost* /* render_view_host */) {
  PasswordManagerInternalsService* service =
      PasswordManagerInternalsServiceFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  if (service) {
    registered_with_logging_service_ = true;
    std::string past_logs(service->RegisterReceiver(this));
    LogSavePasswordProgress(past_logs);
  }
}

void PasswordManagerInternalsUI::LogSavePasswordProgress(
    const std::string& text) {
  if (!registered_with_logging_service_)
    return;
  std::string no_quotes(text);
  std::replace(no_quotes.begin(), no_quotes.end(), '"', ' ');
  base::StringValue text_string_value(net::EscapeForHTML(no_quotes));
  web_ui()->CallJavascriptFunction("addSavePasswordProgressLog",
                                   text_string_value);
}

void PasswordManagerInternalsUI::UnregisterFromLoggingService() {
  if (!registered_with_logging_service_)
    return;
  PasswordManagerInternalsService* service =
      PasswordManagerInternalsServiceFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  if (service)
    service->UnregisterReceiver(this);
}
