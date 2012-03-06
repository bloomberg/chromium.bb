// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_EVENT_QUEUE_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_EVENT_QUEUE_H_

#include <list>
#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"

class AutomationEventObserver;
class AutomationJSONReply;

// AutomationEventQueue maintains a queue of unhandled automation events.
class AutomationEventQueue {
 public:
  AutomationEventQueue();
  virtual ~AutomationEventQueue();

  // AutomationEvent stores return data dictionay for a single event.
  class AutomationEvent {
   public:
    AutomationEvent(int observer_id, DictionaryValue* event_value);
    virtual ~AutomationEvent() {}

    int GetId() const { return observer_id_; }
    DictionaryValue* GetValue() { return event_value_.get(); }
    DictionaryValue* ReleaseValue() { return event_value_.release(); }

   private:
    int observer_id_;
    scoped_ptr<DictionaryValue> event_value_;
  };

  void GetNextEvent(AutomationJSONReply* reply,
                    int observer_id,
                    bool blocking);
  void NotifyEvent(AutomationEvent* event);
  void Clear();
  bool IsEmpty() const;
  AutomationEvent* PopEvent();
  AutomationEvent* PopEvent(int observer_id);

  int AddObserver(AutomationEventObserver* observer);
  bool RemoveObserver(int observer_id);

 private:
  class CompareObserverId {
   public:
    explicit CompareObserverId(int id);
    bool operator()(AutomationEvent* event) const;

   private:
    int id_;
  };

  void ClearEvents();
  void ClearObservers();
  bool CheckReturnEvent();

  std::list<AutomationEvent*> event_queue_;
  std::map<int, AutomationEventObserver*> observers_;
  int observer_id_count_;

  // These store the automation reply data when GetNextEvent is called with no
  // matching event in the queue and blocking is requested.
  scoped_ptr<AutomationJSONReply> wait_automation_reply_;
  int wait_observer_id_;

  DISALLOW_COPY_AND_ASSIGN(AutomationEventQueue);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_EVENT_QUEUE_H_
