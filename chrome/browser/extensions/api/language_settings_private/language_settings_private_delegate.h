// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_DELEGATE_H_

#include "chrome/common/extensions/api/language_settings_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/event_router.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Observes language settings and routes changes as events to listeners of the
// languageSettingsPrivate API.
class LanguageSettingsPrivateDelegate
    : public KeyedService,
      public EventRouter::Observer {
 public:
  static LanguageSettingsPrivateDelegate* Create(
      content::BrowserContext* browser_context);
  ~LanguageSettingsPrivateDelegate() override;

 protected:
  explicit LanguageSettingsPrivateDelegate(content::BrowserContext* context);

  // KeyedService implementation.
  void Shutdown() override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

 private:
  // If there are any JavaScript listeners registered for spellcheck events,
  // ensures we are registered for change notifications. Otherwise, unregisters
  // any observers.
  void StartOrStopListeningForSpellcheckChanges();

  content::BrowserContext* context_;

  // True if there are observers listening for spellcheck events.
  bool listening_spellcheck_;

  DISALLOW_COPY_AND_ASSIGN(LanguageSettingsPrivateDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_DELEGATE_H_
