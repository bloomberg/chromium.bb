// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flags_ui.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#endif

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

ChromeWebUIDataSource* CreateFlagsUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIFlagsHost);

  source->AddLocalizedString("flagsLongTitle", IDS_FLAGS_LONG_TITLE);
  source->AddLocalizedString("flagsTableTitle", IDS_FLAGS_TABLE_TITLE);
  source->AddLocalizedString("flagsNoExperimentsAvailable",
                             IDS_FLAGS_NO_EXPERIMENTS_AVAILABLE);
  source->AddLocalizedString("flagsWarningHeader", IDS_FLAGS_WARNING_HEADER);
  source->AddLocalizedString("flagsBlurb", IDS_FLAGS_WARNING_TEXT);
  source->AddLocalizedString("flagsNotSupported", IDS_FLAGS_NOT_AVAILABLE);
  source->AddLocalizedString("flagsRestartNotice", IDS_FLAGS_RELAUNCH_NOTICE);
  source->AddLocalizedString("flagsRestartButton", IDS_FLAGS_RELAUNCH_BUTTON);
  source->AddLocalizedString("disable", IDS_FLAGS_DISABLE);
  source->AddLocalizedString("enable", IDS_FLAGS_ENABLE);
#if defined(OS_CHROMEOS)
  // Set the strings to show which user can actually change the flags
  source->AddLocalizedString("ownerOnly", IDS_OPTIONS_ACCOUNTS_OWNER_ONLY);
  std::string owner;
  chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner, &owner);
  source->AddString("ownerUserId", UTF8ToUTF16(owner));
#endif

  source->set_json_path("strings.js");
  source->add_resource_path("flags.js", IDR_FLAGS_JS);

  int idr = IDR_FLAGS_HTML;
#if defined (OS_CHROMEOS)
  if (!chromeos::UserManager::Get()->IsCurrentUserOwner() &&
      base::chromeos::IsRunningOnChromeOS())
    idr = IDR_FLAGS_HTML_WARNING;
#endif
  source->set_default_resource(idr);
  return source;
}

////////////////////////////////////////////////////////////////////////////////
//
// FlagsDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages for the about:flags page.
class FlagsDOMHandler : public WebUIMessageHandler {
 public:
  FlagsDOMHandler() {}
  virtual ~FlagsDOMHandler() {}

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

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
  web_ui()->RegisterMessageCallback("requestFlagsExperiments",
      base::Bind(&FlagsDOMHandler::HandleRequestFlagsExperiments,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("enableFlagsExperiment",
      base::Bind(&FlagsDOMHandler::HandleEnableFlagsExperimentMessage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("restartBrowser",
      base::Bind(&FlagsDOMHandler::HandleRestartBrowser,
                 base::Unretained(this)));
}

void FlagsDOMHandler::HandleRequestFlagsExperiments(const ListValue* args) {
  DictionaryValue results;
  results.Set("flagsExperiments",
              about_flags::GetFlagsExperimentsData(
                  g_browser_process->local_state()));
  results.SetBoolean("needsRestart",
                     about_flags::IsRestartNeededToCommitChanges());
  web_ui()->CallJavascriptFunction("returnFlagsExperiments", results);
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
  browser::AttemptRestart();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// FlagsUI
//
///////////////////////////////////////////////////////////////////////////////

FlagsUI::FlagsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new FlagsDOMHandler());

  // Set up the about:flags source.
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, CreateFlagsUIHTMLSource());
}

// static
base::RefCountedMemory* FlagsUI::GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor) {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(IDR_FLAGS_FAVICON, scale_factor);
}

// static
void FlagsUI::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterListPref(prefs::kEnabledLabsExperiments);
}
