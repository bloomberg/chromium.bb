// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/alarms/alarm_manager.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
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

}

AlarmManager::AlarmManager(Profile* profile)
    : profile_(profile),
      delegate_(new DefaultAlarmDelegate(profile)) {
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
  // Cancel the timer first.
  timers_[iter.second->get()]->Stop();
  timers_.erase(iter.second->get());

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
    // Restart the timer, since it may have been set with a shorter delay
    // initially.
    base::Timer* timer = timers_[alarm].get();
    base::TimeDelta alarm_time = base::TimeDelta::FromMicroseconds(
        alarm->delay_in_minutes * base::Time::kMicrosecondsPerMinute);
    timer->Start(FROM_HERE, alarm_time,
                 base::Bind(&AlarmManager::OnAlarm, base::Unretained(this),
                 extension_id, alarm->name));
  }

  WriteToPrefs(extension_id);
}

void AlarmManager::AddAlarmImpl(const std::string& extension_id,
                                const linked_ptr<Alarm>& alarm,
                                base::TimeDelta timer_delay) {
  // Override any old alarm with the same name.
  AlarmIterator old_alarm = GetAlarmIterator(extension_id, alarm->name);
  if (old_alarm.first != alarms_.end())
    RemoveAlarmIterator(old_alarm);

  alarms_[extension_id].push_back(alarm);

  base::Timer* timer = new base::Timer(true, alarm->repeating);
  timers_[alarm.get()] = make_linked_ptr(timer);
  timer->Start(FROM_HERE,
      timer_delay,
      base::Bind(&AlarmManager::OnAlarm, base::Unretained(this),
                 extension_id, alarm->name));
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
      base::Timer* timer = timers_[it->get()].get();
      base::TimeDelta delay =
          timer->desired_run_time() - base::TimeTicks::Now();
      AlarmPref pref;
      pref.alarm = *it;
      pref.scheduled_run_time = base::Time::Now() + delay;
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
