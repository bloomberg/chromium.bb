// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/echo_private_api.h"

#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

EchoPrivateGetRegistrationCodeFunction::
    EchoPrivateGetRegistrationCodeFunction() {}

EchoPrivateGetRegistrationCodeFunction::
    ~EchoPrivateGetRegistrationCodeFunction() {}

void EchoPrivateGetRegistrationCodeFunction::GetRegistrationCode(
    const std::string& type) {
  if (!chromeos::KioskModeSettings::Get()->is_initialized()) {
    chromeos::KioskModeSettings::Get()->Initialize(base::Bind(
        &EchoPrivateGetRegistrationCodeFunction::GetRegistrationCode,
        base::Unretained(this),
        type));
    return;
  }
  // Possible ECHO code type and corresponding key name in StatisticsProvider.
  const std::string kCouponType = "COUPON_CODE";
  const std::string kCouponCodeKey = "ubind_attribute";
  const std::string kGroupType = "GROUP_CODE";
  const std::string kGroupCodeKey = "gbind_attribute";

  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  std::string result;
  if (!chromeos::KioskModeSettings::Get()->IsKioskModeEnabled()) {
    // In Kiosk mode, we effectively disable the registration API
    // by always returning an empty code.
    if (type == kCouponType)
      provider->GetMachineStatistic(kCouponCodeKey, &result);
    else if (type == kGroupType)
      provider->GetMachineStatistic(kGroupCodeKey, &result);
  }
  SetResult(new base::StringValue(result));
  SendResponse(true);
}

bool EchoPrivateGetRegistrationCodeFunction::RunImpl() {
  std::string type;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &type));
  GetRegistrationCode(type);
  return true;
}

EchoPrivateGetOobeTimestampFunction::EchoPrivateGetOobeTimestampFunction() {
}

EchoPrivateGetOobeTimestampFunction::~EchoPrivateGetOobeTimestampFunction() {
}

bool EchoPrivateGetOobeTimestampFunction::RunImpl() {
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &EchoPrivateGetOobeTimestampFunction::GetOobeTimestampOnFileThread,
          this),
      base::Bind(
          &EchoPrivateGetOobeTimestampFunction::SendResponse, this));
  return true;
}

// Get the OOBE timestamp from file /home/chronos/.oobe_completed.
// The timestamp is used to determine when the user first activates the device.
// If we can get the timestamp info, return it as yyyy-mm-dd, otherwise, return
// an empty string.
bool EchoPrivateGetOobeTimestampFunction::GetOobeTimestampOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  const char kOobeTimestampFile[] = "/home/chronos/.oobe_completed";
  std::string timestamp = "";
  base::PlatformFileInfo fileInfo;
  if (file_util::GetFileInfo(base::FilePath(kOobeTimestampFile), &fileInfo)) {
    base::Time::Exploded ctime;
    fileInfo.creation_time.UTCExplode(&ctime);
    timestamp += base::StringPrintf("%u-%u-%u",
                                    ctime.year,
                                    ctime.month,
                                    ctime.day_of_month);
  }
  SetResult(new base::StringValue(timestamp));
  return true;
}

EchoPrivateCheckAllowRedeemOffersFunction::
    EchoPrivateCheckAllowRedeemOffersFunction() {
}

EchoPrivateCheckAllowRedeemOffersFunction::
    ~EchoPrivateCheckAllowRedeemOffersFunction() {
}

void EchoPrivateCheckAllowRedeemOffersFunction::CheckAllowRedeemOffers() {
  chromeos::CrosSettingsProvider::TrustedStatus status =
      chromeos::CrosSettings::Get()->PrepareTrustedValues(
          base::Bind(&EchoPrivateCheckAllowRedeemOffersFunction::
                     CheckAllowRedeemOffers,
                     base::Unretained(this)));
  if (status == chromeos::CrosSettingsProvider::TEMPORARILY_UNTRUSTED)
    return;

  bool allow;
  if (!chromeos::CrosSettings::Get()->GetBoolean(
          chromeos::kAllowRedeemChromeOsRegistrationOffers, &allow)) {
    allow = true;
  }
  SetResult(new base::FundamentalValue(allow));
  SendResponse(true);
}

// Check the enterprise policy kAllowRedeemChromeOsRegistrationOffers flag
// value. This policy is used to control whether user can redeem offers using
// enterprise device.
bool EchoPrivateCheckAllowRedeemOffersFunction::RunImpl() {
  CheckAllowRedeemOffers();
  return true;
}
