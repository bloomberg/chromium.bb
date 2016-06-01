// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_feedback/md_feedback_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

MdFeedbackUI::MdFeedbackUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://feedback data html_source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIFeedbackHost);
  html_source->SetDefaultResource(IDR_MD_FEEDBACK_FEEDBACK_HTML);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);
}

MdFeedbackUI::~MdFeedbackUI() {}
