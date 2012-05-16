// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/logging.h"
#include "chrome/browser/automation/automation_event_observers.h"
#include "chrome/browser/automation/automation_event_queue.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

AutomationEventQueue::CompareObserverId::CompareObserverId(int id) : id_(id) {}

bool AutomationEventQueue::CompareObserverId::operator()(
    AutomationEvent* event) const {
  return event->GetId() < 0 || event->GetId() == id_;
}

AutomationEventQueue::AutomationEventQueue()
    : observer_id_count_(0),
      wait_automation_reply_(NULL),
      wait_observer_id_(-1) {}

AutomationEventQueue::~AutomationEventQueue() {
  Clear();
}

AutomationEventQueue::AutomationEvent::AutomationEvent(
    int observer_id, DictionaryValue* event_value)
    : observer_id_(observer_id), event_value_(event_value) {}

void AutomationEventQueue::GetNextEvent(AutomationJSONReply* reply,
                                        int observer_id,
                                        bool blocking) {
  wait_automation_reply_.reset(reply);
  wait_observer_id_ = observer_id;
  if (!CheckReturnEvent() && !blocking && wait_automation_reply_.get()) {
    wait_automation_reply_->SendSuccess(NULL);
    wait_automation_reply_.reset();
  }
}

void AutomationEventQueue::Clear() {
  ClearObservers();
  ClearEvents();
}

bool AutomationEventQueue::IsEmpty() const {
  return event_queue_.empty();
}

AutomationEventQueue::AutomationEvent* AutomationEventQueue::PopEvent() {
  if (event_queue_.empty()) {
    return NULL;
  }
  AutomationEvent* event = event_queue_.back();
  event_queue_.pop_back();
  return event;
}

AutomationEventQueue::AutomationEvent* AutomationEventQueue::PopEvent(
    int observer_id) {
  AutomationEvent* event = NULL;
  std::list<AutomationEvent*>::reverse_iterator it =
      std::find_if(event_queue_.rbegin(), event_queue_.rend(),
                   CompareObserverId(observer_id));
  if (it != event_queue_.rend()) {
    event = *it;
    event_queue_.remove(event);
  }
  return event;
}

void AutomationEventQueue::NotifyEvent(
    AutomationEventQueue::AutomationEvent* event) {
  DCHECK(event);
  VLOG(2) << "AutomationEventQueue::NotifyEvent id=" << event->GetId();
  event_queue_.push_front(event);
  CheckReturnEvent();
}

int AutomationEventQueue::AddObserver(AutomationEventObserver* observer) {
  int id = observer_id_count_++;
  observer->Init(id);
  observers_[id] = observer;
  return id;
}

bool AutomationEventQueue::RemoveObserver(int observer_id) {
  if (observers_.find(observer_id) != observers_.end()) {
    VLOG(2) << "AutomationEventQueue::RemoveObserver id=" << observer_id;
    delete observers_[observer_id];
    observers_.erase(observer_id);
    return true;
  }
  return false;
}

void AutomationEventQueue::ClearObservers() {
  std::map<int, AutomationEventObserver*>::iterator it;
  for (it = observers_.begin(); it != observers_.end(); it++) {
    delete it->second;
  }
  observers_.clear();
}

void AutomationEventQueue::ClearEvents() {
  std::list<AutomationEvent*>::iterator it;
  for (it = event_queue_.begin(); it != event_queue_.end(); it++) {
    delete *it;
  }
  event_queue_.clear();
}

bool AutomationEventQueue::CheckReturnEvent() {
  if (wait_automation_reply_.get()) {
    AutomationEventQueue::AutomationEvent* event = wait_observer_id_ < 0 ?
                                                   PopEvent() :
                                                   PopEvent(wait_observer_id_);
    if (event) {
      wait_automation_reply_->SendSuccess(event->GetValue());
      wait_automation_reply_.reset();
      wait_observer_id_ = -1;
      delete event;
      return true;
    }
  }
  return false;
}
