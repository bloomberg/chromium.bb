// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/edit_search_engine_dialog_webui.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/string16.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/search_engines/edit_search_engine_controller.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/url_constants.h"
#include "grit/ui_resources.h"
#include "grit/theme_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const int kEditSearchEngineDialogWidth = 394;
const int kEditSearchEngineDialogHeight = 180;
}

////////////////////////////////////////////////////////////////////////////////
// Browser dialog API implementation

namespace browser {
void ConfirmAddSearchProvider(const TemplateURL* template_url,
                              Profile* profile) {
  EditSearchEngineDialogWebUI::ShowEditSearchEngineDialog(template_url,
                                                          profile);
}
}  // namespace browser

////////////////////////////////////////////////////////////////////////////////
// EditSearchEngineDialogWebUI

void EditSearchEngineDialogWebUI::ShowEditSearchEngineDialog(
    const TemplateURL* template_url,
    Profile* profile) {
  EditSearchEngineDialogWebUI* dialog = new EditSearchEngineDialogWebUI(
    new EditSearchEngineDialogHandlerWebUI(template_url, profile));
  dialog->ShowDialog();
}

EditSearchEngineDialogWebUI::EditSearchEngineDialogWebUI(
    EditSearchEngineDialogHandlerWebUI* handler)
    : handler_(handler) {
}

void EditSearchEngineDialogWebUI::ShowDialog() {
  Browser* browser = BrowserList::GetLastActive();
  DCHECK(browser);
  browser->BrowserShowHtmlDialog(this, NULL);
}

// HtmlDialogUIDelegate methods
bool EditSearchEngineDialogWebUI::IsDialogModal() const {
  return true;
}

string16 EditSearchEngineDialogWebUI::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(handler_->IsAdding() ?
                                   IDS_SEARCH_ENGINES_EDITOR_NEW_WINDOW_TITLE :
                                   IDS_SEARCH_ENGINES_EDITOR_EDIT_WINDOW_TITLE);
}

GURL EditSearchEngineDialogWebUI::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIEditSearchEngineDialogURL);
}

void EditSearchEngineDialogWebUI::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  handlers->push_back(handler_);
}

void EditSearchEngineDialogWebUI::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kEditSearchEngineDialogWidth, kEditSearchEngineDialogHeight);
}

std::string EditSearchEngineDialogWebUI::GetDialogArgs() const {
  return handler_->IsAdding() ? "add" : "edit";
}

void EditSearchEngineDialogWebUI::OnDialogClosed(
    const std::string& json_retval) {
  handler_->OnDialogClosed(json_retval);
  delete this;
}

void EditSearchEngineDialogWebUI::OnCloseContents(TabContents* source,
                                             bool* out_close_dialog) {
  NOTIMPLEMENTED();
}

bool EditSearchEngineDialogWebUI::ShouldShowDialogTitle() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// EditSearchEngineDialogHandlerWebUI

EditSearchEngineDialogHandlerWebUI::EditSearchEngineDialogHandlerWebUI(
    const TemplateURL* template_url,
    Profile* profile)
    : template_url_(template_url),
      profile_(profile),
      controller_(new EditSearchEngineController(template_url, NULL, profile)) {
   // Make the chrome://theme/ resource available.
   profile->GetChromeURLDataManager()->AddDataSource(new ThemeSource(profile));
}

EditSearchEngineDialogHandlerWebUI::~EditSearchEngineDialogHandlerWebUI() {
}

// Overridden from WebUIMessageHandler
void EditSearchEngineDialogHandlerWebUI::RegisterMessages() {
  web_ui_->RegisterMessageCallback("requestDetails",
      base::Bind(&EditSearchEngineDialogHandlerWebUI::RequestDetails,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("requestValidation",
      base::Bind(&EditSearchEngineDialogHandlerWebUI::RequestValidation,
                 base::Unretained(this)));
}

void EditSearchEngineDialogHandlerWebUI::RequestDetails(
    const base::ListValue* args) {
  DictionaryValue dict;
  dict.SetString("description", controller_->template_url()->short_name());
  dict.SetString("keyword", controller_->template_url()->keyword());
  dict.SetString("url", controller_->template_url()->url()->DisplayURL());

  // Send list of tab contents details to javascript.
  web_ui_->CallJavascriptFunction("editSearchEngineDialog.setDetails",
                                  dict);
}

void EditSearchEngineDialogHandlerWebUI::RequestValidation(
    const base::ListValue* args) {
  string16 description_str;
  string16 keyword_str;
  std::string url_str;
  if (args && args->GetString(0, &description_str) &&
      args->GetString(1, &keyword_str) &&
      args->GetString(2, &url_str)) {
    DictionaryValue validation;
    bool isDescriptionValid = controller_->IsTitleValid(description_str);
    bool isKeywordValid = controller_->IsKeywordValid(keyword_str);
    bool isUrlValid = controller_->IsURLValid(url_str);
    validation.SetBoolean("description", isDescriptionValid );
    validation.SetBoolean("keyword", isKeywordValid );
    validation.SetBoolean("url", isUrlValid );
    validation.SetBoolean("ok", isDescriptionValid && isKeywordValid &&
                          isUrlValid );
    web_ui_->CallJavascriptFunction("editSearchEngineDialog.setValidation",
                                    validation);
  }
}

// Returns true if adding. Returns false if editing.
bool EditSearchEngineDialogHandlerWebUI::IsAdding() {
  return !template_url_;
}

void EditSearchEngineDialogHandlerWebUI::OnDialogClosed(
    const std::string& json_retval) {
  scoped_ptr<Value> root(base::JSONReader::Read(json_retval, false));
  ListValue* list = NULL;
  DictionaryValue* dict = NULL;
  if (root.get() && root->GetAsList(&list) && list &&
      list->GetDictionary(0, &dict) && dict) {
    string16 description_str;
    string16 keyword_str;
    std::string url_str;
    if (dict->GetString("description", &description_str) &&
        dict->GetString("keyword", &keyword_str) &&
        dict->GetString("url", &url_str)) {
      controller_->AcceptAddOrEdit(description_str,
                                   keyword_str,
                                   url_str);
    } else {
      controller_->CleanUpCancelledAdd();
    }
  }
}
