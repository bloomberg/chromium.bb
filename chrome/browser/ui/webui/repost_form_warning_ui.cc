// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/repost_form_warning_ui.h"

#include <string>

#include "base/basictypes.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/repost_form_warning_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/constrained_html_ui.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/size.h"

using std::string;

namespace browser {

// Declared in browser_dialogs.h so others don't have to depend on our header.
void ShowRepostFormWarningDialog(gfx::NativeWindow parent_window,
                                 TabContents* tab_contents) {
  new RepostFormWarningUI(parent_window, tab_contents);
}

}  // namespace browser

class RepostFormWarningSource : public ChromeURLDataManager::DataSource {
 public:
  RepostFormWarningSource()
      : DataSource(chrome::kChromeUIRepostFormWarningHost,
                   MessageLoop::current()) {
  }

  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id) OVERRIDE {
    DictionaryValue dict;
    dict.SetString("explanation",
                   l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING));
    dict.SetString("resend",
                   l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING_RESEND));
    dict.SetString("cancel", l10n_util::GetStringUTF16(IDS_CANCEL));

    SetFontAndTextDirection(&dict);
    base::StringPiece html =
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_REPOST_FORM_WARNING_HTML);
    string response = jstemplate_builder::GetI18nTemplateHtml(html, &dict);
    SendResponse(request_id, base::RefCountedString::TakeString(&response));
  }

  virtual string GetMimeType(const std::string& path) const OVERRIDE {
    return "text/html";
  }

  static void RegisterDataSource(Profile* profile) {
    ChromeURLDataManager* url_manager = profile->GetChromeURLDataManager();
    url_manager->AddDataSource(new RepostFormWarningSource());
  }

 private:
  virtual ~RepostFormWarningSource() {}

  DISALLOW_COPY_AND_ASSIGN(RepostFormWarningSource);
};

class RepostFormWarningHtmlDelegate : public HtmlDialogUIDelegate {
 public:
  explicit RepostFormWarningHtmlDelegate(RepostFormWarningUI* ui) : ui_(ui) {}

  virtual ~RepostFormWarningHtmlDelegate() {}

  // HtmlDialogUIDelegate implementation.
  virtual bool IsDialogModal() const OVERRIDE {
    return true;
  }

  virtual string16 GetDialogTitle() const OVERRIDE {
    return l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING_TITLE);
  }

  virtual GURL GetDialogContentURL() const OVERRIDE {
    return GURL(chrome::kChromeUIRepostFormWarningURL);
  }

  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE {}

  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE {
    size->SetSize(kDialogWidth, kDialogHeight);
  }

  virtual std::string GetDialogArgs() const OVERRIDE {
    return string();
  }

  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE {
    bool repost = false;
    if (!json_retval.empty()) {
      base::JSONReader reader;
      scoped_ptr<Value> value(reader.JsonToValue(json_retval, false, false));
      if (!value.get() || !value->GetAsBoolean(&repost))
        NOTREACHED() << "Missing or unreadable response from dialog";
    }

    ui_->OnDialogClosed(repost);
    ui_ = NULL;
    delete this;
  }

  virtual void OnCloseContents(TabContents* source,
                               bool* out_close_dialog) OVERRIDE {}

  virtual bool ShouldShowDialogTitle() const OVERRIDE {
    return true;
  }

 private:
  static const int kDialogWidth = 400;
  static const int kDialogHeight = 108;

  RepostFormWarningUI* ui_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(RepostFormWarningHtmlDelegate);
};

RepostFormWarningUI::RepostFormWarningUI(gfx::NativeWindow parent_window,
                                         TabContents* tab_contents)
    : controller_(new RepostFormWarningController(tab_contents)) {
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
  Profile* profile = wrapper->profile();
  RepostFormWarningSource::RegisterDataSource(profile);
  RepostFormWarningHtmlDelegate* delegate =
      new RepostFormWarningHtmlDelegate(this);
  ConstrainedHtmlUI::CreateConstrainedHtmlDialog(profile, delegate, wrapper);
}

RepostFormWarningUI::~RepostFormWarningUI() {}

void RepostFormWarningUI::OnDialogClosed(bool repost) {
  if (repost)
    controller_->Continue();
  else
    controller_->Cancel();
  delete this;
}
