// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/input_window_dialog_webui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/dialog_style.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const int kInputWindowDialogWidth = 300;
const int kInputWindowDialogBaseHeight = 90;
const int kInputWindowDialogContentsHeight = 20;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// InputWindowWebUIDailog private methods

InputWindowDialogWebUI::InputWindowDialogWebUI(
    const string16& window_title,
    const LabelContentsPairs& label_contents_pairs,
    InputWindowDialog::Delegate* delegate,
    ButtonType type)
    : handler_(new InputWindowDialogHandler(delegate)),
      window_title_(window_title),
      label_contents_pairs_(label_contents_pairs),
      closed_(true),
      delegate_(delegate),
      type_(type) {
}

InputWindowDialogWebUI::~InputWindowDialogWebUI() {
}

void InputWindowDialogWebUI::Show() {
  Browser* browser = BrowserList::GetLastActive();
  DCHECK(browser);
  browser->BrowserShowHtmlDialog(this, NULL, STYLE_GENERIC);
  closed_ = false;
}

void InputWindowDialogWebUI::Close() {
  if (!closed_) {
    DCHECK(handler_);
    handler_->CloseDialog();
  }
}

bool InputWindowDialogWebUI::IsDialogModal() const {
  return true;
}

string16 InputWindowDialogWebUI::GetDialogTitle() const {
  return window_title_;
}

GURL InputWindowDialogWebUI::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIInputWindowDialogURL);
}

void InputWindowDialogWebUI::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  handlers->push_back(handler_);
}

void InputWindowDialogWebUI::GetDialogSize(gfx::Size* size) const {
  const int height = kInputWindowDialogBaseHeight +
      kInputWindowDialogContentsHeight * label_contents_pairs_.size();
  size->SetSize(kInputWindowDialogWidth, height);
}

std::string InputWindowDialogWebUI::GetDialogArgs() const {
  DictionaryValue value;
  value.SetString("nameLabel", label_contents_pairs_[0].first);
  value.SetString("name", label_contents_pairs_[0].second);
  if (label_contents_pairs_.size() == 2) {
    value.SetString("urlLabel", label_contents_pairs_[1].first);
    value.SetString("url", label_contents_pairs_[1].second);
  }
  string16 ok_button_title = l10n_util::GetStringUTF16(
      type_ == BUTTON_TYPE_ADD ? IDS_ADD : IDS_SAVE);
  value.SetString("ok_button_title", ok_button_title);
  std::string json;
  base::JSONWriter::Write(&value, false, &json);
  return json;
}

void InputWindowDialogWebUI::OnDialogClosed(const std::string& json_retval) {
  scoped_ptr<Value> root(base::JSONReader::Read(json_retval, false));
  bool response = false;
  ListValue* list = NULL;
  bool accepted = false;
  // If the dialog closes because of a button click then the json is a list
  // containing a bool value.  When OK button is clicked, a string in the text
  // field is added to the list.
  if (root.get() && root->GetAsList(&list) && list &&
      list->GetBoolean(0, &response) && response) {
    DCHECK(list->GetSize() == 2 || list->GetSize() == 3);
    InputTexts texts;
    string16 name;
    if (list->GetString(1, &name)) {
      texts.push_back(name);
    }
    string16 url;
    if (list->GetSize() == 3 && list->GetString(2, &url)) {
      texts.push_back(url);
    }
    if (delegate_->IsValid(texts)) {
      delegate_->InputAccepted(texts);
      accepted = true;
    }
  }
  if (!accepted) {
    delegate_->InputCanceled();
  }
  closed_ = true;
}

void InputWindowDialogWebUI::OnCloseContents(TabContents* source,
                                             bool* out_close_dialog) {
}

bool InputWindowDialogWebUI::ShouldShowDialogTitle() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// InputWindowDialogHandler methods

InputWindowDialogHandler::InputWindowDialogHandler(
    InputWindowDialog::Delegate* delegate)
    : delegate_(delegate) {
}

void InputWindowDialogHandler::CloseDialog() {
  DCHECK(web_ui_);
  static_cast<HtmlDialogUI*>(web_ui_)->CloseDialog(NULL);
}

void InputWindowDialogHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("validate",
      base::Bind(&InputWindowDialogHandler::Validate,
                 base::Unretained(this)));
}

void InputWindowDialogHandler::Validate(const base::ListValue* args) {
  DCHECK(args->GetSize() == 1U || args->GetSize() == 2U);
  InputWindowDialog::InputTexts texts;
  string16 name;
  if (args->GetString(0, &name)) {
    texts.push_back(name);
  }
  string16 url;
  if (args->GetSize() == 2 && args->GetString(0, &url)) {
    texts.push_back(url);
  }
  const bool valid = delegate_->IsValid(texts);
  scoped_ptr<Value> result(Value::CreateBooleanValue(valid));
  web_ui_->CallJavascriptFunction("inputWindowDialog.ackValidation",
                                  *result);
}
