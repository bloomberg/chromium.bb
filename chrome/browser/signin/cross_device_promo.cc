// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/cross_device_promo.h"

#include <stdint.h>

#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/variations/variations_associated_data.h"

namespace {

// Helper method to set a |local_parameter| to the result of fetching a
// |variation_parameter| from the variation seed, converting to an int, and then
// applying |conversion| to create a TimeDelta. Returns false if the
// |variation_parameter| was not found or the string could not be parsed to an
// int.
bool SetParameterFromVariation(
    const std::string& variation_parameter,
    base::Callback<base::TimeDelta(int)> conversion,
    base::TimeDelta* local_parameter) {
  std::string parameter_as_string = variations::GetVariationParamValue(
      CrossDevicePromo::kCrossDevicePromoFieldTrial, variation_parameter);
  if (parameter_as_string.empty())
    return false;

  int parameter_as_int;
  if (!base::StringToInt(parameter_as_string, &parameter_as_int))
    return false;

  *local_parameter = conversion.Run(parameter_as_int);
  return true;
}

}  // namespace

// static
const char CrossDevicePromo::kCrossDevicePromoFieldTrial[] = "CrossDevicePromo";
const char CrossDevicePromo::kParamHoursBetweenDeviceActivityChecks[] =
    "HoursBetweenDeviceActivityChecks";
const char CrossDevicePromo::kParamDaysToVerifySingleUserProfile[] =
    "DaysToVerifySingleUserProfile";
const char CrossDevicePromo::kParamMinutesBetweenBrowsingSessions[] =
    "MinutesBetweenBrowsingSessions";
const char CrossDevicePromo::kParamMinutesMaxContextSwitchDuration[] =
    "MinutesMaxContextSwitchDuration";
const char CrossDevicePromo::kParamRPCThrottle[] =
    "RPCThrottle";

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

void CrossDevicePromo::OnGaiaAccountsInCookieUpdated(
    const std::vector<gaia::ListedAccount>& accounts,
    const GoogleServiceAuthError& error) {
  VLOG(1) << "CrossDevicePromo::OnGaiaAccountsInCookieUpdated. "
          << accounts.size() << " accounts with auth error " << error.state();
  if (error.state() != GoogleServiceAuthError::State::NONE)
    return;

  const bool single_account = accounts.size() == 1;
  const bool has_pref =
      prefs_->HasPrefPath(prefs::kCrossDevicePromoObservedSingleAccountCookie);
  if (!single_account && has_pref) {
    prefs_->ClearPref(prefs::kCrossDevicePromoObservedSingleAccountCookie);
    MarkPromoShouldNotBeShown();
  } else if (single_account && !has_pref) {
    SetTimePref(prefs::kCrossDevicePromoObservedSingleAccountCookie,
                base::Time::Now());
  }
}

void CrossDevicePromo::OnFetchDeviceActivitySuccess(
    const std::vector<DeviceActivityFetcher::DeviceActivity>& devices) {
  VLOG(1) << "CrossDevicePromo::OnFetchDeviceActivitySuccess. "
          << devices.size() << " devices.";
  DetermineEligibilityFromDeviceActivity(devices);

  // |device_activity_fetcher_| must be destroyed last as that object is what
  // called OnFetchDeviceActivitySuccess().
  // TODO(mlerman): Mark the DeviceActivityFetcher as stopped() or completed()
  // rather than deleting the object.
  device_activity_fetcher_.reset();
}

void CrossDevicePromo::OnFetchDeviceActivityFailure() {
  VLOG(1) << "CrossDevicePromo::OnFetchDeviceActivityFailure.";
  signin_metrics::LogXDevicePromoEligible(
      signin_metrics::ERROR_FETCHING_DEVICE_ACTIVITY);

  // |device_activity_fetcher_| must be destroyed last as that object is what
  // called OnFetchDeviceActivityFailure().
  device_activity_fetcher_.reset();
}

bool CrossDevicePromo::ShouldShowPromo() const {
  return prefs_->GetBoolean(prefs::kCrossDevicePromoShouldBeShown);
}

void CrossDevicePromo::OptOut() {
  VLOG(1) << "CrossDevicePromo::OptOut.";
  UnregisterForCookieChanges();
  prefs_->SetBoolean(prefs::kCrossDevicePromoOptedOut, true);
  MarkPromoShouldNotBeShown();
}

void CrossDevicePromo::MaybeBrowsingSessionStarted(
    const base::Time& previous_last_active) {
  // In tests, or the first call for a profile, do nothing.
  if (previous_last_active == base::Time())
    return;

  const base::Time time_now = base::Time::Now();
  // Determine how often this method is called as an upper bound for how often
  // back end servers might be called.
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Signin.XDevicePromo.BrowsingSessionDurationComputed",
      (base::Time::Now() - previous_last_active).InMinutes(), 1,
      base::TimeDelta::FromDays(30).InMinutes(), 50);

  // Check if this is a different browsing session since the last call.
  if (time_now - previous_last_active <
      inactivity_between_browsing_sessions_) {
    VLOG(1) << "CrossDevicePromo::MaybeBrowsingSessionStarted. Same browsing "
               "session as the last call.";
    return;
  }

  if (start_last_browsing_session_ != base::Time())
    signin_metrics::LogBrowsingSessionDuration(previous_last_active);

  start_last_browsing_session_ = time_now;

  if (!CheckPromoEligibility()) {
    VLOG(1) << "CrossDevicePromo::MaybeBrowsingSessionStarted; "
            << "Ineligible for promo.";
    if (ShouldShowPromo())
      MarkPromoShouldNotBeShown();
    return;
  }

  // Check if there is a record of recent browsing activity on another device.
  const base::Time device_last_active = GetTimePref(
      prefs::kCrossDevicePromoLastDeviceActiveTime);
  if (time_now - device_last_active < context_switch_duration_) {
    VLOG(1) << "CrossDevicePromo::MaybeBrowsingSessionStarted; promo active.";
    signin_metrics::LogXDevicePromoEligible(signin_metrics::ELIGIBLE);
    MarkPromoShouldBeShown();
    return;
  }

  // Check for recency of device activity unless a check is already being
  // executed because the number of devices is being updated. Use a small delay
  // to ensure server-side data is synchronized.
  if (!device_activity_fetcher_) {
    VLOG(1) << "CrossDevicePromo::MaybeBrowsingSessionStarted; "
            << "Check device activity.";
    const int kDelayUntilGettingDeviceActivityInMS = 3000;
    device_activity_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kDelayUntilGettingDeviceActivityInMS),
        this, &CrossDevicePromo::GetDevicesActivityForGAIAAccountInCookieJar);
  }
}

bool CrossDevicePromo::CheckPromoEligibilityForTesting() {
  // The field trial may not have been present when Init() was called from the
  // constructor.
  if (!initialized_)
    Init();

  return CheckPromoEligibility();
}

void CrossDevicePromo::Init() {
  DCHECK(!initialized_);
  // We need a default value for |inactivity_between_browsing_sessions_|
  // as it is referenced early in MaybeBrowsingSessionStarted and we want to
  // gather as many stats about browsing sessions as possible.
  const int kDefaultBrowsingSessionDurationInMinutes = 15;
  inactivity_between_browsing_sessions_ =
      base::TimeDelta::FromMinutes(kDefaultBrowsingSessionDurationInMinutes);

  if (prefs_->GetBoolean(prefs::kCrossDevicePromoOptedOut)) {
    signin_metrics::LogXDevicePromoInitialized(
        signin_metrics::UNINITIALIZED_OPTED_OUT);
    return;
  }

  if (!SetParameterFromVariation(kParamHoursBetweenDeviceActivityChecks,
                                 base::Bind(&base::TimeDelta::FromHours),
                                 &delay_until_next_device_activity_fetch_) ||
      !SetParameterFromVariation(kParamDaysToVerifySingleUserProfile,
                                 base::Bind(&base::TimeDelta::FromDays),
                                 &single_account_duration_threshold_) ||
      !SetParameterFromVariation(kParamMinutesBetweenBrowsingSessions,
                                 base::Bind(&base::TimeDelta::FromMinutes),
                                 &inactivity_between_browsing_sessions_) ||
      !SetParameterFromVariation(kParamMinutesMaxContextSwitchDuration,
                                 base::Bind(&base::TimeDelta::FromMinutes),
                                 &context_switch_duration_)) {
    signin_metrics::LogXDevicePromoInitialized(
        signin_metrics::NO_VARIATIONS_CONFIG);
    return;
  }

  const std::string throttle = variations::GetVariationParamValue(
      kCrossDevicePromoFieldTrial, kParamRPCThrottle);
  int throttle_percent;
  if (throttle.empty() || !base::StringToInt(throttle, &throttle_percent)) {
    signin_metrics::LogXDevicePromoInitialized(
        signin_metrics::NO_VARIATIONS_CONFIG);
    return;
  }

  is_throttled_ = throttle_percent && base::RandInt(0, 99) < throttle_percent;

  VLOG(1) << "CrossDevicePromo::Init. Service initialized. Parameters: "
          << "Hour between RPC checks: "
          << delay_until_next_device_activity_fetch_.InHours()
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

void CrossDevicePromo::MarkPromoShouldBeShown() {
  VLOG(1) << "CrossDevicePromo::MarkPromoShouldBeShown.";
  DCHECK(!prefs_->GetBoolean(prefs::kCrossDevicePromoOptedOut));

  if (!prefs_->GetBoolean(prefs::kCrossDevicePromoShouldBeShown)) {
    prefs_->SetBoolean(prefs::kCrossDevicePromoShouldBeShown, true);
    FOR_EACH_OBSERVER(CrossDevicePromo::Observer, observer_list_,
                      OnPromoEligibilityChanged(true));
  }
}

void CrossDevicePromo::MarkPromoShouldNotBeShown() {
  VLOG(1) << "CrossDevicePromo::MarkPromoShouldNotBeShown.";
  if (prefs_->GetBoolean(prefs::kCrossDevicePromoShouldBeShown)) {
    prefs_->SetBoolean(prefs::kCrossDevicePromoShouldBeShown, false);
    FOR_EACH_OBSERVER(CrossDevicePromo::Observer, observer_list_,
                      OnPromoEligibilityChanged(false));
  }
}

bool CrossDevicePromo::CheckPromoEligibility() {
  if (!initialized_)
    return false;

  if (prefs_->GetBoolean(prefs::kCrossDevicePromoOptedOut)) {
    signin_metrics::LogXDevicePromoEligible(signin_metrics::OPTED_OUT);
    return false;
  }

  if (signin_manager_->IsAuthenticated()) {
    signin_metrics::LogXDevicePromoEligible(signin_metrics::SIGNED_IN);
    return false;
  }

  if (!prefs_->HasPrefPath(
          prefs::kCrossDevicePromoObservedSingleAccountCookie) ||
      (GetTimePref(prefs::kCrossDevicePromoObservedSingleAccountCookie) +
          single_account_duration_threshold_ > base::Time::Now())) {
    signin_metrics::LogXDevicePromoEligible(
        signin_metrics::NOT_SINGLE_GAIA_ACCOUNT);
    return false;
  }

  if (!prefs_->HasPrefPath(prefs::kCrossDevicePromoNextFetchListDevicesTime)) {
    // The missing preference indicates CheckPromoEligibility() has never been
    // called. Determine when to call the DeviceActivityFetcher for the first
    // time.
    const uint64_t milliseconds_until_next_activity_fetch = base::RandGenerator(
        delay_until_next_device_activity_fetch_.InMilliseconds());
    const base::Time time_of_next_device_activity_fetch = base::Time::Now() +
        base::TimeDelta::FromMilliseconds(
            milliseconds_until_next_activity_fetch);
    SetTimePref(prefs::kCrossDevicePromoNextFetchListDevicesTime,
        time_of_next_device_activity_fetch);
    signin_metrics::LogXDevicePromoEligible(
        signin_metrics::UNKNOWN_COUNT_DEVICES);
    return false;
  }

  if (!prefs_->HasPrefPath(prefs::kCrossDevicePromoNumDevices)) {
    // The missing pref indicates no knowledge of other device activity yet.
    const base::Time time_next_list_devices = GetTimePref(
        prefs::kCrossDevicePromoNextFetchListDevicesTime);
    // Not time yet to poll the list of devices.
    if (base::Time::Now() < time_next_list_devices) {
      signin_metrics::LogXDevicePromoEligible(
          signin_metrics::UNKNOWN_COUNT_DEVICES);
      return false;
    }
    // We're not eligible... yet! Track metrics in the results.
    GetDevicesActivityForGAIAAccountInCookieJar();
    return false;
  }

  int num_devices = prefs_->GetInteger(prefs::kCrossDevicePromoNumDevices);
  const base::Time time_next_list_devices = GetTimePref(
      prefs::kCrossDevicePromoNextFetchListDevicesTime);
  if (base::Time::Now() > time_next_list_devices) {
    GetDevicesActivityForGAIAAccountInCookieJar();
  } else if (num_devices == 0) {
    signin_metrics::LogXDevicePromoEligible(signin_metrics::ZERO_DEVICES);
    return false;
  }

  DCHECK(VerifyPromoEligibleReadOnly());
  return true;
}

void CrossDevicePromo::DetermineEligibilityFromDeviceActivity(
    const std::vector<DeviceActivityFetcher::DeviceActivity>& devices) {

  const base::Time time_now = base::Time::Now();
  SetTimePref(prefs::kCrossDevicePromoNextFetchListDevicesTime,
      time_now + delay_until_next_device_activity_fetch_);
  prefs_->SetInteger(prefs::kCrossDevicePromoNumDevices, devices.size());

  if (devices.empty()) {
    signin_metrics::LogXDevicePromoEligible(signin_metrics::ZERO_DEVICES);
    return;
  }

  const base::Time most_recent_last_active =
      std::max_element(devices.begin(), devices.end(),
                       [](const DeviceActivityFetcher::DeviceActivity& first,
                          const DeviceActivityFetcher::DeviceActivity& second) {
                         return first.last_active < second.last_active;
                       })->last_active;

  SetTimePref(prefs::kCrossDevicePromoLastDeviceActiveTime,
      most_recent_last_active);

  if (time_now - most_recent_last_active < context_switch_duration_) {
    // Make sure that while the DeviceActivityFetcher was executing the promo
    // wasn't found as ineligible to be shown.
    if (!VerifyPromoEligibleReadOnly())
      return;

    // The context switch will only be valid for so long. Schedule another
    // device activity check for when our switch would expire to check for more
    // recent activity.
    if (!device_activity_timer_.IsRunning()) {
      base::TimeDelta time_to_next_check = most_recent_last_active +
                                           context_switch_duration_ -
                                           time_now;
      device_activity_timer_.Start(
          FROM_HERE, time_to_next_check, this,
          &CrossDevicePromo::GetDevicesActivityForGAIAAccountInCookieJar);
    }

    signin_metrics::LogXDevicePromoEligible(signin_metrics::ELIGIBLE);
    MarkPromoShouldBeShown();
  } else {
    signin_metrics::LogXDevicePromoEligible(signin_metrics::NO_ACTIVE_DEVICES);
    MarkPromoShouldNotBeShown();
  }
}

base::Time CrossDevicePromo::GetTimePref(const std::string& pref) const {
  return base::Time::FromInternalValue(prefs_->GetInt64(pref));
}

void CrossDevicePromo::SetTimePref(const std::string& pref,
                                   const base::Time& value) {
  prefs_->SetInt64(pref, value.ToInternalValue());
}

bool CrossDevicePromo::VerifyPromoEligibleReadOnly() const {
  return initialized_ &&
      !prefs_->GetBoolean(prefs::kCrossDevicePromoOptedOut) &&
      prefs_->HasPrefPath(
          prefs::kCrossDevicePromoObservedSingleAccountCookie) &&
      GetTimePref(prefs::kCrossDevicePromoObservedSingleAccountCookie) +
          single_account_duration_threshold_ <= base::Time::Now();
}

void CrossDevicePromo::GetDevicesActivityForGAIAAccountInCookieJar() {
  // Don't start a fetch while one is processing.
  if (device_activity_fetcher_)
    return;

  if (is_throttled_) {
    signin_metrics::LogXDevicePromoEligible(
        signin_metrics::THROTTLED_FETCHING_DEVICE_ACTIVITY);
    return;
  }

  VLOG(1) << "CrossDevicePromo::GetDevicesActivityForGAIAAccountInCookieJar.";
  DCHECK(VerifyPromoEligibleReadOnly());
  device_activity_fetcher_.reset(
      new DeviceActivityFetcher(signin_client_, this));
  device_activity_fetcher_->Start();
}

void CrossDevicePromo::RegisterForCookieChanges() {
  cookie_manager_service_->AddObserver(this);
}

void CrossDevicePromo::UnregisterForCookieChanges() {
  cookie_manager_service_->RemoveObserver(this);
}
