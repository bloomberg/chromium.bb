// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads_ui.h"

#include "base/memory/singleton.h"
#include "base/string_piece.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/downloads_dom_handler.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

static const char kStringsJsFile[] = "strings.js";
static const char kDownloadsJsFile[]  = "downloads.js";

namespace {

///////////////////////////////////////////////////////////////////////////////
//
// DownloadsUIHTMLSource
//
///////////////////////////////////////////////////////////////////////////////

class DownloadsUIHTMLSource : public ChromeWebUIDataSource {
 public:
  DownloadsUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);

  virtual std::string GetMimeType(const std::string&) const;

  static int PathToIDR(const std::string& path);

 private:
  ~DownloadsUIHTMLSource() {}
  DISALLOW_COPY_AND_ASSIGN(DownloadsUIHTMLSource);
};

DownloadsUIHTMLSource::DownloadsUIHTMLSource()
    : ChromeWebUIDataSource(chrome::kChromeUIDownloadsHost) {
  AddLocalizedString("title", IDS_DOWNLOAD_TITLE);
  AddLocalizedString("searchbutton", IDS_DOWNLOAD_SEARCH_BUTTON);
  AddLocalizedString("no_results", IDS_DOWNLOAD_SEARCH_BUTTON);
  AddLocalizedString("searchresultsfor", IDS_DOWNLOAD_SEARCHRESULTSFOR);
  AddLocalizedString("downloads", IDS_DOWNLOAD_TITLE);
  AddLocalizedString("clear_all", IDS_DOWNLOAD_LINK_CLEAR_ALL);

  // Status.
  AddLocalizedString("status_cancelled", IDS_DOWNLOAD_TAB_CANCELED);
  AddLocalizedString("status_paused", IDS_DOWNLOAD_PROGRESS_PAUSED);
  AddLocalizedString("status_interrupted", IDS_DOWNLOAD_PROGRESS_INTERRUPTED);

  // Dangerous file.
  AddLocalizedString("danger_file_desc", IDS_PROMPT_DANGEROUS_DOWNLOAD);
  AddLocalizedString("danger_url_desc", IDS_PROMPT_UNSAFE_DOWNLOAD_URL);
  AddLocalizedString("danger_save", IDS_SAVE_DOWNLOAD);
  AddLocalizedString("danger_discard", IDS_DISCARD_DOWNLOAD);

  // Controls.
  AddLocalizedString("control_pause", IDS_DOWNLOAD_LINK_PAUSE);
  if (browser_defaults::kDownloadPageHasShowInFolder) {
    AddLocalizedString("control_showinfolder", IDS_DOWNLOAD_LINK_SHOW);
  }
  AddLocalizedString("control_retry", IDS_DOWNLOAD_LINK_RETRY);
  AddLocalizedString("control_cancel", IDS_DOWNLOAD_LINK_CANCEL);
  AddLocalizedString("control_resume", IDS_DOWNLOAD_LINK_RESUME);
  AddLocalizedString("control_removefromlist", IDS_DOWNLOAD_LINK_REMOVE);
}

void DownloadsUIHTMLSource::StartDataRequest(const std::string& path,
                                             bool is_incognito,
                                             int request_id) {
  if (path == kStringsJsFile) {
    SendLocalizedStringsAsJSON(request_id);
  } else {
    int idr = path == kDownloadsJsFile ? IDR_DOWNLOADS_JS : IDR_DOWNLOADS_HTML;
    SendFromResourceBundle(request_id, idr);
  }
}

std::string DownloadsUIHTMLSource::GetMimeType(const std::string& path) const {
  if (path == kStringsJsFile || path == kDownloadsJsFile)
    return "application/javascript";

  return "text/html";
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// DownloadsUI
//
///////////////////////////////////////////////////////////////////////////////

DownloadsUI::DownloadsUI(TabContents* contents) : WebUI(contents) {
  DownloadManager* dlm = GetProfile()->GetDownloadManager();

  DownloadsDOMHandler* handler = new DownloadsDOMHandler(dlm);
  AddMessageHandler(handler);
  handler->Attach(this);
  handler->Init();

  DownloadsUIHTMLSource* html_source = new DownloadsUIHTMLSource();

  // Set up the chrome://downloads/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}

// static
RefCountedMemory* DownloadsUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_DOWNLOADS_FAVICON);
}
