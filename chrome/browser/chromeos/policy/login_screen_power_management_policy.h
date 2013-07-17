// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_LOGIN_SCREEN_POWER_MANAGEMENT_POLICY_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_LOGIN_SCREEN_POWER_MANAGEMENT_POLICY_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class Value;
}

namespace policy {

class PolicyErrorMap;

// Parses power management policy encoded as a JSON string and extracts the
// individual settings.
// TODO(bartfab): Remove this code once the policy system has full support for
// the 'dict' policy type, parsing JSON strings and verifying them against a
// JSON schema internally. http://crbug.com/258339
class LoginScreenPowerManagementPolicy {
 public:
  LoginScreenPowerManagementPolicy();
  ~LoginScreenPowerManagementPolicy();

  bool Init(const std::string& json, PolicyErrorMap* errors);

  const base::Value* GetScreenDimDelayAC() const;
  const base::Value* GetScreenOffDelayAC() const;
  const base::Value* GetIdleDelayAC() const;
  const base::Value* GetScreenDimDelayBattery() const;
  const base::Value* GetScreenOffDelayBattery() const;
  const base::Value* GetIdleDelayBattery() const;
  const base::Value* GetIdleActionAC() const;
  const base::Value* GetIdleActionBattery() const;
  const base::Value* GetLidCloseAction() const;
  const base::Value* GetUserActivityScreenDimDelayScale() const;

 private:
  scoped_ptr<base::Value> screen_dim_delay_ac_;
  scoped_ptr<base::Value> screen_off_delay_ac_;
  scoped_ptr<base::Value> idle_delay_ac_;
  scoped_ptr<base::Value> screen_dim_delay_battery_;
  scoped_ptr<base::Value> screen_off_delay_battery_;
  scoped_ptr<base::Value> idle_delay_battery_;
  scoped_ptr<base::Value> idle_action_ac_;
  scoped_ptr<base::Value> idle_action_battery_;
  scoped_ptr<base::Value> lid_close_action_;
  scoped_ptr<base::Value> user_activity_screen_dim_delay_scale_;

  DISALLOW_COPY_AND_ASSIGN(LoginScreenPowerManagementPolicy);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_LOGIN_SCREEN_POWER_MANAGEMENT_POLICY_H_
