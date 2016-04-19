// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CROSS_DEVICE_PROMO_H_
#define CHROME_BROWSER_SIGNIN_CROSS_DEVICE_PROMO_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/device_activity_fetcher.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"

class PrefService;
class SigninClient;
class SigninManager;

// The Cross Device Promo promotes Chrome Signin within a profile where there is
// one GAIA account signed in to the content area for a long period of time
// (indicating that this profile is used by only one user) and where that
// account uses Chrome Sync on other devices but is not signed in here.
//
// This class determines whether the above criteria have been met and thus
// whether the promo should be displayed. This class imposes additional criteria
// such that the promo is only displayed if there is sufficiently recent
// activity on another device, indicating the user is switching contexts but
// continuing their browsing activity. This class's behavior is controlled by
// the "CrossDevicePromo" field trial.
// The UI is defined elsewhere.
//
// This class implements the preferences necessary to track whether a profile
// meets the above criteria, and whether the user has permanently opted out of
// the promo.
//
// The class relies on the GaiaCookieManagerService to determine if there has
// been a single GAIA account signed in to the content area for a long period of
// time, and relies on the DeviceActivityFetcher to determine if that account
// uses Chrome Sync on other devices.
class CrossDevicePromo : public KeyedService,
                         public GaiaCookieManagerService::Observer,
                         public DeviceActivityFetcher::Observer {
 public:
  class Observer {
   public:
    // Called when the profile moves from being ineligible to eligible for the
    // promo or vice versa; the new state is noted in the |eligible| parameter.
    virtual void OnPromoEligibilityChanged(bool eligible) = 0;
  };

  // The following constants are the parameters for a particular experiment
  // within the field trial that controls this class's behaviors.
  static const char kCrossDevicePromoFieldTrial[];
  // This field trial parameter specifies how often the device activity should
  // be fetched.
  static const char kParamHoursBetweenDeviceActivityChecks[];
  // This field trial parameter defines for how long the profile's cookie must
  // contain exactly one GAIA account before the profile is considered
  // single-user.
  static const char kParamDaysToVerifySingleUserProfile[];
  // This field trial parameter defines how much time must pass between calls
  // to MaybeBrowsingSessionStarted() before the code considers a new browsing
  // session to have started and re-evaluates if the promo should be shown.
  static const char kParamMinutesBetweenBrowsingSessions[];
  // This field trial parameter defines how much time may pass between the last
  // observed device activity and the start of a new browsing session for the
  // promo to consider the new browsing session to be a context switch.
  static const char kParamMinutesMaxContextSwitchDuration[];
  // This field trial parameter defines what percentage of all device activity
  // fetches should be not be executed, to throttle requests to the server.
  static const char kParamRPCThrottle[];

  // Constructor takes non-null pointers to required services. This object does
  // not take ownership of any of the passed objects. This also calls Init().
  explicit CrossDevicePromo(SigninManager* signin_manager,
                            GaiaCookieManagerService* cookie_manager_service,
                            SigninClient* signin_client,
                            PrefService* pref_service);
  ~CrossDevicePromo() override;

  // KeyedService:
  // Ends observation of other services and records the length of any current
  // browsing session (see signin_metrics::LogBrowsingSessionDuration()). This
  // is called only during Chrome shutdown.
  void Shutdown() override;

  // GaiaCookieManagerService::Observer:
  // This supports monitoring whether the content area is signed into exactly
  // one GAIA account for a long period of time. This tracks the earliest time
  // |accounts| contained (and still contains) exactly one account, so that
  // other methods can use kParamDaysToVerifySingleUserProfile to verify if this
  // Profile is considered single-user.
  void OnGaiaAccountsInCookieUpdated(
      const std::vector<gaia::ListedAccount>& accounts,
      const GoogleServiceAuthError& error) override;

  // DeviceActivityFetcher::Observer:
  // OnFetchDeviceActivity* are called from |device_activity_fetcher_| which
  // was created in GetDevicesActivityForGAIAAccountInCookieJar(). Deletes
  // |device_activity_fetcher_| at the end of the method.
  // See DetermineEligibilityFromDeviceActivity() for details.
  void OnFetchDeviceActivitySuccess(
      const std::vector<DeviceActivityFetcher::DeviceActivity>& devices)
      override;
  void OnFetchDeviceActivityFailure() override;

  // Callable by third parties to register or unregister for callbacks when the
  // promo's eligibility-to-be-shown state changes.
  void AddObserver(CrossDevicePromo::Observer* observer);
  void RemoveObserver(CrossDevicePromo::Observer* observer);

  // Returns whether the profile has been marked as eligible to be shown the
  // promo.
  bool ShouldShowPromo() const;

  // Called when the user requests to opt out of the promo. This will set a pref
  // that forever marks the profile ineligible for the promo.
  void OptOut();

  // Called whenever a browser becomes active. Notes the start of a new browsing
  // session if the last call to this method (noted in |previous_last_active|)
  // was more than |inactivity_between_browsing_sessions_| ago. For new browsing
  // sessions, will either determine if the promo is eligible to be shown, or
  // will use |device_activity_timer_| to schedule getting more information with
  // GetDevicesActivityForGAIAAccountInCookieJar().
  void MaybeBrowsingSessionStarted(const base::Time& previous_last_active);

  // Called only in tests; calls Init() if not already initialized. See comments
  // on |initialized_| for details.
  bool CheckPromoEligibilityForTesting();

 private:
  // Initializes configuration parameters from the "CrossDevicePromo" field
  // trial and registers for changes to the relevant GAIA cookie. In tests, this
  // may be called more than once; see |initialized_| for details.
  void Init();

  // Called when the determination of whether to show the promo has been made.
  // This both stores that decision and notifies all registered observers of any
  // change.
  void MarkPromoShouldBeShown();
  void MarkPromoShouldNotBeShown();

  // Performs all checks to determine if this profile could be shown the promo
  // except for initiating a fetch for additional data. This will return false
  // if the data available locally indicates the profile should not be shown
  // the promo; returns true if the profile could be shown the promo (even if
  // additional checks are to be performed).
  bool CheckPromoEligibility();

  // Called whenever new device activity is available. Checks that there is at
  // least one device that had activity within the past
  // kParamMinutesMaxContextSwitchDuration to determine if the promo should be
  // shown. Once determined, the MarkPromoShould[Not]BeShown() method is called.
  // Note that if the device is in a context switch, a followup call to
  // GetDevicesActivityForGAIAAccountInCookieJar() will be scheduled for when
  // the context switch would expire.
  void DetermineEligibilityFromDeviceActivity(
      const std::vector<DeviceActivityFetcher::DeviceActivity>& devices);

  // Helpers to get and set the time value stored in a pref.
  base::Time GetTimePref(const std::string& pref) const;
  void SetTimePref(const std::string& pref, const base::Time& value);

  // Performs checks if the promo is eligible to be displayed to this profile.
  // This will not write any prefs or initiate any checks that are otherwise
  // called in CheckPromoEligibility(). Records no metrics. Used for DCHECKs.
  bool VerifyPromoEligibleReadOnly() const;

  // Adds or removes |this| as an observer of |cookie_manager_service_|.
  // We observe the |cookie_manager_service_| for its lifetime.
  void RegisterForCookieChanges();
  void UnregisterForCookieChanges();

  // Creates a new DeviceActivityFetcher to get the list of devices, and the
  // details of the devices (see DeviceActivityFetcher.h) where the GAIA account
  // in this profile's cookie jar is signed in to Chrome Sync.
  // If a |device_activity_fetcher_| is already executing a fetch, this method
  // will not start a second fetch, as the results would be the same.
  void GetDevicesActivityForGAIAAccountInCookieJar();

  // Set by Init() to indicate if the promo service has been successfully
  // initialized. Initialization will not occur if the user has previously opted
  // out of the promo. Also, successful initialization requires all necessary
  // parameters that control the promo to be read from the field trial.
  // In testing an initial call to Init() may fail and a future call may succeed
  // (see Init()); in non-test scenarios, however, failed initialization is
  // unrecoverable and future calls to other class methods should be no-ops.
  bool initialized_;

  // These four pointers are weak pointers; they are not owned by this object
  // and will outlive this object.
  SigninManager* signin_manager_;
  GaiaCookieManagerService* cookie_manager_service_;
  PrefService* prefs_;
  SigninClient* signin_client_;

  std::unique_ptr<DeviceActivityFetcher> device_activity_fetcher_;
  base::ObserverList<CrossDevicePromo::Observer> observer_list_;

  // Initialized from the |kParamMinutesMaxContextSwitchDuration| field trial
  // parameter. See |kParamMinutesMaxContextSwitchDuration| for details.
  base::TimeDelta context_switch_duration_;

  // If the device activity has never been fetched, the delay until the check
  // will be a random duration between zero and
  // |kParamHoursBetweenDeviceActivityChecks|. For all other
  // fetches, the delay will be |kParamHoursBetweenDeviceActivityChecks|. See
  // |kParamHoursBetweenDeviceActivityChecks| for details.
  base::TimeDelta delay_until_next_device_activity_fetch_;

  // Initialized from the |kParamDaysToVerifySingleUserProfile| field trial
  // parameter. See |kParamDaysToVerifySingleUserProfile| for details.
  base::TimeDelta single_account_duration_threshold_;

  // Initialized from the |kParamMinutesBetweenBrowsingSessions| field trial
  // parameter. See |kParamMinutesBetweenBrowsingSessions| for details.
  base::TimeDelta inactivity_between_browsing_sessions_;

  // Randomly initialized from the |kParamRPCThrottle| field trial parameter.
  // See |kParamRPCThrottle| for details. If true, |device_activity_fetcher_|
  // should never be initialized.
  bool is_throttled_;

  // Metric to help us track how long a browsing session is. This is set in
  // MaybeBrowsingSessionStarted(), see that method for details.
  // Useful for configuring the field trial to manage our server quota.
  base::Time start_last_browsing_session_;

  // Used to delay the check of device activity. See
  // OnFetchDeviceActivitySuccess() or MaybeBrowsingSessionStarted(), as well as
  // |delay_until_next_device_activity_fetch_|, for details.
  base::OneShotTimer device_activity_timer_;

  DISALLOW_COPY_AND_ASSIGN(CrossDevicePromo);
};

#endif  // CHROME_BROWSER_SIGNIN_CROSS_DEVICE_PROMO_H_
