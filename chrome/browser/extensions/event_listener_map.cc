// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/event_listener_map.h"

#include "base/values.h"

#include "chrome/browser/extensions/event_router.h"

namespace extensions {

typedef EventFilter::MatcherID MatcherID;

EventListener::EventListener(const std::string& event_name,
                             const std::string& extension_id,
                             content::RenderProcessHost* process,
                             scoped_ptr<DictionaryValue> filter)
    : event_name(event_name),
      extension_id(extension_id),
      process(process),
      filter(filter.Pass()),
      matcher_id(-1) {}

EventListener::~EventListener() {}

bool EventListener::Equals(const EventListener* other) const {
  // We don't check matcher_id equality because we want a listener with a
  // filter that hasn't been added to EventFilter to match one that is
  // equivalent but has.
  return event_name == other->event_name &&
      extension_id == other->extension_id &&
      process == other->process &&
      ((!!filter.get()) == (!!other->filter.get())) &&
      (!filter.get() || filter->Equals(other->filter.get()));
}

scoped_ptr<EventListener> EventListener::Copy() const {
  scoped_ptr<DictionaryValue> filter_copy;
  if (filter.get())
    filter_copy.reset(filter->DeepCopy());
  return scoped_ptr<EventListener>(new EventListener(event_name, extension_id,
                                                     process,
                                                     filter_copy.Pass()));
}

EventListenerMap::EventListenerMap(Delegate* delegate)
    : delegate_(delegate) {
}

EventListenerMap::~EventListenerMap() {}

bool EventListenerMap::AddListener(scoped_ptr<EventListener> listener) {
  if (HasListener(listener.get()))
    return false;
  if (listener->filter.get()) {
    scoped_ptr<EventMatcher> matcher(ParseEventMatcher(listener->filter.get()));
    MatcherID id = event_filter_.AddEventMatcher(listener->event_name,
                                                 matcher.Pass());
    listener->matcher_id = id;
    listeners_by_matcher_id_[id] = listener.get();
    filtered_events_.insert(listener->event_name);
  }
  linked_ptr<EventListener> listener_ptr(listener.release());
  listeners_[listener_ptr->event_name].push_back(listener_ptr);

  delegate_->OnListenerAdded(listener_ptr.get());

  return true;
}

scoped_ptr<EventMatcher> EventListenerMap::ParseEventMatcher(
    DictionaryValue* filter_dict) {
  return scoped_ptr<EventMatcher>(new EventMatcher(
      scoped_ptr<DictionaryValue>(filter_dict->DeepCopy())));
}

bool EventListenerMap::RemoveListener(const EventListener* listener) {
  ListenerList& listeners = listeners_[listener->event_name];
  for (ListenerList::iterator it = listeners.begin(); it != listeners.end();
       it++) {
    if ((*it)->Equals(listener)) {
      CleanupListener(it->get());
      // Popping from the back should be cheaper than erase(it).
      std::swap(*it, listeners.back());
      listeners.pop_back();
      delegate_->OnListenerRemoved(listener);
      return true;
    }
  }
  return false;
}

bool EventListenerMap::HasListenerForEvent(const std::string& event_name) {
  ListenerMap::iterator it = listeners_.find(event_name);
  return it != listeners_.end() && !it->second.empty();
}

bool EventListenerMap::HasListenerForExtension(
    const std::string& extension_id,
    const std::string& event_name) {
  ListenerMap::iterator it = listeners_.find(event_name);
  if (it == listeners_.end())
    return false;

  for (ListenerList::iterator it2 = it->second.begin();
       it2 != it->second.end(); it2++) {
    if ((*it2)->extension_id == extension_id)
      return true;
  }
  return false;
}

bool EventListenerMap::HasListener(const EventListener* listener) {
  ListenerMap::iterator it = listeners_.find(listener->event_name);
  if (it == listeners_.end())
    return false;
  for (ListenerList::iterator it2 = it->second.begin();
       it2 != it->second.end(); it2++) {
    if ((*it2)->Equals(listener)) {
      return true;
    }
  }
  return false;
}

bool EventListenerMap::HasProcessListener(content::RenderProcessHost* process,
                                          const std::string& extension_id) {
  for (ListenerMap::iterator it = listeners_.begin(); it != listeners_.end();
       it++) {
    for (ListenerList::iterator it2 = it->second.begin();
         it2 != it->second.end(); it2++) {
      if ((*it2)->process == process && (*it2)->extension_id == extension_id)
        return true;
    }
  }
  return false;
}

void EventListenerMap::RemoveLazyListenersForExtension(
    const std::string& extension_id) {
  for (ListenerMap::iterator it = listeners_.begin(); it != listeners_.end();
       it++) {
    for (ListenerList::iterator it2 = it->second.begin();
         it2 != it->second.end();) {
      if (!(*it2)->process && (*it2)->extension_id == extension_id) {
        CleanupListener(it2->get());
        it2 = it->second.erase(it2);
      } else {
        it2++;
      }
    }
  }
}

void EventListenerMap::LoadUnfilteredLazyListeners(
    const std::string& extension_id,
    const std::set<std::string>& event_names) {
  for (std::set<std::string>::const_iterator it = event_names.begin();
       it != event_names.end(); ++it) {
    AddListener(scoped_ptr<EventListener>(new EventListener(
        *it, extension_id, NULL, scoped_ptr<DictionaryValue>())));
  }
}

void EventListenerMap::LoadFilteredLazyListeners(
    const std::string& extension_id,
    const DictionaryValue& filtered) {
  for (DictionaryValue::key_iterator it = filtered.begin_keys();
       it != filtered.end_keys(); ++it) {
    // We skip entries if they are malformed.
    const ListValue* filter_list = NULL;
    if (!filtered.GetListWithoutPathExpansion(*it, &filter_list))
      continue;
    for (size_t i = 0; i < filter_list->GetSize(); i++) {
      const DictionaryValue* filter = NULL;
      if (!filter_list->GetDictionary(i, &filter))
        continue;
      AddListener(scoped_ptr<EventListener>(new EventListener(
          *it, extension_id, NULL,
          scoped_ptr<DictionaryValue>(filter->DeepCopy()))));
    }
  }
}

std::set<const EventListener*> EventListenerMap::GetEventListeners(
    const Event& event) {
  std::set<const EventListener*> interested_listeners;
  if (IsFilteredEvent(event)) {
    // Look up the interested listeners via the EventFilter.
    std::set<MatcherID> ids =
        event_filter_.MatchEvent(event.event_name, event.filter_info);
    for (std::set<MatcherID>::iterator id = ids.begin(); id != ids.end();
         id++) {
      EventListener* listener = listeners_by_matcher_id_[*id];
      CHECK(listener);
      interested_listeners.insert(listener);
    }
  } else {
    ListenerList& listeners = listeners_[event.event_name];
    for (ListenerList::const_iterator it = listeners.begin();
         it != listeners.end(); it++) {
      interested_listeners.insert(it->get());
    }
  }

  return interested_listeners;
}

void EventListenerMap::RemoveListenersForProcess(
    const content::RenderProcessHost* process) {
  CHECK(process);
  for (ListenerMap::iterator it = listeners_.begin(); it != listeners_.end();
       it++) {
    for (ListenerList::iterator it2 = it->second.begin();
         it2 != it->second.end();) {
      if ((*it2)->process == process) {
        linked_ptr<EventListener> listener(*it2);
        CleanupListener(it2->get());
        it2 = it->second.erase(it2);
        delegate_->OnListenerRemoved(listener.get());
      } else {
        it2++;
      }
    }
  }
}

void EventListenerMap::CleanupListener(EventListener* listener) {
  // If the listener doesn't have a filter then we have nothing to clean up.
  if (listener->matcher_id == -1)
    return;
  event_filter_.RemoveEventMatcher(listener->matcher_id);
  CHECK_EQ(1u, listeners_by_matcher_id_.erase(listener->matcher_id));
}

bool EventListenerMap::IsFilteredEvent(const Event& event) const {
  return filtered_events_.count(event.event_name) > 0u;
}

}  // namespace extensions
