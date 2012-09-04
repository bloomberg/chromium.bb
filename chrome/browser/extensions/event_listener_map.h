// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EVENT_LISTENER_MAP_H_
#define CHROME_BROWSER_EXTENSIONS_EVENT_LISTENER_MAP_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/event_filter.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace base {
class DictionaryValue;
}

namespace content {
class RenderProcessHost;
}

class ListenerRemovalListener;

using base::DictionaryValue;

namespace extensions {
struct Event;

// A listener for an extension event. A listener is essentially an endpoint
// that an event can be dispatched to. This is a lazy listener if |process| is
// NULL and a filtered listener if |filter| is defined.
//
// A lazy listener is added to an event to indicate that a lazy background page
// is listening to the event. It is associated with no process, so to dispatch
// an event to a lazy listener one must start a process running the associated
// extension and dispatch the event to that.
//
struct EventListener {
  // |filter| represents a generic filter structure that EventFilter knows how
  // to filter events with. A typical filter instance will look like
  //
  // {
  //   url: [{hostSuffix: 'google.com'}],
  //   tabId: 5
  // }
  EventListener(const std::string& event_name,
                const std::string& extension_id,
                content::RenderProcessHost* process,
                scoped_ptr<DictionaryValue> filter);
  ~EventListener();

  bool Equals(const EventListener* other) const;

  scoped_ptr<EventListener> Copy() const;

  const std::string event_name;
  const std::string extension_id;
  content::RenderProcessHost* process;
  scoped_ptr<DictionaryValue> filter;
  EventFilter::MatcherID matcher_id;

 private:
  DISALLOW_COPY_AND_ASSIGN(EventListener);
};

// Holds listeners for extension events and can answer questions about which
// listeners are interested in what events.
class EventListenerMap {
 public:
  typedef std::vector<linked_ptr<EventListener> > ListenerList;

  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnListenerAdded(const EventListener* listener) = 0;
    virtual void OnListenerRemoved(const EventListener* listener) = 0;
  };

  explicit EventListenerMap(Delegate* delegate);
  ~EventListenerMap();

  // Add a listener for a particular event. GetEventListeners() will include a
  // weak pointer to |listener| in its results if passed a relevant
  // extensions::Event.
  // Returns true if the listener was added (in the case that it has never been
  // seen before).
  bool AddListener(scoped_ptr<EventListener> listener);

  // Remove a listener that .Equals() |listener|.
  // Returns true if the listener was removed .
  bool RemoveListener(const EventListener* listener);

  // Returns the set of listeners that want to be notified of |event|.
  std::set<const EventListener*> GetEventListeners(const Event& event);

  const ListenerList& GetEventListenersByName(const std::string& event_name) {
    return listeners_[event_name];
  }

  // Removes all listeners with process equal to |process|.
  void RemoveListenersForProcess(const content::RenderProcessHost* process);

  // Returns true if there are any listeners on the event named |event_name|.
  bool HasListenerForEvent(const std::string& event_name);

  // Returns true if there are any listeners on |event_name| from
  // |extension_id|.
  bool HasListenerForExtension(const std::string& extension_id,
                               const std::string& event_name);

  // Returns true if this map contains an EventListener that .Equals()
  // |listener|.
  bool HasListener(const EventListener* listener);

  // Returns true if there is a listener for |extension_id| in |process|.
  bool HasProcessListener(content::RenderProcessHost* process,
                          const std::string& extension_id);

  // Removes any lazy listeners that |extension_id| has added.
  void RemoveLazyListenersForExtension(const std::string& extension_id);

  // Adds unfiltered lazy listeners as described their serialised descriptions.
  // |event_names| the names of the lazy events.
  // Note that we can only load lazy listeners in this fashion, because there
  // is no way to serialise a RenderProcessHost*.
  void LoadUnfilteredLazyListeners(const std::string& extension_id,
                                   const std::set<std::string>& event_names);

  // Adds filtered lazy listeners as described their serialised descriptions.
  // |filtered| contains a map from event names to filters, each pairing
  // defining a lazy filtered listener.
  void LoadFilteredLazyListeners(
      const std::string& extension_id,
      const DictionaryValue& filtered);

 private:
  // The key here is an event name.
  typedef std::map<std::string, ListenerList> ListenerMap;

  void CleanupListener(EventListener* listener);
  bool IsFilteredEvent(const Event& event) const;
  scoped_ptr<EventMatcher> ParseEventMatcher(DictionaryValue* filter_dict);

  // Listens for removals from this map.
  Delegate* delegate_;

  std::set<std::string> filtered_events_;
  ListenerMap listeners_;

  std::map<EventFilter::MatcherID, EventListener*> listeners_by_matcher_id_;

  EventFilter event_filter_;

  DISALLOW_COPY_AND_ASSIGN(EventListenerMap);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EVENT_LISTENER_MAP_H_
