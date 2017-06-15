// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EVENTS_LAZY_EVENT_DISPATCHER_H_
#define EXTENSIONS_BROWSER_EVENTS_LAZY_EVENT_DISPATCHER_H_

#include <set>
#include <utility>

#include "base/callback.h"
#include "base/memory/linked_ptr.h"
#include "extensions/common/extension_id.h"

namespace base {
class DictionaryValue;
}

namespace content {
class BrowserContext;
}

namespace extensions {
class EventListener;
class Extension;
class ExtensionHost;
class LazyContextId;
struct Event;

// Helper class for EventRouter to dispatch lazy events to lazy contexts.
//
// Manages waking up lazy contexts if they are stopped.
class LazyEventDispatcher {
 public:
  // TODO(lazyboy): ExtensionHost is specific to events pages, provide a generic
  // context info that works for both event pages and service workers.
  using DispatchFunction =
      base::Callback<void(const linked_ptr<Event>&, ExtensionHost*)>;

  LazyEventDispatcher(content::BrowserContext* browser_context,
                      const linked_ptr<Event>& event,
                      const DispatchFunction& dispatch_function);
  ~LazyEventDispatcher();

  // Dispatches a lazy event to |extension_id|.
  //
  // Ensures that all lazy background pages that are interested in the given
  // event are loaded, and queues the event if the page is not ready yet.
  void DispatchToEventPage(const ExtensionId& extension_id,
                           const base::DictionaryValue* listener_filter);

  // Returns whether or not an event listener identical to |listener| is queued
  // for dispatch already.
  bool HasAlreadyDispatched(content::BrowserContext* context,
                            const EventListener* listener) const;

 private:
  using EventPageDispatchIdentifier =
      std::pair<const content::BrowserContext*, std::string>;

  void DispatchToLazyContext(LazyContextId* dispatch_context,
                             const base::DictionaryValue* listener_filter);

  // Possibly loads given extension's background page or extension Service
  // Worker in preparation to dispatch an event.  Returns true if the event was
  // queued for subsequent dispatch, false otherwise.
  bool QueueEventDispatch(LazyContextId* dispatch_context,
                          const Extension* extension,
                          const base::DictionaryValue* listener_filter);

  bool HasAlreadyDispatchedImpl(const LazyContextId* dispatch_context) const;

  void RecordAlreadyDispatched(LazyContextId* dispatch_context);

  content::BrowserContext* GetIncognitoContext(const Extension* extension);

  content::BrowserContext* const browser_context_;
  linked_ptr<Event> event_;
  DispatchFunction dispatch_function_;

  std::set<EventPageDispatchIdentifier> dispatched_ids_for_event_page_;

  DISALLOW_COPY_AND_ASSIGN(LazyEventDispatcher);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EVENTS_LAZY_EVENT_DISPATCHER_H_
