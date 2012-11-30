// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/event_router.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/runtime/runtime_api.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/extension_devtools_manager.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/lazy_background_task_queue.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/browser/extensions/system_info_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/extension_api.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/view_type.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"

using base::Value;
using content::BrowserThread;

namespace extensions {

namespace {

const char kDispatchEvent[] = "Event.dispatchEvent";

void NotifyEventListenerRemovedOnIOThread(
    void* profile,
    const std::string& extension_id,
    const std::string& sub_event_name) {
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(
      profile, extension_id, sub_event_name);
}

void DispatchOnInstalledEvent(
    Profile* profile,
    const std::string& extension_id,
    const Version& old_version,
    bool chrome_updated) {
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return;

  RuntimeEventRouter::DispatchOnInstalledEvent(profile, extension_id,
                                               old_version, chrome_updated);
}

void DoNothing(extensions::ExtensionHost* host) {}

}  // namespace

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
                                           const std::string& extension_id,
                                           const std::string& event_name,
                                           ListValue* event_args,
                                           const GURL& event_url,
                                           UserGestureState user_gesture,
                                           const EventFilteringInfo& info) {
  ListValue args;
  args.Set(0, Value::CreateStringValue(event_name));
  args.Set(1, event_args);
  args.Set(2, info.AsValue().release());
  ipc_sender->Send(new ExtensionMsg_MessageInvoke(MSG_ROUTING_CONTROL,
      extension_id, kDispatchEvent, args, event_url,
      user_gesture == USER_GESTURE_ENABLED));

  // DispatchExtensionMessage does _not_ take ownership of event_args, so we
  // must ensure that the destruction of args does not attempt to free it.
  Value* removed_event_args = NULL;
  args.Remove(1, &removed_event_args);
}

// static
void EventRouter::DispatchEvent(IPC::Sender* ipc_sender,
                                const std::string& extension_id,
                                const std::string& event_name,
                                scoped_ptr<ListValue> event_args,
                                const GURL& event_url,
                                UserGestureState user_gesture,
                                const EventFilteringInfo& info) {
  DispatchExtensionMessage(ipc_sender, extension_id, event_name,
                           event_args.get(), event_url, user_gesture, info);
}

EventRouter::EventRouter(Profile* profile, ExtensionPrefs* extension_prefs)
    : profile_(profile),
      listeners_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      dispatch_chrome_updated_event_(false) {
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_ENABLED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile_));

  // NULL in unit_tests.
  if (extension_prefs) {
    // Check if registered events are up-to-date. We need to do this before
    // reading the registered events, because it deletes them if they're out of
    // date.
    dispatch_chrome_updated_event_ =
        !extension_prefs->CheckRegisteredEventsUpToDate();
  }
}

EventRouter::~EventRouter() {}

void EventRouter::AddEventListener(const std::string& event_name,
                                   content::RenderProcessHost* process,
                                   const std::string& extension_id) {
  listeners_.AddListener(scoped_ptr<EventListener>(new EventListener(
      event_name, extension_id, process, scoped_ptr<DictionaryValue>())));
}

void EventRouter::RemoveEventListener(const std::string& event_name,
                                      content::RenderProcessHost* process,
                                      const std::string& extension_id) {
  EventListener listener(event_name, extension_id, process,
                         scoped_ptr<DictionaryValue>());
  listeners_.RemoveListener(&listener);
}

void EventRouter::RegisterObserver(Observer* observer,
                                   const std::string& event_name) {
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
  const std::string& event_name = listener->event_name;
  const EventListenerInfo details(event_name, listener->extension_id);
  ObserverMap::iterator observer = observers_.find(event_name);
  if (observer != observers_.end())
    observer->second->OnListenerAdded(details);

  // TODO(yoz): Migrate these to become EventRouter observers.
  // EventRouter shouldn't need to know about them.
  if (listener->process) {
    ExtensionDevToolsManager* extension_devtools_manager =
        ExtensionSystem::Get(profile_)->devtools_manager();
    if (extension_devtools_manager)
      extension_devtools_manager->AddEventListener(event_name,
                                                   listener->process->GetID());
  }

  if (SystemInfoEventRouter::IsSystemInfoEvent(event_name))
    SystemInfoEventRouter::GetInstance()->AddEventListener(event_name);
}

void EventRouter::OnListenerRemoved(const EventListener* listener) {
  const std::string& event_name = listener->event_name;
  const EventListenerInfo details(event_name, listener->extension_id);
  ObserverMap::iterator observer = observers_.find(event_name);
  if (observer != observers_.end())
    observer->second->OnListenerRemoved(details);

  // TODO(yoz): Migrate these to become EventRouter observers.
  // EventRouter shouldn't need to know about them.
  if (listener->process) {
    ExtensionDevToolsManager* extension_devtools_manager =
        ExtensionSystem::Get(profile_)->devtools_manager();
    if (extension_devtools_manager)
      extension_devtools_manager->RemoveEventListener(
          event_name, listener->process->GetID());
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NotifyEventListenerRemovedOnIOThread,
                 profile_, listener->extension_id, event_name));

  if (SystemInfoEventRouter::IsSystemInfoEvent(event_name))
    SystemInfoEventRouter::GetInstance()->RemoveEventListener(event_name);
}

void EventRouter::AddLazyEventListener(const std::string& event_name,
                                       const std::string& extension_id) {
  scoped_ptr<EventListener> listener(new EventListener(
      event_name, extension_id, NULL, scoped_ptr<DictionaryValue>()));
  bool is_new = listeners_.AddListener(listener.Pass());

  if (is_new) {
    ExtensionPrefs* prefs =
        profile_->GetExtensionService()->extension_prefs();
    std::set<std::string> events = prefs->GetRegisteredEvents(extension_id);
    bool prefs_is_new = events.insert(event_name).second;
    if (prefs_is_new)
      prefs->SetRegisteredEvents(extension_id, events);
  }
}

void EventRouter::RemoveLazyEventListener(const std::string& event_name,
                                          const std::string& extension_id) {
  EventListener listener(event_name, extension_id, NULL,
                         scoped_ptr<DictionaryValue>());
  bool did_exist = listeners_.RemoveListener(&listener);

  if (did_exist) {
    ExtensionPrefs* prefs =
        profile_->GetExtensionService()->extension_prefs();
    std::set<std::string> events = prefs->GetRegisteredEvents(extension_id);
    bool prefs_did_exist = events.erase(event_name) > 0;
    DCHECK(prefs_did_exist);
    prefs->SetRegisteredEvents(extension_id, events);
  }
}

void EventRouter::AddFilteredEventListener(const std::string& event_name,
                                           content::RenderProcessHost* process,
                                           const std::string& extension_id,
                                           const base::DictionaryValue& filter,
                                           bool add_lazy_listener) {
  listeners_.AddListener(scoped_ptr<EventListener>(new EventListener(
      event_name, extension_id, process,
      scoped_ptr<DictionaryValue>(filter.DeepCopy()))));

  if (add_lazy_listener) {
    bool added = listeners_.AddListener(scoped_ptr<EventListener>(
        new EventListener(event_name, extension_id, NULL,
        scoped_ptr<DictionaryValue>(filter.DeepCopy()))));

    if (added) {
      ExtensionPrefs* prefs =
          profile_->GetExtensionService()->extension_prefs();
      prefs->AddFilterToEvent(event_name, extension_id, &filter);
    }
  }
}

void EventRouter::RemoveFilteredEventListener(
    const std::string& event_name,
    content::RenderProcessHost* process,
    const std::string& extension_id,
    const base::DictionaryValue& filter,
    bool remove_lazy_listener) {
  EventListener listener(event_name, extension_id, process,
                         scoped_ptr<DictionaryValue>(filter.DeepCopy()));

  listeners_.RemoveListener(&listener);

  if (remove_lazy_listener) {
    listener.process = NULL;
    bool removed = listeners_.RemoveListener(&listener);

    if (removed) {
      ExtensionPrefs* prefs =
          profile_->GetExtensionService()->extension_prefs();
      prefs->RemoveFilterFromEvent(event_name, extension_id, &filter);
    }
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

void EventRouter::DispatchEventToRenderers(const std::string& event_name,
                                           scoped_ptr<ListValue> event_args,
                                           Profile* restrict_to_profile,
                                           const GURL& event_url,
                                           EventFilteringInfo info) {
  linked_ptr<Event> event(new Event(event_name, event_args.Pass(),
                                    event_url, restrict_to_profile,
                                    USER_GESTURE_UNKNOWN, info));
  DispatchEventImpl("", event);
}

void EventRouter::DispatchEventToRenderers(const std::string& event_name,
                                           scoped_ptr<ListValue> event_args,
                                           Profile* restrict_to_profile,
                                           const GURL& event_url) {
  DispatchEventToRenderers(event_name, event_args.Pass(), restrict_to_profile,
                           event_url, EventFilteringInfo());
}

void EventRouter::DispatchEventToRenderers(const std::string& event_name,
                                           scoped_ptr<ListValue> event_args,
                                           Profile* restrict_to_profile,
                                           const GURL& event_url,
                                           UserGestureState user_gesture) {
  EventFilteringInfo info;
  linked_ptr<Event> event(new Event(event_name, event_args.Pass(),
                                    event_url, restrict_to_profile,
                                    user_gesture, info));
  DispatchEventImpl("", event);
}

void EventRouter::DispatchEventToExtension(const std::string& extension_id,
                                           const std::string& event_name,
                                           scoped_ptr<ListValue> event_args,
                                           Profile* restrict_to_profile,
                                           const GURL& event_url) {
  DCHECK(!extension_id.empty());
  linked_ptr<Event> event(new Event(event_name, event_args.Pass(), event_url,
                                    restrict_to_profile, USER_GESTURE_UNKNOWN,
                                    EventFilteringInfo()));
  DispatchEventImpl(extension_id, event);
}

void EventRouter::DispatchEventToExtension(const std::string& extension_id,
                                           const std::string& event_name,
                                           scoped_ptr<ListValue> event_args,
                                           Profile* restrict_to_profile,
                                           const GURL& event_url,
                                           UserGestureState user_gesture) {
  DCHECK(!extension_id.empty());
  linked_ptr<Event> event(new Event(event_name, event_args.Pass(), event_url,
                                    restrict_to_profile, user_gesture,
                                    EventFilteringInfo()));
  DispatchEventImpl(extension_id, event);
}

void EventRouter::DispatchEventsToRenderersAcrossIncognito(
    const std::string& event_name,
    scoped_ptr<ListValue> event_args,
    Profile* restrict_to_profile,
    scoped_ptr<ListValue> cross_incognito_args,
    const GURL& event_url) {
  linked_ptr<Event> event(new Event(event_name, event_args.Pass(), event_url,
                                    restrict_to_profile,
                                    cross_incognito_args.Pass(),
                                    USER_GESTURE_UNKNOWN,
                                    EventFilteringInfo()));
  DispatchEventImpl("", event);
}

void EventRouter::DispatchEventImpl(const std::string& restrict_to_extension_id,
                                    const linked_ptr<Event>& event) {
  // We don't expect to get events from a completely different profile.
  DCHECK(!event->restrict_to_profile ||
         profile_->IsSameProfile(event->restrict_to_profile));

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
        restrict_to_extension_id == listener->extension_id) {
      if (!listener->process) {
        DispatchLazyEvent(listener->extension_id, event, &already_dispatched);
      }
    }
  }

  for (std::set<const EventListener*>::iterator it = listeners.begin();
       it != listeners.end(); it++) {
    const EventListener* listener = *it;
    if (restrict_to_extension_id.empty() ||
        restrict_to_extension_id == listener->extension_id) {

      if (listener->process) {
        EventDispatchIdentifier dispatch_id(
            listener->process->GetBrowserContext(), listener->extension_id);
        if (!ContainsKey(already_dispatched, dispatch_id)) {
          DispatchEventToProcess(listener->extension_id, listener->process,
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
  ExtensionService* service = profile_->GetExtensionService();
  // Check both the original and the incognito profile to see if we
  // should load a lazy bg page to handle the event. The latter case
  // occurs in the case of split-mode extensions.
  const Extension* extension = service->extensions()->GetByID(extension_id);
  if (extension) {
    if (MaybeLoadLazyBackgroundPageToDispatchEvent(
          profile_, extension, event)) {
      already_dispatched->insert(std::make_pair(profile_, extension_id));
    }

    if (profile_->HasOffTheRecordProfile() &&
        extension->incognito_split_mode()) {
      if (MaybeLoadLazyBackgroundPageToDispatchEvent(
          profile_->GetOffTheRecordProfile(), extension, event)) {
        already_dispatched->insert(
            std::make_pair(profile_->GetOffTheRecordProfile(), extension_id));
      }
    }
  }
}

void EventRouter::DispatchEventToProcess(const std::string& extension_id,
                                         content::RenderProcessHost* process,
                                         const linked_ptr<Event>& event) {
  ExtensionService* service = profile_->GetExtensionService();
  const Extension* extension = service->extensions()->GetByID(extension_id);

  // The extension could have been removed, but we do not unregister it until
  // the extension process is unloaded.
  if (!extension)
    return;

  Profile* listener_profile = Profile::FromBrowserContext(
      process->GetBrowserContext());
  ProcessMap* process_map =
      listener_profile->GetExtensionService()->process_map();
  // If the event is privileged, only send to extension processes. Otherwise,
  // it's OK to send to normal renderers (e.g., for content scripts).
  if (ExtensionAPI::GetSharedInstance()->IsPrivileged(event->event_name) &&
      !process_map->Contains(extension->id(), process->GetID())) {
    return;
  }

  ListValue* event_args = NULL;
  if (!CanDispatchEventToProfile(listener_profile, extension,
                                 event, &event_args)) {
    return;
  }

  DispatchExtensionMessage(process, extension_id,
                           event->event_name, event_args,
                           event->event_url, event->user_gesture,
                           event->info);
  IncrementInFlightEvents(listener_profile, extension);
}

bool EventRouter::CanDispatchEventToProfile(Profile* profile,
                                            const Extension* extension,
                                            const linked_ptr<Event>& event,
                                            ListValue** event_args) {
  if (event_args)
    *event_args = event->event_args.get();

  // Is this event from a different profile than the renderer (ie, an
  // incognito tab event sent to a normal process, or vice versa).
  bool cross_incognito =
      event->restrict_to_profile && profile != event->restrict_to_profile;
  if (cross_incognito &&
      !profile->GetExtensionService()->CanCrossIncognito(extension)) {
    if (!event->cross_incognito_args.get())
      return false;
    // Send the event with different arguments to extensions that can't
    // cross incognito.
    if (event_args)
      *event_args = event->cross_incognito_args.get();
  }

  return true;
}

bool EventRouter::MaybeLoadLazyBackgroundPageToDispatchEvent(
    Profile* profile,
    const Extension* extension,
    const linked_ptr<Event>& event) {
  if (!CanDispatchEventToProfile(profile, extension, event, NULL))
    return false;

  LazyBackgroundTaskQueue* queue =
      ExtensionSystem::Get(profile)->lazy_background_task_queue();
  if (queue->ShouldEnqueueTask(profile, extension)) {
    queue->AddPendingTask(profile, extension->id(),
                          base::Bind(&EventRouter::DispatchPendingEvent,
                                     base::Unretained(this), event));
    return true;
  }

  return false;
}

void EventRouter::IncrementInFlightEvents(Profile* profile,
                                          const Extension* extension) {
  // Only increment in-flight events if the lazy background page is active,
  // because that's the only time we'll get an ACK.
  if (extension->has_lazy_background_page()) {
    ExtensionProcessManager* pm =
        ExtensionSystem::Get(profile)->process_manager();
    ExtensionHost* host = pm->GetBackgroundHostForExtension(extension->id());
    if (host)
      pm->IncrementLazyKeepaliveCount(extension);
  }
}

void EventRouter::OnEventAck(Profile* profile,
                             const std::string& extension_id) {
  ExtensionProcessManager* pm =
      ExtensionSystem::Get(profile)->process_manager();
  ExtensionHost* host = pm->GetBackgroundHostForExtension(extension_id);
  // The event ACK is routed to the background host, so this should never be
  // NULL.
  CHECK(host);
  // TODO(mpcomplete): We should never get this message unless
  // has_lazy_background_page is true. Find out why we're getting it anyway.
  if (host->extension() && host->extension()->has_lazy_background_page())
    pm->DecrementLazyKeepaliveCount(host->extension());
}

void EventRouter::DispatchPendingEvent(const linked_ptr<Event>& event,
                                       ExtensionHost* host) {
  if (!host)
    return;

  if (listeners_.HasProcessListener(host->render_process_host(),
                                    host->extension()->id())) {
    DispatchEventToProcess(host->extension()->id(),
                           host->render_process_host(), event);
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
    case chrome::NOTIFICATION_EXTENSIONS_READY: {
      // We're done restarting Chrome after an update.
      dispatch_chrome_updated_event_ = false;
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_ENABLED: {
      // If the extension has a lazy background page, make sure it gets loaded
      // to register the events the extension is interested in.
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (extension->has_lazy_background_page()) {
        LazyBackgroundTaskQueue* queue =
            ExtensionSystem::Get(profile_)->lazy_background_task_queue();
        queue->AddPendingTask(profile_, extension->id(),
                              base::Bind(&DoNothing));
      }
    }
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      // Add all registered lazy listeners to our cache.
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      ExtensionPrefs* prefs =
          profile_->GetExtensionService()->extension_prefs();
      std::set<std::string> registered_events =
          prefs->GetRegisteredEvents(extension->id());
      listeners_.LoadUnfilteredLazyListeners(extension->id(),
                                             registered_events);
      const DictionaryValue* filtered_events =
          prefs->GetFilteredEvents(extension->id());
      if (filtered_events)
        listeners_.LoadFilteredLazyListeners(extension->id(), *filtered_events);

      if (dispatch_chrome_updated_event_) {
        MessageLoop::current()->PostTask(FROM_HERE,
            base::Bind(&DispatchOnInstalledEvent, profile_, extension->id(),
                       Version(), true));
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      // Remove all registered lazy listeners from our cache.
      UnloadedExtensionInfo* unloaded =
          content::Details<UnloadedExtensionInfo>(details).ptr();
      listeners_.RemoveLazyListenersForExtension(unloaded->extension->id());
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
      // Dispatch the onInstalled event.
      const Extension* extension =
          content::Details<const Extension>(details).ptr();

      // Get the previous version, if this is an upgrade.
      ExtensionService* service =
          ExtensionSystem::Get(profile_)->extension_service();
      const Extension* old = service->GetExtensionById(extension->id(), true);
      Version old_version;
      if (old)
        old_version = *old->version();

      MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(&DispatchOnInstalledEvent, profile_, extension->id(),
                     old_version, false));
      break;
    }
    default:
      NOTREACHED();
      return;
  }
}

Event::Event(const std::string& event_name,
             scoped_ptr<ListValue> event_args,
             const GURL& event_url,
             Profile* restrict_to_profile,
             scoped_ptr<ListValue> cross_incognito_args,
             EventRouter::UserGestureState user_gesture,
             const EventFilteringInfo& info)
    : event_name(event_name),
      event_args(event_args.Pass()),
      event_url(event_url),
      restrict_to_profile(restrict_to_profile),
      cross_incognito_args(cross_incognito_args.Pass()),
      user_gesture(user_gesture),
      info(info) {}

Event::Event(const std::string& event_name,
             scoped_ptr<ListValue> event_args,
             const GURL& event_url,
             Profile* restrict_to_profile,
             EventRouter::UserGestureState user_gesture,
             const EventFilteringInfo& info)
    : event_name(event_name),
      event_args(event_args.Pass()),
      event_url(event_url),
      restrict_to_profile(restrict_to_profile),
      cross_incognito_args(NULL),
      user_gesture(user_gesture),
      info(info) {}

Event::~Event() {}

EventListenerInfo::EventListenerInfo(const std::string& event_name,
                                     const std::string& extension_id)
    : event_name(event_name),
      extension_id(extension_id) {}

}  // namespace extensions
