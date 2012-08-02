// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/diagnostics/diagnostics_ui.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"

namespace chromeos {

namespace {

////////////////////////////////////////////////////////////////////////////////
// DiagnosticsHandler

// Class to handle messages from chrome://diagnostics.
class DiagnosticsWebUIHandler : public content::WebUIMessageHandler {
 public:
  DiagnosticsWebUIHandler()
      : weak_ptr_factory_(this) {
  }
  virtual ~DiagnosticsWebUIHandler() {}

 private:
  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Called when the page is first loaded.
  void OnPageLoaded(const base::ListValue* args);

  base::WeakPtrFactory<DiagnosticsWebUIHandler> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsWebUIHandler);
};

void DiagnosticsWebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "pageLoaded",
      base::Bind(&DiagnosticsWebUIHandler::OnPageLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DiagnosticsWebUIHandler::OnPageLoaded(const base::ListValue* args) {
  // TODO: invoke debugd methods to retrieve diagnostics information, and
  // upon completion call javascript function to update status.
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DiagnosticsUI

DiagnosticsUI::DiagnosticsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new DiagnosticsWebUIHandler());

  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIDiagnosticsHost);
  source->set_default_resource(IDR_DIAGNOSTICS_MAIN_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, source);
}

} // namespace chromeos
