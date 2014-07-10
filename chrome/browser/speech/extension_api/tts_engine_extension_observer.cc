// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_engine_extension_observer.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/tts_controller.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "grit/browser_resources.h"

// Factory to load one instance of TtsExtensionLoaderChromeOs per profile.
class TtsEngineExtensionObserverFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static TtsEngineExtensionObserver* GetForProfile(Profile* profile) {
    return static_cast<TtsEngineExtensionObserver*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static TtsEngineExtensionObserverFactory* GetInstance() {
    return Singleton<TtsEngineExtensionObserverFactory>::get();
  }

 private:
  friend struct DefaultSingletonTraits<TtsEngineExtensionObserverFactory>;

  TtsEngineExtensionObserverFactory()
      : BrowserContextKeyedServiceFactory(
            "TtsEngineExtensionObserver",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(extensions::ExtensionSystemFactory::GetInstance());
  }

  virtual ~TtsEngineExtensionObserverFactory() {}

  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE {
    // If given an incognito profile (including the Chrome OS login
    // profile), share the service with the original profile.
    return chrome::GetBrowserContextRedirectedInIncognito(context);
  }

  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE {
    return new TtsEngineExtensionObserver(static_cast<Profile*>(profile));
  }
};

TtsEngineExtensionObserver* TtsEngineExtensionObserver::GetInstance(
    Profile* profile) {
  return TtsEngineExtensionObserverFactory::GetInstance()->GetForProfile(
      profile);
}

TtsEngineExtensionObserver::TtsEngineExtensionObserver(Profile* profile)
    : extension_registry_observer_(this),
      profile_(profile),
      saw_tts_engine_added_(false) {
  extension_registry_observer_.Add(
      extensions::ExtensionRegistry::Get(profile_));

  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(profile_);
  DCHECK(system);
  extensions::EventRouter* event_router = system->event_router();
  DCHECK(event_router);
  event_router->RegisterObserver(this, tts_engine_events::kOnSpeak);
  event_router->RegisterObserver(this, tts_engine_events::kOnStop);
}

TtsEngineExtensionObserver::~TtsEngineExtensionObserver() {
}

bool TtsEngineExtensionObserver::SawExtensionLoad(
    const std::string& extension_id,
    bool update) {
  bool previously_loaded =
      engine_extension_ids_.find(extension_id) != engine_extension_ids_.end();

  if (update)
    engine_extension_ids_.insert(extension_id);

  return previously_loaded;
}

void TtsEngineExtensionObserver::Shutdown() {
  extensions::EventRouter::Get(profile_)->UnregisterObserver(this);
}

bool TtsEngineExtensionObserver::IsLoadedTtsEngine(
    const std::string& extension_id) {
  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(profile_);
  DCHECK(system);
  extensions::EventRouter* event_router = system->event_router();
  DCHECK(event_router);
  if (event_router->ExtensionHasEventListener(extension_id,
                                              tts_engine_events::kOnSpeak) &&
      event_router->ExtensionHasEventListener(extension_id,
                                              tts_engine_events::kOnStop)) {
    return true;
  }

  return false;
}

void TtsEngineExtensionObserver::OnListenerAdded(
    const extensions::EventListenerInfo& details) {
  if (!IsLoadedTtsEngine(details.extension_id))
    return;

  if (!saw_tts_engine_added_) {
    saw_tts_engine_added_ = true;
    TtsController::GetInstance()->RetrySpeakingQueuedUtterances();
  }

  TtsController::GetInstance()->VoicesChanged();
  engine_extension_ids_.insert(details.extension_id);
}

void TtsEngineExtensionObserver::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  if (engine_extension_ids_.find(extension->id()) !=
      engine_extension_ids_.end()) {
    engine_extension_ids_.erase(extension->id());
    TtsController::GetInstance()->VoicesChanged();
  }
}
