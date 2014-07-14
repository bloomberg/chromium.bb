// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/supervised_user_create_confirm_handler.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace options {

SupervisedUserCreateConfirmHandler::SupervisedUserCreateConfirmHandler() {
}

SupervisedUserCreateConfirmHandler::~SupervisedUserCreateConfirmHandler() {
}

void SupervisedUserCreateConfirmHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "managedUserCreatedTitle", IDS_SUPERVISED_USER_CREATED_TITLE },
    { "managedUserCreatedText", IDS_SUPERVISED_USER_CREATED_TEXT },
    { "managedUserCreatedDone", IDS_SUPERVISED_USER_CREATED_DONE_BUTTON },
    { "managedUserCreatedSwitch", IDS_SUPERVISED_USER_CREATED_SWITCH_BUTTON },
  };

  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  if (signin) {
    localized_strings->SetString("custodianEmail",
                                 signin->GetAuthenticatedUsername());
  } else {
    localized_strings->SetString("custodianEmail", std::string());
  }

  RegisterStrings(localized_strings, resources, arraysize(resources));
}

void SupervisedUserCreateConfirmHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("switchToProfile",
      base::Bind(&SupervisedUserCreateConfirmHandler::SwitchToProfile,
                 base::Unretained(this)));
}

void SupervisedUserCreateConfirmHandler::SwitchToProfile(
      const base::ListValue* args) {
  DCHECK(args);
  const base::Value* file_path_value;
  if (!args->Get(0, &file_path_value))
    return;

  base::FilePath profile_file_path;
  if (!base::GetValueAsFilePath(*file_path_value, &profile_file_path))
    return;

  Profile* profile = g_browser_process->profile_manager()->
      GetProfileByPath(profile_file_path);
  DCHECK(profile);

  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  chrome::HostDesktopType desktop_type = chrome::HOST_DESKTOP_TYPE_NATIVE;
  if (browser)
    desktop_type = browser->host_desktop_type();

  profiles::FindOrCreateNewWindowForProfile(
      profile,
      chrome::startup::IS_PROCESS_STARTUP,
      chrome::startup::IS_FIRST_RUN,
      desktop_type,
      false);
}

}  // namespace options
