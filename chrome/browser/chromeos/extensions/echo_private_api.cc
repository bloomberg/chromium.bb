// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/echo_private_api.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/values.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/common/extensions/extension.h"

namespace {

// For a given registration code type, returns the code value from the
// underlying system. Caller owns the returned pointer.
base::Value* GetValueForRegistrationCodeType(std::string& type) {
  // Possible ECHO code type and corresponding key name in StatisticsProvider.
  const std::string kCouponType = "COUPON_CODE";
  const std::string kCouponCodeKey = "ubind_attribute";
  const std::string kGroupType = "GROUP_CODE";
  const std::string kGroupCodeKey = "gbind_attribute";

  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  std::string result;
  if (type == kCouponType)
    provider->GetMachineStatistic(kCouponCodeKey, &result);
  else if (type == kGroupType)
    provider->GetMachineStatistic(kGroupCodeKey, &result);
  return Value::CreateStringValue(result);
}

}  // namespace


GetRegistrationCodeFunction::GetRegistrationCodeFunction() {
}

GetRegistrationCodeFunction::~GetRegistrationCodeFunction() {
}

bool GetRegistrationCodeFunction::RunImpl() {
  std::string type;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &type));
  result_.reset(GetValueForRegistrationCodeType(type));
  return true;
}
