// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/automation/automation_event_observer.h"
#include "chrome/browser/automation/automation_event_queue.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

AutomationEventObserver::AutomationEventObserver(
    AutomationEventQueue* event_queue)
    : event_queue_(event_queue), observer_id_(-1) {
  DCHECK(event_queue_ != NULL);
}

AutomationEventObserver::~AutomationEventObserver() {}

void AutomationEventObserver::NotifyEvent(DictionaryValue* value) {
  if (event_queue_) {
    event_queue_->NotifyEvent(
        new AutomationEventQueue::AutomationEvent(
            GetId(), value));
  }
}

void AutomationEventObserver::Init(int observer_id) {
  if (observer_id_ < 0) {
    observer_id_ = observer_id;
  }
}

int AutomationEventObserver::GetId() const {
  return observer_id_;
}

DomRaisedEventObserver::DomRaisedEventObserver(
    AutomationEventQueue* event_queue, const std::string& event_name)
    : AutomationEventObserver(event_queue), event_name_(event_name) {}

DomRaisedEventObserver::~DomRaisedEventObserver() {}

void DomRaisedEventObserver::OnDomOperationCompleted(const std::string& json) {
  DictionaryValue* dict = new DictionaryValue;
  dict->SetString("type", "raised");
  dict->SetString("name", json);
  dict->SetInteger("observer_id", GetId());
  NotifyEvent(dict);
}

void DomRaisedEventObserver::OnModalDialogShown() {
  DictionaryValue* dict = new DictionaryValue;
  dict->SetString("error", "Blocked by modal dialogue");
  NotifyEvent(dict);
}

void DomRaisedEventObserver::OnJavascriptBlocked() {
  DictionaryValue* dict = new DictionaryValue;
  dict->SetString("error", "Javascript execution was blocked");
  NotifyEvent(dict);
}
