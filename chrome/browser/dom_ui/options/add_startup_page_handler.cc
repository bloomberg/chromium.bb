// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/add_startup_page_handler.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/possible_url_model.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

AddStartupPageHandler::AddStartupPageHandler() {
}

AddStartupPageHandler::~AddStartupPageHandler() {
  if (url_table_model_.get())
    url_table_model_->SetObserver(NULL);
}

void AddStartupPageHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  RegisterTitle(localized_strings, "addStartupPage", IDS_ASI_ADD_TITLE);
  localized_strings->SetString("addStartupPageURLLabel",
      l10n_util::GetStringUTF16(IDS_ASI_URL));
  localized_strings->SetString("addStartupPageAddButton",
      l10n_util::GetStringUTF16(IDS_ASI_ADD));
  localized_strings->SetString("addStartupPageCancelButton",
      l10n_util::GetStringUTF16(IDS_CANCEL));
}

void AddStartupPageHandler::Initialize() {
  url_table_model_.reset(new PossibleURLModel());
  if (url_table_model_.get()) {
    url_table_model_->SetObserver(this);
    url_table_model_->Reload(web_ui_->GetProfile());
  }
}

void AddStartupPageHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(
      "updateAddStartupFieldWithPage",
      NewCallback(this, &AddStartupPageHandler::UpdateFieldWithRecentPage));
}

void AddStartupPageHandler::UpdateFieldWithRecentPage(const ListValue* args) {
  int index;
  if (!ExtractIntegerValue(args, &index)) {
    NOTREACHED();
    return;
  }
  std::string languages =
      web_ui_->GetProfile()->GetPrefs()->GetString(prefs::kAcceptLanguages);
  // Because this gets parsed by FixupURL(), it's safe to omit the scheme or
  // trailing slash, and unescape most characters, but we need to not drop any
  // username/password, or unescape anything that changes the meaning.
  string16 url_string = net::FormatUrl(url_table_model_->GetURL(index),
      languages, net::kFormatUrlOmitAll & ~net::kFormatUrlOmitUsernamePassword,
      UnescapeRule::SPACES, NULL, NULL, NULL);

  scoped_ptr<Value> url_value(Value::CreateStringValue(url_string));
  web_ui_->CallJavascriptFunction(L"AddStartupPageOverlay.setInputFieldValue",
                                  *url_value.get());
}

void AddStartupPageHandler::OnModelChanged() {
  ListValue pages;
  for (int i = 0; i < url_table_model_->RowCount(); ++i) {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString("title", url_table_model_->GetText(i, IDS_ASI_PAGE_COLUMN));
    dict->SetString("displayURL",
                    url_table_model_->GetText(i, IDS_ASI_URL_COLUMN));
    dict->SetString("url", url_table_model_->GetURL(i).spec());
    pages.Append(dict);
  }

  web_ui_->CallJavascriptFunction(L"AddStartupPageOverlay.updateRecentPageList",
                                  pages);
}

void AddStartupPageHandler::OnItemsChanged(int start, int length) {
  OnModelChanged();
}

void AddStartupPageHandler::OnItemsAdded(int start, int length) {
  OnModelChanged();
}

void AddStartupPageHandler::OnItemsRemoved(int start, int length) {
  OnModelChanged();
}
