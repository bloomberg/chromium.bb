// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "ui/base/webui/web_ui_util.h"

SyncConfirmationUI::SyncConfirmationUI(content::WebUI* web_ui)
    : SyncConfirmationUI(web_ui, new SyncConfirmationHandler) {}

SyncConfirmationUI::SyncConfirmationUI(
    content::WebUI* web_ui, SyncConfirmationHandler* handler)
    : WebDialogUI(web_ui){
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISyncConfirmationHost);
  source->SetJsonPath("strings.js");
  source->SetDefaultResource(IDR_SYNC_CONFIRMATION_HTML);
  source->AddResourcePath("sync_confirmation.css", IDR_SYNC_CONFIRMATION_CSS);
  source->AddResourcePath("sync_confirmation.js", IDR_SYNC_CONFIRMATION_JS);

  source->AddLocalizedString("syncConfirmationTitle",
      IDS_SYNC_CONFIRMATION_TITLE);
  source->AddLocalizedString("syncConfirmationBody",
      IDS_SYNC_CONFIRMATION_BODY);
  source->AddLocalizedString("syncConfirmationConfirmLabel",
      IDS_SYNC_CONFIRMATION_CONFIRM_BUTTON_LABEL);
  source->AddLocalizedString("syncConfirmationUndoLabel",
      IDS_SYNC_CONFIRMATION_UNDO_BUTTON_LABEL);

  base::DictionaryValue strings;
  webui::SetLoadTimeDataDefaults(
      g_browser_process->GetApplicationLocale(), &strings);
  source->AddLocalizedStrings(strings);

  content::WebUIDataSource::Add(profile, source);
  web_ui->AddMessageHandler(handler);
}
