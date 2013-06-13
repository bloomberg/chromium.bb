// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/managed_user_create_confirm_handler.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace options {

ManagedUserCreateConfirmHandler::ManagedUserCreateConfirmHandler() {
}

ManagedUserCreateConfirmHandler::~ManagedUserCreateConfirmHandler() {
}

void ManagedUserCreateConfirmHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    {"managedUserCreateConfirmTitle", IDS_NEW_LIMITED_USER_SUCCESS_TITLE},
    {"managedUserCreateConfirmTextSlide1",
     IDS_NEW_LIMITED_USER_SUCCESS_TEXT_SLIDE_1},
    {"managedUserCreateConfirmTextSlide2",
     IDS_NEW_LIMITED_USER_SUCCESS_TEXT_SLIDE_2},
    {"managedUserCreateConfirmTextSlide3",
     IDS_NEW_LIMITED_USER_SUCCESS_TEXT_SLIDE_3},
    {"managedUserCreateConfirmDone", IDS_NEW_LIMITED_USER_SUCCESS_DONE_BUTTON},
    {"managedUserCreateConfirmSwitch",
     IDS_NEW_LIMITED_USER_SUCCESS_SWITCH_BUTTON},
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
}

void ManagedUserCreateConfirmHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("switchToProfile",
      base::Bind(&ManagedUserCreateConfirmHandler::SwitchToProfile,
                 base::Unretained(this)));
}

void ManagedUserCreateConfirmHandler::SwitchToProfile(
      const base::ListValue* args) {
  DCHECK(args);
  const Value* file_path_value;
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

  ProfileManager::FindOrCreateNewWindowForProfile(
      profile,
      chrome::startup::IS_PROCESS_STARTUP,
      chrome::startup::IS_FIRST_RUN,
      desktop_type,
      false);
}

}  // namespace options
