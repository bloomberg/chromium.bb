// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_modal_confirm_dialog_webui.h"

#include <string>

#include "base/basictypes.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/constrained_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
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

using content::WebContents;
using content::WebUIMessageHandler;

namespace browser {

// Declared in browser_dialogs.h so others don't have to depend on our header.
void ShowTabModalConfirmDialog(TabModalConfirmDialogDelegate* delegate,
                               TabContentsWrapper* wrapper) {
  new TabModalConfirmDialogUI(delegate, wrapper);
}

}  // namespace browser

class TabModalConfirmDialogHtmlDelegate : public HtmlDialogUIDelegate {
 public:
  TabModalConfirmDialogHtmlDelegate(
      TabModalConfirmDialogUI* ui,
      TabModalConfirmDialogDelegate* dialog_delegate)
      : ui_(ui),
        dialog_delegate_(dialog_delegate) {}

  virtual ~TabModalConfirmDialogHtmlDelegate() {}

  // HtmlDialogUIDelegate implementation.
  virtual bool IsDialogModal() const OVERRIDE {
    return true;
  }

  virtual string16 GetDialogTitle() const OVERRIDE {
    return dialog_delegate_->GetTitle();
  }

  virtual GURL GetDialogContentURL() const OVERRIDE {
    return GURL(chrome::kChromeUITabModalConfirmDialogURL);
  }

  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE {}

  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE {
    size->SetSize(kDialogWidth, kDialogHeight);
  }

  virtual std::string GetDialogArgs() const OVERRIDE {
    DictionaryValue dict;
    dict.SetString("message", dialog_delegate_->GetMessage());
    dict.SetString("accept", dialog_delegate_->GetAcceptButtonTitle());
    dict.SetString("cancel", dialog_delegate_->GetCancelButtonTitle());
    ChromeWebUIDataSource::SetFontAndTextDirection(&dict);
    std::string json;
    base::JSONWriter::Write(&dict, false, &json);
    return json;
  }

  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE {
    bool accepted = false;
    if (!json_retval.empty()) {
      base::JSONReader reader;
      scoped_ptr<Value> value(reader.JsonToValue(json_retval, false, false));
      DCHECK(value.get() && value->GetAsBoolean(&accepted))
          << "Missing or unreadable response from dialog";
    }

    ui_->OnDialogClosed(accepted);
    delete this;
  }

  virtual void OnCloseContents(WebContents* source,
                               bool* out_close_dialog) OVERRIDE {}

  virtual bool ShouldShowDialogTitle() const OVERRIDE {
    return true;
  }

 private:
  static const int kDialogWidth = 400;
  static const int kDialogHeight = 120;

  scoped_ptr<TabModalConfirmDialogUI> ui_;
  // Owned by TabModalConfirmDialogUI, which we own.
  TabModalConfirmDialogDelegate* dialog_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TabModalConfirmDialogHtmlDelegate);
};

TabModalConfirmDialogUI::TabModalConfirmDialogUI(
    TabModalConfirmDialogDelegate* delegate,
    TabContentsWrapper* wrapper)
    : delegate_(delegate) {
  Profile* profile = wrapper->profile();
  ChromeWebUIDataSource* data_source =
      new ChromeWebUIDataSource(chrome::kChromeUITabModalConfirmDialogHost);
  data_source->set_default_resource(IDR_TAB_MODAL_CONFIRM_DIALOG_HTML);
  profile->GetChromeURLDataManager()->AddDataSource(data_source);

  TabModalConfirmDialogHtmlDelegate* html_delegate =
      new TabModalConfirmDialogHtmlDelegate(this, delegate);
  ConstrainedHtmlUIDelegate* dialog_delegate =
      ConstrainedHtmlUI::CreateConstrainedHtmlDialog(profile, html_delegate,
                                                     wrapper);
  delegate_->set_window(dialog_delegate->window());
}

TabModalConfirmDialogUI::~TabModalConfirmDialogUI() {}

void TabModalConfirmDialogUI::OnDialogClosed(bool accepted) {
  delegate_->set_window(NULL);
  if (accepted)
    delegate_->Accept();
  else
    delegate_->Cancel();
}
