// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_CRD_HOST_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_CRD_HOST_DELEGATE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_start_crd_session_job.h"

namespace policy {

// An implementation of the |DeviceCommandStartCRDSessionJob::Delegate|.
class CRDHostDelegate : public DeviceCommandStartCRDSessionJob::Delegate {
 public:
  CRDHostDelegate();
  ~CRDHostDelegate() override;

 private:
  // DeviceCommandScreenshotJob::Delegate:
  bool HasActiveSession() override;
  void TerminateSession(base::OnceClosure callback) override;
  bool AreServicesReady() override;
  bool IsRunningKiosk() override;
  base::TimeDelta GetIdlenessPeriod() override;
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

  DISALLOW_COPY_AND_ASSIGN(CRDHostDelegate);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_CRD_HOST_DELEGATE_H_
