// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_constants.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/common/extensions/api/speech/tts_engine_manifest_handler.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/console_message_level.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "net/base/network_change_notifier.h"

using extensions::EventRouter;
using extensions::Extension;
using extensions::ExtensionSystem;

namespace constants = tts_extension_api_constants;

namespace tts_engine_events {
const char kOnSpeak[] = "ttsEngine.onSpeak";
const char kOnStop[] = "ttsEngine.onStop";
const char kOnPause[] = "ttsEngine.onPause";
const char kOnResume[] = "ttsEngine.onResume";
};  // namespace tts_engine_events

namespace {

void WarnIfMissingPauseOrResumeListener(
    Profile* profile, EventRouter* event_router, std::string extension_id) {
  bool has_onpause = event_router->ExtensionHasEventListener(
      extension_id, tts_engine_events::kOnPause);
  bool has_onresume = event_router->ExtensionHasEventListener(
      extension_id, tts_engine_events::kOnResume);
  if (has_onpause == has_onresume)
    return;

  extensions::ProcessManager* process_manager =
      ExtensionSystem::Get(profile)->process_manager();
  extensions::ExtensionHost* host =
      process_manager->GetBackgroundHostForExtension(extension_id);
  host->render_process_host()->Send(new ExtensionMsg_AddMessageToConsole(
      host->render_view_host()->GetRoutingID(),
      content::CONSOLE_MESSAGE_LEVEL_WARNING,
      constants::kErrorMissingPauseOrResume));
}

}  // namespace

TtsExtensionEngine* TtsExtensionEngine::GetInstance() {
  return Singleton<TtsExtensionEngine>::get();
}

void TtsExtensionEngine::GetVoices(Profile* profile,
    std::vector<VoiceData>* out_voices) {
  EventRouter* event_router = EventRouter::Get(profile);
  DCHECK(event_router);

  bool is_offline = (net::NetworkChangeNotifier::GetConnectionType() ==
                     net::NetworkChangeNotifier::CONNECTION_NONE);

  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  extensions::ExtensionSet::const_iterator iter;
  for (iter = extensions.begin(); iter != extensions.end(); ++iter) {
    const Extension* extension = iter->get();

    if (!event_router->ExtensionHasEventListener(
            extension->id(), tts_engine_events::kOnSpeak) ||
        !event_router->ExtensionHasEventListener(
            extension->id(), tts_engine_events::kOnStop)) {
      continue;
    }

    const std::vector<extensions::TtsVoice>* tts_voices =
        extensions::TtsVoice::GetTtsVoices(extension);
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
      if (voice.gender == constants::kGenderMale)
        result_voice.gender = TTS_GENDER_MALE;
      else if (voice.gender == constants::kGenderFemale)
        result_voice.gender = TTS_GENDER_FEMALE;
      else
        result_voice.gender = TTS_GENDER_NONE;

      for (std::set<std::string>::const_iterator iter =
               voice.event_types.begin();
           iter != voice.event_types.end();
           ++iter) {
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

void TtsExtensionEngine::Speak(Utterance* utterance,
                               const VoiceData& voice) {
  // See if the engine supports the "end" event; if so, we can keep the
  // utterance around and track it. If not, we're finished with this
  // utterance now.
  bool sends_end_event = voice.events.find(TTS_EVENT_END) != voice.events.end();

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->AppendString(utterance->text());

  // Pass through most options to the speech engine, but remove some
  // that are handled internally.
  scoped_ptr<base::DictionaryValue> options(static_cast<base::DictionaryValue*>(
      utterance->options()->DeepCopy()));
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

  // Add the voice name and language to the options if they're not
  // already there, since they might have been picked by the TTS controller
  // rather than directly by the client that requested the speech.
  if (!options->HasKey(constants::kVoiceNameKey))
    options->SetString(constants::kVoiceNameKey, voice.name);
  if (!options->HasKey(constants::kLangKey))
    options->SetString(constants::kLangKey, voice.lang);

  args->Append(options.release());
  args->AppendInteger(utterance->id());

  scoped_ptr<extensions::Event> event(new extensions::Event(
      tts_engine_events::kOnSpeak, args.Pass()));
  event->restrict_to_browser_context = utterance->profile();
  EventRouter::Get(utterance->profile())
      ->DispatchEventToExtension(utterance->extension_id(), event.Pass());
}

void TtsExtensionEngine::Stop(Utterance* utterance) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  scoped_ptr<extensions::Event> event(new extensions::Event(
      tts_engine_events::kOnStop, args.Pass()));
  event->restrict_to_browser_context = utterance->profile();
  EventRouter::Get(utterance->profile())
      ->DispatchEventToExtension(utterance->extension_id(), event.Pass());
}

void TtsExtensionEngine::Pause(Utterance* utterance) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  scoped_ptr<extensions::Event> event(new extensions::Event(
      tts_engine_events::kOnPause, args.Pass()));
  Profile* profile = utterance->profile();
  event->restrict_to_browser_context = profile;
  EventRouter* event_router = EventRouter::Get(profile);
  std::string id = utterance->extension_id();
  event_router->DispatchEventToExtension(id, event.Pass());
  WarnIfMissingPauseOrResumeListener(profile, event_router, id);
}

void TtsExtensionEngine::Resume(Utterance* utterance) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  scoped_ptr<extensions::Event> event(new extensions::Event(
      tts_engine_events::kOnResume, args.Pass()));
  Profile* profile = utterance->profile();
  event->restrict_to_browser_context = profile;
  EventRouter* event_router = EventRouter::Get(profile);
  std::string id = utterance->extension_id();
  event_router->DispatchEventToExtension(id, event.Pass());
  WarnIfMissingPauseOrResumeListener(profile, event_router, id);
}

bool ExtensionTtsEngineSendTtsEventFunction::RunSync() {
  int utterance_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &utterance_id));

  base::DictionaryValue* event;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &event));

  std::string event_type;
  EXTENSION_FUNCTION_VALIDATE(
      event->GetString(constants::kEventTypeKey, &event_type));

  int char_index = 0;
  if (event->HasKey(constants::kCharIndexKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        event->GetInteger(constants::kCharIndexKey, &char_index));
  }

  // Make sure the extension has included this event type in its manifest.
  bool event_type_allowed = false;
  const Extension* extension = GetExtension();
  const std::vector<extensions::TtsVoice>* tts_voices =
      extensions::TtsVoice::GetTtsVoices(extension);
  if (!tts_voices) {
    error_ = constants::kErrorUndeclaredEventType;
    return false;
  }

  for (size_t i = 0; i < tts_voices->size(); i++) {
    const extensions::TtsVoice& voice = tts_voices->at(i);
    if (voice.event_types.find(event_type) != voice.event_types.end()) {
      event_type_allowed = true;
      break;
    }
  }
  if (!event_type_allowed) {
    error_ = constants::kErrorUndeclaredEventType;
    return false;
  }

  TtsController* controller = TtsController::GetInstance();
  if (event_type == constants::kEventTypeStart) {
    controller->OnTtsEvent(
        utterance_id, TTS_EVENT_START, char_index, std::string());
  } else if (event_type == constants::kEventTypeEnd) {
    controller->OnTtsEvent(
        utterance_id, TTS_EVENT_END, char_index, std::string());
  } else if (event_type == constants::kEventTypeWord) {
    controller->OnTtsEvent(
        utterance_id, TTS_EVENT_WORD, char_index, std::string());
  } else if (event_type == constants::kEventTypeSentence) {
    controller->OnTtsEvent(
        utterance_id, TTS_EVENT_SENTENCE, char_index, std::string());
  } else if (event_type == constants::kEventTypeMarker) {
    controller->OnTtsEvent(
        utterance_id, TTS_EVENT_MARKER, char_index, std::string());
  } else if (event_type == constants::kEventTypeError) {
    std::string error_message;
    event->GetString(constants::kErrorMessageKey, &error_message);
    controller->OnTtsEvent(
        utterance_id, TTS_EVENT_ERROR, char_index, error_message);
  } else if (event_type == constants::kEventTypePause) {
    controller->OnTtsEvent(
        utterance_id, TTS_EVENT_PAUSE, char_index, std::string());
  } else if (event_type == constants::kEventTypeResume) {
    controller->OnTtsEvent(
        utterance_id, TTS_EVENT_RESUME, char_index, std::string());
  } else {
    EXTENSION_FUNCTION_VALIDATE(false);
  }

  return true;
}
