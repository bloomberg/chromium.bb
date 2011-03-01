// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_event_router_forwarder.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "googleurl/src/gurl.h"

ExtensionEventRouterForwarder::ExtensionEventRouterForwarder() {
}

ExtensionEventRouterForwarder::~ExtensionEventRouterForwarder() {
}

void ExtensionEventRouterForwarder::BroadcastEventToRenderers(
    const std::string& event_name, const std::string& event_args,
    const GURL& event_url) {
  HandleEvent("", event_name, event_args, 0, true, event_url);
}

void ExtensionEventRouterForwarder::DispatchEventToRenderers(
    const std::string& event_name, const std::string& event_args,
    ProfileId profile_id, bool use_profile_to_restrict_events,
    const GURL& event_url) {
  if (profile_id == Profile::kInvalidProfileId)
    return;
  HandleEvent("", event_name, event_args, profile_id,
              use_profile_to_restrict_events, event_url);
}

void ExtensionEventRouterForwarder::BroadcastEventToExtension(
    const std::string& extension_id,
    const std::string& event_name, const std::string& event_args,
    const GURL& event_url) {
  HandleEvent(extension_id, event_name, event_args, 0, true, event_url);
}

void ExtensionEventRouterForwarder::DispatchEventToExtension(
    const std::string& extension_id,
    const std::string& event_name, const std::string& event_args,
    ProfileId profile_id, bool use_profile_to_restrict_events,
    const GURL& event_url) {
  if (profile_id == Profile::kInvalidProfileId)
    return;
  HandleEvent(extension_id, event_name, event_args, profile_id,
              use_profile_to_restrict_events, event_url);
}

void ExtensionEventRouterForwarder::HandleEvent(
    const std::string& extension_id,
    const std::string& event_name, const std::string& event_args,
    ProfileId profile_id, bool use_profile_to_restrict_events,
    const GURL& event_url) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(
            this,
            &ExtensionEventRouterForwarder::HandleEvent,
            extension_id, event_name, event_args, profile_id,
            use_profile_to_restrict_events, event_url));
    return;
  }

  if (!g_browser_process || !g_browser_process->profile_manager())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = NULL;
  if (profile_id != Profile::kInvalidProfileId) {
    profile = profile_manager->GetProfileWithId(profile_id);
    if (!profile)
      return;
  }
  if (profile) {
    CallExtensionEventRouter(
        profile, extension_id, event_name, event_args,
        use_profile_to_restrict_events ? profile : NULL, event_url);
  } else {
    ProfileManager::iterator i;
    for (i = profile_manager->begin(); i != profile_manager->end(); ++i) {
      CallExtensionEventRouter(
          *i, extension_id, event_name, event_args,
          use_profile_to_restrict_events ? (*i) : NULL, event_url);
    }
  }
}

void ExtensionEventRouterForwarder::CallExtensionEventRouter(
    Profile* profile, const std::string& extension_id,
    const std::string& event_name, const std::string& event_args,
    Profile* restrict_to_profile, const GURL& event_url) {
  if (extension_id.empty()) {
    profile->GetExtensionEventRouter()->
        DispatchEventToRenderers(
            event_name, event_args, restrict_to_profile, event_url);
  } else {
    profile->GetExtensionEventRouter()->
        DispatchEventToExtension(
            extension_id,
            event_name, event_args, restrict_to_profile, event_url);
  }
}

