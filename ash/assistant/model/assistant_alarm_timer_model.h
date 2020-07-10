// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_H_

#include <map>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class AssistantAlarmTimerModelObserver;

enum class AlarmTimerType {
  kAlarm,
  kTimer,
};

struct COMPONENT_EXPORT(ASSISTANT_MODEL) AlarmTimer {
  std::string id;
  AlarmTimerType type;
  base::TimeTicks end_time;

  // Returns true if this alarm/timer has expired.
  bool expired() const { return base::TimeTicks::Now() >= end_time; }
};

// The model belonging to AssistantAlarmTimerController which tracks alarm/timer
// state and notifies a pool of observers.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantAlarmTimerModel {
 public:
  AssistantAlarmTimerModel();
  ~AssistantAlarmTimerModel();

  // Adds/removes the specified alarm/timer model |observer|.
  void AddObserver(AssistantAlarmTimerModelObserver* observer);
  void RemoveObserver(AssistantAlarmTimerModelObserver* observer);

  // Adds the specified alarm/timer to the model.
  void AddAlarmTimer(const AlarmTimer& alarm_timer);

  // Remove all alarms/timers from the model.
  void RemoveAllAlarmsTimers();

  // Returns the alarm/timer uniquely identified by |id|.
  const AlarmTimer* GetAlarmTimerById(const std::string& id) const;

  // Invoke to tick any alarms/timers and to notify observers of time remaining.
  void Tick();

 private:
  void NotifyAlarmTimerAdded(const AlarmTimer& alarm_timer,
                             const base::TimeDelta& time_remaining);
  void NotifyAlarmsTimersTicked(
      const std::map<std::string, base::TimeDelta>& times_remaining);
  void NotifyAllAlarmsTimersRemoved();

  std::map<std::string, AlarmTimer> alarms_timers_;

  base::ObserverList<AssistantAlarmTimerModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AssistantAlarmTimerModel);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_H_
