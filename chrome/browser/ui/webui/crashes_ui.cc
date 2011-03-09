// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/crashes_ui.h"

#include "base/i18n/time_formatting.h"
#include "base/ref_counted_memory.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/crash_upload_list.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

///////////////////////////////////////////////////////////////////////////////
//
// CrashesUIHTMLSource
//
///////////////////////////////////////////////////////////////////////////////

class CrashesUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  CrashesUIHTMLSource()
      : DataSource(chrome::kChromeUICrashesHost, MessageLoop::current()) {}

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~CrashesUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(CrashesUIHTMLSource);
};

void CrashesUIHTMLSource::StartDataRequest(const std::string& path,
                                           bool is_off_the_record,
                                           int request_id) {
  DictionaryValue localized_strings;
  localized_strings.SetString("crashesTitle",
      l10n_util::GetStringUTF16(IDS_CRASHES_TITLE));
  localized_strings.SetString("crashCountFormat",
      l10n_util::GetStringUTF16(IDS_CRASHES_CRASH_COUNT_BANNER_FORMAT));
  localized_strings.SetString("crashHeaderFormat",
      l10n_util::GetStringUTF16(IDS_CRASHES_CRASH_HEADER_FORMAT));
  localized_strings.SetString("crashTimeFormat",
      l10n_util::GetStringUTF16(IDS_CRASHES_CRASH_TIME_FORMAT));
  localized_strings.SetString("bugLinkText",
      l10n_util::GetStringUTF16(IDS_CRASHES_BUG_LINK_LABEL));
  localized_strings.SetString("noCrashesMessage",
      l10n_util::GetStringUTF16(IDS_CRASHES_NO_CRASHES_MESSAGE));
  localized_strings.SetString("disabledHeader",
      l10n_util::GetStringUTF16(IDS_CRASHES_DISABLED_HEADER));
  localized_strings.SetString("disabledMessage",
      l10n_util::GetStringUTF16(IDS_CRASHES_DISABLED_MESSAGE));

  ChromeURLDataManager::DataSource::SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece crashes_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(IDR_CRASHES_HTML));
  std::string full_html =
      jstemplate_builder::GetI18nTemplateHtml(crashes_html, &localized_strings);
  jstemplate_builder::AppendJsTemplateSourceHtml(&full_html);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

////////////////////////////////////////////////////////////////////////////////
//
// CrashesDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages for the chrome://crashes/ page.
class CrashesDOMHandler : public WebUIMessageHandler,
                          public CrashUploadList::Delegate {
 public:
  explicit CrashesDOMHandler();
  virtual ~CrashesDOMHandler();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

  // CrashUploadList::Delegate implemenation.
  virtual void OnCrashListAvailable();

 private:
  // Asynchronously fetches the list of crashes. Called from JS.
  void HandleRequestCrashes(const ListValue* args);

  // Sends the recent crashes list JS.
  void UpdateUI();

  // Returns whether or not crash reporting is enabled.
  bool CrashReportingEnabled() const;

  scoped_refptr<CrashUploadList> upload_list_;
  bool list_available_;
  bool js_request_pending_;

  DISALLOW_COPY_AND_ASSIGN(CrashesDOMHandler);
};

CrashesDOMHandler::CrashesDOMHandler()
    : list_available_(false), js_request_pending_(false) {
  upload_list_ = new CrashUploadList(this);
}

CrashesDOMHandler::~CrashesDOMHandler() {
  upload_list_->ClearDelegate();
}

WebUIMessageHandler* CrashesDOMHandler::Attach(WebUI* web_ui) {
  upload_list_->LoadCrashListAsynchronously();
  return WebUIMessageHandler::Attach(web_ui);
}

void CrashesDOMHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("requestCrashList",
      NewCallback(this, &CrashesDOMHandler::HandleRequestCrashes));
}

void CrashesDOMHandler::HandleRequestCrashes(const ListValue* args) {
  if (!CrashReportingEnabled() || list_available_)
    UpdateUI();
  else
    js_request_pending_ = true;
}

void CrashesDOMHandler::OnCrashListAvailable() {
  list_available_ = true;
  if (js_request_pending_)
    UpdateUI();
}

void CrashesDOMHandler::UpdateUI() {
  bool crash_reporting_enabled = CrashReportingEnabled();
  ListValue crash_list;

  if (crash_reporting_enabled) {
    std::vector<CrashUploadList::CrashInfo> crashes;
    upload_list_->GetUploadedCrashes(50, &crashes);

    for (std::vector<CrashUploadList::CrashInfo>::iterator i = crashes.begin();
         i != crashes.end(); ++i) {
      DictionaryValue* crash = new DictionaryValue();
      crash->SetString("id", i->crash_id);
      crash->SetString("time",
                       base::TimeFormatFriendlyDateAndTime(i->crash_time));
      crash_list.Append(crash);
    }
  }

  FundamentalValue enabled(crash_reporting_enabled);

  web_ui_->CallJavascriptFunction("updateCrashList", enabled, crash_list);
}

bool CrashesDOMHandler::CrashReportingEnabled() const {
#if defined(GOOGLE_CHROME_BUILD)
  PrefService* prefs = g_browser_process->local_state();
  return prefs->GetBoolean(prefs::kMetricsReportingEnabled);
#else
  return false;
#endif
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// CrashesUI
//
///////////////////////////////////////////////////////////////////////////////

CrashesUI::CrashesUI(TabContents* contents) : WebUI(contents) {
  AddMessageHandler((new CrashesDOMHandler())->Attach(this));

  CrashesUIHTMLSource* html_source = new CrashesUIHTMLSource();

  // Set up the chrome://crashes/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}

// static
RefCountedMemory* CrashesUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_SAD_FAVICON);
}
