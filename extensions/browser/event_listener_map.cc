// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/event_listener_map.h"

#include "base/values.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/event_router.h"
#include "ipc/ipc_message.h"

using base::DictionaryValue;

namespace extensions {

typedef EventFilter::MatcherID MatcherID;

EventListener::EventListener(const std::string& event_name,
                             const std::string& extension_id,
                             content::RenderProcessHost* process,
                             scoped_ptr<DictionaryValue> filter)
    : event_name_(event_name),
      extension_id_(extension_id),
      process_(process),
      filter_(filter.Pass()),
      matcher_id_(-1) {
}

EventListener::~EventListener() {}

bool EventListener::Equals(const EventListener* other) const {
  // We don't check matcher_id equality because we want a listener with a
  // filter that hasn't been added to EventFilter to match one that is
  // equivalent but has.
  return event_name_ == other->event_name_ &&
         extension_id_ == other->extension_id_ && process_ == other->process_ &&
         ((!!filter_.get()) == (!!other->filter_.get())) &&
         (!filter_.get() || filter_->Equals(other->filter_.get()));
}

scoped_ptr<EventListener> EventListener::Copy() const {
  scoped_ptr<DictionaryValue> filter_copy;
  if (filter_)
    filter_copy.reset(filter_->DeepCopy());
  return scoped_ptr<EventListener>(new EventListener(
      event_name_, extension_id_, process_, filter_copy.Pass()));
}

bool EventListener::IsLazy() const {
  return !process_;
}

void EventListener::MakeLazy() {
  process_ = NULL;
}

content::BrowserContext* EventListener::GetBrowserContext() const {
  return process_ ? process_->GetBrowserContext() : NULL;
}

EventListenerMap::EventListenerMap(Delegate* delegate)
    : delegate_(delegate) {
}

EventListenerMap::~EventListenerMap() {}

bool EventListenerMap::AddListener(scoped_ptr<EventListener> listener) {
  if (HasListener(listener.get()))
    return false;
  if (listener->filter()) {
    scoped_ptr<EventMatcher> matcher(ParseEventMatcher(listener->filter()));
    MatcherID id =
        event_filter_.AddEventMatcher(listener->event_name(), matcher.Pass());
    listener->set_matcher_id(id);
    listeners_by_matcher_id_[id] = listener.get();
    filtered_events_.insert(listener->event_name());
  }
  linked_ptr<EventListener> listener_ptr(listener.release());
  listeners_[listener_ptr->event_name()].push_back(listener_ptr);

  delegate_->OnListenerAdded(listener_ptr.get());

  return true;
}

scoped_ptr<EventMatcher> EventListenerMap::ParseEventMatcher(
    DictionaryValue* filter_dict) {
  return scoped_ptr<EventMatcher>(new EventMatcher(
      scoped_ptr<DictionaryValue>(filter_dict->DeepCopy()), MSG_ROUTING_NONE));
}

bool EventListenerMap::RemoveListener(const EventListener* listener) {
  ListenerList& listeners = listeners_[listener->event_name()];
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
    if ((*it2)->extension_id() == extension_id)
      return true;
  }
  return false;
}

bool EventListenerMap::HasListener(const EventListener* listener) {
  ListenerMap::iterator it = listeners_.find(listener->event_name());
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
      if ((*it2)->process() == process &&
          (*it2)->extension_id() == extension_id)
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
      if ((*it2)->IsLazy() && (*it2)->extension_id() == extension_id) {
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
  for (DictionaryValue::Iterator it(filtered); !it.IsAtEnd(); it.Advance()) {
    // We skip entries if they are malformed.
    const base::ListValue* filter_list = NULL;
    if (!it.value().GetAsList(&filter_list))
      continue;
    for (size_t i = 0; i < filter_list->GetSize(); i++) {
      const DictionaryValue* filter = NULL;
      if (!filter_list->GetDictionary(i, &filter))
        continue;
      AddListener(scoped_ptr<EventListener>(new EventListener(
          it.key(), extension_id, NULL,
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
        event_filter_.MatchEvent(event.event_name, event.filter_info,
            MSG_ROUTING_NONE);
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
      if ((*it2)->process() == process) {
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
  if (listener->matcher_id() == -1)
    return;
  event_filter_.RemoveEventMatcher(listener->matcher_id());
  CHECK_EQ(1u, listeners_by_matcher_id_.erase(listener->matcher_id()));
}

bool EventListenerMap::IsFilteredEvent(const Event& event) const {
  return filtered_events_.count(event.event_name) > 0u;
}

}  // namespace extensions
