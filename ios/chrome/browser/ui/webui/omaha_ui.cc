// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/omaha_ui.h"

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/omaha/omaha_service.h"
#include "ios/chrome/grit/ios_resources.h"
#include "ios/web/public/web_ui_ios_data_source.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

using web::WebUIIOSMessageHandler;

namespace {

web::WebUIIOSDataSource* CreateOmahaUIHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIOmahaHost);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("omaha.js", IDR_IOS_OMAHA_JS);
  source->SetDefaultResource(IDR_IOS_OMAHA_HTML);
  source->UseGzip();
  return source;
}

// OmahaDOMHandler

// The handler for Javascript messages for the chrome://omaha/ page.
class OmahaDOMHandler : public WebUIIOSMessageHandler {
 public:
  OmahaDOMHandler();
  ~OmahaDOMHandler() override;

  // WebUIIOSMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Asynchronously fetches the debug information. Called from JS.
  void HandleRequestDebugInformation(const base::ListValue* args);

  // Called when the debug information have been computed.
  void OnDebugInformationAvailable(base::DictionaryValue* debug_information);

  // WeakPtr factory needed because this object might be deleted before
  // receiving the callbacks from the OmahaService.
  base::WeakPtrFactory<OmahaDOMHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OmahaDOMHandler);
};

OmahaDOMHandler::OmahaDOMHandler() : weak_ptr_factory_(this) {}

OmahaDOMHandler::~OmahaDOMHandler() {}

void OmahaDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestOmahaDebugInformation",
      base::Bind(&OmahaDOMHandler::HandleRequestDebugInformation,
                 base::Unretained(this)));
}

void OmahaDOMHandler::HandleRequestDebugInformation(
    const base::ListValue* args) {
  OmahaService::GetDebugInformation(
      base::Bind(&OmahaDOMHandler::OnDebugInformationAvailable,
                 weak_ptr_factory_.GetWeakPtr()));
}

void OmahaDOMHandler::OnDebugInformationAvailable(
    base::DictionaryValue* debug_information) {
  web_ui()->CallJavascriptFunction("updateOmahaDebugInformation",
                                   *debug_information);
}

}  // namespace

// OmahaUI
OmahaUI::OmahaUI(web::WebUIIOS* web_ui) : WebUIIOSController(web_ui) {
  web_ui->AddMessageHandler(base::MakeUnique<OmahaDOMHandler>());

  // Set up the chrome://omaha/ source.
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromWebUIIOS(web_ui);
  web::WebUIIOSDataSource::Add(browser_state, CreateOmahaUIHTMLSource());
}

OmahaUI::~OmahaUI() {}
