// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads_ui.h"

#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/downloads_dom_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserContext;
using content::DownloadManager;
using content::WebContents;

namespace {

content::WebUIDataSource* CreateDownloadsUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIDownloadsHost);

  source->AddLocalizedString("title", IDS_DOWNLOAD_TITLE);
  source->AddLocalizedString("searchResultsFor", IDS_DOWNLOAD_SEARCHRESULTSFOR);
  source->AddLocalizedString("downloads", IDS_DOWNLOAD_TITLE);
  source->AddLocalizedString("clearAll", IDS_DOWNLOAD_LINK_CLEAR_ALL);
  source->AddLocalizedString("openDownloadsFolder",
                             IDS_DOWNLOAD_LINK_OPEN_DOWNLOADS_FOLDER);
  source->AddLocalizedString("searchButton", IDS_DOWNLOAD_SEARCH_BUTTON);

  // No results message that shows instead of the downloads list.
  source->AddLocalizedString("noDownloads", IDS_DOWNLOAD_NO_DOWNLOADS);
  source->AddLocalizedString("noSearchResults",
                             IDS_DOWNLOAD_NO_SEARCH_RESULTS);

  // Status.
  source->AddLocalizedString("statusCancelled", IDS_DOWNLOAD_TAB_CANCELLED);
  source->AddLocalizedString("statusRemoved", IDS_DOWNLOAD_FILE_REMOVED);

  // Dangerous file.
  source->AddLocalizedString("dangerFileDesc", IDS_PROMPT_DANGEROUS_DOWNLOAD);
  source->AddLocalizedString("dangerUrlDesc",
                             IDS_PROMPT_MALICIOUS_DOWNLOAD_URL);
  source->AddLocalizedString("dangerContentDesc",
                             IDS_PROMPT_MALICIOUS_DOWNLOAD_CONTENT);
  source->AddLocalizedString("dangerUncommonDesc",
                             IDS_PROMPT_UNCOMMON_DOWNLOAD_CONTENT);
  source->AddLocalizedString("dangerSettingsDesc",
                             IDS_PROMPT_DOWNLOAD_CHANGES_SETTINGS);
  source->AddLocalizedString("dangerSave", IDS_CONFIRM_DOWNLOAD);
  source->AddLocalizedString("dangerRestore", IDS_CONFIRM_DOWNLOAD_RESTORE);
  source->AddLocalizedString("dangerDiscard", IDS_DISCARD_DOWNLOAD);

  // Controls.
  source->AddLocalizedString("controlPause", IDS_DOWNLOAD_LINK_PAUSE);
  if (browser_defaults::kDownloadPageHasShowInFolder)
    source->AddLocalizedString("controlShowInFolder", IDS_DOWNLOAD_LINK_SHOW);
  source->AddLocalizedString("controlCancel", IDS_DOWNLOAD_LINK_CANCEL);
  source->AddLocalizedString("controlResume", IDS_DOWNLOAD_LINK_RESUME);
  source->AddLocalizedString("controlRemoveFromList",
                             IDS_DOWNLOAD_LINK_REMOVE);
  source->AddLocalizedString("controlRetry", IDS_DOWNLOAD_LINK_RETRY);
  source->AddLocalizedString("controlByExtension",
                             IDS_DOWNLOAD_BY_EXTENSION);

  PrefService* prefs = profile->GetPrefs();
  source->AddBoolean("allowDeletingHistory",
                     prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory) &&
                     !profile->IsSupervised());

  source->AddResourcePath("constants.html", IDR_DOWNLOADS_CONSTANTS_HTML);
  source->AddResourcePath("constants.js", IDR_DOWNLOADS_CONSTANTS_JS);
  source->AddResourcePath("item_view.js", IDR_DOWNLOADS_ITEM_VIEW_JS);
  source->AddResourcePath("focus_row.js", IDR_DOWNLOADS_FOCUS_ROW_JS);
  source->AddResourcePath("manager.js", IDR_DOWNLOADS_MANAGER_JS);
  source->AddResourcePath("throttled_icon_loader.html",
                          IDR_DOWNLOADS_THROTTLED_ICON_LOADER_HTML);
  source->AddResourcePath("throttled_icon_loader.js",
                          IDR_DOWNLOADS_THROTTLED_ICON_LOADER_JS);

  source->SetDefaultResource(IDR_DOWNLOADS_DOWNLOADS_HTML);
  source->SetJsonPath("strings.js");

  return source;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// DownloadsUI
//
///////////////////////////////////////////////////////////////////////////////

DownloadsUI::DownloadsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  DownloadManager* dlm = BrowserContext::GetDownloadManager(profile);

  DownloadsDOMHandler* handler = new DownloadsDOMHandler(dlm);
  web_ui->AddMessageHandler(handler);

  // Set up the chrome://downloads/ source.
  content::WebUIDataSource* source = CreateDownloadsUIHTMLSource(profile);
  content::WebUIDataSource::Add(profile, source);
#if defined(ENABLE_THEMES)
  ThemeSource* theme = new ThemeSource(profile);
  content::URLDataSource::Add(profile, theme);
#endif
}

// static
base::RefCountedMemory* DownloadsUI::GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor) {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytesForScale(IDR_DOWNLOADS_FAVICON, scale_factor);
}
