// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/cross_device_promo.h"

#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/variations/variations_associated_data.h"
#include "net/cookies/canonical_cookie.h"

namespace {

const int kDelayUntilGettingDeviceActivityInMS = 3000;
const int kDefaultBrowsingSessionDurationInMinutes = 15;

// Helper method to set a parameter based on a particular variable from the
// configuration. Returns false if the parameter was not found or the conversion
// could not succeed.
bool SetParameterFromVariation(
    const std::string& variation_parameter,
    base::TimeDelta* local_parameter,
    base::Callback<base::TimeDelta(int)> conversion) {
  std::string parameter_as_string = variations::GetVariationParamValue(
      "CrossDevicePromo", variation_parameter);
  if (parameter_as_string.empty())
    return false;

  int parameter_as_int;
  if (!base::StringToInt(parameter_as_string, &parameter_as_int))
    return false;

  *local_parameter = conversion.Run(parameter_as_int);
  return true;
}

}  // namespace

CrossDevicePromo::CrossDevicePromo(
    SigninManager* signin_manager,
    GaiaCookieManagerService* cookie_manager_service,
    SigninClient* signin_client,
    PrefService* pref_service)
    : initialized_(false),
      signin_manager_(signin_manager),
      cookie_manager_service_(cookie_manager_service),
      prefs_(pref_service),
      signin_client_(signin_client),
      is_throttled_(true),
      start_last_browsing_session_(base::Time()) {
  VLOG(1) << "CrossDevicePromo::CrossDevicePromo.";
  DCHECK(signin_manager_);
  DCHECK(cookie_manager_service_);
  DCHECK(prefs_);
  DCHECK(signin_client_);
  Init();
}

CrossDevicePromo::~CrossDevicePromo() {
}

void CrossDevicePromo::Shutdown() {
  VLOG(1) << "CrossDevicePromo::Shutdown.";
  UnregisterForCookieChanges();
  if (start_last_browsing_session_ != base::Time())
    signin_metrics::LogBrowsingSessionDuration(start_last_browsing_session_);
}

void CrossDevicePromo::AddObserver(CrossDevicePromo::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CrossDevicePromo::RemoveObserver(CrossDevicePromo::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool CrossDevicePromo::IsPromoActive() {
  return prefs_->GetBoolean(prefs::kCrossDevicePromoActive);
}

void CrossDevicePromo::Init() {
  DCHECK(!initialized_);
  // We need a default value for this as it is referenced early in
  // UpdateLastActiveTime and we want to gather as many stats about Browsing
  // Sessions as possible.
  inactivity_between_browsing_sessions_ =
      base::TimeDelta::FromMinutes(kDefaultBrowsingSessionDurationInMinutes);

  if (prefs_->GetBoolean(prefs::kCrossDevicePromoOptedOut)) {
    signin_metrics::LogXDevicePromoInitialized(
        signin_metrics::UNINITIALIZED_OPTED_OUT);
    return;
  }

  if (!SetParameterFromVariation("HoursBetweenSyncDeviceChecks",
                                 &delay_until_next_list_devices_,
                                 base::Bind(&base::TimeDelta::FromHours)) ||
      !SetParameterFromVariation("DaysToVerifySingleUserProfile",
                                 &single_account_duration_threshold_,
                                 base::Bind(&base::TimeDelta::FromDays)) ||
      !SetParameterFromVariation("MinutesBetweenBrowsingSessions",
                                 &inactivity_between_browsing_sessions_,
                                 base::Bind(&base::TimeDelta::FromMinutes)) ||
      !SetParameterFromVariation("MinutesMaxContextSwitchDuration",
                                 &context_switch_duration_,
                                 base::Bind(&base::TimeDelta::FromMinutes))) {
    signin_metrics::LogXDevicePromoInitialized(
        signin_metrics::NO_VARIATIONS_CONFIG);
    return;
  }

  std::string throttle =
      variations::GetVariationParamValue("CrossDevicePromo", "RPCThrottle");
  uint64 throttle_percent;
  if (throttle.empty() || !base::StringToUint64(throttle, &throttle_percent)) {
    signin_metrics::LogXDevicePromoInitialized(
        signin_metrics::NO_VARIATIONS_CONFIG);
    return;
  }

  is_throttled_ =
      throttle_percent && base::RandGenerator(100) < throttle_percent;

  VLOG(1) << "CrossDevicePromo::Init. Service initialized. Parameters: "
          << "Hour between RPC checks: "
          << delay_until_next_list_devices_.InHours()
          << " Days to verify an account in the cookie: "
          << single_account_duration_threshold_.InDays()
          << " Minutes between browsing sessions: "
          << inactivity_between_browsing_sessions_.InMinutes()
          << " Window (in minutes) for a context switch: "
          << context_switch_duration_.InMinutes()
          << " Throttle rate for RPC calls: " << throttle_percent
          << " This promo is throttled: " << is_throttled_;
  RegisterForCookieChanges();
  initialized_ = true;
  signin_metrics::LogXDevicePromoInitialized(signin_metrics::INITIALIZED);
  return;
}

void CrossDevicePromo::OptOut() {
  VLOG(1) << "CrossDevicePromo::OptOut.";
  UnregisterForCookieChanges();
  prefs_->SetBoolean(prefs::kCrossDevicePromoOptedOut, true);
  MarkPromoInactive();
}

bool CrossDevicePromo::VerifyPromoEligibleReadOnly() {
  if (!initialized_)
    return false;

  if (prefs_->GetBoolean(prefs::kCrossDevicePromoOptedOut))
    return false;

  if (!prefs_->HasPrefPath(prefs::kCrossDevicePromoObservedSingleAccountCookie))
    return false;

  if (GetTimePref(prefs::kCrossDevicePromoObservedSingleAccountCookie) +
          single_account_duration_threshold_ > base::Time::Now()) {
    return false;
  }

  return true;
}

bool CrossDevicePromo::CheckPromoEligibility() {
  if (!initialized_) {
    // In tests the variations may not be present when Init() was first called.
    Init();
    if (!initialized_)
      return false;
  }

  if (prefs_->GetBoolean(prefs::kCrossDevicePromoOptedOut)) {
    signin_metrics::LogXDevicePromoEligible(signin_metrics::OPTED_OUT);
    return false;
  }

  if (signin_manager_->IsAuthenticated()) {
    signin_metrics::LogXDevicePromoEligible(signin_metrics::SIGNED_IN);
    return false;
  }

  base::Time cookie_has_one_account_since = GetTimePref(
      prefs::kCrossDevicePromoObservedSingleAccountCookie);
  if (!prefs_->HasPrefPath(
          prefs::kCrossDevicePromoObservedSingleAccountCookie) ||
      cookie_has_one_account_since + single_account_duration_threshold_ >
          base::Time::Now()) {
    signin_metrics::LogXDevicePromoEligible(
        signin_metrics::NOT_SINGLE_GAIA_ACCOUNT);
    return false;
  }

  // This is the first time the promo's being run; determine when to call the
  // DeviceActivityFetcher.
  if (!prefs_->HasPrefPath(prefs::kCrossDevicePromoNextFetchListDevicesTime)) {
    const int minutes_until_next_activity_fetch =
        base::RandGenerator(delay_until_next_list_devices_.InMinutes());
    const base::Time time_of_next_device_activity_fetch = base::Time::Now() +
        base::TimeDelta::FromMinutes(minutes_until_next_activity_fetch);
    prefs_->SetInt64(prefs::kCrossDevicePromoNextFetchListDevicesTime,
                     time_of_next_device_activity_fetch.ToInternalValue());
    signin_metrics::LogXDevicePromoEligible(
        signin_metrics::UNKNOWN_COUNT_DEVICES);
    return false;
  }

  // We have no knowledge of other device activity yet.
  if (!prefs_->HasPrefPath(prefs::kCrossDevicePromoNumDevices)) {
    base::Time time_next_list_devices = GetTimePref(
        prefs::kCrossDevicePromoNextFetchListDevicesTime);
    // Not time yet to poll the list of devices.
    if (time_next_list_devices > base::Time::Now()) {
      signin_metrics::LogXDevicePromoEligible(
          signin_metrics::UNKNOWN_COUNT_DEVICES);
      return false;
    }
    // We're not eligible... but might be! Track metrics in the results.
    GetDevicesActivityForAccountInCookie();
    return false;
  }

  int num_devices = prefs_->GetInteger(prefs::kCrossDevicePromoNumDevices);
  base::Time time_next_list_devices = GetTimePref(
      prefs::kCrossDevicePromoNextFetchListDevicesTime);
  if (time_next_list_devices < base::Time::Now()) {
    GetDevicesActivityForAccountInCookie();
  } else if (num_devices == 0) {
    signin_metrics::LogXDevicePromoEligible(signin_metrics::ZERO_DEVICES);
    return false;
  }

  DCHECK(VerifyPromoEligibleReadOnly());
  return true;
}

void CrossDevicePromo::MaybeBrowsingSessionStarted(
    const base::Time& previous_last_active) {
  // In tests, or the first call for a profile, don't pass go.
  if (previous_last_active == base::Time())
    return;

  base::Time time_now = base::Time::Now();
  // Determine how often this method is called. Need an estimate for QPS.
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Signin.XDevicePromo.BrowsingSessionDurationComputed",
      (base::Time::Now() - previous_last_active).InMinutes(), 1,
      base::TimeDelta::FromDays(30).InMinutes(), 50);

  // Check if this is a different browsing session since the last call.
  if (time_now - previous_last_active <=
      inactivity_between_browsing_sessions_) {
    VLOG(1) << "CrossDevicePromo::UpdateLastActiveTime. Same browsing session "
               "as the last call.";
    return;
  }

  if (start_last_browsing_session_ != base::Time())
    signin_metrics::LogBrowsingSessionDuration(previous_last_active);

  start_last_browsing_session_ = time_now;

  if (!CheckPromoEligibility()) {
    VLOG(1) << "CrossDevicePromo::UpdateLastActiveTime; Ineligible for promo.";
    if (IsPromoActive())
      MarkPromoInactive();
    return;
  }

  // Check if there is a record of recent browsing activity on another device.
  // If there is none, we set a timer to update the records after a small delay
  // to ensure server-side data is synchronized.
  base::Time device_last_active = GetTimePref(
      prefs::kCrossDevicePromoLastDeviceActiveTime);
  if (time_now - device_last_active < context_switch_duration_) {
    VLOG(1) << "CrossDevicePromo::UpdateLastActiveTime; promo active.";
    signin_metrics::LogXDevicePromoEligible(signin_metrics::ELIGIBLE);
    MarkPromoActive();
    return;
  }

  // Check for recency of device activity unless a check is already being
  // executed because the number of devices is being updated.
  if (!device_activity_fetcher_.get()) {
    VLOG(1) << "CrossDevicePromo::UpdateLastActiveTime; Check device activity.";
    device_activity_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kDelayUntilGettingDeviceActivityInMS),
        this, &CrossDevicePromo::GetDevicesActivityForAccountInCookie);
  }
}

void CrossDevicePromo::GetDevicesActivityForAccountInCookie() {
  // Don't start a fetch while one is processing.
  if (device_activity_fetcher_.get())
    return;

  if (is_throttled_) {
    signin_metrics::LogXDevicePromoEligible(
        signin_metrics::THROTTLED_FETCHING_DEVICE_ACTIVITY);
    return;
  }

  VLOG(1) << "CrossDevicePromo::GetDevicesActivityForAccountInCookie. Start.";
  DCHECK(VerifyPromoEligibleReadOnly());
  device_activity_fetcher_.reset(
      new DeviceActivityFetcher(signin_client_, this));
  device_activity_fetcher_->Start();
}

void CrossDevicePromo::OnFetchDeviceActivitySuccess(
    const std::vector<DeviceActivityFetcher::DeviceActivity>& devices) {
  const base::Time time_now = base::Time::Now();
  VLOG(1) << "CrossDevicePromo::OnFetchDeviceActivitySuccess. "
          << devices.size() << " devices.";
  prefs_->SetInt64(
      prefs::kCrossDevicePromoNextFetchListDevicesTime,
      (time_now + delay_until_next_list_devices_).ToInternalValue());
  prefs_->SetInteger(prefs::kCrossDevicePromoNumDevices, devices.size());

  if (devices.empty()) {
    signin_metrics::LogXDevicePromoEligible(signin_metrics::ZERO_DEVICES);
    return;
  }

  base::Time most_recent_last_active =
      std::max_element(devices.begin(), devices.end(),
                       [](const DeviceActivityFetcher::DeviceActivity& first,
                          const DeviceActivityFetcher::DeviceActivity& second) {
                         return first.last_active < second.last_active;
                       })->last_active;

  prefs_->SetInt64(prefs::kCrossDevicePromoLastDeviceActiveTime,
                   most_recent_last_active.ToInternalValue());

  if (time_now - most_recent_last_active < context_switch_duration_) {
    // Make sure eligibility wasn't lost while executing the remote call.
    if (!VerifyPromoEligibleReadOnly())
      return;

    // The context switch will only be valid for so long. Schedule another
    // DeviceActivity check for when our switch would expire to check for more
    // recent activity.
    if (!device_activity_timer_.IsRunning()) {
      base::TimeDelta time_to_next_check = most_recent_last_active +
                                           context_switch_duration_ -
                                           time_now;
      device_activity_timer_.Start(
          FROM_HERE, time_to_next_check, this,
          &CrossDevicePromo::GetDevicesActivityForAccountInCookie);
    }

    signin_metrics::LogXDevicePromoEligible(signin_metrics::ELIGIBLE);
    MarkPromoActive();
  } else {
    signin_metrics::LogXDevicePromoEligible(signin_metrics::NO_ACTIVE_DEVICES);
    MarkPromoInactive();
  }
  device_activity_fetcher_.reset();
}

void CrossDevicePromo::OnFetchDeviceActivityFailure() {
  VLOG(1) << "CrossDevicePromo::OnFetchDeviceActivityFailure.";
  signin_metrics::LogXDevicePromoEligible(
      signin_metrics::ERROR_FETCHING_DEVICE_ACTIVITY);
  device_activity_fetcher_.reset();
}

void CrossDevicePromo::RegisterForCookieChanges() {
  cookie_manager_service_->AddObserver(this);
}

void CrossDevicePromo::UnregisterForCookieChanges() {
  cookie_manager_service_->RemoveObserver(this);
}

base::Time CrossDevicePromo::GetTimePref(const std::string& pref) {
  return base::Time::FromInternalValue(prefs_->GetInt64(pref));
}

void CrossDevicePromo::OnGaiaAccountsInCookieUpdated(
    const std::vector<gaia::ListedAccount>& accounts,
    const GoogleServiceAuthError& error) {
  VLOG(1) << "CrossDevicePromo::OnGaiaAccountsInCookieUpdated. "
          << accounts.size() << " accounts with auth error " << error.state();
  if (error.state() != GoogleServiceAuthError::State::NONE)
    return;

  // Multiple accounts seen - clear the pref.
  bool single_account = accounts.size() == 1;
  bool has_pref =
      prefs_->HasPrefPath(prefs::kCrossDevicePromoObservedSingleAccountCookie);
  if (!single_account && has_pref) {
    prefs_->ClearPref(prefs::kCrossDevicePromoObservedSingleAccountCookie);
    MarkPromoInactive();
  } else if (single_account && !has_pref) {
    prefs_->SetInt64(prefs::kCrossDevicePromoObservedSingleAccountCookie,
                     base::Time::Now().ToInternalValue());
  }
}

void CrossDevicePromo::MarkPromoActive() {
  VLOG(1) << "CrossDevicePromo::MarkPromoActive.";
  DCHECK(!prefs_->GetBoolean(prefs::kCrossDevicePromoOptedOut));

  if (!prefs_->GetBoolean(prefs::kCrossDevicePromoActive)) {
    prefs_->SetBoolean(prefs::kCrossDevicePromoActive, true);
    FOR_EACH_OBSERVER(CrossDevicePromo::Observer, observer_list_,
                      OnPromoActivationChanged(true));
  }
}

void CrossDevicePromo::MarkPromoInactive() {
  VLOG(1) << "CrossDevicePromo::MarkPromoInactive.";
  if (prefs_->GetBoolean(prefs::kCrossDevicePromoActive)) {
    prefs_->SetBoolean(prefs::kCrossDevicePromoActive, false);
    FOR_EACH_OBSERVER(CrossDevicePromo::Observer, observer_list_,
                      OnPromoActivationChanged(false));
  }
}
