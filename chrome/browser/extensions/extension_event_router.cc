// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_event_router.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_devtools_manager.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_processes_api.h"
#include "chrome/browser/extensions/extension_processes_api_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_webrequest_api.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/api/extension_api.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/public/browser/notification_service.h"

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

struct ExtensionEventRouter::EventListener {
  RenderProcessHost* process;
  std::string extension_id;

  EventListener(RenderProcessHost* process, const std::string& extension_id)
      : process(process), extension_id(extension_id) {}

  bool operator<(const EventListener& that) const {
    if (process < that.process)
      return true;
    if (process == that.process && extension_id < that.extension_id)
      return true;
    return false;
  }
};

struct ExtensionEventRouter::ExtensionEvent {
  std::string extension_id;
  std::string event_name;
  std::string event_args;
  GURL event_url;
  Profile* restrict_to_profile;
  std::string cross_incognito_args;

  ExtensionEvent(const std::string& extension_id,
                 const std::string& event_name,
                 const std::string& event_args,
                 const GURL& event_url,
                 Profile* restrict_to_profile,
                 const std::string& cross_incognito_args)
    : extension_id(extension_id),
      event_name(event_name),
      event_args(event_args),
      event_url(event_url),
      restrict_to_profile(restrict_to_profile),
      cross_incognito_args(cross_incognito_args) {}
};

// static
void ExtensionEventRouter::DispatchEvent(IPC::Message::Sender* ipc_sender,
                                         const std::string& extension_id,
                                         const std::string& event_name,
                                         const std::string& event_args,
                                         const GURL& event_url) {
  ListValue args;
  args.Set(0, Value::CreateStringValue(event_name));
  args.Set(1, Value::CreateStringValue(event_args));
  ipc_sender->Send(new ExtensionMsg_MessageInvoke(MSG_ROUTING_CONTROL,
      extension_id, kDispatchEvent, args, event_url));
}

ExtensionEventRouter::ExtensionEventRouter(Profile* profile)
    : profile_(profile),
      extension_devtools_manager_(profile->GetExtensionDevToolsManager()) {
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
                 content::Source<Profile>(profile_));
  // TODO(tessamac): also get notified for background page crash/failure.
}

ExtensionEventRouter::~ExtensionEventRouter() {}

void ExtensionEventRouter::AddEventListener(
    const std::string& event_name,
    RenderProcessHost* process,
    const std::string& extension_id) {
  EventListener listener(process, extension_id);
  DCHECK_EQ(listeners_[event_name].count(listener), 0u) << event_name;
  listeners_[event_name].insert(listener);

  if (extension_devtools_manager_.get())
    extension_devtools_manager_->AddEventListener(event_name, process->id());

  // We lazily tell the TaskManager to start updating when listeners to the
  // processes.onUpdated event arrive.
  if (event_name.compare(extension_processes_api_constants::kOnUpdated) == 0)
    ExtensionProcessesEventRouter::GetInstance()->ListenerAdded();
}

void ExtensionEventRouter::RemoveEventListener(
    const std::string& event_name,
    RenderProcessHost* process,
    const std::string& extension_id) {
  EventListener listener(process, extension_id);
  DCHECK_EQ(listeners_[event_name].count(listener), 1u) <<
      " PID=" << process->id() << " extension=" << extension_id <<
      " event=" << event_name;
  listeners_[event_name].erase(listener);
  // Note: extension_id may point to data in the now-deleted listeners_ object.
  // Do not use.

  if (extension_devtools_manager_.get())
    extension_devtools_manager_->RemoveEventListener(event_name, process->id());

  // If a processes.onUpdated event listener is removed (or a process with one
  // exits), then we let the TaskManager know that it has one fewer listener.
  if (event_name.compare(extension_processes_api_constants::kOnUpdated) == 0)
    ExtensionProcessesEventRouter::GetInstance()->ListenerRemoved();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &NotifyEventListenerRemovedOnIOThread,
          profile_, listener.extension_id, event_name));
}

bool ExtensionEventRouter::HasEventListener(const std::string& event_name) {
  return (listeners_.find(event_name) != listeners_.end() &&
          !listeners_[event_name].empty());
}

bool ExtensionEventRouter::ExtensionHasEventListener(
    const std::string& extension_id, const std::string& event_name) {
  ListenerMap::iterator it = listeners_.find(event_name);
  if (it == listeners_.end())
    return false;

  std::set<EventListener>& listeners = it->second;
  for (std::set<EventListener>::iterator listener = listeners.begin();
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
      new ExtensionEvent("", event_name, event_args, event_url,
                         restrict_to_profile, ""));
  DispatchEventImpl(event, false);
}

void ExtensionEventRouter::DispatchEventToExtension(
    const std::string& extension_id,
    const std::string& event_name,
    const std::string& event_args,
    Profile* restrict_to_profile,
    const GURL& event_url) {
  DCHECK(!extension_id.empty());
  linked_ptr<ExtensionEvent> event(
      new ExtensionEvent(extension_id, event_name, event_args, event_url,
                         restrict_to_profile, ""));
  DispatchEventImpl(event, false);
}

void ExtensionEventRouter::DispatchEventsToRenderersAcrossIncognito(
    const std::string& event_name,
    const std::string& event_args,
    Profile* restrict_to_profile,
    const std::string& cross_incognito_args,
    const GURL& event_url) {
  linked_ptr<ExtensionEvent> event(
      new ExtensionEvent("", event_name, event_args, event_url,
                         restrict_to_profile, cross_incognito_args));
  DispatchEventImpl(event, false);
}

bool ExtensionEventRouter::CanDispatchEventNow(
    const std::string& extension_id) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLazyBackgroundPages))
    return true;

  if (extension_id.empty())
    // TODO(tessamac): Create all background pages. Wait for all to be loaded?
    //                 or dispatch event to each extension when it's ready?
    return true;

  const Extension* extension = profile_->GetExtensionService()->
      GetExtensionById(extension_id, false);  // exclude disabled extensions
  if (extension && extension->background_url().is_valid()) {
    ExtensionProcessManager* pm = profile_->GetExtensionProcessManager();
    if (!pm->GetBackgroundHostForExtension(extension_id)) {
      pm->CreateBackgroundHost(extension, extension->background_url());
      return false;
    }
  }

  return true;
}

void ExtensionEventRouter::DispatchEventImpl(
    const linked_ptr<ExtensionEvent>& event, bool was_pending) {
  if (!profile_)
    return;

  // We don't expect to get events from a completely different profile.
  DCHECK(!event->restrict_to_profile ||
         profile_->IsSameProfile(event->restrict_to_profile));

  if (!CanDispatchEventNow(event->extension_id)) {
    // Events should not be made pending twice. This may happen if the
    // background page is shutdown before we finish dispatching pending events.
    CHECK(!was_pending);
    // TODO(tessamac): make sure Background Page notification doesn't
    //                 happen before the event is added to the pending list.
    AppendEvent(event);
    return;
  }

  ListenerMap::iterator it = listeners_.find(event->event_name);
  if (it == listeners_.end())
    return;

  std::set<EventListener>& listeners = it->second;
  ExtensionService* service = profile_->GetExtensionService();

  // Send the event only to renderers that are listening for it.
  for (std::set<EventListener>::iterator listener = listeners.begin();
       listener != listeners.end(); ++listener) {
    if (!event->extension_id.empty() &&
        event->extension_id != listener->extension_id)
      continue;

    const Extension* extension = service->GetExtensionById(
        listener->extension_id, false);

    // The extension could have been removed, but we do not unregister it until
    // the extension process is unloaded.
    if (!extension)
      continue;

    Profile* listener_profile = Profile::FromBrowserContext(
        listener->process->browser_context());
    extensions::ProcessMap* process_map =
        listener_profile->GetExtensionService()->process_map();

    // If the event is privileged, only send to extension processes. Otherwise,
    // it's OK to send to normal renderers (e.g., for content scripts).
    if (ExtensionAPI::GetInstance()->IsPrivileged(event->event_name) &&
        !process_map->Contains(extension->id(), listener->process->id())) {
      continue;
    }

    // Is this event from a different profile than the renderer (ie, an
    // incognito tab event sent to a normal process, or vice versa).
    bool cross_incognito = event->restrict_to_profile &&
        listener_profile != event->restrict_to_profile;
    // Send the event with different arguments to extensions that can't
    // cross incognito, if necessary.
    if (cross_incognito && !service->CanCrossIncognito(extension)) {
      if (!event->cross_incognito_args.empty()) {
        DispatchEvent(listener->process, listener->extension_id,
                      event->event_name, event->cross_incognito_args,
                      event->event_url);
        IncrementInFlightEvents(listener->extension_id);
      }
      continue;
    }

    DispatchEvent(listener->process, listener->extension_id,
                  event->event_name, event->event_args, event->event_url);
    IncrementInFlightEvents(listener->extension_id);
  }
}

void ExtensionEventRouter::IncrementInFlightEvents(
    const std::string& extension_id) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLazyBackgroundPages))
    in_flight_events_[extension_id]++;
}

void ExtensionEventRouter::OnExtensionEventAck(
    const std::string& extension_id) {
  CHECK(CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableLazyBackgroundPages));
  CHECK(in_flight_events_[extension_id] > 0);
  in_flight_events_[extension_id]--;
}

bool ExtensionEventRouter::HasInFlightEvents(const std::string& extension_id) {
  return in_flight_events_[extension_id] > 0;
}

void ExtensionEventRouter::AppendEvent(
    const linked_ptr<ExtensionEvent>& event) {
  PendingEventsList* events_list = NULL;
  PendingEventsPerExtMap::iterator it =
      pending_events_.find(event->extension_id);
  if (it == pending_events_.end()) {
    events_list = new PendingEventsList();
    pending_events_[event->extension_id] =
        linked_ptr<PendingEventsList>(events_list);
  } else {
    events_list = it->second.get();
  }

  events_list->push_back(event);
}

void ExtensionEventRouter::DispatchPendingEvents(
    const std::string &extension_id) {
  // Find the list of pending events for this extension.
  PendingEventsPerExtMap::const_iterator map_it =
      pending_events_.find(extension_id);
  if (map_it == pending_events_.end())
    return;

  PendingEventsList* events_list = map_it->second.get();
  for (PendingEventsList::const_iterator it = events_list->begin();
       it != events_list->end(); ++it)
    DispatchEventImpl(*it, true);

  // Delete list.
  events_list->clear();
  pending_events_.erase(extension_id);
}

void ExtensionEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED:
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      RenderProcessHost* renderer =
          content::Source<RenderProcessHost>(source).ptr();
      // Remove all event listeners associated with this renderer
      for (ListenerMap::iterator it = listeners_.begin();
           it != listeners_.end(); ) {
        ListenerMap::iterator current_it = it++;
        for (std::set<EventListener>::iterator jt = current_it->second.begin();
             jt != current_it->second.end(); ) {
          std::set<EventListener>::iterator current_jt = jt++;
          if (current_jt->process == renderer) {
            RemoveEventListener(current_it->first,
                                current_jt->process,
                                current_jt->extension_id);
          }
        }
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING: {
      // TODO: dispatch events in queue.  ExtensionHost is in the details.
      ExtensionHost* eh = content::Details<ExtensionHost>(details).ptr();
      DispatchPendingEvents(eh->extension_id());
      break;
    }
    // TODO(tessamac): if background page crashed/failed clear queue.
    default:
      NOTREACHED();
      return;
  }
}
