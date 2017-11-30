// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_IDLE_EVENT_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_IDLE_EVENT_NOTIFIER_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/public/interfaces/compositing/video_detector_observer.mojom.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"

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
class IdleEventNotifier : public PowerManagerClient::Observer,
                          public ui::UserActivityObserver,
                          public viz::mojom::VideoDetectorObserver {
 public:
  struct ActivityData {
    ActivityData();
    ActivityData(base::Time last_activity_time,
                 base::Time earliest_activity_time);

    ActivityData(const ActivityData& input_data);

    // Last activity time. This is the activity that triggers idle event.
    base::Time last_activity_time;

    // Earliest activity time of a *sequence* of activities: whose idle period
    // gap is smaller than |idle_delay_|. This value will be the same as
    // |last_activity_time| if the earlier activity happened more than
    // |idle_delay_| ago.
    base::Time earliest_activity_time;

    // Last user activity time of the sequence of activities ending in the last
    // activity. It could be different from |last_activity_time| if the last
    // activity is not a user activity (e.g. video). It is unset (i.e. 0) if
    // there is no user activity before the idle event is fired.
    base::Time last_user_activity_time;

    // Last mouse/key event time of the sequence of activities ending in the
    // last activity. It is unset (i.e. 0) if there is no mouse/key activity
    // before the idle event.
    base::Time last_mouse_time;
    base::Time last_key_time;
  };

  class Observer {
   public:
    // Called when an idle event is observed.
    virtual void OnIdleEventObserved(const ActivityData& activity_data) = 0;

   protected:
    virtual ~Observer() {}
  };

  IdleEventNotifier(PowerManagerClient* power_client,
                    ui::UserActivityDetector* detector,
                    viz::mojom::VideoDetectorObserverRequest request);
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

  // ui::UserActivityObserver overrides:
  void OnUserActivity(const ui::Event* event) override;

  // viz::mojom::VideoDetectorObserver overrides:
  void OnVideoActivityStarted() override;
  void OnVideoActivityEnded() override;

  const base::TimeDelta& idle_delay() const { return idle_delay_; }
  void set_idle_delay(const base::TimeDelta& delay) { idle_delay_ = delay; }

 private:
  FRIEND_TEST_ALL_PREFIXES(IdleEventNotifierTest, CheckInitialValues);
  friend class IdleEventNotifierTest;

  enum class ActivityType {
    USER_OTHER,  // All other user-related activities.
    KEY,
    MOUSE,
    VIDEO,
  };

  // Resets |idle_delay_timer_|, it's called when a new activity is observed.
  void ResetIdleDelayTimer();

  // Called when |idle_delay_timer_| expires.
  void OnIdleDelayTimeout();

  // Updates all activity-related timestamps.
  void UpdateActivityData(ActivityType type);

  base::TimeDelta idle_delay_ = base::TimeDelta::FromSeconds(10);

  // It is base::DefaultClock, but will be set to a mock clock for tests.
  std::unique_ptr<base::Clock> clock_;
  base::OneShotTimer idle_delay_timer_;

  ScopedObserver<chromeos::PowerManagerClient,
                 chromeos::PowerManagerClient::Observer>
      power_manager_client_observer_;
  ScopedObserver<ui::UserActivityDetector, ui::UserActivityObserver>
      user_activity_observer_;

  // Last-received external power state. Changes are treated as user activity.
  base::Optional<power_manager::PowerSupplyProperties_ExternalPower>
      external_power_;

  base::ObserverList<Observer> observers_;

  ActivityData data_;

  // Whether video is playing.
  bool video_playing_ = false;
  mojo::Binding<viz::mojom::VideoDetectorObserver> binding_;

  DISALLOW_COPY_AND_ASSIGN(IdleEventNotifier);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_IDLE_EVENT_NOTIFIER_H_
