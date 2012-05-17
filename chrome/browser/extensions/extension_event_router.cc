// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_event_router.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/runtime/runtime_api.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/extension_devtools_manager.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_processes_api.h"
#include "chrome/browser/extensions/extension_processes_api_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/lazy_background_task_queue.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_view_type.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/api/extension_api.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;
using extensions::ExtensionAPI;

namespace {

const char kDispatchEvent[] = "Event.dispatchJSON";

void NotifyEventListenerRemovedOnIOThread(
    void* profile,
    const std::string& extension_id,
    const std::string& sub_event_name) {
  ExtensionWebRequestEventRouter::GetInstance()->RemoveEventListener(
      profile, extension_id, sub_event_name);
}

}  // namespace

struct ExtensionEventRouter::ListenerProcess {
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

struct ExtensionEventRouter::ExtensionEvent {
  std::string event_name;
  std::string event_args;
  GURL event_url;
  Profile* restrict_to_profile;
  std::string cross_incognito_args;
  UserGestureState user_gesture;

  ExtensionEvent(const std::string& event_name,
                 const std::string& event_args,
                 const GURL& event_url,
                 Profile* restrict_to_profile,
                 const std::string& cross_incognito_args,
                 UserGestureState user_gesture)
    : event_name(event_name),
      event_args(event_args),
      event_url(event_url),
      restrict_to_profile(restrict_to_profile),
      cross_incognito_args(cross_incognito_args),
      user_gesture(user_gesture) {}
};

// static
void ExtensionEventRouter::DispatchEvent(IPC::Message::Sender* ipc_sender,
                                         const std::string& extension_id,
                                         const std::string& event_name,
                                         const std::string& event_args,
                                         const GURL& event_url,
                                         UserGestureState user_gesture) {
  ListValue args;
  args.Set(0, Value::CreateStringValue(event_name));
  args.Set(1, Value::CreateStringValue(event_args));
  ipc_sender->Send(new ExtensionMsg_MessageInvoke(MSG_ROUTING_CONTROL,
      extension_id, kDispatchEvent, args, event_url,
      user_gesture == USER_GESTURE_ENABLED));
}

ExtensionEventRouter::ExtensionEventRouter(Profile* profile)
    : profile_(profile),
      extension_devtools_manager_(
          ExtensionSystem::Get(profile)->devtools_manager()) {
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile_));
}

ExtensionEventRouter::~ExtensionEventRouter() {}

void ExtensionEventRouter::AddEventListener(
    const std::string& event_name,
    content::RenderProcessHost* process,
    const std::string& extension_id) {
  ListenerProcess listener(process, extension_id);
  DCHECK_EQ(listeners_[event_name].count(listener), 0u) << event_name;
  listeners_[event_name].insert(listener);

  if (extension_devtools_manager_.get())
    extension_devtools_manager_->AddEventListener(event_name,
                                                  process->GetID());

  // We lazily tell the TaskManager to start updating when listeners to the
  // processes.onUpdated or processes.onUpdatedWithMemory events arrive.
  if (event_name.compare(extension_processes_api_constants::kOnUpdated) == 0 ||
      event_name.compare(
          extension_processes_api_constants::kOnUpdatedWithMemory) == 0)
    ExtensionProcessesEventRouter::GetInstance()->ListenerAdded();
}

void ExtensionEventRouter::RemoveEventListener(
    const std::string& event_name,
    content::RenderProcessHost* process,
    const std::string& extension_id) {
  ListenerProcess listener(process, extension_id);
  DCHECK_EQ(listeners_[event_name].count(listener), 1u) <<
      " PID=" << process->GetID() << " extension=" << extension_id <<
      " event=" << event_name;
  listeners_[event_name].erase(listener);
  // Note: extension_id may point to data in the now-deleted listeners_ object.
  // Do not use.

  if (extension_devtools_manager_.get())
    extension_devtools_manager_->RemoveEventListener(event_name,
                                                     process->GetID());

  // If a processes.onUpdated or processes.onUpdatedWithMemory event listener
  // is removed (or a process with one exits), then we let the extension API
  // know that it has one fewer listener.
  if (event_name.compare(extension_processes_api_constants::kOnUpdated) == 0 ||
      event_name.compare(
          extension_processes_api_constants::kOnUpdatedWithMemory) == 0)
    ExtensionProcessesEventRouter::GetInstance()->ListenerRemoved();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &NotifyEventListenerRemovedOnIOThread,
          profile_, listener.extension_id, event_name));
}

void ExtensionEventRouter::AddLazyEventListener(
    const std::string& event_name,
    const std::string& extension_id) {
  ListenerProcess lazy_listener(NULL, extension_id);
  bool is_new = lazy_listeners_[event_name].insert(lazy_listener).second;
  if (is_new) {
    ExtensionPrefs* prefs = profile_->GetExtensionService()->extension_prefs();
    std::set<std::string> events = prefs->GetRegisteredEvents(extension_id);
    bool prefs_is_new = events.insert(event_name).second;
    if (prefs_is_new)
      prefs->SetRegisteredEvents(extension_id, events);
  }
}

void ExtensionEventRouter::RemoveLazyEventListener(
    const std::string& event_name,
    const std::string& extension_id) {
  ListenerProcess lazy_listener(NULL, extension_id);
  bool did_exist = lazy_listeners_[event_name].erase(lazy_listener) > 0;
  if (did_exist) {
    ExtensionPrefs* prefs = profile_->GetExtensionService()->extension_prefs();
    std::set<std::string> events = prefs->GetRegisteredEvents(extension_id);
    bool prefs_did_exist = events.erase(event_name) > 0;
    DCHECK(prefs_did_exist);
    prefs->SetRegisteredEvents(extension_id, events);
  }
}

bool ExtensionEventRouter::HasEventListener(const std::string& event_name) {
  return (HasEventListenerImpl(listeners_, "", event_name) ||
          HasEventListenerImpl(lazy_listeners_, "", event_name));
}

bool ExtensionEventRouter::ExtensionHasEventListener(
    const std::string& extension_id, const std::string& event_name) {
  return (HasEventListenerImpl(listeners_, extension_id, event_name) ||
          HasEventListenerImpl(lazy_listeners_, extension_id, event_name));
}

bool ExtensionEventRouter::HasEventListenerImpl(
    const ListenerMap& listener_map,
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

void ExtensionEventRouter::DispatchEventToRenderers(
    const std::string& event_name,
    const std::string& event_args,
    Profile* restrict_to_profile,
    const GURL& event_url) {
  linked_ptr<ExtensionEvent> event(
      new ExtensionEvent(event_name, event_args, event_url,
                         restrict_to_profile, "", USER_GESTURE_UNKNOWN));
  DispatchEventImpl("", event);
}

void ExtensionEventRouter::DispatchEventToExtension(
    const std::string& extension_id,
    const std::string& event_name,
    const std::string& event_args,
    Profile* restrict_to_profile,
    const GURL& event_url) {
  DCHECK(!extension_id.empty());
  linked_ptr<ExtensionEvent> event(
      new ExtensionEvent(event_name, event_args, event_url,
                         restrict_to_profile, "", USER_GESTURE_UNKNOWN));
  DispatchEventImpl(extension_id, event);
}

void ExtensionEventRouter::DispatchEventToExtension(
    const std::string& extension_id,
    const std::string& event_name,
    const std::string& event_args,
    Profile* restrict_to_profile,
    const GURL& event_url,
    UserGestureState user_gesture) {
  DCHECK(!extension_id.empty());
  linked_ptr<ExtensionEvent> event(
      new ExtensionEvent(event_name, event_args, event_url,
                         restrict_to_profile, "", user_gesture));
  DispatchEventImpl(extension_id, event);
}

void ExtensionEventRouter::DispatchEventsToRenderersAcrossIncognito(
    const std::string& event_name,
    const std::string& event_args,
    Profile* restrict_to_profile,
    const std::string& cross_incognito_args,
    const GURL& event_url) {
  linked_ptr<ExtensionEvent> event(
      new ExtensionEvent(event_name, event_args, event_url,
                         restrict_to_profile, cross_incognito_args,
                         USER_GESTURE_UNKNOWN));
  DispatchEventImpl("", event);
}

void ExtensionEventRouter::DispatchEventImpl(
    const std::string& extension_id,
    const linked_ptr<ExtensionEvent>& event) {
  // We don't expect to get events from a completely different profile.
  DCHECK(!event->restrict_to_profile ||
         profile_->IsSameProfile(event->restrict_to_profile));

  LoadLazyBackgroundPagesForEvent(extension_id, event);

  ListenerMap::iterator it = listeners_.find(event->event_name);
  if (it == listeners_.end())
    return;

  std::set<ListenerProcess>& listeners = it->second;
  for (std::set<ListenerProcess>::iterator listener = listeners.begin();
       listener != listeners.end(); ++listener) {
    if (!extension_id.empty() && extension_id != listener->extension_id)
      continue;

    DispatchEventToListener(*listener, event);
  }
}

void ExtensionEventRouter::DispatchEventToListener(
    const ListenerProcess& listener,
    const linked_ptr<ExtensionEvent>& event) {
  ExtensionService* service = profile_->GetExtensionService();
  const Extension* extension = service->extensions()->GetByID(
      listener.extension_id);

  // The extension could have been removed, but we do not unregister it until
  // the extension process is unloaded.
  if (!extension)
    return;

  Profile* listener_profile = Profile::FromBrowserContext(
      listener.process->GetBrowserContext());
  extensions::ProcessMap* process_map =
      listener_profile->GetExtensionService()->process_map();
  // If the event is privileged, only send to extension processes. Otherwise,
  // it's OK to send to normal renderers (e.g., for content scripts).
  if (ExtensionAPI::GetSharedInstance()->IsPrivileged(event->event_name) &&
      !process_map->Contains(extension->id(), listener.process->GetID())) {
    return;
  }

  const std::string* event_args;
  if (!CanDispatchEventToProfile(listener_profile, extension,
                                 event, &event_args))
    return;

  DispatchEvent(listener.process, listener.extension_id,
                event->event_name, *event_args,
                event->event_url, event->user_gesture);
  IncrementInFlightEvents(listener_profile, extension);
}

bool ExtensionEventRouter::CanDispatchEventToProfile(
    Profile* profile,
    const Extension* extension,
    const linked_ptr<ExtensionEvent>& event,
    const std::string** event_args) {
  *event_args = &event->event_args;

  // Is this event from a different profile than the renderer (ie, an
  // incognito tab event sent to a normal process, or vice versa).
  bool cross_incognito = event->restrict_to_profile &&
      profile != event->restrict_to_profile;
  if (cross_incognito &&
      !profile->GetExtensionService()->CanCrossIncognito(extension)) {
    if (event->cross_incognito_args.empty())
      return false;
    // Send the event with different arguments to extensions that can't
    // cross incognito.
    *event_args = &event->cross_incognito_args;
  }

  return true;
}

void ExtensionEventRouter::LoadLazyBackgroundPagesForEvent(
    const std::string& extension_id,
    const linked_ptr<ExtensionEvent>& event) {
  ExtensionService* service = profile_->GetExtensionService();

  ListenerMap::iterator it = lazy_listeners_.find(event->event_name);
  if (it == lazy_listeners_.end())
    return;

  std::set<ListenerProcess>& listeners = it->second;
  for (std::set<ListenerProcess>::iterator listener = listeners.begin();
       listener != listeners.end(); ++listener) {
    if (!extension_id.empty() && extension_id != listener->extension_id)
      continue;

    // Check both the original and the incognito profile to see if we
    // should load a lazy bg page to handle the event. The latter case
    // occurs in the case of split-mode extensions.
    const Extension* extension = service->extensions()->GetByID(
        listener->extension_id);
    if (extension) {
      MaybeLoadLazyBackgroundPage(profile_, extension, event);
      if (profile_->HasOffTheRecordProfile() &&
          extension->incognito_split_mode()) {
        MaybeLoadLazyBackgroundPage(
            profile_->GetOffTheRecordProfile(), extension, event);
      }
    }
  }
}

void ExtensionEventRouter::MaybeLoadLazyBackgroundPage(
    Profile* profile,
    const Extension* extension,
    const linked_ptr<ExtensionEvent>& event) {
  const std::string* event_args;
  if (!CanDispatchEventToProfile(profile, extension, event, &event_args))
    return;

  extensions::LazyBackgroundTaskQueue* queue =
      ExtensionSystem::Get(profile)->lazy_background_task_queue();
  if (queue->ShouldEnqueueTask(profile, extension)) {
    queue->AddPendingTask(
        profile, extension->id(),
        base::Bind(&ExtensionEventRouter::DispatchPendingEvent,
                   base::Unretained(this), event));
  }
}

void ExtensionEventRouter::IncrementInFlightEvents(
    Profile* profile, const Extension* extension) {
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

void ExtensionEventRouter::OnEventAck(
    Profile* profile, const std::string& extension_id) {
  ExtensionProcessManager* pm =
      ExtensionSystem::Get(profile)->process_manager();
  ExtensionHost* host = pm->GetBackgroundHostForExtension(extension_id);
  // The event ACK is routed to the background host, so this should never be
  // NULL.
  CHECK(host);
  CHECK(host->extension()->has_lazy_background_page());
  pm->DecrementLazyKeepaliveCount(host->extension());
}

void ExtensionEventRouter::DispatchPendingEvent(
    const linked_ptr<ExtensionEvent>& event, ExtensionHost* host) {
  if (!host)
    return;

  ListenerProcess listener(host->render_process_host(),
                           host->extension()->id());
  if (listeners_[event->event_name].count(listener) > 0u)
    DispatchEventToListener(listener, event);
}

void ExtensionEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED:
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      content::RenderProcessHost* renderer =
          content::Source<content::RenderProcessHost>(source).ptr();
      // Remove all event listeners associated with this renderer.
      for (ListenerMap::iterator it = listeners_.begin();
           it != listeners_.end(); ) {
        ListenerMap::iterator current_it = it++;
        for (std::set<ListenerProcess>::iterator jt =
                 current_it->second.begin();
             jt != current_it->second.end(); ) {
          std::set<ListenerProcess>::iterator current_jt = jt++;
          if (current_jt->process == renderer) {
            RemoveEventListener(current_it->first,
                                current_jt->process,
                                current_jt->extension_id);
          }
        }
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      // Add all registered lazy listeners to our cache.
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      std::set<std::string> registered_events =
          profile_->GetExtensionService()->extension_prefs()->
              GetRegisteredEvents(extension->id());
      ListenerProcess lazy_listener(NULL, extension->id());
      for (std::set<std::string>::iterator it = registered_events.begin();
           it != registered_events.end(); ++it) {
        lazy_listeners_[*it].insert(lazy_listener);
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      // Remove all registered lazy listeners from our cache.
      UnloadedExtensionInfo* unloaded =
          content::Details<UnloadedExtensionInfo>(details).ptr();
      ListenerProcess lazy_listener(NULL, unloaded->extension->id());
      for (ListenerMap::iterator it = lazy_listeners_.begin();
           it != lazy_listeners_.end(); ++it) {
        it->second.erase(lazy_listener);
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
      // Dispatch the onInstalled event.
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(&extensions::RuntimeEventRouter::DispatchOnInstalledEvent,
                     profile_, extension->id()));
      break;
    }
    default:
      NOTREACHED();
      return;
  }
}
