// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARM_MANAGER_H__
#define CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARM_MANAGER_H__
#pragma once

#include <string>
#include <map>
#include <vector>

#include "base/timer.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/api/alarms.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace extensions {

class ExtensionAlarmsSchedulingTest;

// Manages the currently pending alarms for every extension in a profile.
// There is one manager per virtual Profile.
class AlarmManager : public content::NotificationObserver {
 public:
  typedef extensions::api::alarms::Alarm Alarm;
  typedef std::vector<linked_ptr<Alarm> > AlarmList;

  class Delegate {
   public:
    virtual ~Delegate() {}
    // Called when an alarm fires.
    virtual void OnAlarm(const std::string& extension_id,
                         const Alarm& alarm) = 0;
  };

  explicit AlarmManager(Profile* profile);
  virtual ~AlarmManager();

  // Override the default delegate. Callee assumes onwership. Used for testing.
  void set_delegate(Delegate* delegate) { delegate_.reset(delegate); }

  // Adds |alarm| for the given extension, and starts the timer.
  void AddAlarm(const std::string& extension_id,
                const linked_ptr<Alarm>& alarm);

  // Returns the alarm with the given name, or NULL if none exists.
  const Alarm* GetAlarm(const std::string& extension_id,
                        const std::string& name);

  // Returns the list of pending alarms for the given extension, or NULL
  // if none exist.
  const AlarmList* GetAllAlarms(const std::string& extension_id);

  // Cancels and removes the alarm with the given name.
  bool RemoveAlarm(const std::string& extension_id,
                   const std::string& name);

  // Cancels and removes all alarms for the given extension.
  void RemoveAllAlarms(const std::string& extension_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionAlarmsTest, CreateRepeating);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAlarmsTest, Clear);
  friend class ExtensionAlarmsSchedulingTest;
  FRIEND_TEST_ALL_PREFIXES(ExtensionAlarmsSchedulingTest, PollScheduling);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAlarmsSchedulingTest, TimerRunning);

  typedef std::string ExtensionId;
  typedef std::map<ExtensionId, AlarmList> AlarmMap;

  // Iterator used to identify a particular alarm within the Map/List pair.
  // "Not found" is represented by <alarms_.end(), invalid_iterator>.
  typedef std::pair<AlarmMap::iterator, AlarmList::iterator> AlarmIterator;

  struct AlarmRuntimeInfo {
    std::string extension_id;
    base::Time time;
  };
  typedef std::map<const Alarm*, AlarmRuntimeInfo> AlarmRuntimeInfoMap;

  // Helper to return the iterators within the AlarmMap and AlarmList for the
  // matching alarm, or an iterator to the end of the AlarmMap if none were
  // found.
  AlarmIterator GetAlarmIterator(const std::string& extension_id,
                                 const std::string& name);

  // Helper to cancel and remove the alarm at the given iterator. The iterator
  // must be valid.
  void RemoveAlarmIterator(const AlarmIterator& iter);

  // Callback for when an alarm fires.
  void OnAlarm(const std::string& extension_id, const std::string& name);

  // Internal helper to add an alarm and start the timer with the given delay.
  void AddAlarmImpl(const std::string& extension_id,
                    const linked_ptr<Alarm>& alarm,
                    base::TimeDelta time_delay);

  // Syncs our alarm data for the given extension to/from the prefs file.
  void WriteToPrefs(const std::string& extension_id);
  void ReadFromPrefs(const std::string& extension_id);

  // Schedules the next poll of alarms for when the next soonest alarm runs,
  // but do not more often than min_period.
  void ScheduleNextPoll(base::TimeDelta min_period);

  // Polls the alarms, running any that have elapsed. After running them and
  // rescheduling repeating alarms, schedule the next poll.
  void PollAlarms();

  // NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Profile* profile_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<Delegate> delegate_;

  // The timer for this alarm manager.
  base::OneShotTimer<AlarmManager> timer_;

  // A map of our pending alarms, per extension.
  AlarmMap alarms_;

  // A map of the next scheduled times associated with each alarm.
  AlarmRuntimeInfoMap scheduled_times_;

  // The previous and next time that alarms were and will be run.
  base::Time last_poll_time_;
  base::Time next_poll_time_;
};

// Contains the data we store in the extension prefs for each alarm.
struct AlarmPref {
  linked_ptr<AlarmManager::Alarm> alarm;
  base::Time scheduled_run_time;

  AlarmPref();
  ~AlarmPref();
};

} //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARM_MANAGER_H__
