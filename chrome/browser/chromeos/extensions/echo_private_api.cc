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
#include "chrome/common/extensions/api/echo_private.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"

namespace echo_api = extensions::api::echo_private;

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
        this, type));
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

  results_ = echo_api::GetRegistrationCode::Results::Create(result);
  SendResponse(true);
}

bool EchoPrivateGetRegistrationCodeFunction::RunImpl() {
  scoped_ptr<echo_api::GetRegistrationCode::Params> params =
      echo_api::GetRegistrationCode::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  GetRegistrationCode(params->type);
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
  results_ = echo_api::GetOobeTimestamp::Results::Create(timestamp);
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
      chromeos::CrosSettings::Get()->PrepareTrustedValues(base::Bind(
          &EchoPrivateCheckAllowRedeemOffersFunction::CheckAllowRedeemOffers,
          this));
  if (status == chromeos::CrosSettingsProvider::TEMPORARILY_UNTRUSTED)
    return;

  bool allow = true;
  chromeos::CrosSettings::Get()->GetBoolean(
      chromeos::kAllowRedeemChromeOsRegistrationOffers, &allow);
  results_ = echo_api::CheckAllowRedeemOffers::Results::Create(allow);
  SendResponse(true);
}

// Check the enterprise policy kAllowRedeemChromeOsRegistrationOffers flag
// value. This policy is used to control whether user can redeem offers using
// enterprise device.
bool EchoPrivateCheckAllowRedeemOffersFunction::RunImpl() {
  CheckAllowRedeemOffers();
  return true;
}

EchoPrivateGetUserConsentFunction::EchoPrivateGetUserConsentFunction()
    : redeem_offers_allowed_(false) {
}

EchoPrivateGetUserConsentFunction::~EchoPrivateGetUserConsentFunction() {}

bool EchoPrivateGetUserConsentFunction::RunImpl() {
  scoped_ptr<echo_api::GetUserConsent::Params> params =
       echo_api::GetUserConsent::Params::Create(*args_);
   EXTENSION_FUNCTION_VALIDATE(params);

   if (!GURL(params->consent_requester.origin).is_valid()) {
     error_ = "Invalid origin.";
     return false;
   }

   CheckRedeemOffersAllowed();
   return true;
}

void EchoPrivateGetUserConsentFunction::CheckRedeemOffersAllowed() {
  chromeos::CrosSettingsProvider::TrustedStatus status =
      chromeos::CrosSettings::Get()->PrepareTrustedValues(base::Bind(
          &EchoPrivateGetUserConsentFunction::CheckRedeemOffersAllowed,
          this));
  if (status == chromeos::CrosSettingsProvider::TEMPORARILY_UNTRUSTED)
    return;

  bool allow = true;
  chromeos::CrosSettings::Get()->GetBoolean(
      chromeos::kAllowRedeemChromeOsRegistrationOffers, &allow);

  OnRedeemOffersAllowedChecked(allow);
}

void EchoPrivateGetUserConsentFunction::OnRedeemOffersAllowedChecked(
    bool is_allowed) {
  redeem_offers_allowed_ = is_allowed;

  // TODO(tbarzic): Implement dialogs to be used here.
  results_ = echo_api::GetUserConsent::Results::Create(false);
  SendResponse(true);
}
