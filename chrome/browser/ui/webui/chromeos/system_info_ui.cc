// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/system_info_ui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/system/syslogs_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
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

using content::WebContents;
using content::WebUIMessageHandler;

class SystemInfoUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  SystemInfoUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~SystemInfoUIHTMLSource() {}

  void SyslogsComplete(chromeos::system::LogDictionaryType* sys_info,
                       std::string* ignored_content);

  CancelableRequestConsumer consumer_;

  // Stored data from StartDataRequest()
  std::string path_;
  int request_id_;

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

SystemInfoUIHTMLSource::SystemInfoUIHTMLSource()
    : DataSource(chrome::kChromeUISystemInfoHost, MessageLoop::current()),
      request_id_(0) {
}

void SystemInfoUIHTMLSource::StartDataRequest(const std::string& path,
                                              bool is_incognito,
                                              int request_id) {
  path_ = path;
  request_id_ = request_id;

  chromeos::system::SyslogsProvider* provider =
      chromeos::system::SyslogsProvider::GetInstance();
  if (provider) {
    provider->RequestSyslogs(
        false,  // don't compress.
        chromeos::system::SyslogsProvider::SYSLOGS_SYSINFO,
        &consumer_,
        base::Bind(&SystemInfoUIHTMLSource::SyslogsComplete,
                   base::Unretained(this)));
  }
}

void SystemInfoUIHTMLSource::SyslogsComplete(
    chromeos::system::LogDictionaryType* sys_info,
    std::string* ignored_content) {
  DCHECK(!ignored_content);

  DictionaryValue strings;
  strings.SetString("title", l10n_util::GetStringUTF16(IDS_ABOUT_SYS_TITLE));
  strings.SetString("description",
                    l10n_util::GetStringUTF16(IDS_ABOUT_SYS_DESC));
  strings.SetString("table_title",
                    l10n_util::GetStringUTF16(IDS_ABOUT_SYS_TABLE_TITLE));
  strings.SetString("expand_all_btn",
                    l10n_util::GetStringUTF16(IDS_ABOUT_SYS_EXPAND_ALL));
  strings.SetString("collapse_all_btn",
                    l10n_util::GetStringUTF16(IDS_ABOUT_SYS_COLLAPSE_ALL));
  strings.SetString("expand_btn",
                    l10n_util::GetStringUTF16(IDS_ABOUT_SYS_EXPAND));
  strings.SetString("collapse_btn",
                    l10n_util::GetStringUTF16(IDS_ABOUT_SYS_COLLAPSE));
  SetFontAndTextDirection(&strings);

  if (sys_info) {
     ListValue* details = new ListValue();
     strings.Set("details", details);
     chromeos::system::LogDictionaryType::iterator it;
     for (it = sys_info->begin(); it != sys_info->end(); ++it) {
       DictionaryValue* val = new DictionaryValue;
       val->SetString("stat_name", it->first);
       val->SetString("stat_value", it->second);
       details->Append(val);
     }
     strings.SetString("anchor", path_);
     delete sys_info;
  }
  static const base::StringPiece systeminfo_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_ABOUT_SYS_HTML));
  std::string full_html = jstemplate_builder::GetTemplatesHtml(
      systeminfo_html, &strings, "t" /* template root node id */);

  SendResponse(request_id_, base::RefCountedString::TakeString(&full_html));
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
  ChromeURLDataManager::AddDataSource(profile, html_source);
}
