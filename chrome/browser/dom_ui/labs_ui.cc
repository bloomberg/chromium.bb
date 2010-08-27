// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/labs_ui.h"

#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/singleton.h"
#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/labs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

///////////////////////////////////////////////////////////////////////////////
//
// LabsUIHTMLSource
//
///////////////////////////////////////////////////////////////////////////////

class LabsUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  LabsUIHTMLSource()
      : DataSource(chrome::kChromeUILabsHost, MessageLoop::current()) {}

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~LabsUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(LabsUIHTMLSource);
};

void LabsUIHTMLSource::StartDataRequest(const std::string& path,
                                           bool is_off_the_record,
                                           int request_id) {
  // Strings used in the JsTemplate file.
  DictionaryValue localized_strings;
  localized_strings.SetString("labsTitle",
      l10n_util::GetStringUTF16(IDS_LABS_TITLE));
  localized_strings.SetString("labsLongTitle",
      l10n_util::GetStringUTF16(IDS_LABS_LONG_TITLE));
  localized_strings.SetString("labsTableTitle",
      l10n_util::GetStringUTF16(IDS_LABS_TABLE_TITLE));
  localized_strings.SetString("labsNoExperimentsAvailable",
      l10n_util::GetStringUTF16(IDS_LABS_NO_EXPERIMENTS_AVAILABLE));
  localized_strings.SetString("labsBlurb", l10n_util::GetStringFUTF16(
      IDS_LABS_BLURB,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  localized_strings.SetString("labsRestartNotice", l10n_util::GetStringFUTF16(
      IDS_LABS_RESTART_NOTICE,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  localized_strings.SetString("labsRestartButton",
      l10n_util::GetStringUTF16(IDS_LABS_RESTART_BUTTON));
  localized_strings.SetString("disable",
      l10n_util::GetStringUTF16(IDS_LABS_DISABLE));
  localized_strings.SetString("enable",
      l10n_util::GetStringUTF16(IDS_LABS_ENABLE));

  ChromeURLDataManager::DataSource::SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece labs_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(IDR_LABS_HTML));
  std::string full_html(labs_html.data(), labs_html.size());
  jstemplate_builder::AppendJsonHtml(&localized_strings, &full_html);
  jstemplate_builder::AppendI18nTemplateSourceHtml(&full_html);
  jstemplate_builder::AppendI18nTemplateProcessHtml(&full_html);
  jstemplate_builder::AppendJsTemplateSourceHtml(&full_html);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

////////////////////////////////////////////////////////////////////////////////
//
// LabsDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages for the chrome://labs/ page.
class LabsDOMHandler : public DOMMessageHandler {
 public:
  LabsDOMHandler() {}
  virtual ~LabsDOMHandler() {}

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // Callback for the "requestLabsExperiments" message.
  void HandleRequestLabsExperiments(const ListValue* args);

  // Callback for the "enableLabsExperiment" message.
  void HandleEnableLabsExperimentMessage(const ListValue* args);

  // Callback for the "restartBrowser" message. Restores all tabs on restart.
  void HandleRestartBrowser(const ListValue* args);

 private:
  DISALLOW_COPY_AND_ASSIGN(LabsDOMHandler);
};

void LabsDOMHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("requestLabsExperiments",
      NewCallback(this, &LabsDOMHandler::HandleRequestLabsExperiments));
  dom_ui_->RegisterMessageCallback("enableLabsExperiment",
      NewCallback(this, &LabsDOMHandler::HandleEnableLabsExperimentMessage));
  dom_ui_->RegisterMessageCallback("restartBrowser",
      NewCallback(this, &LabsDOMHandler::HandleRestartBrowser));
}

void LabsDOMHandler::HandleRequestLabsExperiments(const ListValue* args) {
  DictionaryValue results;
  results.Set("labsExperiments",
              about_labs::GetLabsExperimentsData(dom_ui_->GetProfile()));
  results.SetBoolean("needsRestart",
                     about_labs::IsRestartNeededToCommitChanges());
  dom_ui_->CallJavascriptFunction(L"returnLabsExperiments", results);
}

void LabsDOMHandler::HandleEnableLabsExperimentMessage(const ListValue* args) {
  DCHECK_EQ(2u, args->GetSize());
  if (args->GetSize() != 2)
    return;

  std::string experiment_internal_name;
  std::string enable_str;
  if (!args->GetString(0, &experiment_internal_name) ||
      !args->GetString(1, &enable_str))
    return;

  about_labs::SetExperimentEnabled(
      dom_ui_->GetProfile(), experiment_internal_name, enable_str == "true");
}

void LabsDOMHandler::HandleRestartBrowser(const ListValue* args) {
  // Set the flag to restore state after the restart.
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, true);
  BrowserList::CloseAllBrowsersAndExit();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// LabsUI
//
///////////////////////////////////////////////////////////////////////////////

LabsUI::LabsUI(TabContents* contents) : DOMUI(contents) {
  AddMessageHandler((new LabsDOMHandler())->Attach(this));

  LabsUIHTMLSource* html_source = new LabsUIHTMLSource();

  // Set up the chrome://labs/ source.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}

// static
RefCountedMemory* LabsUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_LABS);
}

// static
void LabsUI::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterListPref(prefs::kEnabledLabsExperiments);
}
