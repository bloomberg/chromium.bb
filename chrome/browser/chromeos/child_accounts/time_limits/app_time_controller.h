// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_notification_delegate.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "chromeos/dbus/system_clock/system_clock_client.h"
#include "chromeos/settings/timezone_settings.h"

class Profile;
class PrefRegistrySimple;
class PrefChangeRegistrar;
class PrefService;
class Profile;

namespace base {
class OneShotTimer;
}  // namespace base

namespace gfx {
class ImageSkia;
} // namespace gfx

namespace chromeos {
namespace app_time {

extern const char kAppsWithTimeLimitMetric[];
extern const char kBlockedAppsCountMetric[];
extern const char kPolicyUpdateCountMetric[];
extern const char kEngagementMetric[];

class AppServiceWrapper;
class WebTimeLimitEnforcer;
class WebTimeActivityProvider;

// Coordinates per-app time limit for child user.
class AppTimeController : public SystemClockClient::Observer,
                          public system::TimezoneSettings::Observer,
                          public AppTimeNotificationDelegate,
                          public AppActivityRegistry::AppStateObserver {
 public:
  // Used for tests to get internal implementation details.
  class TestApi {
   public:
    explicit TestApi(AppTimeController* controller);
    ~TestApi();

    void SetLastResetTime(base::Time time);
    base::Time GetNextResetTime() const;
    base::Time GetLastResetTime() const;

    AppActivityRegistry* app_registry();

   private:
    AppTimeController* const controller_;
  };

  static bool ArePerAppTimeLimitsEnabled();
  static bool IsAppActivityReportingEnabled();

  // Registers preferences
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit AppTimeController(Profile* profile);
  AppTimeController(const AppTimeController&) = delete;
  AppTimeController& operator=(const AppTimeController&) = delete;
  ~AppTimeController() override;

  bool IsExtensionWhitelisted(const std::string& extension_id) const;

  // Returns current time limit for the app identified by |app_service_id| and
  // |app_type|.Will return nullopt if there is no limit set or app is not
  // tracked.
  base::Optional<base::TimeDelta> GetTimeLimitForApp(
      const std::string& app_service_id,
      apps::mojom::AppType app_type) const;

  // Called by ChildUserService when it is being destructed to save metrics.
  void RecordMetricsOnShutdown() const;

  // SystemClockClient::Observer:
  void SystemClockUpdated() override;

  // system::TimezoneSetting::Observer:
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  // AppTimeNotificationDelegate:
  void ShowAppTimeLimitNotification(
      const AppId& app_id,
      const base::Optional<base::TimeDelta>& time_limit,
      AppNotification notification) override;

  // AppActivityRegistry::AppStateObserver:
  void OnAppLimitReached(const AppId& app_id,
                         base::TimeDelta time_limit,
                         bool was_active) override;
  void OnAppLimitRemoved(const AppId& app_id) override;
  void OnAppInstalled(const AppId& app_id) override;

  const WebTimeLimitEnforcer* web_time_enforcer() const {
    return web_time_enforcer_.get();
  }

  WebTimeLimitEnforcer* web_time_enforcer() { return web_time_enforcer_.get(); }

  const AppActivityRegistry* app_registry() const {
    return app_registry_.get();
  }

  AppActivityRegistry* app_registry() { return app_registry_.get(); }

  const WebTimeActivityProvider* web_time_activity_provider() const {
    return web_time_activity_provider_.get();
  }

  WebTimeActivityProvider* web_time_activity_provider() {
    return web_time_activity_provider_.get();
  }

 private:
  void RegisterProfilePrefObservers(PrefService* pref_service);
  void TimeLimitsPolicyUpdated(const std::string& pref_name);
  void TimeLimitsWhitelistPolicyUpdated(const std::string& pref_name);

  base::Time GetNextResetTime() const;

  void ScheduleForTimeLimitReset();
  void OnResetTimeReached();

  void RestoreLastResetTime();
  void SetLastResetTime(base::Time timestamp);

  // Called when the system time or timezone may have changed.
  bool HasTimeCrossedResetBoundary() const;

  void OpenFamilyLinkApp();

  void ShowNotificationForApp(const std::string& app_name,
                              AppNotification notification,
                              base::Optional<base::TimeDelta> time_limit,
                              base::Optional<gfx::ImageSkia> icon);
  // Profile
  Profile* const profile_;

  // The time of the day when app time limits should be reset.
  // Defaults to 6am local time.
  base::TimeDelta limits_reset_time_ = base::TimeDelta::FromHours(6);

  // The last time when |reset_timer_| fired.
  base::Time last_limits_reset_time_;

  // Timer scheduled for the next reset of app time limits.
  // Only set when |reset_time_| is
  base::OneShotTimer reset_timer_{base::DefaultTickClock::GetInstance()};

  std::unique_ptr<AppServiceWrapper> app_service_wrapper_;
  std::unique_ptr<AppActivityRegistry> app_registry_;
  std::unique_ptr<WebTimeActivityProvider> web_time_activity_provider_;
  std::unique_ptr<WebTimeLimitEnforcer> web_time_enforcer_;

  // Used to observe when policy preferences change.
  std::unique_ptr<PrefChangeRegistrar> pref_registrar_;

  // Metrics information to be recorded for PerAppTimeLimits.
  int patl_policy_update_count_ = 0;
  int apps_with_limit_ = 0;

  base::WeakPtrFactory<AppTimeController> weak_ptr_factory_{this};
};

}  // namespace app_time
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_CONTROLLER_H_
