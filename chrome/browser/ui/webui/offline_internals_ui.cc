// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/offline_internals_ui.h"

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"

namespace {

// Class acting as a controller of the chrome://offline-internals WebUI.
class OfflineInternalsUIMessageHandler : public content::WebUIMessageHandler {
 public:
  OfflineInternalsUIMessageHandler();
  ~OfflineInternalsUIMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Deletes all the pages in the store.
  void HandleDeleteAllPages(const base::ListValue* args);

  // Delete selected list of page ids from the store.
  void HandleDeleteSelectedPages(const base::ListValue* args);

  // Load all information.
  void HandleGetOfflineInternalsInfo(const base::ListValue* args);

  // Factory for creating references in callbacks.
  base::WeakPtrFactory<OfflineInternalsUIMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflineInternalsUIMessageHandler);
};

OfflineInternalsUIMessageHandler::OfflineInternalsUIMessageHandler()
    : weak_ptr_factory_(this) {}

OfflineInternalsUIMessageHandler::~OfflineInternalsUIMessageHandler() {}

void OfflineInternalsUIMessageHandler::HandleDeleteAllPages(
    const base::ListValue* args) {
  const base::Value* callback_id;
  args->Get(0, &callback_id);
  CallJavascriptFunction("cr.webUIResponse",
      *callback_id,
      base::FundamentalValue(true),
      base::StringValue("success"));
}

void OfflineInternalsUIMessageHandler::HandleDeleteSelectedPages(
    const base::ListValue* args) {
  const base::Value* callback_id;
  args->Get(0, &callback_id);
  CallJavascriptFunction("cr.webUIResponse",
      *callback_id,
      base::FundamentalValue(true),
      base::StringValue("success"));
}

void OfflineInternalsUIMessageHandler::HandleGetOfflineInternalsInfo(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  args->Get(0, &callback_id);
  base::DictionaryValue results;
  results.Set("AllPages", new base::ListValue());
  results.Set("Queue", new base::ListValue());

  CallJavascriptFunction("cr.webUIResponse",
      *callback_id,
      base::FundamentalValue(true),
      results);
}

void OfflineInternalsUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "deleteAllPages",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleDeleteAllPages,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "deleteSelectedPages",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleDeleteSelectedPages,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getOfflineInternalsInfo",
      base::Bind(
          &OfflineInternalsUIMessageHandler::HandleGetOfflineInternalsInfo,
          weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace

OfflineInternalsUI::OfflineInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // chrome://offline-internals source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIOfflineInternalsHost);

  // Required resources.
  html_source->SetJsonPath("strings.js");
  html_source->AddResourcePath("offline_internals.css",
                               IDR_OFFLINE_INTERNALS_CSS);
  html_source->AddResourcePath("offline_internals.js",
                               IDR_OFFLINE_INTERNALS_JS);
  html_source->SetDefaultResource(IDR_OFFLINE_INTERNALS_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);

  web_ui->AddMessageHandler(new OfflineInternalsUIMessageHandler());
}

OfflineInternalsUI::~OfflineInternalsUI() {}
