// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/downloads_ui.h"

#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/downloads_dom_handler.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

///////////////////////////////////////////////////////////////////////////////
//
// DownloadsHTMLSource
//
///////////////////////////////////////////////////////////////////////////////

class DownloadsUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  DownloadsUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~DownloadsUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(DownloadsUIHTMLSource);
};

DownloadsUIHTMLSource::DownloadsUIHTMLSource()
    : DataSource(chrome::kChromeUIDownloadsHost, MessageLoop::current()) {
}

void DownloadsUIHTMLSource::StartDataRequest(const std::string& path,
                                             bool is_off_the_record,
                                             int request_id) {
  DictionaryValue localized_strings;
  localized_strings.SetString("title",
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_TITLE));
  localized_strings.SetString("searchbutton",
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_SEARCH_BUTTON));
  localized_strings.SetString("no_results",
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_SEARCH_BUTTON));
  localized_strings.SetString("searchresultsfor",
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_SEARCHRESULTSFOR));
  localized_strings.SetString("downloads",
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_TITLE));
  localized_strings.SetString("clear_all",
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_LINK_CLEAR_ALL));

  // Status.
  localized_strings.SetString("status_cancelled",
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_TAB_CANCELED));
  localized_strings.SetString("status_paused",
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_PROGRESS_PAUSED));

  // Dangerous file.
  localized_strings.SetString("danger_desc",
      l10n_util::GetStringUTF16(IDS_PROMPT_DANGEROUS_DOWNLOAD));
  localized_strings.SetString("danger_save",
      l10n_util::GetStringUTF16(IDS_SAVE_DOWNLOAD));
  localized_strings.SetString("danger_discard",
      l10n_util::GetStringUTF16(IDS_DISCARD_DOWNLOAD));

  // Controls.
  localized_strings.SetString("control_pause",
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_LINK_PAUSE));
  if (browser_defaults::kDownloadPageHasShowInFolder) {
    localized_strings.SetString("control_showinfolder",
        l10n_util::GetStringUTF16(IDS_DOWNLOAD_LINK_SHOW));
  }
  localized_strings.SetString("control_retry",
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_LINK_RETRY));
  localized_strings.SetString("control_cancel",
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_LINK_CANCEL));
  localized_strings.SetString("control_resume",
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_LINK_RESUME));
  localized_strings.SetString("control_removefromlist",
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_LINK_REMOVE));

  SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece downloads_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_DOWNLOADS_HTML));
  const std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      downloads_html, &localized_strings);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// DownloadsUI
//
///////////////////////////////////////////////////////////////////////////////

DownloadsUI::DownloadsUI(TabContents* contents) : DOMUI(contents) {
  DownloadManager* dlm = GetProfile()->GetDownloadManager();

  DownloadsDOMHandler* handler = new DownloadsDOMHandler(dlm);
  AddMessageHandler(handler);
  handler->Attach(this);
  handler->Init();

  DownloadsUIHTMLSource* html_source = new DownloadsUIHTMLSource();

  // Set up the chrome://downloads/ source.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(ChromeURLDataManager::GetInstance(),
                        &ChromeURLDataManager::AddDataSource,
                        make_scoped_refptr(html_source)));
}

// static
RefCountedMemory* DownloadsUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_DOWNLOADS_FAVICON);
}
