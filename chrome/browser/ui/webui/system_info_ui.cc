// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/system_info_ui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/feedback/system_logs/about_system_logs_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/directory_lister.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

using content::WebContents;
using content::WebUIMessageHandler;
using system_logs::SystemLogsResponse;
using system_logs::AboutSystemLogsFetcher;

class SystemInfoUIHTMLSource : public content::URLDataSource{
 public:
  SystemInfoUIHTMLSource();

  // content::URLDataSource implementation.
  virtual std::string GetSource() const OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
  virtual std::string GetMimeType(const std::string&) const OVERRIDE {
    return "text/html";
  }
  virtual bool ShouldAddContentSecurityPolicy() const OVERRIDE {
    return false;
  }

 private:
  virtual ~SystemInfoUIHTMLSource() {}

  void SysInfoComplete(scoped_ptr<SystemLogsResponse> response);
  void RequestComplete();
  void WaitForData();

  // Stored data from StartDataRequest()
  std::string path_;
  content::URLDataSource::GotDataCallback callback_;

  scoped_ptr<SystemLogsResponse> response_;
  base::WeakPtrFactory<SystemInfoUIHTMLSource> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(SystemInfoUIHTMLSource);
};

// The handler for Javascript messages related to the "system" view.
class SystemInfoHandler : public WebUIMessageHandler,
                          public base::SupportsWeakPtr<SystemInfoHandler> {
 public:
  SystemInfoHandler();
  virtual ~SystemInfoHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemInfoHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// SystemInfoUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

SystemInfoUIHTMLSource::SystemInfoUIHTMLSource() : weak_ptr_factory_(this) {}

std::string SystemInfoUIHTMLSource::GetSource() const {
  return chrome::kChromeUISystemInfoHost;
}

void SystemInfoUIHTMLSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  path_ = path;
  callback_ = callback;

  AboutSystemLogsFetcher* fetcher = new AboutSystemLogsFetcher();
  fetcher->Fetch(base::Bind(&SystemInfoUIHTMLSource::SysInfoComplete,
                            weak_ptr_factory_.GetWeakPtr()));
}


void SystemInfoUIHTMLSource::SysInfoComplete(
    scoped_ptr<SystemLogsResponse> sys_info) {
  response_ = sys_info.Pass();
  RequestComplete();
}

void SystemInfoUIHTMLSource::RequestComplete() {
  base::DictionaryValue strings;
  strings.SetString("title", l10n_util::GetStringUTF16(IDS_ABOUT_SYS_TITLE));
  strings.SetString("description",
                    l10n_util::GetStringUTF16(IDS_ABOUT_SYS_DESC));
  strings.SetString("tableTitle",
                    l10n_util::GetStringUTF16(IDS_ABOUT_SYS_TABLE_TITLE));
  strings.SetString(
      "logFileTableTitle",
      l10n_util::GetStringUTF16(IDS_ABOUT_SYS_LOG_FILE_TABLE_TITLE));
  strings.SetString("expandAllBtn",
                    l10n_util::GetStringUTF16(IDS_ABOUT_SYS_EXPAND_ALL));
  strings.SetString("collapseAllBtn",
                    l10n_util::GetStringUTF16(IDS_ABOUT_SYS_COLLAPSE_ALL));
  strings.SetString("expandBtn",
                    l10n_util::GetStringUTF16(IDS_ABOUT_SYS_EXPAND));
  strings.SetString("collapseBtn",
                    l10n_util::GetStringUTF16(IDS_ABOUT_SYS_COLLAPSE));
  strings.SetString("parseError",
                    l10n_util::GetStringUTF16(IDS_ABOUT_SYS_PARSE_ERROR));
  webui::SetFontAndTextDirection(&strings);
  if (response_.get()) {
    base::ListValue* details = new base::ListValue();
    strings.Set("details", details);
    for (SystemLogsResponse::const_iterator it = response_->begin();
         it != response_->end();
         ++it) {
      base::DictionaryValue* val = new base::DictionaryValue;
      val->SetString("statName", it->first);
      val->SetString("statValue", it->second);
      details->Append(val);
    }
  }
  static const base::StringPiece systeminfo_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_ABOUT_SYS_HTML));
  std::string full_html = webui::GetTemplatesHtml(
      systeminfo_html, &strings, "t" /* template root node id */);
  callback_.Run(base::RefCountedString::TakeString(&full_html));
}

////////////////////////////////////////////////////////////////////////////////
//
// SystemInfoHandler
//
////////////////////////////////////////////////////////////////////////////////
SystemInfoHandler::SystemInfoHandler() {
}

SystemInfoHandler::~SystemInfoHandler() {
}

void SystemInfoHandler::RegisterMessages() {
  // TODO(stevenjb): add message registration, callbacks...
}

////////////////////////////////////////////////////////////////////////////////
//
// SystemInfoUI
//
////////////////////////////////////////////////////////////////////////////////

SystemInfoUI::SystemInfoUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  SystemInfoHandler* handler = new SystemInfoHandler();
  web_ui->AddMessageHandler(handler);
  SystemInfoUIHTMLSource* html_source = new SystemInfoUIHTMLSource();

  // Set up the chrome://system/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, html_source);
}
