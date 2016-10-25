// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_ANDROID_MANAGEMENT_CHECKER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_ANDROID_MANAGEMENT_CHECKER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/android_management_client.h"
#include "google_apis/gaia/oauth2_token_service.h"

class ProfileOAuth2TokenService;

namespace arc {

class ArcAndroidManagementCheckerDelegate;

class ArcAndroidManagementChecker : public OAuth2TokenService::Observer {
 public:
  ArcAndroidManagementChecker(ArcAndroidManagementCheckerDelegate* delegate,
                              ProfileOAuth2TokenService* token_service,
                              const std::string& account_id,
                              bool background_mode);
  ~ArcAndroidManagementChecker() override;

  static void StartClient();

  // OAuth2TokenService::Observer:
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokensLoaded() override;

  bool background_mode() const { return background_mode_; }

 private:
  void StartCheck();
  void ScheduleCheck();
  void DispatchResult(policy::AndroidManagementClient::Result result);
  void OnAndroidManagementChecked(
      policy::AndroidManagementClient::Result result);

  // Unowned pointers.
  ArcAndroidManagementCheckerDelegate* const delegate_;
  ProfileOAuth2TokenService* const token_service_;

  const std::string account_id_;

  // In background mode errors are ignored and retry is attempted. There is no
  // retry in foreground mode and result is passed to delegate directly.
  bool background_mode_;

  // Keeps current retry time for background mode.
  int retry_time_ms_;

  policy::AndroidManagementClient android_management_client_;

  base::WeakPtrFactory<ArcAndroidManagementChecker> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcAndroidManagementChecker);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_ANDROID_MANAGEMENT_CHECKER_H_
