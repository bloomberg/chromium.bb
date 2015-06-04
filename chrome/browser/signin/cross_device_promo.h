// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CROSS_DEVICE_PROMO_H_
#define CHROME_BROWSER_SIGNIN_CROSS_DEVICE_PROMO_H_

#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/device_activity_fetcher.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"

class PrefService;
class SigninClient;
class SigninManager;

// This defines whether the cross device signin promo should be displayed for
// this profile, and owns whether the user has opted out of the promo. The promo
// targets those who sign into Chrome on other devices, who are not signed in
// locally but use only one account in the content area.
class CrossDevicePromo : public KeyedService,
                         public GaiaCookieManagerService::Observer,
                         public DeviceActivityFetcher::Observer {
 public:
  class Observer {
   public:
    // Called if the state changes.
    virtual void OnPromoActivationChanged(bool active) = 0;
  };

  // Constructor accepts weak pointers to required services.
  explicit CrossDevicePromo(SigninManager* signin_manager,
                            GaiaCookieManagerService* cookie_manager_service,
                            SigninClient* signin_client,
                            PrefService* pref_service);
  ~CrossDevicePromo() override;

  // KeyedService:
  void Shutdown() override;

  void AddObserver(CrossDevicePromo::Observer* observer);
  void RemoveObserver(CrossDevicePromo::Observer* observer);

  // GaiaCookieManagerService::Observer:
  void OnGaiaAccountsInCookieUpdated(
      const std::vector<gaia::ListedAccount>& accounts,
      const GoogleServiceAuthError& error) override;

  // DeviceActivityFetcher::Observer:
  void OnFetchDeviceActivitySuccess(
      const std::vector<DeviceActivityFetcher::DeviceActivity>& devices)
      override;
  void OnFetchDeviceActivityFailure() override;

  // Profile should be shown the promo.
  bool IsPromoActive();

  // User elects to opt out of this promo.
  void OptOut();

  // Called whenever the Last Active Time changes. This is used to determine
  // when a new browsing session occurs.
  void MaybeBrowsingSessionStarted(const base::Time& previous_last_active);

  // Call this only in tests, please!
  bool CheckPromoEligibilityForTesting() { return CheckPromoEligibility(); }

 private:
  // Load configuration parameters from the Variations Seed.
  void Init();

  // Set whether the promo is active or inactive.
  void MarkPromoActive();
  void MarkPromoInactive();

  // Perform checks if the promo should be displayed to this profile.
  bool CheckPromoEligibility();

  // Helper to get the time value stored in a pref.
  base::Time GetTimePref(const std::string& pref);

  // Perform checks if the promo should be displayed to this profile. This will
  // not write any prefs or initiate any checks that are otherwise called in
  // CheckPromoEligibility. Records no metrics. Used for DCHECKs.
  bool VerifyPromoEligibleReadOnly();

  // Track that there is exactly one account in the cookie jar for a period
  // of time in order to activate the promo.
  void RegisterForCookieChanges();
  void UnregisterForCookieChanges();

  // Begin authenticating and then fetching the devices with the same account.
  void GetDevicesActivityForAccountInCookie();

  // Has the service been initialized. If false, the promo is inactive.
  bool initialized_;

  // ProfileKeyedServices and the PrefService are weak pointers.
  SigninManager* signin_manager_;
  GaiaCookieManagerService* cookie_manager_service_;
  PrefService* prefs_;
  SigninClient* signin_client_;

  scoped_ptr<DeviceActivityFetcher> device_activity_fetcher_;
  base::ObserverList<CrossDevicePromo::Observer> observer_list_;

  // Maximum time since activity seen on another device that activity is
  // considered within a context switch.
  base::TimeDelta context_switch_duration_;

  // Max time until the next Device Activity check. For the first test, this
  // will be a uniformly random time between now and the max delay specified
  // from Variations; otherwise we use the max delay as read from variations.
  base::TimeDelta delay_until_next_list_devices_;

  // Minimum time a single account must be in the cookie jar to consider the
  // machine as used by only one person.
  base::TimeDelta single_account_duration_threshold_;

  // Time between noted browsing activity to determine when a new Browsing
  // Session has started.
  base::TimeDelta inactivity_between_browsing_sessions_;

  // Throttles some portion of RPCs so they never get executed, based on the
  // variations configuration.
  bool is_throttled_;

  // Metric to help us track how long a browsing session is. Useful for
  // configurigng the promo.
  base::Time start_last_browsing_session_;

  // Used to delay the check of Device Activity.
  base::OneShotTimer<CrossDevicePromo> device_activity_timer_;

  DISALLOW_COPY_AND_ASSIGN(CrossDevicePromo);
};

#endif  // CHROME_BROWSER_SIGNIN_CROSS_DEVICE_PROMO_H_
