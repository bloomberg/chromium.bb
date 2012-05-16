// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "chrome/browser/automation/automation_event_observers.h"
#include "chrome/browser/automation/automation_event_queue.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

const char* DomEventObserver::kSubstringReplaceWithObserverId = "$(id)";

AutomationEventObserver::AutomationEventObserver(
    AutomationEventQueue* event_queue, bool recurring)
    : event_queue_(event_queue),
      recurring_(recurring),
      observer_id_(-1),
      event_count_(0) {
  DCHECK(event_queue_ != NULL);
}

AutomationEventObserver::~AutomationEventObserver() {}

void AutomationEventObserver::NotifyEvent(DictionaryValue* value) {
  if (event_queue_) {
    if (!value)
      value = new DictionaryValue;
    value->SetInteger("observer_id", GetId());
    event_queue_->NotifyEvent(
        new AutomationEventQueue::AutomationEvent(
            GetId(), value));
    event_count_++;
  } else if (value) {
    delete value;
  }
}

void AutomationEventObserver::Init(int observer_id) {
    observer_id_ = observer_id;
}

int AutomationEventObserver::GetId() const {
  return observer_id_;
}

void AutomationEventObserver::RemoveIfDone() {
  if (!recurring_ && event_count_ > 0 && event_queue_) {
    event_queue_->RemoveObserver(GetId());
  }
}

DomEventObserver::DomEventObserver(
    AutomationEventQueue* event_queue,
    const std::string& event_name,
    int automation_id,
    bool recurring)
    : AutomationEventObserver(event_queue, recurring),
      event_name_(event_name),
      event_name_base_(event_name),
      automation_id_(automation_id) {
  registrar_.Add(this, content::NOTIFICATION_DOM_OPERATION_RESPONSE,
                 content::NotificationService::AllSources());
}

DomEventObserver::~DomEventObserver() {}

void DomEventObserver::Init(int observer_id) {
    AutomationEventObserver::Init(observer_id);
    std::string id_string = base::StringPrintf("%d", observer_id);
    event_name_ = event_name_base_;
    int replace_pos =
        event_name_.find(kSubstringReplaceWithObserverId);
    if (replace_pos != (int)std::string::npos) {
      event_name_.replace(replace_pos,
                          ::strlen(kSubstringReplaceWithObserverId),
                          id_string);
    }
}

void DomEventObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_DOM_OPERATION_RESPONSE) {
    content::Details<content::DomOperationNotificationDetails> dom_op_details(
        details);
    if ((dom_op_details->automation_id == automation_id_ ||
         automation_id_ == -1) &&
        (event_name_.length() == 0 ||
         event_name_.compare(dom_op_details->json) == 0)) {
      DictionaryValue* dict = new DictionaryValue;
      dict->SetString("type", "raised_event");
      dict->SetString("name", dom_op_details->json);
      NotifyEvent(dict);
    }
  }
  // Nothing should happen after RemoveIfDone() as it may delete the object.
  RemoveIfDone();
}
