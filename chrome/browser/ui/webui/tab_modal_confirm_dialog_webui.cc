// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  new TabModalConfirmDialogWebUI(delegate, wrapper);
}

}  // namespace browser

const int kDialogWidth = 400;
const int kDialogHeight = 120;

TabModalConfirmDialogWebUI::TabModalConfirmDialogWebUI(
    TabModalConfirmDialogDelegate* delegate,
    TabContentsWrapper* wrapper)
    : delegate_(delegate) {
  Profile* profile = wrapper->profile();
  ChromeWebUIDataSource* data_source =
      new ChromeWebUIDataSource(chrome::kChromeUITabModalConfirmDialogHost);
  data_source->set_default_resource(IDR_TAB_MODAL_CONFIRM_DIALOG_HTML);
  profile->GetChromeURLDataManager()->AddDataSource(data_source);

  constrained_html_ui_delegate_ =
      ConstrainedHtmlUI::CreateConstrainedHtmlDialog(profile, this, wrapper);
  delegate_->set_window(constrained_html_ui_delegate_->window());
}

TabModalConfirmDialogWebUI::~TabModalConfirmDialogWebUI() {}

bool TabModalConfirmDialogWebUI::IsDialogModal() const {
  return true;
}

string16 TabModalConfirmDialogWebUI::GetDialogTitle() const {
  return delegate_->GetTitle();
}

GURL TabModalConfirmDialogWebUI::GetDialogContentURL() const {
  return GURL(chrome::kChromeUITabModalConfirmDialogURL);
}

void TabModalConfirmDialogWebUI::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {}

void TabModalConfirmDialogWebUI::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDialogWidth, kDialogHeight);
}

std::string TabModalConfirmDialogWebUI::GetDialogArgs() const {
  DictionaryValue dict;
  dict.SetString("message", delegate_->GetMessage());
  dict.SetString("accept", delegate_->GetAcceptButtonTitle());
  dict.SetString("cancel", delegate_->GetCancelButtonTitle());
  ChromeWebUIDataSource::SetFontAndTextDirection(&dict);
  std::string json;
  base::JSONWriter::Write(&dict, false, &json);
  return json;
}

void TabModalConfirmDialogWebUI::OnDialogClosed(
    const std::string& json_retval) {
  bool accepted = false;
  if (!json_retval.empty()) {
    base::JSONReader reader;
    scoped_ptr<Value> value(reader.JsonToValue(json_retval, false, false));
    if (!value.get() || !value->GetAsBoolean(&accepted))
      NOTREACHED() << "Missing or unreadable response from dialog";
  }

  delegate_->set_window(NULL);
  if (accepted)
    delegate_->Accept();
  else
    delegate_->Cancel();
  delete this;
}

void TabModalConfirmDialogWebUI::OnCloseContents(WebContents* source,
                                                 bool* out_close_dialog) {}

bool TabModalConfirmDialogWebUI::ShouldShowDialogTitle() const {
  return true;
}
