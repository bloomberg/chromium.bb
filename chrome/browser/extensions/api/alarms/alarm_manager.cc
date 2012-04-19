// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/alarms/alarm_manager.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"

namespace extensions {

namespace {

const char kOnAlarmEvent[] = "experimental.alarms.onAlarm";

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
}

AlarmManager::~AlarmManager() {
}

void AlarmManager::AddAlarm(const std::string& extension_id,
                            const linked_ptr<Alarm>& alarm) {
  // TODO(mpcomplete): Better handling of granularity.
  // http://crbug.com/122683

  // Override any old alarm with the same name.
  AlarmIterator old_alarm = GetAlarmIterator(extension_id, alarm->name);
  if (old_alarm.first != alarms_.end())
    RemoveAlarmIterator(old_alarm);

  alarms_[extension_id].push_back(alarm);
  base::Timer* timer = new base::Timer(true, alarm->repeating);
  timers_[alarm.get()] = make_linked_ptr(timer);
  timer->Start(FROM_HERE,
      base::TimeDelta::FromSeconds(alarm->delay_in_seconds),
      base::Bind(&AlarmManager::OnAlarm, base::Unretained(this),
                 extension_id, alarm->name));
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
  delegate_->OnAlarm(extension_id, *it.second->get());

  if (!(*it.second)->repeating)
    RemoveAlarmIterator(it);
}

}  // namespace extensions
