// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_feedback/md_feedback_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

#if !defined(OS_MACOSX)
#include "chrome/browser/ui/browser_dialogs.h"
#endif

namespace {

content::WebUIDataSource* CreateMdFeedbackUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIFeedbackHost);

  // General strings.
  html_source->AddLocalizedString("headingText",
                                  IDS_MD_FEEDBACK_HEADING);
#if defined(GOOGLE_CHROME_BUILD)
  html_source->AddLocalizedString("privacyNote",
                                  IDS_MD_FEEDBACK_PRIVACY_NOTE);
#endif

  // Input labels.
  html_source->AddLocalizedString("emailLabel",
                                  IDS_MD_FEEDBACK_USER_EMAIL_LABEL);
  html_source->AddLocalizedString("openEndedLabel",
                                  IDS_MD_FEEDBACK_OPEN_ENDED_LABEL);
  html_source->AddLocalizedString("urlLabel",
                                  IDS_MD_FEEDBACK_URL_LABEL);

  // Buttons.
  html_source->AddLocalizedString("cancelButton",
                                  IDS_MD_FEEDBACK_CANCEL_BUTTON);
  html_source->AddLocalizedString("sendReportButton",
                                  IDS_MD_FEEDBACK_SEND_REPORT_BUTTON);

  // Checkbox labels.
  html_source->AddLocalizedString("includeScreenshotLabel",
                                  IDS_MD_FEEDBACK_SCREENSHOT_LABEL);
  html_source->AddLocalizedString("sendSystemInfoLabel",
                                  IDS_MD_FEEDBACK_SEND_SYSTEM_INFO_LABEL);

  // Polymer resources.
  html_source->AddResourcePath("feedback_container.html",
        IDR_MD_FEEDBACK_FEEDBACK_CONTAINER_HTML);
  html_source->AddResourcePath("feedback_container.js",
        IDR_MD_FEEDBACK_FEEDBACK_CONTAINER_JS);

  html_source->SetJsonPath("strings.js");
  html_source->SetDefaultResource(IDR_MD_FEEDBACK_FEEDBACK_HTML);
  return html_source;
}

}  // namespace

MdFeedbackUI::MdFeedbackUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://feedback data html_source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      CreateMdFeedbackUIHTMLSource(profile);
  content::WebUIDataSource::Add(profile, html_source);
}

MdFeedbackUI::~MdFeedbackUI() {}
