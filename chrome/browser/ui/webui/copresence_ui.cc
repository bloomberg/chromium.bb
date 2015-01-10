// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/copresence_ui.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/webui/copresence_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

using content::WebUIDataSource;

namespace {

scoped_ptr<WebUIDataSource> CreateDataSource() {
  scoped_ptr<WebUIDataSource> data_source(
      WebUIDataSource::Create(chrome::kChromeUICopresenceHost));

  data_source->AddLocalizedString("title", IDS_COPRESENCE_TITLE);

  data_source->AddLocalizedString("directives_title",
                                  IDS_COPRESENCE_DIRECTIVES_TITLE);
  data_source->AddLocalizedString("directive_type",
                                  IDS_COPRESENCE_DIRECTIVE_TYPE);
  data_source->AddLocalizedString("token_medium", IDS_COPRESENCE_TOKEN_MEDIUM);
  data_source->AddLocalizedString("duration", IDS_COPRESENCE_DURATION);

  data_source->AddLocalizedString("transmitted_tokens_title",
                                  IDS_COPRESENCE_TRANSMITTED_TOKENS_TITLE);
  data_source->AddLocalizedString("received_tokens_title",
                                  IDS_COPRESENCE_RECEIVED_TOKENS_TITLE);
  data_source->AddLocalizedString("token_id",
                                  IDS_COPRESENCE_TOKEN_ID);
  data_source->AddLocalizedString("token_status",
                                  IDS_COPRESENCE_TOKEN_STATUS);
  data_source->AddLocalizedString("token_transmit_time",
                                  IDS_COPRESENCE_TOKEN_TRANSMIT_TIME);
  data_source->AddLocalizedString("token_receive_time",
                                  IDS_COPRESENCE_TOKEN_RECEIVE_TIME);

  data_source->AddLocalizedString("clear_state",
                                  IDS_COPRESENCE_CLEAR_STATE);
  data_source->AddLocalizedString("confirm_delete",
                                  IDS_COPRESENCE_CONFIRM_DELETE);

  data_source->SetJsonPath("strings.js");
  data_source->AddResourcePath("copresence.css", IDR_COPRESENCE_CSS);
  data_source->AddResourcePath("copresence.js", IDR_COPRESENCE_JS);
  data_source->SetDefaultResource(IDR_COPRESENCE_HTML);

  return data_source.Pass();
}

}  // namespace

CopresenceUI::CopresenceUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // This call takes ownership of the WebUIMessageHandler.
  web_ui->AddMessageHandler(new CopresenceUIHandler(web_ui));

  // This call takes ownership of the WebUIDataSource.
  WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                       CreateDataSource().release());
}

CopresenceUI::~CopresenceUI() {}
