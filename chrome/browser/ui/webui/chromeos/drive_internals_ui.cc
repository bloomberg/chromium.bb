// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/drive_internals_ui.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/gdata/gdata_auth_service.h"
#include "chrome/browser/chromeos/gdata/gdata_documents_service.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"

namespace chromeos {

namespace {

// Class to handle messages from chrome://drive-internals.
class DriveInternalsWebUIHandler : public content::WebUIMessageHandler {
 public:
  DriveInternalsWebUIHandler()
      : weak_ptr_factory_(this) {
  }

  virtual ~DriveInternalsWebUIHandler() {
  }

 private:
  // WebUIMessageHandler override.
  virtual void RegisterMessages() OVERRIDE {
    web_ui()->RegisterMessageCallback(
        "pageLoaded",
        base::Bind(&DriveInternalsWebUIHandler::OnPageLoaded,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // Called when the page is first loaded.
  void OnPageLoaded(const base::ListValue* args) {
    Profile* profile = Profile::FromWebUI(web_ui());
    gdata::GDataSystemService* system_service =
        gdata::GDataSystemServiceFactory::GetForProfile(profile);
    // |system_service| may be NULL in the guest/incognito mode.
    if (!system_service)
      return;

    gdata::DocumentsServiceInterface* documents_service =
        system_service->docs_service();
    DCHECK(documents_service);

    // Update the auth status section.
    base::DictionaryValue auth_status;
    auth_status.SetBoolean("has-refresh-token",
                           documents_service->HasRefreshToken());
    auth_status.SetBoolean("has-access-token",
                           documents_service->HasAccessToken());
    web_ui()->CallJavascriptFunction("UpdateAuthStatus", auth_status);
  }

  base::WeakPtrFactory<DriveInternalsWebUIHandler> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DriveInternalsWebUIHandler);
};

}  // namespace

DriveInternalsUI::DriveInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new DriveInternalsWebUIHandler());

  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIDriveInternalsHost);
  source->add_resource_path("drive_internals.js", IDR_DRIVE_INTERNALS_JS);
  source->set_default_resource(IDR_DRIVE_INTERNALS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, source);
}

}  // namespace chromeos
