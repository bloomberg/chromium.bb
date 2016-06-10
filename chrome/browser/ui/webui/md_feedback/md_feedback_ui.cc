// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_feedback/md_feedback_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

#if !defined(OS_MACOSX)
#include "chrome/browser/ui/browser_dialogs.h"
#endif

namespace {

// The WebDialogDelegate that specifies what the MD Feedback dialog will look
// like.
class MdFeedbackDialogDelegate : public ui::WebDialogDelegate {
 public:
  MdFeedbackDialogDelegate() {}
  ~MdFeedbackDialogDelegate() override {}

  ui::ModalType GetDialogModalType() const override {
    return ui::MODAL_TYPE_SYSTEM;
  }

  base::string16 GetDialogTitle() const override {
    return base::string16();
  }

  GURL GetDialogContentURL() const override {
    return GURL(chrome::kChromeUIFeedbackURL);
  }

  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override {}

  void GetDialogSize(gfx::Size* size) const override {
    // TODO(apacible): Update when WebUI sizing behavior is finalized.
    size->SetSize(400, 600);
  }

  std::string GetDialogArgs() const override {
    return std::string();
  }

  void OnDialogClosed(const std::string& json_retval) override {
    delete this;
  }

  void OnCloseContents(
      content::WebContents* source, bool* out_close_dialog) override {}

  bool ShouldShowDialogTitle() const override {
    return false;
  }

  bool HandleContextMenu(const content::ContextMenuParams& params) override {
    // Do not show the contextual menu.
    return true;
  }
};

}  // namespace

namespace {

content::WebUIDataSource* CreateMdFeedbackUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIFeedbackHost);

  // General strings.
  html_source->AddLocalizedString("headingText",
                                  IDS_MD_FEEDBACK_HEADING);

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

void ShowFeedbackWebDialog(
    content::BrowserContext* browser_context) {
  // TODO(apacible): Platform dependent implementations.
#if !defined(OS_MACOSX)
  // TODO(apacible): If a feedback dialog is already open, bring that dialog
  // to the front rather than creating a new dialog.
  chrome::ShowWebDialog(NULL, browser_context, new MdFeedbackDialogDelegate());
#endif  // !OS_MACOSX
}
