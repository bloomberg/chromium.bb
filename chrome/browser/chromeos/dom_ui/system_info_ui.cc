// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/system_info_ui.h"

#include "base/callback.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/weak_ptr.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/syslogs_library.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "net/base/escape.h"
#include "net/base/directory_lister.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

class SystemInfoUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  SystemInfoUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~SystemInfoUIHTMLSource() {}

  void SyslogsComplete(chromeos::LogDictionaryType* sys_info,
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
  virtual WebUIMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

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
                                              bool is_off_the_record,
                                              int request_id) {
  path_ = path;
  request_id_ = request_id;

  chromeos::SyslogsLibrary* syslogs_lib =
      chromeos::CrosLibrary::Get()->GetSyslogsLibrary();
  if (syslogs_lib) {
    syslogs_lib->RequestSyslogs(
        false, false,
        &consumer_,
        NewCallback(this, &SystemInfoUIHTMLSource::SyslogsComplete));
  }
}

void SystemInfoUIHTMLSource::SyslogsComplete(
    chromeos::LogDictionaryType* sys_info,
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
     chromeos::LogDictionaryType::iterator it;
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
  const std::string full_html = jstemplate_builder::GetTemplatesHtml(
      systeminfo_html, &strings, "t" /* template root node id */);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id_, html_bytes);
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

WebUIMessageHandler* SystemInfoHandler::Attach(DOMUI* dom_ui) {
  // TODO(stevenjb): customize handler attach if needed...
  return WebUIMessageHandler::Attach(dom_ui);
}

void SystemInfoHandler::RegisterMessages() {
  // TODO(stevenjb): add message registration, callbacks...
}

////////////////////////////////////////////////////////////////////////////////
//
// SystemInfoUI
//
////////////////////////////////////////////////////////////////////////////////

SystemInfoUI::SystemInfoUI(TabContents* contents) : DOMUI(contents) {
  SystemInfoHandler* handler = new SystemInfoHandler();
  AddMessageHandler((handler)->Attach(this));
  SystemInfoUIHTMLSource* html_source = new SystemInfoUIHTMLSource();

  // Set up the chrome://system/ source.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          ChromeURLDataManager::GetInstance(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}
