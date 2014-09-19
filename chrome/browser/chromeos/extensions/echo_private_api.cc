// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/echo_private_api.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/ui/echo_dialog_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/api/echo_private.h"
#include "chrome/common/pref_names.h"
#include "chromeos/system/statistics_provider.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension.h"

namespace echo_api = extensions::api::echo_private;

using content::BrowserThread;

namespace {

// URL of "More info" link shown in echo dialog in GetUserConsent function.
const char kMoreInfoLink[] =
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html?"
    "answer=2677280";

}  // namespace

namespace chromeos {

namespace echo_offer {

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kEchoCheckedOffers);
}

} // namespace echo_offer

} // namespace chromeos

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
  const std::string kGroupType = "GROUP_CODE";

  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  std::string result;
  if (!chromeos::KioskModeSettings::Get()->IsKioskModeEnabled()) {
    // In Kiosk mode, we effectively disable the registration API
    // by always returning an empty code.
    if (type == kCouponType) {
      provider->GetMachineStatistic(chromeos::system::kOffersCouponCodeKey,
                                    &result);
    } else if (type == kGroupType) {
      provider->GetMachineStatistic(chromeos::system::kOffersGroupCodeKey,
                                    &result);
    }
  }

  results_ = echo_api::GetRegistrationCode::Results::Create(result);
  SendResponse(true);
}

bool EchoPrivateGetRegistrationCodeFunction::RunSync() {
  scoped_ptr<echo_api::GetRegistrationCode::Params> params =
      echo_api::GetRegistrationCode::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  GetRegistrationCode(params->type);
  return true;
}

EchoPrivateSetOfferInfoFunction::EchoPrivateSetOfferInfoFunction() {}

EchoPrivateSetOfferInfoFunction::~EchoPrivateSetOfferInfoFunction() {}

bool EchoPrivateSetOfferInfoFunction::RunSync() {
  scoped_ptr<echo_api::SetOfferInfo::Params> params =
      echo_api::SetOfferInfo::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string& service_id = params->id;
  base::DictionaryValue* dict = params->offer_info.
      additional_properties.DeepCopyWithoutEmptyChildren();

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate offer_update(local_state, prefs::kEchoCheckedOffers);
  offer_update->SetWithoutPathExpansion("echo." + service_id, dict);
  return true;
}

EchoPrivateGetOfferInfoFunction::EchoPrivateGetOfferInfoFunction() {}

EchoPrivateGetOfferInfoFunction::~EchoPrivateGetOfferInfoFunction() {}

bool EchoPrivateGetOfferInfoFunction::RunSync() {
  scoped_ptr<echo_api::GetOfferInfo::Params> params =
      echo_api::GetOfferInfo::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string& service_id = params->id;
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* offer_infos = local_state->
      GetDictionary(prefs::kEchoCheckedOffers);

  const base::DictionaryValue* offer_info = NULL;
  if (!offer_infos->GetDictionaryWithoutPathExpansion(
         "echo." + service_id, &offer_info)) {
    error_ = "Not found";
    return false;
  }

  echo_api::GetOfferInfo::Results::Result result;
  result.additional_properties.MergeDictionary(offer_info);
  results_ = echo_api::GetOfferInfo::Results::Create(result);
  return true;
}

EchoPrivateGetOobeTimestampFunction::EchoPrivateGetOobeTimestampFunction() {
}

EchoPrivateGetOobeTimestampFunction::~EchoPrivateGetOobeTimestampFunction() {
}

bool EchoPrivateGetOobeTimestampFunction::RunAsync() {
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
  base::File::Info fileInfo;
  if (base::GetFileInfo(base::FilePath(kOobeTimestampFile), &fileInfo)) {
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

EchoPrivateGetUserConsentFunction::EchoPrivateGetUserConsentFunction()
    : redeem_offers_allowed_(false) {
}

// static
scoped_refptr<EchoPrivateGetUserConsentFunction>
EchoPrivateGetUserConsentFunction::CreateForTest(
      const DialogShownTestCallback& dialog_shown_callback) {
  scoped_refptr<EchoPrivateGetUserConsentFunction> function(
      new EchoPrivateGetUserConsentFunction());
  function->dialog_shown_callback_ = dialog_shown_callback;
  return function;
}

EchoPrivateGetUserConsentFunction::~EchoPrivateGetUserConsentFunction() {}

bool EchoPrivateGetUserConsentFunction::RunAsync() {
   CheckRedeemOffersAllowed();
   return true;
}

void EchoPrivateGetUserConsentFunction::OnAccept() {
  Finalize(true);
}

void EchoPrivateGetUserConsentFunction::OnCancel() {
  Finalize(false);
}

void EchoPrivateGetUserConsentFunction::OnMoreInfoLinkClicked() {
  chrome::NavigateParams params(
      GetProfile(), GURL(kMoreInfoLink), ui::PAGE_TRANSITION_LINK);
  // Open the link in a new window. The echo dialog is modal, so the current
  // window is useless until the dialog is closed.
  params.disposition = NEW_WINDOW;
  chrome::Navigate(&params);
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

  scoped_ptr<echo_api::GetUserConsent::Params> params =
      echo_api::GetUserConsent::Params::Create(*args_);

  // Verify that the passed origin URL is valid.
  GURL service_origin = GURL(params->consent_requester.origin);
  if (!service_origin.is_valid()) {
    error_ = "Invalid origin.";
    SendResponse(false);
    return;
  }

  // Add ref to ensure the function stays around until the dialog listener is
  // called. The reference is release in |Finalize|.
  AddRef();

  // Create and show the dialog.
  chromeos::EchoDialogView* dialog = new chromeos::EchoDialogView(this);
  if (redeem_offers_allowed_) {
    dialog->InitForEnabledEcho(
        base::UTF8ToUTF16(params->consent_requester.service_name),
        base::UTF8ToUTF16(params->consent_requester.origin));
  } else {
    dialog->InitForDisabledEcho();
  }
  dialog->Show(GetCurrentBrowser()->window()->GetNativeWindow());

  // If there is a dialog_shown_callback_, invoke it with the created dialog.
  if (!dialog_shown_callback_.is_null())
    dialog_shown_callback_.Run(dialog);
}

void EchoPrivateGetUserConsentFunction::Finalize(bool consent) {
  // Consent should not be true if offers redeeming is disabled.
  CHECK(redeem_offers_allowed_ || !consent);
  results_ = echo_api::GetUserConsent::Results::Create(consent);
  SendResponse(true);

  // Release the reference added in |OnRedeemOffersAllowedChecked|, before
  // showing the dialog.
  Release();
}
