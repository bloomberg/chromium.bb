// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_delegate_impl.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer.h"
#include "chrome/browser/speech/extension_api/tts_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_constants.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/common/extensions/api/speech/tts_engine_manifest_handler.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::Extension;

namespace constants = tts_extension_api_constants;

namespace tts_engine_events {
const char kOnSpeak[] = "ttsEngine.onSpeak";
const char kOnStop[] = "ttsEngine.onStop";
const char kOnPause[] = "ttsEngine.onPause";
const char kOnResume[] = "ttsEngine.onResume";
};  // namespace tts_engine_events

ExtensionFunction::ResponseAction
ExtensionTtsEngineUpdateVoicesFunction::Run() {
  base::ListValue* voices_data = nullptr;
  EXTENSION_FUNCTION_VALIDATE(args_->GetList(0, &voices_data));
  auto tts_voices = std::make_unique<extensions::TtsVoices>();
  const char* error = nullptr;
  for (size_t i = 0; i < voices_data->GetSize(); i++) {
    extensions::TtsVoice voice;
    base::DictionaryValue* voice_data = nullptr;
    voices_data->GetDictionary(i, &voice_data);

    // Note partial validation of these attributes occurs based on tts engine's
    // json schema (e.g. for data type matching). The missing checks follow
    // similar checks in manifest parsing.
    if (voice_data->HasKey(constants::kVoiceNameKey))
      voice_data->GetString(constants::kVoiceNameKey, &voice.voice_name);
    if (voice_data->HasKey(constants::kLangKey)) {
      voice_data->GetString(constants::kLangKey, &voice.lang);
      if (!l10n_util::IsValidLocaleSyntax(voice.lang)) {
        error = constants::kErrorInvalidLang;
        continue;
      }
    }
    if (voice_data->HasKey(constants::kRemoteKey))
      voice_data->GetBoolean(constants::kRemoteKey, &voice.remote);
    if (voice_data->HasKey(constants::kExtensionIdKey)) {
      // Allow this for clients who might have used |chrome.tts.getVoices| to
      // update existing voices. However, trying to update the voice of another
      // extension should trigger an error.
      std::string extension_id;
      voice_data->GetString(constants::kExtensionIdKey, &extension_id);
      if (extension()->id() != extension_id) {
        error = constants::kErrorExtensionIdMismatch;
        continue;
      }
    }
    base::ListValue* event_types = nullptr;
    if (voice_data->HasKey(constants::kEventTypesKey))
      voice_data->GetList(constants::kEventTypesKey, &event_types);

    if (event_types) {
      for (size_t j = 0; j < event_types->GetSize(); j++) {
        std::string event_type;
        event_types->GetString(j, &event_type);
        voice.event_types.insert(event_type);
      }
    }

    tts_voices->voices.push_back(voice);
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  TtsEngineExtensionObserver::GetInstance(profile)->SetRuntimeVoices(
      std::move(tts_voices), extension()->id());

  if (error)
    return RespondNow(Error(error));

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionTtsEngineSendTtsEventFunction::Run() {
  int utterance_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &utterance_id));

  base::DictionaryValue* event = nullptr;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &event));

  std::string event_type;
  EXTENSION_FUNCTION_VALIDATE(
      event->GetString(constants::kEventTypeKey, &event_type));

  int char_index = 0;
  if (event->HasKey(constants::kCharIndexKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        event->GetInteger(constants::kCharIndexKey, &char_index));
  }

  // Make sure the extension has included this event type in its manifest
  // or in the runtime list of voices exposed.
  bool event_type_allowed = false;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  const std::vector<extensions::TtsVoice>* tts_voices =
      TtsEngineExtensionObserver::GetInstance(profile)->GetAllVoices(
          extension());
  if (!tts_voices)
    return RespondNow(Error(constants::kErrorUndeclaredEventType));

  for (size_t i = 0; i < tts_voices->size(); i++) {
    const extensions::TtsVoice& voice = tts_voices->at(i);
    if (voice.event_types.find(event_type) != voice.event_types.end()) {
      event_type_allowed = true;
      break;
    }
  }
  if (!event_type_allowed)
    return RespondNow(Error(constants::kErrorUndeclaredEventType));

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

  return RespondNow(NoArguments());
}
