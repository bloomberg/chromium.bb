// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_startup_pages_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"

namespace settings {

StartupPagesHandler::StartupPagesHandler(content::WebUI* webui)
    : startup_custom_pages_table_model_(Profile::FromWebUI(webui)) {
  startup_custom_pages_table_model_.SetObserver(this);
}

StartupPagesHandler::~StartupPagesHandler() {
}

void StartupPagesHandler::RegisterMessages() {
  DCHECK(!Profile::FromWebUI(web_ui())->IsOffTheRecord());

  web_ui()->RegisterMessageCallback("setStartupPagesToCurrentPages",
      base::Bind(&StartupPagesHandler::SetStartupPagesToCurrentPages,
                 base::Unretained(this)));
}

void StartupPagesHandler::OnModelChanged() {
  base::ListValue startup_pages;
  int page_count = startup_custom_pages_table_model_.RowCount();
  std::vector<GURL> urls = startup_custom_pages_table_model_.GetURLs();
  for (int i = 0; i < page_count; ++i) {
    scoped_ptr<base::DictionaryValue> entry(new base::DictionaryValue());
    entry->SetString("title", startup_custom_pages_table_model_.GetText(i, 0));
    entry->SetString("url", urls[i].spec());
    entry->SetString("tooltip",
                     startup_custom_pages_table_model_.GetTooltip(i));
    entry->SetInteger("modelIndex", i);
    startup_pages.Append(entry.release());
  }

  web_ui()->CallJavascriptFunction("Settings.updateStartupPages",
                                   startup_pages);
}

void StartupPagesHandler::OnItemsChanged(int start, int length) {
  OnModelChanged();
}

void StartupPagesHandler::OnItemsAdded(int start, int length) {
  OnModelChanged();
}

void StartupPagesHandler::OnItemsRemoved(int start, int length) {
  OnModelChanged();
}

void StartupPagesHandler::SetStartupPagesToCurrentPages(
    const base::ListValue* args) {
  startup_custom_pages_table_model_.SetToCurrentlyOpenPages();
}

}  // namespace settings
