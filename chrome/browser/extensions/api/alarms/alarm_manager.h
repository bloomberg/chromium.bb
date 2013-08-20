// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARM_MANAGER_H__
#define CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARM_MANAGER_H__

#include <map>
#include <queue>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/api/alarms.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace base {
class Clock;
}  // namespace base

namespace extensions {

class ExtensionAlarmsSchedulingTest;

struct Alarm {
  Alarm();
  Alarm(const std::string& name,
        const api::alarms::AlarmCreateInfo& create_info,
        base::TimeDelta min_granularity,
        base::Time now);
  ~Alarm();

  linked_ptr<api::alarms::Alarm> js_alarm;
  // The granularity isn't exposed to the extension's javascript, but we poll at
  // least as often as the shortest alarm's granularity.  It's initialized as
  // the relative delay requested in creation, even if creation uses an absolute
  // time.  This will always be at least as large as the min_granularity
  // constructor argument.
  base::TimeDelta granularity;
  // The minimum granularity is the minimum allowed polling rate. This stops
  // alarms from polling too often.
  base::TimeDelta minimum_granularity;
};

// Manages the currently pending alarms for every extension in a profile.
// There is one manager per virtual Profile.
class AlarmManager
    : public ProfileKeyedAPI,
      public content::NotificationObserver,
      public base::SupportsWeakPtr<AlarmManager> {
 public:
  typedef std::vector<Alarm> AlarmList;

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

  typedef base::Callback<void()> AddAlarmCallback;
  // Adds |alarm| for the given extension, and starts the timer. Invokes
  // |callback| when done.
  void AddAlarm(const std::string& extension_id,
                const Alarm& alarm,
                const AddAlarmCallback& callback);

  typedef base::Callback<void(Alarm*)> GetAlarmCallback;
  // Passes the alarm with the given name, or NULL if none exists, to
  // |callback|.
  void GetAlarm(const std::string& extension_id,
                const std::string& name,
                const GetAlarmCallback& callback);

  typedef base::Callback<void(const AlarmList*)> GetAllAlarmsCallback;
  // Passes the list of pending alarms for the given extension, or
  // NULL if none exist, to |callback|.
  void GetAllAlarms(
      const std::string& extension_id, const GetAllAlarmsCallback& callback);

  typedef base::Callback<void(bool)> RemoveAlarmCallback;
  // Cancels and removes the alarm with the given name. Invokes |callback| when
  // done.
  void RemoveAlarm(const std::string& extension_id,
                   const std::string& name,
                   const RemoveAlarmCallback& callback);

  typedef base::Callback<void()> RemoveAllAlarmsCallback;
  // Cancels and removes all alarms for the given extension. Invokes |callback|
  // when done.
  void RemoveAllAlarms(
      const std::string& extension_id, const RemoveAllAlarmsCallback& callback);

  // Replaces AlarmManager's owned clock with |clock| and takes ownership of it.
  void SetClockForTesting(base::Clock* clock);

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<AlarmManager>* GetFactoryInstance();

  // Convenience method to get the AlarmManager for a profile.
  static AlarmManager* Get(Profile* profile);

 private:
  friend void RunScheduleNextPoll(AlarmManager*);
  friend class ExtensionAlarmsSchedulingTest;
  FRIEND_TEST_ALL_PREFIXES(ExtensionAlarmsSchedulingTest, PollScheduling);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAlarmsSchedulingTest,
                           ReleasedExtensionPollsInfrequently);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAlarmsSchedulingTest, TimerRunning);
  FRIEND_TEST_ALL_PREFIXES(ExtensionAlarmsSchedulingTest, MinimumGranularity);
  friend class ProfileKeyedAPIFactory<AlarmManager>;

  typedef std::string ExtensionId;
  typedef std::map<ExtensionId, AlarmList> AlarmMap;

  typedef base::Callback<void(const std::string&)> ReadyAction;
  typedef std::queue<ReadyAction> ReadyQueue;
  typedef std::map<ExtensionId, ReadyQueue> ReadyMap;

  // Iterator used to identify a particular alarm within the Map/List pair.
  // "Not found" is represented by <alarms_.end(), invalid_iterator>.
  typedef std::pair<AlarmMap::iterator, AlarmList::iterator> AlarmIterator;

  // Part of AddAlarm that is executed after alarms are loaded.
  void AddAlarmWhenReady(const Alarm& alarm,
                         const AddAlarmCallback& callback,
                         const std::string& extension_id);

  // Part of GetAlarm that is executed after alarms are loaded.
  void GetAlarmWhenReady(const std::string& name,
                         const GetAlarmCallback& callback,
                         const std::string& extension_id);

  // Part of GetAllAlarms that is executed after alarms are loaded.
  void GetAllAlarmsWhenReady(const GetAllAlarmsCallback& callback,
                             const std::string& extension_id);

  // Part of RemoveAlarm that is executed after alarms are loaded.
  void RemoveAlarmWhenReady(const std::string& name,
                            const RemoveAlarmCallback& callback,
                            const std::string& extension_id);

  // Part of RemoveAllAlarms that is executed after alarms are loaded.
  void RemoveAllAlarmsWhenReady(
      const RemoveAllAlarmsCallback& callback, const std::string& extension_id);

  // Helper to return the iterators within the AlarmMap and AlarmList for the
  // matching alarm, or an iterator to the end of the AlarmMap if none were
  // found.
  AlarmIterator GetAlarmIterator(const std::string& extension_id,
                                 const std::string& name);

  // Helper to cancel and remove the alarm at the given iterator. The iterator
  // must be valid.
  void RemoveAlarmIterator(const AlarmIterator& iter);

  // Callback for when an alarm fires.
  void OnAlarm(AlarmIterator iter);

  // Internal helper to add an alarm and start the timer with the given delay.
  void AddAlarmImpl(const std::string& extension_id,
                    const Alarm& alarm);

  // Syncs our alarm data for the given extension to/from the state storage.
  void WriteToStorage(const std::string& extension_id);
  void ReadFromStorage(const std::string& extension_id,
                       scoped_ptr<base::Value> value);

  // Schedules the next poll of alarms for when the next soonest alarm runs,
  // but not more often than the minimum granularity of all alarms.
  void ScheduleNextPoll();

  // Polls the alarms, running any that have elapsed. After running them and
  // rescheduling repeating alarms, schedule the next poll.
  void PollAlarms();

  // Executes |action| for given extension, making sure that the extension's
  // alarm data has been synced from the storage.
  void RunWhenReady(const std::string& extension_id, const ReadyAction& action);

  // NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "AlarmManager";
  }
  static const bool kServiceHasOwnInstanceInIncognito = true;

  Profile* const profile_;
  scoped_ptr<base::Clock> clock_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<Delegate> delegate_;

  // The timer for this alarm manager.
  base::OneShotTimer<AlarmManager> timer_;

  // A map of our pending alarms, per extension.
  // Invariant: None of the AlarmLists are empty.
  AlarmMap alarms_;

  // A map of actions waiting for alarm data to be synced from storage, per
  // extension.
  ReadyMap ready_actions_;

  // The previous time that alarms were run.
  base::Time last_poll_time_;

  // Next poll's time. Used only by unit tests.
  base::Time test_next_poll_time_;

  DISALLOW_COPY_AND_ASSIGN(AlarmManager);
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ALARMS_ALARM_MANAGER_H__
