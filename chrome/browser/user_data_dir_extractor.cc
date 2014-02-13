// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_data_dir_extractor.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "components/startup_metric_utils/startup_metric_utils.h"
#include "content/public/common/main_function_params.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_LINUX)
#include "base/environment.h"
#endif

#if defined(OS_MACOSX) || defined(OS_WIN)
#include "chrome/browser/policy/policy_path_parser.h"
#endif

namespace chrome {

namespace {

GetUserDataDirCallback* custom_get_user_data_dir_callback = NULL;

void ShowUserDataDirWarning(const base::FilePath& user_data_dir) {
  startup_metric_utils::SetNonBrowserUIDisplayed();

  // Ensure the ResourceBundle is initialized for string resource access.
  bool cleanup_resource_bundle = false;
  if (!ResourceBundle::HasSharedInstance()) {
    cleanup_resource_bundle = true;
    std::string locale = l10n_util::GetApplicationLocale(std::string());
    const char kUserDataDirDialogFallbackLocale[] = "en-US";
    if (locale.empty())
      locale = kUserDataDirDialogFallbackLocale;
    ResourceBundle::InitSharedInstanceWithLocale(locale, NULL);
  }

  const base::string16& title =
      l10n_util::GetStringUTF16(IDS_CANT_WRITE_USER_DIRECTORY_TITLE);
  const base::string16& message =
      l10n_util::GetStringFUTF16(IDS_CANT_WRITE_USER_DIRECTORY_SUMMARY,
                                 user_data_dir.LossyDisplayName());

  if (cleanup_resource_bundle)
    ResourceBundle::CleanupSharedInstance();

  // More complex dialogs cannot be shown before the earliest calls here.
  ShowMessageBox(NULL, title, message, chrome::MESSAGE_BOX_TYPE_WARNING);
}

}  // namespace

void InstallCustomGetUserDataDirCallbackForTest(
    GetUserDataDirCallback* callback) {
  custom_get_user_data_dir_callback = callback;
}

base::FilePath GetUserDataDir(const content::MainFunctionParams& parameters) {
  if (custom_get_user_data_dir_callback)
    return custom_get_user_data_dir_callback->Run();

  base::FilePath user_data_dir =
      parameters.command_line.GetSwitchValuePath(switches::kUserDataDir);
  std::string process_type =
      parameters.command_line.GetSwitchValueASCII(switches::kProcessType);

#if defined(OS_LINUX)
  // On Linux, Chrome does not support running multiple copies under different
  // DISPLAYs, so the profile directory can be specified in the environment to
  // support the virtual desktop use-case.
  if (user_data_dir.empty()) {
    std::string user_data_dir_string;
    scoped_ptr<base::Environment> environment(base::Environment::Create());
    if (environment->GetVar("CHROME_USER_DATA_DIR", &user_data_dir_string) &&
        IsStringUTF8(user_data_dir_string)) {
      user_data_dir = base::FilePath::FromUTF8Unsafe(user_data_dir_string);
    }
  }
#endif
#if defined(OS_MACOSX) || defined(OS_WIN)
  policy::path_parser::CheckUserDataDirPolicy(&user_data_dir);
#endif

  // If the directory does not exist and cannot be created, prompt the user.
  if (!user_data_dir.empty() &&
      !PathService::OverrideAndCreateIfNeeded(chrome::DIR_USER_DATA,
          user_data_dir, chrome::ProcessNeedsProfileDir(process_type))) {
    ShowUserDataDirWarning(user_data_dir);
  }

  // Getting the user data directory can fail if the directory isn't creatable.
  // ProcessSingleton needs a real user data directory on Mac/Linux, so it's
  // better to fail here than fail mysteriously elsewhere.
  CHECK(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
      << "Must be able to get user data directory!";
  return user_data_dir;
}

}  // namespace chrome
