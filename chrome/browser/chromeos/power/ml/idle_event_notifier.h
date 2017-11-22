// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_IDLE_EVENT_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_IDLE_EVENT_NOTIFIER_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager_client.h"

namespace base {
class Clock;
}

namespace chromeos {
namespace power {
namespace ml {

// IdleEventNotifier listens to signals and notifies its observers when an idle
// event is generated. An idle event is generated when the idle period reaches
// |idle_delay_|. No further idle events will be generated until user becomes
// active again, followed by an idle period of |idle_delay_|.
class IdleEventNotifier : public PowerManagerClient::Observer {
 public:
  struct ActivityData {
    // Last activity time.
    base::Time last_activity_time;
  };

  class Observer {
   public:
    // Called when an idle event is observed.
    virtual void OnIdleEventObserved(const ActivityData& activity_data) = 0;

   protected:
    virtual ~Observer() {}
  };

  explicit IdleEventNotifier(const base::TimeDelta& idle_delay);
  ~IdleEventNotifier() override;

  // Set test clock so that we can check activity time.
  void SetClockForTesting(std::unique_ptr<base::Clock> test_clock);

  // Adds or removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // chromeos::PowerManagerClient::Observer overrides:
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        const base::TimeTicks& timestamp) override;
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  const base::TimeDelta& idle_delay() const { return idle_delay_; }
  void set_idle_delay(const base::TimeDelta& delay) { idle_delay_ = delay; }

 private:
  FRIEND_TEST_ALL_PREFIXES(IdleEventNotifierTest, CheckInitialValues);
  friend class IdleEventNotifierTest;

  // Resets |idle_delay_timer_|, it's called when a new activity is observed.
  void ResetIdleDelayTimer();
  // Called when |idle_delay_timer_| expires.
  void OnIdleDelayTimeout();

  base::TimeDelta idle_delay_;

  // It is base::DefaultClock, but will be set to a mock clock for tests.
  std::unique_ptr<base::Clock> clock_;
  base::OneShotTimer idle_delay_timer_;

  // Last-received external power state. Changes are treated as user activity.
  base::Optional<power_manager::PowerSupplyProperties_ExternalPower>
      external_power_;

  base::ObserverList<Observer> observers_;

  // Time of last activity, which may be a user or non-user activity (e.g.
  // video).
  base::Time last_activity_time_;

  DISALLOW_COPY_AND_ASSIGN(IdleEventNotifier);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_IDLE_EVENT_NOTIFIER_H_
