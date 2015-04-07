// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/settings_private_event_router.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

SettingsPrivateEventRouter::SettingsPrivateEventRouter(
    content::BrowserContext* context)
    : context_(context), listening_(false) {
  // Register with the event router so we know when renderers are listening to
  // our events. We first check and see if there *is* an event router, because
  // some unit tests try to create all context services, but don't initialize
  // the event router first.
  EventRouter* event_router = EventRouter::Get(context_);
  if (event_router) {
    event_router->RegisterObserver(
        this, api::settings_private::OnPrefsChanged::kEventName);
    StartOrStopListeningForPrefsChanges();
  }

  Profile* profile = Profile::FromBrowserContext(context_);
  user_prefs_registrar_.Init(profile->GetPrefs());
  local_state_registrar_.Init(g_browser_process->local_state());
}

SettingsPrivateEventRouter::~SettingsPrivateEventRouter() {
  DCHECK(!listening_);
}

void SettingsPrivateEventRouter::Shutdown() {
  // Unregister with the event router. We first check and see if there *is* an
  // event router, because some unit tests try to shutdown all context services,
  // but didn't initialize the event router first.
  EventRouter* event_router = EventRouter::Get(context_);
  if (event_router)
    event_router->UnregisterObserver(this);

  if (listening_) {
    const prefs_util::TypedPrefMap& keys = prefs_util::GetWhitelistedKeys();
    for (const auto& it : keys) {
      FindRegistrarForPref(it.first)->Remove(it.first);
    }
  }
  listening_ = false;
}

void SettingsPrivateEventRouter::OnListenerAdded(
    const EventListenerInfo& details) {
  // Start listening to events from the PrefChangeRegistrars.
  StartOrStopListeningForPrefsChanges();
}

void SettingsPrivateEventRouter::OnListenerRemoved(
    const EventListenerInfo& details) {
  // Stop listening to events from the PrefChangeRegistrars if there are no
  // more listeners.
  StartOrStopListeningForPrefsChanges();
}

PrefChangeRegistrar* SettingsPrivateEventRouter::FindRegistrarForPref(
    const std::string& pref_name) {
  Profile* profile = Profile::FromBrowserContext(context_);
  if (prefs_util::FindServiceForPref(profile, pref_name) ==
      profile->GetPrefs()) {
    return &user_prefs_registrar_;
  }
  return &local_state_registrar_;
}

void SettingsPrivateEventRouter::StartOrStopListeningForPrefsChanges() {
  EventRouter* event_router = EventRouter::Get(context_);
  bool should_listen = event_router->HasEventListener(
      api::settings_private::OnPrefsChanged::kEventName);

  if (should_listen && !listening_) {
    const prefs_util::TypedPrefMap& keys = prefs_util::GetWhitelistedKeys();
    for (const auto& it : keys) {
      FindRegistrarForPref(it.first)->Add(
          it.first,
          base::Bind(&SettingsPrivateEventRouter::OnPreferenceChanged,
                     base::Unretained(this), user_prefs_registrar_.prefs()));
    }
  } else if (!should_listen && listening_) {
    const prefs_util::TypedPrefMap& keys = prefs_util::GetWhitelistedKeys();
    for (const auto& it : keys) {
      FindRegistrarForPref(it.first)->Remove(it.first);
    }
  }
  listening_ = should_listen;
}

void SettingsPrivateEventRouter::OnPreferenceChanged(
    PrefService* service,
    const std::string& pref_name) {
  EventRouter* event_router = EventRouter::Get(context_);
  if (!event_router->HasEventListener(
          api::settings_private::OnPrefsChanged::kEventName)) {
    return;
  }

  Profile* profile = Profile::FromBrowserContext(context_);
  api::settings_private::PrefObject* pref_object =
      prefs_util::GetPref(profile, pref_name).release();

  std::vector<linked_ptr<api::settings_private::PrefObject>> prefs;
  prefs.push_back(linked_ptr<api::settings_private::PrefObject>(pref_object));

  scoped_ptr<base::ListValue> args(
      api::settings_private::OnPrefsChanged::Create(prefs));

  scoped_ptr<Event> extension_event(new Event(
      api::settings_private::OnPrefsChanged::kEventName, args.Pass()));
  event_router->BroadcastEvent(extension_event.Pass());
}

SettingsPrivateEventRouter* SettingsPrivateEventRouter::Create(
    content::BrowserContext* context) {
  return new SettingsPrivateEventRouter(context);
}

}  // namespace extensions
