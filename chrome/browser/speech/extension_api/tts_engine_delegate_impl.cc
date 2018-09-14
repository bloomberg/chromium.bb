// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_engine_delegate_impl.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_delegate_impl.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer.h"
#include "chrome/browser/speech/extension_api/tts_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_constants.h"
#include "chrome/common/extensions/api/speech/tts_engine_manifest_handler.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/common/console_message_level.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "net/base/network_change_notifier.h"

using extensions::EventRouter;
using extensions::Extension;
using extensions::ExtensionSystem;

namespace constants = tts_extension_api_constants;

namespace {

void WarnIfMissingPauseOrResumeListener(Profile* profile,
                                        EventRouter* event_router,
                                        std::string extension_id) {
  bool has_onpause = event_router->ExtensionHasEventListener(
      extension_id, tts_engine_events::kOnPause);
  bool has_onresume = event_router->ExtensionHasEventListener(
      extension_id, tts_engine_events::kOnResume);
  if (has_onpause == has_onresume)
    return;

  extensions::ExtensionHost* host =
      extensions::ProcessManager::Get(profile)->GetBackgroundHostForExtension(
          extension_id);
  host->host_contents()->GetMainFrame()->AddMessageToConsole(
      content::CONSOLE_MESSAGE_LEVEL_WARNING,
      constants::kErrorMissingPauseOrResume);
}

}  // namespace

TtsEngineDelegateImpl::TtsEngineDelegateImpl(
    content::BrowserContext* browser_context)
#if defined(OS_CHROMEOS)
    : browser_context_(browser_context)
#endif  // OS_CHROMEOS
{
}

void TtsEngineDelegateImpl::GetVoices(content::BrowserContext* browser_context,
                                      std::vector<VoiceData>* out_voices) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  EventRouter* event_router = EventRouter::Get(profile);
  DCHECK(event_router);

  bool is_offline = (net::NetworkChangeNotifier::GetConnectionType() ==
                     net::NetworkChangeNotifier::CONNECTION_NONE);

  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  extensions::ExtensionSet::const_iterator iter;
  for (iter = extensions.begin(); iter != extensions.end(); ++iter) {
    const Extension* extension = iter->get();

    if (!event_router->ExtensionHasEventListener(extension->id(),
                                                 tts_engine_events::kOnSpeak) ||
        !event_router->ExtensionHasEventListener(extension->id(),
                                                 tts_engine_events::kOnStop)) {
      continue;
    }

    const std::vector<extensions::TtsVoice>* tts_voices =
        TtsEngineExtensionObserver::GetInstance(profile)->GetAllVoices(
            extension);
    if (!tts_voices)
      continue;

    for (size_t i = 0; i < tts_voices->size(); ++i) {
      const extensions::TtsVoice& voice = tts_voices->at(i);

      // Don't return remote voices when the system is offline.
      if (voice.remote && is_offline)
        continue;

      out_voices->push_back(VoiceData());
      VoiceData& result_voice = out_voices->back();

      result_voice.native = false;
      result_voice.name = voice.voice_name;
      result_voice.lang = voice.lang;
      result_voice.remote = voice.remote;
      result_voice.extension_id = extension->id();

      for (std::set<std::string>::const_iterator iter =
               voice.event_types.begin();
           iter != voice.event_types.end(); ++iter) {
        result_voice.events.insert(TtsEventTypeFromString(*iter));
      }

      // If the extension sends end events, the controller will handle
      // queueing and send interrupted and cancelled events.
      if (voice.event_types.find(constants::kEventTypeEnd) !=
          voice.event_types.end()) {
        result_voice.events.insert(TTS_EVENT_CANCELLED);
        result_voice.events.insert(TTS_EVENT_INTERRUPTED);
      }
    }
  }
}

void TtsEngineDelegateImpl::Speak(Utterance* utterance,
                                  const VoiceData& voice) {
  // See if the engine supports the "end" event; if so, we can keep the
  // utterance around and track it. If not, we're finished with this
  // utterance now.
  bool sends_end_event = voice.events.find(TTS_EVENT_END) != voice.events.end();

  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->AppendString(utterance->text());

  // Pass through most options to the speech engine, but remove some
  // that are handled internally.
  std::unique_ptr<base::DictionaryValue> options(
      static_cast<base::DictionaryValue*>(utterance->options()->DeepCopy()));
  if (options->HasKey(constants::kRequiredEventTypesKey))
    options->Remove(constants::kRequiredEventTypesKey, NULL);
  if (options->HasKey(constants::kDesiredEventTypesKey))
    options->Remove(constants::kDesiredEventTypesKey, NULL);
  if (sends_end_event && options->HasKey(constants::kEnqueueKey))
    options->Remove(constants::kEnqueueKey, NULL);
  if (options->HasKey(constants::kSrcIdKey))
    options->Remove(constants::kSrcIdKey, NULL);
  if (options->HasKey(constants::kIsFinalEventKey))
    options->Remove(constants::kIsFinalEventKey, NULL);
  if (options->HasKey(constants::kOnEventKey))
    options->Remove(constants::kOnEventKey, NULL);

  // Get the volume, pitch, and rate, but only if they weren't already in
  // the options. TODO(dmazzoni): these shouldn't be redundant.
  // http://crbug.com/463264
  if (!options->HasKey(constants::kRateKey)) {
    options->SetDouble(constants::kRateKey,
                       utterance->continuous_parameters().rate);
  }
  if (!options->HasKey(constants::kPitchKey)) {
    options->SetDouble(constants::kPitchKey,
                       utterance->continuous_parameters().pitch);
  }
  if (!options->HasKey(constants::kVolumeKey)) {
    options->SetDouble(constants::kVolumeKey,
                       utterance->continuous_parameters().volume);
  }

  // Add the voice name and language to the options if they're not
  // already there, since they might have been picked by the TTS controller
  // rather than directly by the client that requested the speech.
  if (!options->HasKey(constants::kVoiceNameKey))
    options->SetString(constants::kVoiceNameKey, voice.name);
  if (!options->HasKey(constants::kLangKey))
    options->SetString(constants::kLangKey, voice.lang);

  args->Append(std::move(options));
  args->AppendInteger(utterance->id());

  std::string json;
  base::JSONWriter::Write(*args, &json);

  Profile* profile = Profile::FromBrowserContext(utterance->browser_context());
  auto event = std::make_unique<extensions::Event>(
      extensions::events::TTS_ENGINE_ON_SPEAK, tts_engine_events::kOnSpeak,
      std::move(args), profile);
  EventRouter::Get(profile)->DispatchEventToExtension(utterance->extension_id(),
                                                      std::move(event));
}

void TtsEngineDelegateImpl::Stop(Utterance* utterance) {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  Profile* profile = Profile::FromBrowserContext(utterance->browser_context());
  auto event = std::make_unique<extensions::Event>(
      extensions::events::TTS_ENGINE_ON_STOP, tts_engine_events::kOnStop,
      std::move(args), profile);
  EventRouter::Get(profile)->DispatchEventToExtension(utterance->extension_id(),
                                                      std::move(event));
}

void TtsEngineDelegateImpl::Pause(Utterance* utterance) {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  Profile* profile = Profile::FromBrowserContext(utterance->browser_context());
  auto event = std::make_unique<extensions::Event>(
      extensions::events::TTS_ENGINE_ON_PAUSE, tts_engine_events::kOnPause,
      std::move(args), profile);
  EventRouter* event_router = EventRouter::Get(profile);
  std::string id = utterance->extension_id();
  event_router->DispatchEventToExtension(id, std::move(event));
  WarnIfMissingPauseOrResumeListener(profile, event_router, id);
}

void TtsEngineDelegateImpl::Resume(Utterance* utterance) {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  Profile* profile = Profile::FromBrowserContext(utterance->browser_context());
  auto event = std::make_unique<extensions::Event>(
      extensions::events::TTS_ENGINE_ON_RESUME, tts_engine_events::kOnResume,
      std::move(args), profile);
  EventRouter* event_router = EventRouter::Get(profile);
  std::string id = utterance->extension_id();
  event_router->DispatchEventToExtension(id, std::move(event));
  WarnIfMissingPauseOrResumeListener(profile, event_router, id);
}

bool TtsEngineDelegateImpl::LoadBuiltInTtsExtension() {
#if defined(OS_CHROMEOS)
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  // Check to see if the engine was previously loaded.
  if (TtsEngineExtensionObserver::GetInstance(profile)->SawExtensionLoad(
          extension_misc::kSpeechSynthesisExtensionId, true)) {
    return false;
  }

  // Load the component extension into this profile.
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  DCHECK(extension_service);
  extension_service->component_loader()->AddChromeOsSpeechSynthesisExtension();
  return true;
#else
  return false;
#endif
}

void TtsEngineDelegateImpl::Shutdown() {}
