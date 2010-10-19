// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_event_router.h"

#include "base/values.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/extensions/extension_devtools_manager.h"
#include "chrome/browser/extensions/extension_processes_api.h"
#include "chrome/browser/extensions/extension_processes_api_constants.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"

namespace {

const char kDispatchEvent[] = "Event.dispatchJSON";

static void DispatchEvent(RenderProcessHost* renderer,
                          const std::string& event_name,
                          const std::string& event_args,
                          bool cross_incognito,
                          const GURL& event_url) {
  ListValue args;
  args.Set(0, Value::CreateStringValue(event_name));
  args.Set(1, Value::CreateStringValue(event_args));
  renderer->Send(new ViewMsg_ExtensionMessageInvoke(MSG_ROUTING_CONTROL,
      kDispatchEvent, args, cross_incognito, event_url));
}

}  // namespace

// static
std::string ExtensionEventRouter::GetPerExtensionEventName(
    const std::string& event_name, const std::string& extension_id) {
  // This should match the method we use in extension_process_binding.js when
  // setting up the corresponding chrome.Event object.
  return event_name + "/" + extension_id;
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
    int render_process_id) {
  DCHECK_EQ(listeners_[event_name].count(render_process_id), 0u) << event_name;
  listeners_[event_name].insert(render_process_id);

  if (extension_devtools_manager_.get()) {
    extension_devtools_manager_->AddEventListener(event_name,
                                                  render_process_id);
  }

  // We lazily tell the TaskManager to start updating when listeners to the
  // processes.onUpdated event arrive.
  if (event_name.compare(extension_processes_api_constants::kOnUpdated) == 0)
    ExtensionProcessesEventRouter::GetInstance()->ListenerAdded();
}

void ExtensionEventRouter::RemoveEventListener(
    const std::string& event_name,
    int render_process_id) {
  DCHECK_EQ(listeners_[event_name].count(render_process_id), 1u) <<
      " PID=" << render_process_id << " event=" << event_name;
  listeners_[event_name].erase(render_process_id);

  if (extension_devtools_manager_.get()) {
    extension_devtools_manager_->RemoveEventListener(event_name,
                                                     render_process_id);
  }

  // If a processes.onUpdated event listener is removed (or a process with one
  // exits), then we let the TaskManager know that it has one fewer listener.
  if (event_name.compare(extension_processes_api_constants::kOnUpdated) == 0)
    ExtensionProcessesEventRouter::GetInstance()->ListenerRemoved();
}

bool ExtensionEventRouter::HasEventListener(const std::string& event_name) {
  return (listeners_.find(event_name) != listeners_.end() &&
          !listeners_[event_name].empty());
}

void ExtensionEventRouter::DispatchEventToRenderers(
    const std::string& event_name, const std::string& event_args,
    Profile* restrict_to_profile, const GURL& event_url) {
  if (!profile_)
    return;

  // We don't expect to get events from a completely different profile.
  DCHECK(!restrict_to_profile || profile_->IsSameProfile(restrict_to_profile));

  ListenerMap::iterator it = listeners_.find(event_name);
  if (it == listeners_.end())
    return;

  std::set<int>& pids = it->second;

  // Send the event only to renderers that are listening for it.
  for (std::set<int>::iterator pid = pids.begin(); pid != pids.end(); ++pid) {
    RenderProcessHost* renderer = RenderProcessHost::FromID(*pid);
    if (!renderer)
      continue;
    if (!ChildProcessSecurityPolicy::GetInstance()->
            HasExtensionBindings(*pid)) {
      // Don't send browser-level events to unprivileged processes.
      continue;
    }

    // Is this event from a different profile than the renderer (ie, an
    // incognito tab event sent to a normal process, or vice versa).
    bool cross_incognito =
        restrict_to_profile && renderer->profile() != restrict_to_profile;
    DispatchEvent(renderer, event_name, event_args, cross_incognito, event_url);
  }
}

void ExtensionEventRouter::DispatchEventToExtension(
    const std::string& extension_id,
    const std::string& event_name, const std::string& event_args,
    Profile* restrict_to_profile, const GURL& event_url) {
  DispatchEventToRenderers(GetPerExtensionEventName(event_name, extension_id),
                           event_args, restrict_to_profile, event_url);
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
        ListenerMap::iterator current = it++;
        if (current->second.count(renderer->id()) != 0)
          RemoveEventListener(current->first, renderer->id());
      }
      break;
    }
    default:
      NOTREACHED();
      return;
  }
}
