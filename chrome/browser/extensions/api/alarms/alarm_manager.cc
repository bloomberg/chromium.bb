// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/alarms/alarm_manager.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

namespace extensions {

namespace {

const char kOnAlarmEvent[] = "alarms.onAlarm";

// The minimum period between polling for alarms to run.
const base::TimeDelta kMinPollPeriod = base::TimeDelta::FromMinutes(5);

class DefaultAlarmDelegate : public AlarmManager::Delegate {
 public:
  explicit DefaultAlarmDelegate(Profile* profile) : profile_(profile) {}
  virtual ~DefaultAlarmDelegate() {}

  virtual void OnAlarm(const std::string& extension_id,
                       const AlarmManager::Alarm& alarm) {
    ListValue args;
    std::string json_args;
    args.Append(alarm.ToValue().release());
    base::JSONWriter::Write(&args, &json_args);
    ExtensionSystem::Get(profile_)->event_router()->DispatchEventToExtension(
        extension_id, kOnAlarmEvent, json_args, NULL, GURL());
  }

 private:
  Profile* profile_;
};

// Creates a TimeDelta from a delay as specified in the API.
base::TimeDelta TimeDeltaFromDelay(double delay_in_minutes) {
  return base::TimeDelta::FromMicroseconds(
      delay_in_minutes * base::Time::kMicrosecondsPerMinute);
}

}  // namespace

// AlarmManager

AlarmManager::AlarmManager(Profile* profile)
    : profile_(profile),
      delegate_(new DefaultAlarmDelegate(profile)),
      last_poll_time_(base::Time()) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_));
}

AlarmManager::~AlarmManager() {
}

void AlarmManager::AddAlarm(const std::string& extension_id,
                            const linked_ptr<Alarm>& alarm) {
  base::TimeDelta alarm_time = base::TimeDelta::FromMicroseconds(
      alarm->delay_in_minutes * base::Time::kMicrosecondsPerMinute);
  AddAlarmImpl(extension_id, alarm, alarm_time);
  WriteToPrefs(extension_id);
}

const AlarmManager::Alarm* AlarmManager::GetAlarm(
    const std::string& extension_id, const std::string& name) {
  AlarmIterator it = GetAlarmIterator(extension_id, name);
  if (it.first == alarms_.end())
    return NULL;
  return it.second->get();
}

const AlarmManager::AlarmList* AlarmManager::GetAllAlarms(
    const std::string& extension_id) {
  AlarmMap::iterator list = alarms_.find(extension_id);
  if (list == alarms_.end())
    return NULL;
  return &list->second;
}

AlarmManager::AlarmIterator AlarmManager::GetAlarmIterator(
    const std::string& extension_id, const std::string& name) {
  AlarmMap::iterator list = alarms_.find(extension_id);
  if (list == alarms_.end())
    return make_pair(alarms_.end(), AlarmList::iterator());

  for (AlarmList::iterator it = list->second.begin();
       it != list->second.end(); ++it) {
    if ((*it)->name == name)
      return make_pair(list, it);
  }

  return make_pair(alarms_.end(), AlarmList::iterator());
}

bool AlarmManager::RemoveAlarm(const std::string& extension_id,
                               const std::string& name) {
  AlarmIterator it = GetAlarmIterator(extension_id, name);
  if (it.first == alarms_.end())
    return false;

  RemoveAlarmIterator(it);
  WriteToPrefs(extension_id);
  return true;
}

void AlarmManager::RemoveAllAlarms(const std::string& extension_id) {
  AlarmMap::iterator list = alarms_.find(extension_id);
  if (list == alarms_.end())
    return;

  // Note: I'm using indices rather than iterators here because
  // RemoveAlarmIterator will delete the list when it becomes empty.
  for (size_t i = 0, size = list->second.size(); i < size; ++i)
    RemoveAlarmIterator(AlarmIterator(list, list->second.begin()));

  CHECK(alarms_.find(extension_id) == alarms_.end());
  WriteToPrefs(extension_id);
}

void AlarmManager::RemoveAlarmIterator(const AlarmIterator& iter) {
  // Cancel the timer if there are no more alarms.
  // We don't need to reschedule the poll otherwise, because in
  // the worst case we would just poll one extra time.
  scheduled_times_.erase(iter.second->get());
  if (scheduled_times_.empty())
    timer_.Stop();

  // Clean up our alarm list.
  AlarmList& list = iter.first->second;
  list.erase(iter.second);
  if (list.empty())
    alarms_.erase(iter.first);
}

void AlarmManager::OnAlarm(const std::string& extension_id,
                           const std::string& name) {
  AlarmIterator it = GetAlarmIterator(extension_id, name);
  CHECK(it.first != alarms_.end());
  const Alarm* alarm = it.second->get();
  delegate_->OnAlarm(extension_id, *alarm);

  if (!alarm->repeating) {
    RemoveAlarmIterator(it);
  } else {
    // Update our scheduled time for the next alarm.
    scheduled_times_[alarm].time =
        last_poll_time_ + TimeDeltaFromDelay(alarm->delay_in_minutes);
  }

  WriteToPrefs(extension_id);
}

void AlarmManager::AddAlarmImpl(const std::string& extension_id,
                                const linked_ptr<Alarm>& alarm,
                                base::TimeDelta time_delay) {
  // Override any old alarm with the same name.
  AlarmIterator old_alarm = GetAlarmIterator(extension_id, alarm->name);
  if (old_alarm.first != alarms_.end())
    RemoveAlarmIterator(old_alarm);

  alarms_[extension_id].push_back(alarm);
  AlarmRuntimeInfo info;
  info.extension_id = extension_id;
  info.time = base::Time::Now() + time_delay;
  scheduled_times_[alarm.get()] = info;

  // TODO(yoz): Is 0 really sane? There could be thrashing.
  ScheduleNextPoll(base::TimeDelta::FromMinutes(0));
}

void AlarmManager::WriteToPrefs(const std::string& extension_id) {
  ExtensionService* service =
      ExtensionSystem::Get(profile_)->extension_service();
  if (!service || !service->extension_prefs())
    return;

  std::vector<AlarmPref> alarm_prefs;

  AlarmMap::iterator list = alarms_.find(extension_id);
  if (list != alarms_.end()) {
    for (AlarmList::iterator it = list->second.begin();
         it != list->second.end(); ++it) {
      AlarmPref pref;
      pref.alarm = *it;
      pref.scheduled_run_time = scheduled_times_[it->get()].time;
      alarm_prefs.push_back(pref);
    }
  }

  service->extension_prefs()->SetRegisteredAlarms(extension_id, alarm_prefs);
}

void AlarmManager::ReadFromPrefs(const std::string& extension_id) {
  ExtensionService* service =
      ExtensionSystem::Get(profile_)->extension_service();
  if (!service || !service->extension_prefs())
    return;

  std::vector<AlarmPref> alarm_prefs =
      service->extension_prefs()->GetRegisteredAlarms(extension_id);
  for (size_t i = 0; i < alarm_prefs.size(); ++i) {
    base::TimeDelta delay =
        alarm_prefs[i].scheduled_run_time - base::Time::Now();
    if (delay < base::TimeDelta::FromSeconds(0))
      delay = base::TimeDelta::FromSeconds(0);

    AddAlarmImpl(extension_id, alarm_prefs[i].alarm, delay);
  }
}

void AlarmManager::ScheduleNextPoll(base::TimeDelta min_period) {
  // 0. If there are no alarms, stop the timer.
  if (scheduled_times_.empty()) {
    timer_.Stop();
    return;
  }

  // TODO(yoz): Try not to reschedule every single time if we're adding
  // a lot of alarms.

  base::Time next_poll(last_poll_time_ + min_period);

  // Find the soonest alarm that is scheduled to run.
  AlarmRuntimeInfoMap::iterator min_it = scheduled_times_.begin();
  for (AlarmRuntimeInfoMap::iterator it = min_it;
           it != scheduled_times_.end(); ++it) {
    if (it->second.time < min_it->second.time)
      min_it = it;
  }
  base::Time soonest_alarm_time(min_it->second.time);

  // If the next alarm is more than min_period in the future, wait for it.
  // Otherwise, only poll as often as min_period.
  if (last_poll_time_.is_null() || next_poll < soonest_alarm_time) {
    next_poll = soonest_alarm_time;
  }

  // Schedule the poll.
  next_poll_time_ = next_poll;
  base::TimeDelta delay = std::max(base::TimeDelta::FromSeconds(0),
                                   next_poll - base::Time::Now());
  timer_.Start(FROM_HERE,
               delay,
               this,
               &AlarmManager::PollAlarms);
}

void AlarmManager::PollAlarms() {
  last_poll_time_ = base::Time::Now();

  // Run any alarms scheduled in the past. Note that we could remove alarms
  // during iteration if they are non-repeating.
  AlarmRuntimeInfoMap::iterator iter = scheduled_times_.begin();
  while (iter != scheduled_times_.end()) {
    AlarmRuntimeInfoMap::iterator it = iter;
    ++iter;
    if (it->second.time <= next_poll_time_) {
      OnAlarm(it->second.extension_id, it->first->name);
    }
  }

  ScheduleNextPoll(kMinPollPeriod);
}

void AlarmManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      ReadFromPrefs(extension->id());
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

AlarmPref::AlarmPref() {
}

AlarmPref::~AlarmPref() {
}

}  // namespace extensions
