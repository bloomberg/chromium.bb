// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/alarms/alarm_manager.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/state_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

namespace extensions {

namespace {

const char kOnAlarmEvent[] = "alarms.onAlarm";

// A list of alarms that this extension has set.
const char kRegisteredAlarms[] = "alarms";
const char kAlarmGranularity[] = "granularity";

// The minimum period between polling for alarms to run.
const base::TimeDelta kDefaultMinPollPeriod = base::TimeDelta::FromDays(1);

class DefaultAlarmDelegate : public AlarmManager::Delegate {
 public:
  explicit DefaultAlarmDelegate(Profile* profile) : profile_(profile) {}
  virtual ~DefaultAlarmDelegate() {}

  virtual void OnAlarm(const std::string& extension_id,
                       const Alarm& alarm) {
    scoped_ptr<ListValue> args(new ListValue());
    args->Append(alarm.js_alarm->ToValue().release());
    scoped_ptr<Event> event(new Event(kOnAlarmEvent, args.Pass()));
    ExtensionSystem::Get(profile_)->event_router()->DispatchEventToExtension(
        extension_id, event.Pass());
  }

 private:
  Profile* profile_;
};

// Creates a TimeDelta from a delay as specified in the API.
base::TimeDelta TimeDeltaFromDelay(double delay_in_minutes) {
  return base::TimeDelta::FromMicroseconds(
      delay_in_minutes * base::Time::kMicrosecondsPerMinute);
}

std::vector<Alarm> AlarmsFromValue(const base::ListValue* list) {
  std::vector<Alarm> alarms;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    const base::DictionaryValue* alarm_dict = NULL;
    Alarm alarm;
    if (list->GetDictionary(i, &alarm_dict) &&
        api::alarms::Alarm::Populate(*alarm_dict, alarm.js_alarm.get())) {
      const base::Value* time_value = NULL;
      if (alarm_dict->Get(kAlarmGranularity, &time_value))
        base::GetValueAsTimeDelta(*time_value, &alarm.granularity);
      alarms.push_back(alarm);
    }
  }
  return alarms;
}

scoped_ptr<base::ListValue> AlarmsToValue(const std::vector<Alarm>& alarms) {
  scoped_ptr<base::ListValue> list(new ListValue());
  for (size_t i = 0; i < alarms.size(); ++i) {
    scoped_ptr<base::DictionaryValue> alarm =
        alarms[i].js_alarm->ToValue().Pass();
    alarm->Set(kAlarmGranularity,
               base::CreateTimeDeltaValue(alarms[i].granularity));
    list->Append(alarm.release());
  }
  return list.Pass();
}


}  // namespace

// AlarmManager

AlarmManager::AlarmManager(Profile* profile, TimeProvider now)
    : profile_(profile),
      now_(now),
      delegate_(new DefaultAlarmDelegate(profile)),
      last_poll_time_(base::Time()) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_));

  StateStore* storage = ExtensionSystem::Get(profile_)->state_store();
  if (storage)
    storage->RegisterKey(kRegisteredAlarms);
}

AlarmManager::~AlarmManager() {
}

void AlarmManager::AddAlarm(const std::string& extension_id,
                            const Alarm& alarm) {
  AddAlarmImpl(extension_id, alarm);
  WriteToStorage(extension_id);
}

const Alarm* AlarmManager::GetAlarm(
    const std::string& extension_id, const std::string& name) {
  AlarmIterator it = GetAlarmIterator(extension_id, name);
  if (it.first == alarms_.end())
    return NULL;
  return &*it.second;
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
    if (it->js_alarm->name == name)
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
  WriteToStorage(extension_id);
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
  WriteToStorage(extension_id);
}

void AlarmManager::RemoveAlarmIterator(const AlarmIterator& iter) {
  AlarmList& list = iter.first->second;
  list.erase(iter.second);
  if (list.empty())
    alarms_.erase(iter.first);

  // Cancel the timer if there are no more alarms.
  // We don't need to reschedule the poll otherwise, because in
  // the worst case we would just poll one extra time.
  if (alarms_.empty())
    timer_.Stop();
}

void AlarmManager::OnAlarm(AlarmIterator it) {
  CHECK(it.first != alarms_.end());
  Alarm& alarm = *it.second;
  std::string extension_id_copy(it.first->first);
  delegate_->OnAlarm(extension_id_copy, alarm);

  // Update our scheduled time for the next alarm.
  if (double* period_in_minutes =
      alarm.js_alarm->period_in_minutes.get()) {
    alarm.js_alarm->scheduled_time =
        (last_poll_time_ +
         TimeDeltaFromDelay(*period_in_minutes)).ToJsTime();
  } else {
    RemoveAlarmIterator(it);
  }
  WriteToStorage(extension_id_copy);
}

void AlarmManager::AddAlarmImpl(const std::string& extension_id,
                                const Alarm& alarm) {
  // Override any old alarm with the same name.
  AlarmIterator old_alarm = GetAlarmIterator(extension_id,
                                             alarm.js_alarm->name);
  if (old_alarm.first != alarms_.end())
    RemoveAlarmIterator(old_alarm);

  alarms_[extension_id].push_back(alarm);

  ScheduleNextPoll();
}

void AlarmManager::WriteToStorage(const std::string& extension_id) {
  StateStore* storage = ExtensionSystem::Get(profile_)->state_store();
  if (!storage)
    return;

  scoped_ptr<Value> alarms;
  AlarmMap::iterator list = alarms_.find(extension_id);
  if (list != alarms_.end())
    alarms.reset(AlarmsToValue(list->second).release());
  else
    alarms.reset(AlarmsToValue(std::vector<Alarm>()).release());
  storage->SetExtensionValue(extension_id, kRegisteredAlarms, alarms.Pass());
}

void AlarmManager::ReadFromStorage(const std::string& extension_id,
                                   scoped_ptr<base::Value> value) {
  base::ListValue* list = NULL;
  if (!value.get() || !value->GetAsList(&list))
    return;

  std::vector<Alarm> alarm_states = AlarmsFromValue(list);
  for (size_t i = 0; i < alarm_states.size(); ++i) {
    AddAlarmImpl(extension_id, alarm_states[i]);
  }
}

void AlarmManager::ScheduleNextPoll() {
  // 0. If there are no alarms, stop the timer.
  if (alarms_.empty()) {
    timer_.Stop();
    return;
  }

  // TODO(yoz): Try not to reschedule every single time if we're adding
  // a lot of alarms.

  // Find the soonest alarm that is scheduled to run and the smallest
  // granularity of any alarm.
  // alarms_ guarantees that none of its contained lists are empty.
  base::Time soonest_alarm_time = base::Time::FromJsTime(
      alarms_.begin()->second.begin()->js_alarm->scheduled_time);
  base::TimeDelta min_granularity = kDefaultMinPollPeriod;
  for (AlarmMap::const_iterator m_it = alarms_.begin(), m_end = alarms_.end();
       m_it != m_end; ++m_it) {
    for (AlarmList::const_iterator l_it = m_it->second.begin();
         l_it != m_it->second.end(); ++l_it) {
      base::Time cur_alarm_time =
          base::Time::FromJsTime(l_it->js_alarm->scheduled_time);
      if (cur_alarm_time < soonest_alarm_time)
        soonest_alarm_time = cur_alarm_time;
      if (l_it->granularity < min_granularity)
        min_granularity = l_it->granularity;
    }
  }

  base::Time next_poll(last_poll_time_ + min_granularity);
  // If the next alarm is more than min_granularity in the future, wait for it.
  // Otherwise, only poll as often as min_granularity.
  // As a special case, if we've never checked for an alarm before
  // (e.g. during startup), let alarms fire asap.
  if (last_poll_time_.is_null() || next_poll < soonest_alarm_time)
    next_poll = soonest_alarm_time;

  // Schedule the poll.
  next_poll_time_ = next_poll;
  base::TimeDelta delay = std::max(base::TimeDelta::FromSeconds(0),
                                   next_poll - now_());
  timer_.Start(FROM_HERE,
               delay,
               this,
               &AlarmManager::PollAlarms);
}

void AlarmManager::PollAlarms() {
  last_poll_time_ = now_();

  // Run any alarms scheduled in the past. OnAlarm uses vector::erase to remove
  // elements from the AlarmList, and map::erase to remove AlarmLists from the
  // AlarmMap.
  for (AlarmMap::iterator m_it = alarms_.begin(), m_end = alarms_.end();
       m_it != m_end;) {
    AlarmMap::iterator cur_extension = m_it++;

    // Iterate (a) backwards so that removing elements doesn't affect
    // upcoming iterations, and (b) with indices so that if the last
    // iteration destroys the AlarmList, I'm not about to use the end
    // iterator that the destruction invalidates.
    for (size_t i = cur_extension->second.size(); i > 0; --i) {
      AlarmList::iterator cur_alarm = cur_extension->second.begin() + i - 1;
      if (base::Time::FromJsTime(cur_alarm->js_alarm->scheduled_time) <=
          next_poll_time_) {
        OnAlarm(make_pair(cur_extension, cur_alarm));
      }
    }
  }

  ScheduleNextPoll();
}

void AlarmManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      StateStore* storage = ExtensionSystem::Get(profile_)->state_store();
      if (storage) {
        storage->GetExtensionValue(extension->id(), kRegisteredAlarms,
            base::Bind(&AlarmManager::ReadFromStorage,
                       AsWeakPtr(), extension->id()));
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

// AlarmManager::Alarm

Alarm::Alarm()
    : js_alarm(new api::alarms::Alarm()) {
}

Alarm::Alarm(const std::string& name,
             const api::alarms::AlarmCreateInfo& create_info,
             base::TimeDelta min_granularity,
             TimeProvider now)
    : js_alarm(new api::alarms::Alarm()) {
  js_alarm->name = name;

  if (create_info.when.get()) {
    // Absolute scheduling.
    js_alarm->scheduled_time = *create_info.when;
    granularity = base::Time::FromJsTime(js_alarm->scheduled_time) - now();
  } else {
    // Relative scheduling.
    double* delay_in_minutes = create_info.delay_in_minutes.get();
    if (delay_in_minutes == NULL)
      delay_in_minutes = create_info.period_in_minutes.get();
    CHECK(delay_in_minutes != NULL)
        << "ValidateAlarmCreateInfo in alarms_api.cc should have "
        << "prevented this call.";
    base::TimeDelta delay = TimeDeltaFromDelay(*delay_in_minutes);
    js_alarm->scheduled_time = (now() + delay).ToJsTime();
    granularity = delay;
  }

  if (granularity < min_granularity)
    granularity = min_granularity;

  // Check for repetition.
  if (create_info.period_in_minutes.get()) {
    js_alarm->period_in_minutes.reset(
        new double(*create_info.period_in_minutes));
  }
}

Alarm::~Alarm() {
}

}  // namespace extensions
