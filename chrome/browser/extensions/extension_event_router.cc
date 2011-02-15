// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_event_router.h"

#include "base/values.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/extensions/extension_devtools_manager.h"
#include "chrome/browser/extensions/extension_processes_api.h"
#include "chrome/browser/extensions/extension_processes_api_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_webrequest_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"

namespace {

const char kDispatchEvent[] = "Event.dispatchJSON";

static void DispatchEvent(RenderProcessHost* renderer,
                          const std::string& extension_id,
                          const std::string& event_name,
                          const std::string& event_args,
                          const GURL& event_url) {
  ListValue args;
  args.Set(0, Value::CreateStringValue(event_name));
  args.Set(1, Value::CreateStringValue(event_args));
  renderer->Send(new ViewMsg_ExtensionMessageInvoke(MSG_ROUTING_CONTROL,
      extension_id, kDispatchEvent, args, event_url));
}

}  // namespace

struct ExtensionEventRouter::EventListener {
  RenderProcessHost* process;
  std::string extension_id;

  explicit EventListener(RenderProcessHost* process,
                         const std::string& extension_id)
      : process(process), extension_id(extension_id) {}

  bool operator<(const EventListener& that) const {
    if (process < that.process)
      return true;
    if (process == that.process && extension_id < that.extension_id)
      return true;
    return false;
  }
};

// static
bool ExtensionEventRouter::CanCrossIncognito(Profile* profile,
                                             const std::string& extension_id) {
  const Extension* extension =
      profile->GetExtensionService()->GetExtensionById(extension_id, false);
  return CanCrossIncognito(profile, extension);
}

// static
bool ExtensionEventRouter::CanCrossIncognito(Profile* profile,
                                             const Extension* extension) {
  // We allow the extension to see events and data from another profile iff it
  // uses "spanning" behavior and it has incognito access. "split" mode
  // extensions only see events for a matching profile.
  return (profile->GetExtensionService()->IsIncognitoEnabled(extension) &&
          !extension->incognito_split_mode());
}

ExtensionEventRouter::ExtensionEventRouter(Profile* profile)
    : profile_(profile),
      extension_devtools_manager_(profile->GetExtensionDevToolsManager()) {
  registrar_.Add(this, NotificationType::RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::RENDERER_PROCESS_CLOSED,
                 NotificationService::AllSources());
}

ExtensionEventRouter::~ExtensionEventRouter() {
}

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

  ExtensionWebRequestEventRouter::RemoveEventListenerOnUIThread(
      listener.extension_id, event_name);
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
    const std::string& event_name, const std::string& event_args,
    Profile* restrict_to_profile, const GURL& event_url) {
  DispatchEventImpl("", event_name, event_args, restrict_to_profile, event_url);
}

void ExtensionEventRouter::DispatchEventToExtension(
    const std::string& extension_id,
    const std::string& event_name, const std::string& event_args,
    Profile* restrict_to_profile, const GURL& event_url) {
  DCHECK(!extension_id.empty());
  DispatchEventImpl(extension_id, event_name, event_args, restrict_to_profile,
                    event_url);
}

void ExtensionEventRouter::DispatchEventImpl(
    const std::string& extension_id,
    const std::string& event_name, const std::string& event_args,
    Profile* restrict_to_profile, const GURL& event_url) {
  if (!profile_)
    return;

  // We don't expect to get events from a completely different profile.
  DCHECK(!restrict_to_profile || profile_->IsSameProfile(restrict_to_profile));

  ListenerMap::iterator it = listeners_.find(event_name);
  if (it == listeners_.end())
    return;

  std::set<EventListener>& listeners = it->second;
  ExtensionService* service = profile_->GetExtensionService();

  // Send the event only to renderers that are listening for it.
  for (std::set<EventListener>::iterator listener = listeners.begin();
       listener != listeners.end(); ++listener) {
    if (!ChildProcessSecurityPolicy::GetInstance()->
            HasExtensionBindings(listener->process->id())) {
      // Don't send browser-level events to unprivileged processes.
      continue;
    }

    if (!extension_id.empty() && extension_id != listener->extension_id)
      continue;

    // Is this event from a different profile than the renderer (ie, an
    // incognito tab event sent to a normal process, or vice versa).
    bool cross_incognito = restrict_to_profile &&
        listener->process->profile() != restrict_to_profile;
    const Extension* extension = service->GetExtensionById(
        listener->extension_id, false);
    if (cross_incognito && !service->CanCrossIncognito(extension))
      continue;

    DispatchEvent(listener->process, listener->extension_id,
                  event_name, event_args, event_url);
  }
}

void ExtensionEventRouter::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::RENDERER_PROCESS_TERMINATED:
    case NotificationType::RENDERER_PROCESS_CLOSED: {
      RenderProcessHost* renderer = Source<RenderProcessHost>(source).ptr();
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
    default:
      NOTREACHED();
      return;
  }
}
