// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_EVENT_ROUTER_H_

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_listener_map.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/event_filtering_info.h"
#include "ipc/ipc_sender.h"

class GURL;
class Profile;

namespace content {
class BrowserContext;
class RenderProcessHost;
}

namespace extensions {
class ActivityLog;
class Extension;
class ExtensionHost;
class ExtensionPrefs;

struct Event;
struct EventListenerInfo;

class EventRouter : public content::NotificationObserver,
                    public EventListenerMap::Delegate {
 public:
  // These constants convey the state of our knowledge of whether we're in
  // a user-caused gesture as part of DispatchEvent.
  enum UserGestureState {
    USER_GESTURE_UNKNOWN = 0,
    USER_GESTURE_ENABLED = 1,
    USER_GESTURE_NOT_ENABLED = 2,
  };

  // Observers register interest in events with a particular name and are
  // notified when a listener is added or removed for that |event_name|.
  class Observer {
   public:
    // Called when a listener is added.
    virtual void OnListenerAdded(const EventListenerInfo& details) {}
    // Called when a listener is removed.
    virtual void OnListenerRemoved(const EventListenerInfo& details) {}
  };

  // Sends an event via ipc_sender to the given extension. Can be called on any
  // thread.
  static void DispatchEvent(IPC::Sender* ipc_sender,
                            void* profile_id,
                            const std::string& extension_id,
                            const std::string& event_name,
                            scoped_ptr<base::ListValue> event_args,
                            const GURL& event_url,
                            UserGestureState user_gesture,
                            const EventFilteringInfo& info);

  EventRouter(Profile* profile, ExtensionPrefs* extension_prefs);
  virtual ~EventRouter();

  // Add or remove the process/extension pair as a listener for |event_name|.
  // Note that multiple extensions can share a process due to process
  // collapsing. Also, a single extension can have 2 processes if it is a split
  // mode extension.
  void AddEventListener(const std::string& event_name,
                        content::RenderProcessHost* process,
                        const std::string& extension_id);
  void RemoveEventListener(const std::string& event_name,
                           content::RenderProcessHost* process,
                           const std::string& extension_id);

  EventListenerMap& listeners() { return listeners_; }

  // Registers an observer to be notified when an event listener for
  // |event_name| is added or removed. There can currently be only one observer
  // for each distinct |event_name|.
  void RegisterObserver(Observer* observer,
                        const std::string& event_name);

  // Unregisters an observer from all events.
  void UnregisterObserver(Observer* observer);

  // Add or remove the extension as having a lazy background page that listens
  // to the event. The difference from the above methods is that these will be
  // remembered even after the process goes away. We use this list to decide
  // which extension pages to load when dispatching an event.
  void AddLazyEventListener(const std::string& event_name,
                            const std::string& extension_id);
  void RemoveLazyEventListener(const std::string& event_name,
                               const std::string& extension_id);

  // If |add_lazy_listener| is true also add the lazy version of this listener.
  void AddFilteredEventListener(const std::string& event_name,
                                content::RenderProcessHost* process,
                                const std::string& extension_id,
                                const base::DictionaryValue& filter,
                                bool add_lazy_listener);

  // If |remove_lazy_listener| is true also remove the lazy version of this
  // listener.
  void RemoveFilteredEventListener(const std::string& event_name,
                                   content::RenderProcessHost* process,
                                   const std::string& extension_id,
                                   const base::DictionaryValue& filter,
                                   bool remove_lazy_listener);

  // Returns true if there is at least one listener for the given event.
  bool HasEventListener(const std::string& event_name);

  // Returns true if the extension is listening to the given event.
  bool ExtensionHasEventListener(const std::string& extension_id,
                                 const std::string& event_name);

  // Broadcasts an event to every listener registered for that event.
  virtual void BroadcastEvent(scoped_ptr<Event> event);

  // Dispatches an event to the given extension.
  virtual void DispatchEventToExtension(const std::string& extension_id,
                                        scoped_ptr<Event> event);

  // Record the Event Ack from the renderer. (One less event in-flight.)
  void OnEventAck(Profile* profile, const std::string& extension_id);

 private:
  // The extension and process that contains the event listener for a given
  // event.
  struct ListenerProcess;

  // A map between an event name and a set of extensions that are listening
  // to that event.
  typedef std::map<std::string, std::set<ListenerProcess> > ListenerMap;

  // An identifier for an event dispatch that is used to prevent double dispatch
  // due to race conditions between the direct and lazy dispatch paths.
  typedef std::pair<const content::BrowserContext*, std::string>
      EventDispatchIdentifier;

  // Records an event notification in the extension activity log.  Can be
  // called from any thread.
  static void LogExtensionEventMessage(void* profile_id,
                                       const std::string& extension_id,
                                       const std::string& event_name,
                                       scoped_ptr<ListValue> event_args);

  // TODO(gdk): Document this.
  static void DispatchExtensionMessage(
      IPC::Sender* ipc_sender,
      void* profile_id,
      const std::string& extension_id,
      const std::string& event_name,
      base::ListValue* event_args,
      const GURL& event_url,
      UserGestureState user_gesture,
      const extensions::EventFilteringInfo& info);

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Returns true if the given listener map contains a event listeners for
  // the given event. If |extension_id| is non-empty, we also check that that
  // extension is one of the listeners.
  bool HasEventListenerImpl(const ListenerMap& listeners,
                            const std::string& extension_id,
                            const std::string& event_name);

  // Shared by DispatchEvent*. If |restrict_to_extension_id| is empty, the
  // event is broadcast.
  // An event that just came off the pending list may not be delayed again.
  void DispatchEventImpl(const std::string& restrict_to_extension_id,
                         const linked_ptr<Event>& event);

  // Ensures that all lazy background pages that are interested in the given
  // event are loaded, and queues the event if the page is not ready yet.
  // Inserts an EventDispatchIdentifier into |already_dispatched| for each lazy
  // event dispatch that is queued.
  void DispatchLazyEvent(const std::string& extension_id,
                         const linked_ptr<Event>& event,
                         std::set<EventDispatchIdentifier>* already_dispatched);

  // Dispatches the event to the specified extension running in |process|.
  void DispatchEventToProcess(const std::string& extension_id,
                              content::RenderProcessHost* process,
                              const linked_ptr<Event>& event);

  // Returns false when the event is scoped to a profile and the listening
  // extension does not have access to events from that profile. Also fills
  // |event_args| with the proper arguments to send, which may differ if
  // the event crosses the incognito boundary.
  bool CanDispatchEventToProfile(Profile* profile,
                                 const Extension* extension,
                                 const linked_ptr<Event>& event);

  // Possibly loads given extension's background page in preparation to
  // dispatch an event.  Returns true if the event was queued for subsequent
  // dispatch, false otherwise.
  bool MaybeLoadLazyBackgroundPageToDispatchEvent(
      Profile* profile,
      const Extension* extension,
      const linked_ptr<Event>& event);

  // Track of the number of dispatched events that have not yet sent an
  // ACK from the renderer.
  void IncrementInFlightEvents(Profile* profile,
                               const Extension* extension);

  void DispatchPendingEvent(const linked_ptr<Event>& event,
                            ExtensionHost* host);

  // Implementation of EventListenerMap::Delegate.
  virtual void OnListenerAdded(const EventListener* listener) OVERRIDE;
  virtual void OnListenerRemoved(const EventListener* listener) OVERRIDE;

  Profile* profile_;

  content::NotificationRegistrar registrar_;

  EventListenerMap listeners_;

  typedef base::hash_map<std::string, Observer*> ObserverMap;
  ObserverMap observers_;

  ActivityLog* activity_log_;

  // True if we should dispatch the event signalling that Chrome was updated
  // upon loading an extension.
  bool dispatch_chrome_updated_event_;

  DISALLOW_COPY_AND_ASSIGN(EventRouter);
};

struct Event {
  typedef base::Callback<
      void(Profile*, const Extension*, base::ListValue*)> WillDispatchCallback;

  // The event to dispatch.
  std::string event_name;

  // Arguments to send to the event listener.
  scoped_ptr<base::ListValue> event_args;

  // If non-NULL, then the event will not be sent to other profiles unless the
  // extension has permission (e.g. incognito tab update -> normal profile only
  // works if extension is allowed incognito access).
  Profile* restrict_to_profile;

  // If not empty, the event is only sent to extensions with host permissions
  // for this url.
  GURL event_url;

  // Whether a user gesture triggered the event.
  EventRouter::UserGestureState user_gesture;

  // Extra information used to filter which events are sent to the listener.
  EventFilteringInfo filter_info;

  // If specified, this is called before dispatching an event to each
  // extension. The third argument is a mutable reference to event_args,
  // allowing the caller to provide different arguments depending on the
  // extension and profile. This is guaranteed to be called synchronously with
  // DispatchEvent, so callers don't need to worry about lifetime.
  WillDispatchCallback will_dispatch_callback;

  Event(const std::string& event_name,
        scoped_ptr<base::ListValue> event_args);

  Event(const std::string& event_name,
        scoped_ptr<base::ListValue> event_args,
        Profile* restrict_to_profile);

  Event(const std::string& event_name,
        scoped_ptr<base::ListValue> event_args,
        Profile* restrict_to_profile,
        const GURL& event_url,
        EventRouter::UserGestureState user_gesture,
        const EventFilteringInfo& info);

  ~Event();

  // Makes a deep copy of this instance. Ownership is transferred to the
  // caller.
  Event* DeepCopy();
};

struct EventListenerInfo {
  EventListenerInfo(const std::string& event_name,
                    const std::string& extension_id);

  const std::string event_name;
  const std::string extension_id;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EVENT_ROUTER_H_
