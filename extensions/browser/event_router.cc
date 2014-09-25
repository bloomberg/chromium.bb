// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/event_router.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/profiler/scoped_profile.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api_activity_monitor.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/permissions/permissions_data.h"

using base::DictionaryValue;
using base::ListValue;
using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

namespace {

void DoNothing(ExtensionHost* host) {}

// A dictionary of event names to lists of filters that this extension has
// registered from its lazy background page.
const char kFilteredEvents[] = "filtered_events";

// Sends a notification about an event to the API activity monitor on the
// UI thread. Can be called from any thread.
void NotifyApiEventDispatched(void* browser_context_id,
                              const std::string& extension_id,
                              const std::string& event_name,
                              scoped_ptr<ListValue> args) {
  // The ApiActivityMonitor can only be accessed from the UI thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&NotifyApiEventDispatched,
                   browser_context_id,
                   extension_id,
                   event_name,
                   base::Passed(&args)));
    return;
  }

  // Notify the ApiActivityMonitor about the event dispatch.
  BrowserContext* context = static_cast<BrowserContext*>(browser_context_id);
  if (!ExtensionsBrowserClient::Get()->IsValidContext(context))
    return;
  ApiActivityMonitor* monitor =
      ExtensionsBrowserClient::Get()->GetApiActivityMonitor(context);
  if (monitor)
    monitor->OnApiEventDispatched(extension_id, event_name, args.Pass());
}

}  // namespace

const char EventRouter::kRegisteredEvents[] = "events";

struct EventRouter::ListenerProcess {
  content::RenderProcessHost* process;
  std::string extension_id;

  ListenerProcess(content::RenderProcessHost* process,
                  const std::string& extension_id)
      : process(process), extension_id(extension_id) {}

  bool operator<(const ListenerProcess& that) const {
    if (process < that.process)
      return true;
    if (process == that.process && extension_id < that.extension_id)
      return true;
    return false;
  }
};

// static
void EventRouter::DispatchExtensionMessage(IPC::Sender* ipc_sender,
                                           void* browser_context_id,
                                           const std::string& extension_id,
                                           const std::string& event_name,
                                           ListValue* event_args,
                                           UserGestureState user_gesture,
                                           const EventFilteringInfo& info) {
  NotifyApiEventDispatched(browser_context_id,
                           extension_id,
                           event_name,
                           make_scoped_ptr(event_args->DeepCopy()));

  ListValue args;
  args.Set(0, new base::StringValue(event_name));
  args.Set(1, event_args);
  args.Set(2, info.AsValue().release());
  ipc_sender->Send(new ExtensionMsg_MessageInvoke(
      MSG_ROUTING_CONTROL,
      extension_id,
      kEventBindings,
      "dispatchEvent",
      args,
      user_gesture == USER_GESTURE_ENABLED));

  // DispatchExtensionMessage does _not_ take ownership of event_args, so we
  // must ensure that the destruction of args does not attempt to free it.
  scoped_ptr<base::Value> removed_event_args;
  args.Remove(1, &removed_event_args);
  ignore_result(removed_event_args.release());
}

// static
EventRouter* EventRouter::Get(content::BrowserContext* browser_context) {
  return ExtensionSystem::Get(browser_context)->event_router();
}

// static
std::string EventRouter::GetBaseEventName(const std::string& full_event_name) {
  size_t slash_sep = full_event_name.find('/');
  return full_event_name.substr(0, slash_sep);
}

// static
void EventRouter::DispatchEvent(IPC::Sender* ipc_sender,
                                void* browser_context_id,
                                const std::string& extension_id,
                                const std::string& event_name,
                                scoped_ptr<ListValue> event_args,
                                UserGestureState user_gesture,
                                const EventFilteringInfo& info) {
  DispatchExtensionMessage(ipc_sender,
                           browser_context_id,
                           extension_id,
                           event_name,
                           event_args.get(),
                           user_gesture,
                           info);

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&EventRouter::IncrementInFlightEventsOnUI,
                  browser_context_id,
                  extension_id));
}

EventRouter::EventRouter(BrowserContext* browser_context,
                         ExtensionPrefs* extension_prefs)
    : browser_context_(browser_context),
      extension_prefs_(extension_prefs),
      extension_registry_observer_(this),
      listeners_(this) {
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_ENABLED,
                 content::Source<BrowserContext>(browser_context_));
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

EventRouter::~EventRouter() {}

void EventRouter::AddEventListener(const std::string& event_name,
                                   content::RenderProcessHost* process,
                                   const std::string& extension_id) {
  listeners_.AddListener(EventListener::ForExtension(
      event_name, extension_id, process, scoped_ptr<DictionaryValue>()));
}

void EventRouter::RemoveEventListener(const std::string& event_name,
                                      content::RenderProcessHost* process,
                                      const std::string& extension_id) {
  scoped_ptr<EventListener> listener = EventListener::ForExtension(
      event_name, extension_id, process, scoped_ptr<DictionaryValue>());
  listeners_.RemoveListener(listener.get());
}

void EventRouter::AddEventListenerForURL(const std::string& event_name,
                                         content::RenderProcessHost* process,
                                         const GURL& listener_url) {
  listeners_.AddListener(EventListener::ForURL(
      event_name, listener_url, process, scoped_ptr<DictionaryValue>()));
}

void EventRouter::RemoveEventListenerForURL(const std::string& event_name,
                                            content::RenderProcessHost* process,
                                            const GURL& listener_url) {
  scoped_ptr<EventListener> listener = EventListener::ForURL(
      event_name, listener_url, process, scoped_ptr<DictionaryValue>());
  listeners_.RemoveListener(listener.get());
}

void EventRouter::RegisterObserver(Observer* observer,
                                   const std::string& event_name) {
  // Observing sub-event names like "foo.onBar/123" is not allowed.
  DCHECK(event_name.find('/') == std::string::npos);
  observers_[event_name] = observer;
}

void EventRouter::UnregisterObserver(Observer* observer) {
  std::vector<ObserverMap::iterator> iters_to_remove;
  for (ObserverMap::iterator iter = observers_.begin();
       iter != observers_.end(); ++iter) {
    if (iter->second == observer)
      iters_to_remove.push_back(iter);
  }
  for (size_t i = 0; i < iters_to_remove.size(); ++i)
    observers_.erase(iters_to_remove[i]);
}

void EventRouter::OnListenerAdded(const EventListener* listener) {
  const EventListenerInfo details(listener->event_name(),
                                  listener->extension_id(),
                                  listener->listener_url(),
                                  listener->GetBrowserContext());
  std::string base_event_name = GetBaseEventName(listener->event_name());
  ObserverMap::iterator observer = observers_.find(base_event_name);
  if (observer != observers_.end()) {
    // TODO(vadimt): Remove ScopedProfile below once crbug.com/417106 is fixed.
    tracked_objects::ScopedProfile tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "EventRouter_OnListenerAdded_ObserverCall"));
    observer->second->OnListenerAdded(details);
  }
}

void EventRouter::OnListenerRemoved(const EventListener* listener) {
  const EventListenerInfo details(listener->event_name(),
                                  listener->extension_id(),
                                  listener->listener_url(),
                                  listener->GetBrowserContext());
  std::string base_event_name = GetBaseEventName(listener->event_name());
  ObserverMap::iterator observer = observers_.find(base_event_name);
  if (observer != observers_.end())
    observer->second->OnListenerRemoved(details);
}

void EventRouter::AddLazyEventListener(const std::string& event_name,
                                       const std::string& extension_id) {
  bool is_new = listeners_.AddListener(EventListener::ForExtension(
      event_name, extension_id, NULL, scoped_ptr<DictionaryValue>()));

  if (is_new) {
    std::set<std::string> events = GetRegisteredEvents(extension_id);
    bool prefs_is_new = events.insert(event_name).second;
    if (prefs_is_new)
      SetRegisteredEvents(extension_id, events);
  }
}

void EventRouter::RemoveLazyEventListener(const std::string& event_name,
                                          const std::string& extension_id) {
  scoped_ptr<EventListener> listener = EventListener::ForExtension(
      event_name, extension_id, NULL, scoped_ptr<DictionaryValue>());
  bool did_exist = listeners_.RemoveListener(listener.get());

  if (did_exist) {
    std::set<std::string> events = GetRegisteredEvents(extension_id);
    bool prefs_did_exist = events.erase(event_name) > 0;
    DCHECK(prefs_did_exist);
    SetRegisteredEvents(extension_id, events);
  }
}

void EventRouter::AddFilteredEventListener(const std::string& event_name,
                                           content::RenderProcessHost* process,
                                           const std::string& extension_id,
                                           const base::DictionaryValue& filter,
                                           bool add_lazy_listener) {
  listeners_.AddListener(EventListener::ForExtension(
      event_name,
      extension_id,
      process,
      scoped_ptr<DictionaryValue>(filter.DeepCopy())));

  if (add_lazy_listener) {
    bool added = listeners_.AddListener(EventListener::ForExtension(
        event_name,
        extension_id,
        NULL,
        scoped_ptr<DictionaryValue>(filter.DeepCopy())));

    if (added)
      AddFilterToEvent(event_name, extension_id, &filter);
  }
}

void EventRouter::RemoveFilteredEventListener(
    const std::string& event_name,
    content::RenderProcessHost* process,
    const std::string& extension_id,
    const base::DictionaryValue& filter,
    bool remove_lazy_listener) {
  scoped_ptr<EventListener> listener = EventListener::ForExtension(
      event_name,
      extension_id,
      process,
      scoped_ptr<DictionaryValue>(filter.DeepCopy()));

  listeners_.RemoveListener(listener.get());

  if (remove_lazy_listener) {
    listener->MakeLazy();
    bool removed = listeners_.RemoveListener(listener.get());

    if (removed)
      RemoveFilterFromEvent(event_name, extension_id, &filter);
  }
}

bool EventRouter::HasEventListener(const std::string& event_name) {
  return listeners_.HasListenerForEvent(event_name);
}

bool EventRouter::ExtensionHasEventListener(const std::string& extension_id,
                                            const std::string& event_name) {
  return listeners_.HasListenerForExtension(extension_id, event_name);
}

bool EventRouter::HasEventListenerImpl(const ListenerMap& listener_map,
                                       const std::string& extension_id,
                                       const std::string& event_name) {
  ListenerMap::const_iterator it = listener_map.find(event_name);
  if (it == listener_map.end())
    return false;

  const std::set<ListenerProcess>& listeners = it->second;
  if (extension_id.empty())
    return !listeners.empty();

  for (std::set<ListenerProcess>::const_iterator listener = listeners.begin();
       listener != listeners.end(); ++listener) {
    if (listener->extension_id == extension_id)
      return true;
  }
  return false;
}

std::set<std::string> EventRouter::GetRegisteredEvents(
    const std::string& extension_id) {
  std::set<std::string> events;
  const ListValue* events_value = NULL;

  if (!extension_prefs_ ||
      !extension_prefs_->ReadPrefAsList(
           extension_id, kRegisteredEvents, &events_value)) {
    return events;
  }

  for (size_t i = 0; i < events_value->GetSize(); ++i) {
    std::string event;
    if (events_value->GetString(i, &event))
      events.insert(event);
  }
  return events;
}

void EventRouter::SetRegisteredEvents(const std::string& extension_id,
                                      const std::set<std::string>& events) {
  ListValue* events_value = new ListValue;
  for (std::set<std::string>::const_iterator iter = events.begin();
       iter != events.end(); ++iter) {
    events_value->Append(new base::StringValue(*iter));
  }
  extension_prefs_->UpdateExtensionPref(
      extension_id, kRegisteredEvents, events_value);
}

void EventRouter::AddFilterToEvent(const std::string& event_name,
                                   const std::string& extension_id,
                                   const DictionaryValue* filter) {
  ExtensionPrefs::ScopedDictionaryUpdate update(
      extension_prefs_, extension_id, kFilteredEvents);
  DictionaryValue* filtered_events = update.Get();
  if (!filtered_events)
    filtered_events = update.Create();

  ListValue* filter_list = NULL;
  if (!filtered_events->GetList(event_name, &filter_list)) {
    filter_list = new ListValue;
    filtered_events->SetWithoutPathExpansion(event_name, filter_list);
  }

  filter_list->Append(filter->DeepCopy());
}

void EventRouter::RemoveFilterFromEvent(const std::string& event_name,
                                        const std::string& extension_id,
                                        const DictionaryValue* filter) {
  ExtensionPrefs::ScopedDictionaryUpdate update(
      extension_prefs_, extension_id, kFilteredEvents);
  DictionaryValue* filtered_events = update.Get();
  ListValue* filter_list = NULL;
  if (!filtered_events ||
      !filtered_events->GetListWithoutPathExpansion(event_name, &filter_list)) {
    return;
  }

  for (size_t i = 0; i < filter_list->GetSize(); i++) {
    DictionaryValue* filter = NULL;
    CHECK(filter_list->GetDictionary(i, &filter));
    if (filter->Equals(filter)) {
      filter_list->Remove(i, NULL);
      break;
    }
  }
}

const DictionaryValue* EventRouter::GetFilteredEvents(
    const std::string& extension_id) {
  const DictionaryValue* events = NULL;
  extension_prefs_->ReadPrefAsDictionary(
      extension_id, kFilteredEvents, &events);
  return events;
}

void EventRouter::BroadcastEvent(scoped_ptr<Event> event) {
  DispatchEventImpl(std::string(), linked_ptr<Event>(event.release()));
}

void EventRouter::DispatchEventToExtension(const std::string& extension_id,
                                           scoped_ptr<Event> event) {
  DCHECK(!extension_id.empty());
  DispatchEventImpl(extension_id, linked_ptr<Event>(event.release()));
}

void EventRouter::DispatchEventWithLazyListener(const std::string& extension_id,
                                                scoped_ptr<Event> event) {
  DCHECK(!extension_id.empty());
  std::string event_name = event->event_name;
  bool has_listener = ExtensionHasEventListener(extension_id, event_name);
  if (!has_listener)
    AddLazyEventListener(event_name, extension_id);
  DispatchEventToExtension(extension_id, event.Pass());
  if (!has_listener)
    RemoveLazyEventListener(event_name, extension_id);
}

void EventRouter::DispatchEventImpl(const std::string& restrict_to_extension_id,
                                    const linked_ptr<Event>& event) {
  // We don't expect to get events from a completely different browser context.
  DCHECK(!event->restrict_to_browser_context ||
         ExtensionsBrowserClient::Get()->IsSameContext(
             browser_context_, event->restrict_to_browser_context));

  std::set<const EventListener*> listeners(
      listeners_.GetEventListeners(*event));

  std::set<EventDispatchIdentifier> already_dispatched;

  // We dispatch events for lazy background pages first because attempting to do
  // so will cause those that are being suspended to cancel that suspension.
  // As canceling a suspension entails sending an event to the affected
  // background page, and as that event needs to be delivered before we dispatch
  // the event we are dispatching here, we dispatch to the lazy listeners here
  // first.
  for (std::set<const EventListener*>::iterator it = listeners.begin();
       it != listeners.end(); it++) {
    const EventListener* listener = *it;
    if (restrict_to_extension_id.empty() ||
        restrict_to_extension_id == listener->extension_id()) {
      if (listener->IsLazy()) {
        DispatchLazyEvent(listener->extension_id(), event, &already_dispatched);
      }
    }
  }

  for (std::set<const EventListener*>::iterator it = listeners.begin();
       it != listeners.end(); it++) {
    const EventListener* listener = *it;
    if (restrict_to_extension_id.empty() ||
        restrict_to_extension_id == listener->extension_id()) {
      if (listener->process()) {
        EventDispatchIdentifier dispatch_id(listener->GetBrowserContext(),
                                            listener->extension_id());
        if (!ContainsKey(already_dispatched, dispatch_id)) {
          DispatchEventToProcess(listener->extension_id(),
                                 listener->listener_url(),
                                 listener->process(),
                                 event);
        }
      }
    }
  }
}

void EventRouter::DispatchLazyEvent(
    const std::string& extension_id,
    const linked_ptr<Event>& event,
    std::set<EventDispatchIdentifier>* already_dispatched) {
  // Check both the original and the incognito browser context to see if we
  // should load a lazy bg page to handle the event. The latter case
  // occurs in the case of split-mode extensions.
  const Extension* extension =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions().GetByID(
          extension_id);
  if (!extension)
    return;

  if (MaybeLoadLazyBackgroundPageToDispatchEvent(
          browser_context_, extension, event)) {
    already_dispatched->insert(std::make_pair(browser_context_, extension_id));
  }

  ExtensionsBrowserClient* browser_client = ExtensionsBrowserClient::Get();
  if (browser_client->HasOffTheRecordContext(browser_context_) &&
      IncognitoInfo::IsSplitMode(extension)) {
    BrowserContext* incognito_context =
        browser_client->GetOffTheRecordContext(browser_context_);
    if (MaybeLoadLazyBackgroundPageToDispatchEvent(
            incognito_context, extension, event)) {
      already_dispatched->insert(
          std::make_pair(incognito_context, extension_id));
    }
  }
}

void EventRouter::DispatchEventToProcess(const std::string& extension_id,
                                         const GURL& listener_url,
                                         content::RenderProcessHost* process,
                                         const linked_ptr<Event>& event) {
  BrowserContext* listener_context = process->GetBrowserContext();
  ProcessMap* process_map = ProcessMap::Get(listener_context);

  // TODO(kalman): Convert this method to use
  // ProcessMap::GetMostLikelyContextType.

  const Extension* extension =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions().GetByID(
          extension_id);
  // NOTE: |extension| being NULL does not necessarily imply that this event
  // shouldn't be dispatched. Events can be dispatched to WebUI as well.

  if (!extension && !extension_id.empty()) {
    // Trying to dispatch an event to an extension that doesn't exist. The
    // extension could have been removed, but we do not unregister it until the
    // extension process is unloaded.
    return;
  }

  if (extension) {
    // Dispatching event to an extension.
    // If the event is privileged, only send to extension processes. Otherwise,
    // it's OK to send to normal renderers (e.g., for content scripts).
    if (!process_map->Contains(extension->id(), process->GetID()) &&
        !ExtensionAPI::GetSharedInstance()->IsAvailableInUntrustedContext(
            event->event_name, extension)) {
      return;
    }

    // If the event is restricted to a URL, only dispatch if the extension has
    // permission for it (or if the event originated from itself).
    if (!event->event_url.is_empty() &&
        event->event_url.host() != extension->id() &&
        !extension->permissions_data()
             ->active_permissions()
             ->HasEffectiveAccessToURL(event->event_url)) {
      return;
    }

    if (!CanDispatchEventToBrowserContext(listener_context, extension, event)) {
      return;
    }
  } else if (content::ChildProcessSecurityPolicy::GetInstance()
                 ->HasWebUIBindings(process->GetID())) {
    // Dispatching event to WebUI.
    if (!ExtensionAPI::GetSharedInstance()->IsAvailableToWebUI(
            event->event_name, listener_url)) {
      return;
    }
  } else {
    // Dispatching event to a webpage - however, all such events (e.g.
    // messaging) don't go through EventRouter so this should be impossible.
    NOTREACHED();
    return;
  }

  if (!event->will_dispatch_callback.is_null()) {
    event->will_dispatch_callback.Run(
        listener_context, extension, event->event_args.get());
  }

  DispatchExtensionMessage(process,
                           listener_context,
                           extension_id,
                           event->event_name,
                           event->event_args.get(),
                           event->user_gesture,
                           event->filter_info);

  if (extension)
    IncrementInFlightEvents(listener_context, extension);
}

bool EventRouter::CanDispatchEventToBrowserContext(
    BrowserContext* context,
    const Extension* extension,
    const linked_ptr<Event>& event) {
  // Is this event from a different browser context than the renderer (ie, an
  // incognito tab event sent to a normal process, or vice versa).
  bool cross_incognito = event->restrict_to_browser_context &&
                         context != event->restrict_to_browser_context;
  if (!cross_incognito)
    return true;
  return ExtensionsBrowserClient::Get()->CanExtensionCrossIncognito(
      extension, context);
}

bool EventRouter::MaybeLoadLazyBackgroundPageToDispatchEvent(
    BrowserContext* context,
    const Extension* extension,
    const linked_ptr<Event>& event) {
  if (!CanDispatchEventToBrowserContext(context, extension, event))
    return false;

  LazyBackgroundTaskQueue* queue = ExtensionSystem::Get(
      context)->lazy_background_task_queue();
  if (queue->ShouldEnqueueTask(context, extension)) {
    linked_ptr<Event> dispatched_event(event);

    // If there's a dispatch callback, call it now (rather than dispatch time)
    // to avoid lifetime issues. Use a separate copy of the event args, so they
    // last until the event is dispatched.
    if (!event->will_dispatch_callback.is_null()) {
      dispatched_event.reset(event->DeepCopy());
      dispatched_event->will_dispatch_callback.Run(
          context, extension, dispatched_event->event_args.get());
      // Ensure we don't call it again at dispatch time.
      dispatched_event->will_dispatch_callback.Reset();
    }

    queue->AddPendingTask(context, extension->id(),
                          base::Bind(&EventRouter::DispatchPendingEvent,
                                     base::Unretained(this), dispatched_event));
    return true;
  }

  return false;
}

// static
void EventRouter::IncrementInFlightEventsOnUI(
    void* browser_context_id,
    const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserContext* browser_context =
      reinterpret_cast<BrowserContext*>(browser_context_id);
  if (!ExtensionsBrowserClient::Get()->IsValidContext(browser_context))
    return;
  EventRouter* event_router = EventRouter::Get(browser_context);
  if (!event_router)
    return;
  const Extension* extension =
      ExtensionRegistry::Get(browser_context)->enabled_extensions().GetByID(
          extension_id);
  if (!extension)
    return;
  event_router->IncrementInFlightEvents(browser_context, extension);
}

void EventRouter::IncrementInFlightEvents(BrowserContext* context,
                                          const Extension* extension) {
  // Only increment in-flight events if the lazy background page is active,
  // because that's the only time we'll get an ACK.
  if (BackgroundInfo::HasLazyBackgroundPage(extension)) {
    ProcessManager* pm = ExtensionSystem::Get(context)->process_manager();
    ExtensionHost* host = pm->GetBackgroundHostForExtension(extension->id());
    if (host)
      pm->IncrementLazyKeepaliveCount(extension);
  }
}

void EventRouter::OnEventAck(BrowserContext* context,
                             const std::string& extension_id) {
  ProcessManager* pm = ExtensionSystem::Get(context)->process_manager();
  ExtensionHost* host = pm->GetBackgroundHostForExtension(extension_id);
  // The event ACK is routed to the background host, so this should never be
  // NULL.
  CHECK(host);
  // TODO(mpcomplete): We should never get this message unless
  // HasLazyBackgroundPage is true. Find out why we're getting it anyway.
  if (host->extension() &&
      BackgroundInfo::HasLazyBackgroundPage(host->extension()))
    pm->DecrementLazyKeepaliveCount(host->extension());
}

void EventRouter::DispatchPendingEvent(const linked_ptr<Event>& event,
                                       ExtensionHost* host) {
  if (!host)
    return;

  if (listeners_.HasProcessListener(host->render_process_host(),
                                    host->extension()->id())) {
    // URL events cannot be lazy therefore can't be pending, hence the GURL().
    DispatchEventToProcess(
        host->extension()->id(), GURL(), host->render_process_host(), event);
  }
}

void EventRouter::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED:
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      content::RenderProcessHost* renderer =
          content::Source<content::RenderProcessHost>(source).ptr();
      // Remove all event listeners associated with this renderer.
      listeners_.RemoveListenersForProcess(renderer);
      break;
    }
    case extensions::NOTIFICATION_EXTENSION_ENABLED: {
      // If the extension has a lazy background page, make sure it gets loaded
      // to register the events the extension is interested in.
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (BackgroundInfo::HasLazyBackgroundPage(extension)) {
        LazyBackgroundTaskQueue* queue = ExtensionSystem::Get(
            browser_context_)->lazy_background_task_queue();
        queue->AddPendingTask(browser_context_, extension->id(),
                              base::Bind(&DoNothing));
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void EventRouter::OnExtensionLoaded(content::BrowserContext* browser_context,
                                    const Extension* extension) {
  // Add all registered lazy listeners to our cache.
  std::set<std::string> registered_events =
      GetRegisteredEvents(extension->id());
  listeners_.LoadUnfilteredLazyListeners(extension->id(), registered_events);
  const DictionaryValue* filtered_events = GetFilteredEvents(extension->id());
  if (filtered_events)
    listeners_.LoadFilteredLazyListeners(extension->id(), *filtered_events);
}

void EventRouter::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UnloadedExtensionInfo::Reason reason) {
  // Remove all registered lazy listeners from our cache.
  listeners_.RemoveLazyListenersForExtension(extension->id());
}

Event::Event(const std::string& event_name,
             scoped_ptr<base::ListValue> event_args)
    : event_name(event_name),
      event_args(event_args.Pass()),
      restrict_to_browser_context(NULL),
      user_gesture(EventRouter::USER_GESTURE_UNKNOWN) {
  DCHECK(this->event_args.get());
}

Event::Event(const std::string& event_name,
             scoped_ptr<base::ListValue> event_args,
             BrowserContext* restrict_to_browser_context)
    : event_name(event_name),
      event_args(event_args.Pass()),
      restrict_to_browser_context(restrict_to_browser_context),
      user_gesture(EventRouter::USER_GESTURE_UNKNOWN) {
  DCHECK(this->event_args.get());
}

Event::Event(const std::string& event_name,
             scoped_ptr<ListValue> event_args,
             BrowserContext* restrict_to_browser_context,
             const GURL& event_url,
             EventRouter::UserGestureState user_gesture,
             const EventFilteringInfo& filter_info)
    : event_name(event_name),
      event_args(event_args.Pass()),
      restrict_to_browser_context(restrict_to_browser_context),
      event_url(event_url),
      user_gesture(user_gesture),
      filter_info(filter_info) {
  DCHECK(this->event_args.get());
}

Event::~Event() {}

Event* Event::DeepCopy() {
  Event* copy = new Event(event_name,
                          scoped_ptr<base::ListValue>(event_args->DeepCopy()),
                          restrict_to_browser_context,
                          event_url,
                          user_gesture,
                          filter_info);
  copy->will_dispatch_callback = will_dispatch_callback;
  return copy;
}

EventListenerInfo::EventListenerInfo(const std::string& event_name,
                                     const std::string& extension_id,
                                     const GURL& listener_url,
                                     content::BrowserContext* browser_context)
    : event_name(event_name),
      extension_id(extension_id),
      listener_url(listener_url),
      browser_context(browser_context) {
}

}  // namespace extensions
