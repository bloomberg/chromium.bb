// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_main_delegate_staging_android.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "components/policy/core/browser/android/android_combined_policy_provider.h"

#if defined(SAFE_BROWSING_DB_REMOTE)
#include "chrome/browser/safe_browsing/safe_browsing_api_handler.h"
#endif

ChromeMainDelegateStagingAndroid::ChromeMainDelegateStagingAndroid() {
}

ChromeMainDelegateStagingAndroid::~ChromeMainDelegateStagingAndroid() {
}

bool ChromeMainDelegateStagingAndroid::BasicStartupComplete(int* exit_code) {
#if defined(SAFE_BROWSING_DB_REMOTE)
  safe_browsing_api_handler_.reset(CreateSafeBrowsingApiHandler());
  SafeBrowsingApiHandler::SetInstance(safe_browsing_api_handler_.get());
#endif

  policy::android::AndroidCombinedPolicyProvider::SetShouldWaitForPolicy(true);

  return ChromeMainDelegateAndroid::BasicStartupComplete(exit_code);
}

int ChromeMainDelegateStagingAndroid::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (process_type.empty()) {
    // By default, Android creates the directory accessible by others.
    // We'd like to tighten security and make it accessible only by
    // the browser process.
    base::FilePath data_path;
    bool ok = PathService::Get(base::DIR_ANDROID_APP_DATA, &data_path);
    if (ok) {
      int permissions;
      ok = base::GetPosixFilePermissions(data_path, &permissions);
      if (ok)
        permissions &= base::FILE_PERMISSION_USER_MASK;
      else
        permissions = base::FILE_PERMISSION_READ_BY_USER |
                      base::FILE_PERMISSION_WRITE_BY_USER |
                      base::FILE_PERMISSION_EXECUTE_BY_USER;

      ok = base::SetPosixFilePermissions(data_path, permissions);
    }
    if (!ok)
      LOG(ERROR) << "Failed to set permission of " << data_path.value().c_str();

  }

  return ChromeMainDelegateAndroid::RunProcess(
                process_type, main_function_params);
}

void ChromeMainDelegateStagingAndroid::ProcessExiting(
    const std::string& process_type) {
#if defined(SAFE_BROWSING_DB_REMOTE)
  SafeBrowsingApiHandler::SetInstance(NULL);
#endif
}

#if defined(SAFE_BROWSING_DB_REMOTE)
SafeBrowsingApiHandler*
ChromeMainDelegateStagingAndroid::CreateSafeBrowsingApiHandler() {
  return NULL;
}
#endif
