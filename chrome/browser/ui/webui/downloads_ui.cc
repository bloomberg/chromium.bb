// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads_ui.h"

#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/downloads_dom_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
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
  source->AddLocalizedString("searchbutton", IDS_DOWNLOAD_SEARCH_BUTTON);
  source->AddLocalizedString("searchresultsfor", IDS_DOWNLOAD_SEARCHRESULTSFOR);
  source->AddLocalizedString("downloads", IDS_DOWNLOAD_TITLE);
  source->AddLocalizedString("clear_all", IDS_DOWNLOAD_LINK_CLEAR_ALL);
  source->AddLocalizedString("open_downloads_folder",
                             IDS_DOWNLOAD_LINK_OPEN_DOWNLOADS_FOLDER);

  // Status.
  source->AddLocalizedString("status_cancelled", IDS_DOWNLOAD_TAB_CANCELLED);
  source->AddLocalizedString("status_removed", IDS_DOWNLOAD_FILE_REMOVED);
  source->AddLocalizedString("status_paused", IDS_DOWNLOAD_PROGRESS_PAUSED);

  // Dangerous file.
  source->AddLocalizedString("danger_file_desc", IDS_PROMPT_DANGEROUS_DOWNLOAD);
  source->AddLocalizedString("danger_url_desc",
                             IDS_PROMPT_MALICIOUS_DOWNLOAD_URL);
  source->AddLocalizedString("danger_content_desc",
                             IDS_PROMPT_MALICIOUS_DOWNLOAD_CONTENT);
  source->AddLocalizedString("danger_uncommon_desc",
                             IDS_PROMPT_UNCOMMON_DOWNLOAD_CONTENT);
  source->AddLocalizedString("danger_settings_desc",
                             IDS_PROMPT_DOWNLOAD_CHANGES_SETTINGS);
  source->AddLocalizedString("danger_save", IDS_CONFIRM_DOWNLOAD);
  source->AddLocalizedString("danger_restore", IDS_CONFIRM_DOWNLOAD_RESTORE);
  source->AddLocalizedString("danger_discard", IDS_DISCARD_DOWNLOAD);

  // Controls.
  source->AddLocalizedString("control_pause", IDS_DOWNLOAD_LINK_PAUSE);
  if (browser_defaults::kDownloadPageHasShowInFolder) {
    source->AddLocalizedString("control_showinfolder", IDS_DOWNLOAD_LINK_SHOW);
  }
  source->AddLocalizedString("control_retry", IDS_DOWNLOAD_LINK_RETRY);
  source->AddLocalizedString("control_cancel", IDS_DOWNLOAD_LINK_CANCEL);
  source->AddLocalizedString("control_resume", IDS_DOWNLOAD_LINK_RESUME);
  source->AddLocalizedString("control_removefromlist",
                             IDS_DOWNLOAD_LINK_REMOVE);
  source->AddLocalizedString("control_by_extension",
                             IDS_DOWNLOAD_BY_EXTENSION);

  PrefService* prefs = profile->GetPrefs();
  source->AddBoolean("allow_deleting_history",
                     prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory));
  source->AddBoolean("show_delete_history", !profile->IsSupervised());

  source->SetJsonPath("strings.js");
  source->AddResourcePath("downloads.css", IDR_DOWNLOADS_CSS);
  source->AddResourcePath("downloads.js", IDR_DOWNLOADS_JS);
  source->SetDefaultResource(IDR_DOWNLOADS_HTML);
  source->SetUseJsonJSFormatV2();

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
