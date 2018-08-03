// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_CRD_HOST_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_CRD_HOST_DELEGATE_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_start_crd_session_job.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace network {
class SimpleURLLoader;
}

class Profile;

namespace policy {

// An implementation of the |DeviceCommandStartCRDSessionJob::Delegate|.
class CRDHostDelegate : public DeviceCommandStartCRDSessionJob::Delegate,
                        public OAuth2TokenService::Consumer {
 public:
  CRDHostDelegate();
  ~CRDHostDelegate() override;

 private:
  // DeviceCommandScreenshotJob::Delegate:
  bool HasActiveSession() const override;
  void TerminateSession(base::OnceClosure callback) override;
  bool AreServicesReady() const override;
  bool IsRunningKiosk() const override;
  base::TimeDelta GetIdlenessPeriod() const override;
  void FetchOAuthToken(
      DeviceCommandStartCRDSessionJob::OAuthTokenCallback success_callback,
      DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) override;
  void FetchICEConfig(
      const std::string& oauth_token,
      DeviceCommandStartCRDSessionJob::ICEConfigCallback success_callback,
      DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) override;
  void StartCRDHostAndGetCode(
      const std::string& directory_bot_jid,
      const std::string& oauth_token,
      base::Value ice_config,
      DeviceCommandStartCRDSessionJob::AuthCodeCallback success_callback,
      DeviceCommandStartCRDSessionJob::ErrorCallback error_callback) override;

  // OAuth2TokenService::Consumer:
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  void OnICEConfigurationLoaded(std::unique_ptr<std::string> response_body);

  Profile* GetKioskProfile() const;

  DeviceCommandStartCRDSessionJob::OAuthTokenCallback oauth_success_callback_;
  DeviceCommandStartCRDSessionJob::ICEConfigCallback ice_success_callback_;
  DeviceCommandStartCRDSessionJob::ErrorCallback error_callback_;

  std::unique_ptr<OAuth2TokenService::Request> oauth_request_;
  std::unique_ptr<network::SimpleURLLoader> ice_config_loader_;

  base::WeakPtrFactory<CRDHostDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CRDHostDelegate);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_CRD_HOST_DELEGATE_H_
