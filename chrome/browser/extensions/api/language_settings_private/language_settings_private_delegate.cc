// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate.h"

#include "content/public/browser/browser_context.h"

namespace extensions {

namespace language_settings_private = api::language_settings_private;

LanguageSettingsPrivateDelegate::LanguageSettingsPrivateDelegate(
    content::BrowserContext* context)
    : context_(context),
      listening_spellcheck_(false) {
  // Register with the event router so we know when renderers are listening to
  // our events. We first check and see if there *is* an event router, because
  // some unit tests try to create all context services, but don't initialize
  // the event router first.
  EventRouter* event_router = EventRouter::Get(context_);
  if (!event_router)
    return;

  event_router->RegisterObserver(this,
      language_settings_private::OnSpellcheckDictionariesChanged::kEventName);
  event_router->RegisterObserver(this,
      language_settings_private::OnCustomDictionaryChanged::kEventName);

  StartOrStopListeningForSpellcheckChanges();
}

LanguageSettingsPrivateDelegate::~LanguageSettingsPrivateDelegate() {
  DCHECK(!listening_spellcheck_);
}

LanguageSettingsPrivateDelegate* LanguageSettingsPrivateDelegate::Create(
    content::BrowserContext* context) {
  return new LanguageSettingsPrivateDelegate(context);
}

void LanguageSettingsPrivateDelegate::Shutdown() {
  // Unregister with the event router. We first check and see if there *is* an
  // event router, because some unit tests try to shutdown all context services,
  // but didn't initialize the event router first.
  EventRouter* event_router = EventRouter::Get(context_);
  if (event_router)
    event_router->UnregisterObserver(this);

  if (listening_spellcheck_) {
    // TODO(michaelpg): unregister observers.
  }

  listening_spellcheck_ = false;
}

void LanguageSettingsPrivateDelegate::OnListenerAdded(
    const EventListenerInfo& details) {
  // Start listening to spellcheck change events.
  if (details.event_name ==
      language_settings_private::OnSpellcheckDictionariesChanged::kEventName ||
      details.event_name ==
      language_settings_private::OnCustomDictionaryChanged::kEventName) {
    StartOrStopListeningForSpellcheckChanges();
  }
}

void LanguageSettingsPrivateDelegate::OnListenerRemoved(
    const EventListenerInfo& details) {
  // Stop listening to events if there are no more listeners.
  StartOrStopListeningForSpellcheckChanges();
}

void LanguageSettingsPrivateDelegate::
    StartOrStopListeningForSpellcheckChanges() {
  EventRouter* event_router = EventRouter::Get(context_);
  bool should_listen =
      event_router->HasEventListener(language_settings_private::
          OnSpellcheckDictionariesChanged::kEventName) ||
      event_router->HasEventListener(language_settings_private::
          OnCustomDictionaryChanged::kEventName);

  if (should_listen && !listening_spellcheck_) {
    // TODO: register observers.
  } else if (!should_listen && listening_spellcheck_) {
    // TODO(michaelpg): unregister observers.
  }

  listening_spellcheck_ = should_listen;
}

}  // namespace extensions
