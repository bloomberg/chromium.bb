// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/supervised_user_create_confirm_handler.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_cache_observer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

namespace options {

// ProfileUpdateObserver------------------------------------------------------

class SupervisedUserCreateConfirmHandler::ProfileUpdateObserver
    : public ProfileInfoCacheObserver {
 public:
  ProfileUpdateObserver(ProfileInfoCache* profile_info_cache,
                        SupervisedUserCreateConfirmHandler* handler)
      : profile_info_cache_(profile_info_cache),
        create_confirm_handler_(handler),
        scoped_observer_(this) {
    DCHECK(profile_info_cache_);
    DCHECK(create_confirm_handler_);
    scoped_observer_.Add(profile_info_cache_);
  }

 private:
  // ProfileInfoCacheObserver implementation:
  // Forward possibly relevant changes to the dialog, which will check the
  // affected profile and update or close as needed.
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override {
    scoped_ptr<base::StringValue> profile_path_value(
        base::CreateFilePathValue(profile_path));
    create_confirm_handler_->web_ui()->CallJavascriptFunction(
        "SupervisedUserCreateConfirmOverlay.onDeletedProfile",
        *profile_path_value);
  }

  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override {
    size_t profile_index =
        profile_info_cache_->GetIndexOfProfileWithPath(profile_path);
    if (profile_index == std::string::npos)
      return;
    base::string16 new_profile_name =
        profile_info_cache_->GetNameOfProfileAtIndex(profile_index);
    scoped_ptr<base::StringValue> profile_path_value(
        base::CreateFilePathValue(profile_path));
    create_confirm_handler_->web_ui()->CallJavascriptFunction(
        "SupervisedUserCreateConfirmOverlay.onUpdatedProfileName",
        *profile_path_value,
        base::StringValue(new_profile_name));
  }

  // Weak.
  ProfileInfoCache* profile_info_cache_;

  // Weak; owns us.
  SupervisedUserCreateConfirmHandler* create_confirm_handler_;

  // Manages any sources we're observing, ensuring that they're all removed
  // on destruction.
  ScopedObserver<ProfileInfoCache, ProfileUpdateObserver> scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(ProfileUpdateObserver);
};


// SupervisedUserCreateConfirmHandler-----------------------------------------

SupervisedUserCreateConfirmHandler::SupervisedUserCreateConfirmHandler() {
  profile_info_cache_observer_.reset(
      new SupervisedUserCreateConfirmHandler::ProfileUpdateObserver(
          &g_browser_process->profile_manager()->GetProfileInfoCache(), this));
}

SupervisedUserCreateConfirmHandler::~SupervisedUserCreateConfirmHandler() {
}

void SupervisedUserCreateConfirmHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "supervisedUserCreatedTitle", IDS_LEGACY_SUPERVISED_USER_CREATED_TITLE },
    { "supervisedUserCreatedDone",
        IDS_LEGACY_SUPERVISED_USER_CREATED_DONE_BUTTON },
    { "supervisedUserCreatedSwitch",
        IDS_LEGACY_SUPERVISED_USER_CREATED_SWITCH_BUTTON },
  };

  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  if (signin) {
    localized_strings->SetString("custodianEmail",
                                 signin->GetAuthenticatedAccountInfo().email);
  } else {
    localized_strings->SetString("custodianEmail", std::string());
  }

  base::string16 supervised_user_dashboard_url =
      base::ASCIIToUTF16(chrome::kLegacySupervisedUserManagementURL);
  base::string16 supervised_user_dashboard_display =
      base::ASCIIToUTF16(chrome::kLegacySupervisedUserManagementDisplayURL);
  // The first two substitution parameters need to remain; they will be filled
  // by the page's JS.
  localized_strings->SetString("supervisedUserCreatedText",
      l10n_util::GetStringFUTF16(IDS_LEGACY_SUPERVISED_USER_CREATED_TEXT,
                                 base::ASCIIToUTF16("$1"),
                                 base::ASCIIToUTF16("$2"),
                                 supervised_user_dashboard_url,
                                 supervised_user_dashboard_display));

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

  profiles::FindOrCreateNewWindowForProfile(
      profile, chrome::startup::IS_PROCESS_STARTUP,
      chrome::startup::IS_FIRST_RUN, false);
}

}  // namespace options
