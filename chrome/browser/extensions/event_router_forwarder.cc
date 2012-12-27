// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/event_router_forwarder.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;

namespace extensions {

EventRouterForwarder::EventRouterForwarder() {
}

EventRouterForwarder::~EventRouterForwarder() {
}

void EventRouterForwarder::BroadcastEventToRenderers(
    const std::string& event_name,
    scoped_ptr<base::ListValue> event_args,
    const GURL& event_url) {
  HandleEvent("", event_name, event_args.Pass(), 0, true, event_url);
}

void EventRouterForwarder::DispatchEventToRenderers(
    const std::string& event_name,
    scoped_ptr<base::ListValue> event_args,
    void* profile,
    bool use_profile_to_restrict_events,
    const GURL& event_url) {
  if (!profile)
    return;
  HandleEvent("", event_name, event_args.Pass(), profile,
              use_profile_to_restrict_events, event_url);
}

void EventRouterForwarder::BroadcastEventToExtension(
    const std::string& extension_id,
    const std::string& event_name,
    scoped_ptr<base::ListValue> event_args,
    const GURL& event_url) {
  HandleEvent(extension_id, event_name, event_args.Pass(), 0, true, event_url);
}

void EventRouterForwarder::DispatchEventToExtension(
    const std::string& extension_id,
    const std::string& event_name,
    scoped_ptr<base::ListValue> event_args,
    void* profile,
    bool use_profile_to_restrict_events,
    const GURL& event_url) {
  if (!profile)
    return;
  HandleEvent(extension_id, event_name, event_args.Pass(), profile,
              use_profile_to_restrict_events, event_url);
}

void EventRouterForwarder::HandleEvent(const std::string& extension_id,
                                       const std::string& event_name,
                                       scoped_ptr<ListValue> event_args,
                                       void* profile_ptr,
                                       bool use_profile_to_restrict_events,
                                       const GURL& event_url) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&EventRouterForwarder::HandleEvent, this,
                   extension_id, event_name, Passed(&event_args), profile_ptr,
                   use_profile_to_restrict_events, event_url));
    return;
  }

  if (!g_browser_process || !g_browser_process->profile_manager())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = NULL;
  if (profile_ptr) {
    profile = reinterpret_cast<Profile*>(profile_ptr);
    if (!profile_manager->IsValidProfile(profile))
      return;
  }
  if (profile) {
    CallEventRouter(profile, extension_id, event_name, event_args.Pass(),
                    use_profile_to_restrict_events ? profile : NULL, event_url);
  } else {
    std::vector<Profile*> profiles(profile_manager->GetLoadedProfiles());
    for (size_t i = 0; i < profiles.size(); ++i) {
      scoped_ptr<ListValue> per_profile_event_args(event_args->DeepCopy());
      CallEventRouter(
          profiles[i], extension_id, event_name, per_profile_event_args.Pass(),
          use_profile_to_restrict_events ? profiles[i] : NULL, event_url);
    }
  }
}

void EventRouterForwarder::CallEventRouter(Profile* profile,
                                           const std::string& extension_id,
                                           const std::string& event_name,
                                           scoped_ptr<ListValue> event_args,
                                           Profile* restrict_to_profile,
                                           const GURL& event_url) {
  // We may not have an extension in cases like chromeos login
  // (crosbug.com/12856), chrome_frame_net_tests.exe which reuses the chrome
  // browser single process framework.
  if (!extensions::ExtensionSystem::Get(profile)->event_router())
    return;

  scoped_ptr<Event> event(new Event(event_name, event_args.Pass()));
  event->restrict_to_profile = restrict_to_profile;
  event->event_url = event_url;
  if (extension_id.empty()) {
    ExtensionSystem::Get(profile)->event_router()->BroadcastEvent(event.Pass());
  } else {
    ExtensionSystem::Get(profile)->event_router()->
        DispatchEventToExtension(extension_id, event.Pass());
  }
}

}  // namespace extensions
