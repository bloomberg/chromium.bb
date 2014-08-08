// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/sync_internals_message_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/common/extension_messages.h"
#include "grit/sync_internals_resources.h"

namespace {

content::WebUIDataSource* CreateSyncInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISyncInternalsHost);

  source->SetUseJsonJSFormatV2();
  source->SetJsonPath("strings.js");
  source->AddResourcePath("sync_index.js", IDR_SYNC_INTERNALS_INDEX_JS);
  source->AddResourcePath("chrome_sync.js",
                          IDR_SYNC_INTERNALS_CHROME_SYNC_JS);
  source->AddResourcePath("types.js", IDR_SYNC_INTERNALS_TYPES_JS);
  source->AddResourcePath("sync_log.js", IDR_SYNC_INTERNALS_SYNC_LOG_JS);
  source->AddResourcePath("sync_node_browser.js",
                          IDR_SYNC_INTERNALS_SYNC_NODE_BROWSER_JS);
  source->AddResourcePath("sync_search.js",
                          IDR_SYNC_INTERNALS_SYNC_SEARCH_JS);
  source->AddResourcePath("about.js", IDR_SYNC_INTERNALS_ABOUT_JS);
  source->AddResourcePath("data.js", IDR_SYNC_INTERNALS_DATA_JS);
  source->AddResourcePath("events.js", IDR_SYNC_INTERNALS_EVENTS_JS);
  source->AddResourcePath("search.js", IDR_SYNC_INTERNALS_SEARCH_JS);
  source->SetDefaultResource(IDR_SYNC_INTERNALS_INDEX_HTML);
  return source;
}

}  // namespace

SyncInternalsUI::SyncInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateSyncInternalsHTMLSource());

  web_ui->AddMessageHandler(new SyncInternalsMessageHandler);
}

SyncInternalsUI::~SyncInternalsUI() {}

