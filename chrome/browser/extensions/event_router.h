// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_EVENT_ROUTER_H_

#include <map>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_listener_map.h"
#include "chrome/common/extensions/event_filtering_info.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ipc/ipc_sender.h"

class GURL;
class ExtensionDevToolsManager;
class Profile;

namespace content {
class RenderProcessHost;
}

namespace extensions {
class Extension;
class ExtensionHost;

struct Event;

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

  // Sends an event via ipc_sender to the given extension. Can be called on any
  // thread.
  static void DispatchEvent(IPC::Sender* ipc_sender,
                            const std::string& extension_id,
                            const std::string& event_name,
                            scoped_ptr<base::ListValue> event_args,
                            const GURL& event_url,
                            UserGestureState user_gesture,
                            const EventFilteringInfo& info);

  explicit EventRouter(Profile* profile);
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

  // Send an event to every registered extension renderer. If
  // |restrict_to_profile| is non-NULL, then the event will not be sent to other
  // profiles unless the extension has permission (e.g. incognito tab update ->
  // normal profile only works if extension is allowed incognito access). If
  // |event_url| is not empty, the event is only sent to extension with host
  // permissions for this url.
  void DispatchEventToRenderers(const std::string& event_name,
                                scoped_ptr<base::ListValue> event_args,
                                Profile* restrict_to_profile,
                                const GURL& event_url,
                                EventFilteringInfo info);

  // As above, but defaults |info| to EventFilteringInfo().
  void DispatchEventToRenderers(const std::string& event_name,
                                scoped_ptr<base::ListValue> event_args,
                                Profile* restrict_to_profile,
                                const GURL& event_url);

  // As above, but enables sending an explicit user gesture indicator.
  void DispatchEventToRenderers(const std::string& event_name,
                                scoped_ptr<ListValue> event_args,
                                Profile* restrict_to_profile,
                                const GURL& event_url,
                                UserGestureState user_gesture);

  // Same as above, except only send the event to the given extension.
  virtual void DispatchEventToExtension(const std::string& extension_id,
                                        const std::string& event_name,
                                        scoped_ptr<base::ListValue> event_args,
                                        Profile* restrict_to_profile,
                                        const GURL& event_url);

  // Dispatch an event to particular extension. Also include an
  // explicit user gesture indicator.
  virtual void DispatchEventToExtension(const std::string& extension_id,
                                        const std::string& event_name,
                                        scoped_ptr<base::ListValue> event_args,
                                        Profile* restrict_to_profile,
                                        const GURL& event_url,
                                        UserGestureState user_gesture);

  // Send different versions of an event to extensions in different profiles.
  // This is used in the case of sending one event to extensions that have
  // incognito access, and another event to extensions that don't (here),
  // in order to avoid sending 2 events to "spanning" extensions.
  // If |cross_incognito_profile| is non-NULL and different from
  // restrict_to_profile, send the event with cross_incognito_args to the
  // extensions in that profile that can't cross incognito.
  void DispatchEventsToRenderersAcrossIncognito(
      const std::string& event_name,
      scoped_ptr<base::ListValue> event_args,
      Profile* restrict_to_profile,
      scoped_ptr<base::ListValue> cross_incognito_args,
      const GURL& event_url);

  // Record the Event Ack from the renderer. (One less event in-flight.)
  void OnEventAck(Profile* profile, const std::string& extension_id);

 private:
  // The extension and process that contains the event listener for a given
  // event.
  struct ListenerProcess;

  // A map between an event name and a set of extensions that are listening
  // to that event.
  typedef std::map<std::string, std::set<ListenerProcess> > ListenerMap;

  // TODO(gdk): Document this.
  static void DispatchExtensionMessage(
      IPC::Sender* ipc_sender,
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
  void DispatchLazyEvent(const std::string& extension_id,
                         const linked_ptr<Event>& event);

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
                                 const linked_ptr<Event>& event,
                                 base::ListValue** event_args);

  // Possibly loads given extension's background page in preparation to
  // dispatch an event.
  void MaybeLoadLazyBackgroundPageToDispatchEvent(
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

  scoped_refptr<ExtensionDevToolsManager> extension_devtools_manager_;

  EventListenerMap listeners_;

  DISALLOW_COPY_AND_ASSIGN(EventRouter);
};

struct Event {
  std::string event_name;
  scoped_ptr<base::ListValue> event_args;
  GURL event_url;
  Profile* restrict_to_profile;
  scoped_ptr<base::ListValue> cross_incognito_args;
  EventRouter::UserGestureState user_gesture;
  EventFilteringInfo info;

  Event(const std::string& event_name,
        scoped_ptr<base::ListValue> event_args,
        const GURL& event_url,
        Profile* restrict_to_profile,
        scoped_ptr<base::ListValue> cross_incognito_args,
        EventRouter::UserGestureState user_gesture,
        const EventFilteringInfo& info);

  Event(const std::string& event_name,
        scoped_ptr<base::ListValue> event_args,
        const GURL& event_url,
        Profile* restrict_to_profile,
        EventRouter::UserGestureState user_gesture,
        const EventFilteringInfo& info);

  ~Event();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EVENT_ROUTER_H_
