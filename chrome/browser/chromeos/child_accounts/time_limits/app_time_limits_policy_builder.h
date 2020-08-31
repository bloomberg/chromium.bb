// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMITS_POLICY_BUILDER_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMITS_POLICY_BUILDER_H_

#include "base/values.h"

namespace chromeos {
namespace app_time {

class AppId;
class AppLimit;

// Builds PerAppTimeLimits policy for tests.
class AppTimeLimitsPolicyBuilder {
 public:
  AppTimeLimitsPolicyBuilder();
  AppTimeLimitsPolicyBuilder(const AppTimeLimitsPolicyBuilder&) = delete;
  AppTimeLimitsPolicyBuilder& operator=(const AppTimeLimitsPolicyBuilder&) =
      delete;
  ~AppTimeLimitsPolicyBuilder();

  // Adds app limit data to the policy.
  void AddAppLimit(const AppId& app_id, const AppLimit& app_limit);

  // Sets reset time in the policy.
  void SetResetTime(int hour, int minutes);

  void SetAppActivityReportingEnabled(bool enabled);

  const base::Value& value() const { return value_; }

 private:
  base::Value value_{base::Value::Type::DICTIONARY};
};

}  // namespace app_time
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMITS_POLICY_BUILDER_H_
