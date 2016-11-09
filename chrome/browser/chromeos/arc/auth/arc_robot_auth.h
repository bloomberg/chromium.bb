// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_ROBOT_AUTH_H_
#define CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_ROBOT_AUTH_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "components/policy/core/common/cloud/device_management_service.h"

namespace arc {

// This class is responsible to fetch auth code for robot account. Robot auth
// code is used for creation an account on Android side in ARC kiosk mode.
class ArcRobotAuth {
 public:
  using RobotAuthCodeCallback = base::Callback<void(const std::string&)>;
  ArcRobotAuth();
  ~ArcRobotAuth();

  // Fetches robot auth code. When auth code is fetched, the callback is
  // invoked. Invoking callback with empty string means a fetch error.
  // FetchRobotAuthCode() should not be called while another inflight operation
  // is running.
  void FetchRobotAuthCode(const RobotAuthCodeCallback& callback);

 private:
  void OnFetchRobotAuthCodeCompleted(
      RobotAuthCodeCallback callback,
      policy::DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  std::unique_ptr<policy::DeviceManagementRequestJob> fetch_request_job_;
  base::WeakPtrFactory<ArcRobotAuth> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcRobotAuth);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_ROBOT_AUTH_H_
