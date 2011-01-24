// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/flags_ui.h"

#include <string>

#include "base/singleton.h"
#include "base/values.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

///////////////////////////////////////////////////////////////////////////////
//
// FlagsUIHTMLSource
//
///////////////////////////////////////////////////////////////////////////////

class FlagsUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  FlagsUIHTMLSource()
      : DataSource(chrome::kChromeUIFlagsHost, MessageLoop::current()) {}

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~FlagsUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(FlagsUIHTMLSource);
};

void FlagsUIHTMLSource::StartDataRequest(const std::string& path,
                                        bool is_off_the_record,
                                        int request_id) {
  // Strings used in the JsTemplate file.
  DictionaryValue localized_strings;
  localized_strings.SetString("flagsLongTitle",
      l10n_util::GetStringUTF16(IDS_FLAGS_LONG_TITLE));
  localized_strings.SetString("flagsTableTitle",
      l10n_util::GetStringUTF16(IDS_FLAGS_TABLE_TITLE));
  localized_strings.SetString("flagsNoExperimentsAvailable",
      l10n_util::GetStringUTF16(IDS_FLAGS_NO_EXPERIMENTS_AVAILABLE));
  localized_strings.SetString("flagsWarningHeader", l10n_util::GetStringUTF16(
      IDS_FLAGS_WARNING_HEADER));
  localized_strings.SetString("flagsBlurb", l10n_util::GetStringUTF16(
      IDS_FLAGS_WARNING_TEXT));
  localized_strings.SetString("flagsRestartNotice", l10n_util::GetStringFUTF16(
      IDS_FLAGS_RESTART_NOTICE,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  localized_strings.SetString("flagsRestartButton",
      l10n_util::GetStringUTF16(IDS_FLAGS_RESTART_BUTTON));
  localized_strings.SetString("disable",
      l10n_util::GetStringUTF16(IDS_FLAGS_DISABLE));
  localized_strings.SetString("enable",
      l10n_util::GetStringUTF16(IDS_FLAGS_ENABLE));

  ChromeURLDataManager::DataSource::SetFontAndTextDirection(&localized_strings);

  static const base::StringPiece flags_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(IDR_FLAGS_HTML));
  std::string full_html(flags_html.data(), flags_html.size());
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
// FlagsDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages for the about:flags page.
class FlagsDOMHandler : public DOMMessageHandler {
 public:
  FlagsDOMHandler() {}
  virtual ~FlagsDOMHandler() {}

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // Callback for the "requestFlagsExperiments" message.
  void HandleRequestFlagsExperiments(const ListValue* args);

  // Callback for the "enableFlagsExperiment" message.
  void HandleEnableFlagsExperimentMessage(const ListValue* args);

  // Callback for the "restartBrowser" message. Restores all tabs on restart.
  void HandleRestartBrowser(const ListValue* args);

 private:
  DISALLOW_COPY_AND_ASSIGN(FlagsDOMHandler);
};

void FlagsDOMHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("requestFlagsExperiments",
      NewCallback(this, &FlagsDOMHandler::HandleRequestFlagsExperiments));
  dom_ui_->RegisterMessageCallback("enableFlagsExperiment",
      NewCallback(this, &FlagsDOMHandler::HandleEnableFlagsExperimentMessage));
  dom_ui_->RegisterMessageCallback("restartBrowser",
      NewCallback(this, &FlagsDOMHandler::HandleRestartBrowser));
}

void FlagsDOMHandler::HandleRequestFlagsExperiments(const ListValue* args) {
  DictionaryValue results;
  results.Set("flagsExperiments",
              about_flags::GetFlagsExperimentsData(
                  g_browser_process->local_state()));
  results.SetBoolean("needsRestart",
                     about_flags::IsRestartNeededToCommitChanges());
  dom_ui_->CallJavascriptFunction(L"returnFlagsExperiments", results);
}

void FlagsDOMHandler::HandleEnableFlagsExperimentMessage(
    const ListValue* args) {
  DCHECK_EQ(2u, args->GetSize());
  if (args->GetSize() != 2)
    return;

  std::string experiment_internal_name;
  std::string enable_str;
  if (!args->GetString(0, &experiment_internal_name) ||
      !args->GetString(1, &enable_str))
    return;

  about_flags::SetExperimentEnabled(
      g_browser_process->local_state(),
      experiment_internal_name,
      enable_str == "true");
}

void FlagsDOMHandler::HandleRestartBrowser(const ListValue* args) {
  // Set the flag to restore state after the restart.
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, true);
  BrowserList::CloseAllBrowsersAndExit();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// FlagsUI
//
///////////////////////////////////////////////////////////////////////////////

FlagsUI::FlagsUI(TabContents* contents) : DOMUI(contents) {
  AddMessageHandler((new FlagsDOMHandler())->Attach(this));

  FlagsUIHTMLSource* html_source = new FlagsUIHTMLSource();

  // Set up the about:flags source.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(ChromeURLDataManager::GetInstance(),
                        &ChromeURLDataManager::AddDataSource,
                        make_scoped_refptr(html_source)));
}

// static
RefCountedMemory* FlagsUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_FLAGS);
}

// static
void FlagsUI::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterListPref(prefs::kEnabledLabsExperiments);
}
