// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_extension_loader_chromeos.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "grit/browser_resources.h"

// Factory to load one instance of TtsExtensionLoaderChromeOs per profile.
class TtsExtensionLoaderChromeOsFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static TtsExtensionLoaderChromeOs* GetForProfile(Profile* profile) {
    return static_cast<TtsExtensionLoaderChromeOs*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static TtsExtensionLoaderChromeOsFactory* GetInstance() {
    return Singleton<TtsExtensionLoaderChromeOsFactory>::get();
  }

 private:
  friend struct DefaultSingletonTraits<TtsExtensionLoaderChromeOsFactory>;

  TtsExtensionLoaderChromeOsFactory() : BrowserContextKeyedServiceFactory(
      "TtsExtensionLoaderChromeOs",
      BrowserContextDependencyManager::GetInstance()) {
    DependsOn(extensions::ExtensionSystemFactory::GetInstance());
  }

  virtual ~TtsExtensionLoaderChromeOsFactory() {}

  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE{
    // If given an incognito profile (including the Chrome OS login
    // profile), share the service with the original profile.
    return chrome::GetBrowserContextRedirectedInIncognito(context);
  }

  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE {
    return new TtsExtensionLoaderChromeOs(static_cast<Profile*>(profile));
  }
};

TtsExtensionLoaderChromeOs*
TtsExtensionLoaderChromeOs::GetInstance(Profile* profile) {
  return TtsExtensionLoaderChromeOsFactory::GetInstance()
      ->GetForProfile(profile);
}

TtsExtensionLoaderChromeOs::TtsExtensionLoaderChromeOs(
    Profile* profile)
    : profile_(profile) {
  tts_state_ = IsTtsLoadedInThisProfile() ? TTS_LOADED : TTS_NOT_LOADED;

  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(profile_);
  DCHECK(system);
  extensions::EventRouter* event_router = system->event_router();
  DCHECK(event_router);
  event_router->RegisterObserver(this, tts_engine_events::kOnSpeak);
  event_router->RegisterObserver(this, tts_engine_events::kOnStop);
}

bool TtsExtensionLoaderChromeOs::LoadTtsExtension() {
  if (tts_state_ == TTS_LOADED || tts_state_ == TTS_LOADING)
    return false;

  // Load the component extension into this profile.
  VLOG(1) << "Loading TTS component extension.";
  tts_state_ = TTS_LOADING;
  ExtensionService* extension_service = profile_->GetExtensionService();
  DCHECK(extension_service);
  extension_service->component_loader()->AddChromeOsSpeechSynthesisExtension();
  return true;
}

void TtsExtensionLoaderChromeOs::Shutdown() {
  extensions::ExtensionSystem::Get(profile_)->
      event_router()->UnregisterObserver(this);
}

bool TtsExtensionLoaderChromeOs::IsTtsLoadedInThisProfile() {
  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(profile_);
  DCHECK(system);
  extensions::EventRouter* event_router = system->event_router();
  DCHECK(event_router);
  if (event_router->ExtensionHasEventListener(
          extension_misc::kSpeechSynthesisExtensionId,
          tts_engine_events::kOnSpeak) &&
      event_router->ExtensionHasEventListener(
          extension_misc::kSpeechSynthesisExtensionId,
          tts_engine_events::kOnStop)) {
    return true;
  }

  return false;
}

void TtsExtensionLoaderChromeOs::OnListenerAdded(
    const extensions::EventListenerInfo& details) {
  if (details.extension_id != extension_misc::kSpeechSynthesisExtensionId)
    return;

  if (!IsTtsLoadedInThisProfile())
    return;

  if (tts_state_ == TTS_LOADING) {
    VLOG(1) << "TTS component extension loaded, retrying queued utterances.";
    tts_state_ = TTS_LOADED;
    TtsController::GetInstance()->RetrySpeakingQueuedUtterances();
  }
}
