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
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace {

const int kInputWindowDialogWidth = 240;
const int kInputWindowDialogHeight = 120;

}

////////////////////////////////////////////////////////////////////////////////
// InputWindowWebUIDailog private methods

InputWindowDialogWebUI::InputWindowDialogWebUI(
    const string16& window_title,
    const string16& label,
    const string16& contents,
    InputWindowDialog::Delegate* delegate)
    : handler_(new InputWindowDialogHandler(delegate)),
      window_title_(window_title),
      label_(label),
      contents_(contents),
      closed_(true),
      delegate_(delegate) {
}

InputWindowDialogWebUI::~InputWindowDialogWebUI() {
}

void InputWindowDialogWebUI::Show() {
  Browser* browser = BrowserList::GetLastActive();
  DCHECK(browser);
  browser->BrowserShowHtmlDialog(this, NULL);
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
  size->SetSize(kInputWindowDialogWidth, kInputWindowDialogHeight);
}

std::string InputWindowDialogWebUI::GetDialogArgs() const {
  DictionaryValue value;
  value.SetString("label", label_);
  value.SetString("contents", contents_);
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
    DCHECK_EQ(2U, list->GetSize());
    string16 text;
    if (list->GetString(1, &text)) {
      delegate_->InputAccepted(text);
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
  DCHECK_EQ(1U, args->GetSize());
  bool valid = false;
  string16 text;
  if (args->GetString(0, &text)) {
    valid = delegate_->IsValid(text);
  }
  scoped_ptr<Value> result(Value::CreateBooleanValue(valid));
  web_ui_->CallJavascriptFunction("inputWindowDialog.ackValidation",
                                  *result);
}
