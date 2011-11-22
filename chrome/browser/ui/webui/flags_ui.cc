// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flags_ui.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

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
#if defined(OS_CHROMEOS)
  int ids = IDS_PRODUCT_OS_NAME;
#else
  int ids = IDS_PRODUCT_NAME;
#endif
  source->AddString("flagsRestartNotice", l10n_util::GetStringFUTF16(
      IDS_FLAGS_RELAUNCH_NOTICE, l10n_util::GetStringUTF16(ids)));
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
  if (!chromeos::UserManager::Get()->current_user_is_owner())
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
  web_ui_->RegisterMessageCallback("requestFlagsExperiments",
      base::Bind(&FlagsDOMHandler::HandleRequestFlagsExperiments,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("enableFlagsExperiment",
      base::Bind(&FlagsDOMHandler::HandleEnableFlagsExperimentMessage,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("restartBrowser",
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
  web_ui_->CallJavascriptFunction("returnFlagsExperiments", results);
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
  BrowserList::AttemptRestart();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// FlagsUI
//
///////////////////////////////////////////////////////////////////////////////

FlagsUI::FlagsUI(TabContents* contents) : ChromeWebUI(contents) {
  AddMessageHandler((new FlagsDOMHandler())->Attach(this));

  // Set up the about:flags source.
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(CreateFlagsUIHTMLSource());
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
