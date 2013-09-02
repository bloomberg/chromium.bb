// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/first_run/first_run_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

namespace {

const char kFirstRunJSPath[] = "first_run.js";

content::WebUIDataSource* CreateDataSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIFirstRunHost);
  source->SetUseJsonJSFormatV2();
  source->SetJsonPath("strings.js");
  source->SetDefaultResource(IDR_FIRST_RUN_HTML);
  source->AddResourcePath(kFirstRunJSPath, IDR_FIRST_RUN_JS);
  return source;
}

}  // anonymous namespace

namespace chromeos {

FirstRunUI::FirstRunUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      actor_(NULL) {
  FirstRunHandler* handler = new FirstRunHandler();
  actor_ = handler;
  web_ui->AddMessageHandler(handler);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), CreateDataSource());
}

}  // namespace chromeos

