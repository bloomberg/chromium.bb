// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_constants.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_controller.h"
#include "chrome/common/extensions/extension.h"

namespace constants = tts_extension_api_constants;

namespace events {
const char kOnSpeak[] = "ttsEngine.onSpeak";
const char kOnStop[] = "ttsEngine.onStop";
};  // namespace events

void GetExtensionVoices(Profile* profile, ListValue* result_voices) {
  ExtensionService* service = profile->GetExtensionService();
  DCHECK(service);
  ExtensionEventRouter* event_router = profile->GetExtensionEventRouter();
  DCHECK(event_router);

  const ExtensionSet* extensions = service->extensions();
  ExtensionSet::const_iterator iter;
  for (iter = extensions->begin(); iter != extensions->end(); ++iter) {
    const Extension* extension = *iter;

    if (!event_router->ExtensionHasEventListener(
            extension->id(), events::kOnSpeak) ||
        !event_router->ExtensionHasEventListener(
            extension->id(), events::kOnStop)) {
      continue;
    }

    const std::vector<Extension::TtsVoice>& tts_voices =
        extension->tts_voices();
    for (size_t i = 0; i < tts_voices.size(); ++i) {
      const Extension::TtsVoice& voice = tts_voices[i];
      DictionaryValue* result_voice = new DictionaryValue();
      if (!voice.voice_name.empty())
        result_voice->SetString(constants::kVoiceNameKey, voice.voice_name);
      if (!voice.lang.empty())
        result_voice->SetString(constants::kLangKey, voice.lang);
      if (!voice.gender.empty())
        result_voice->SetString(constants::kGenderKey, voice.gender);
      result_voice->SetString(constants::kExtensionIdKey, extension->id());

      ListValue* event_types = new ListValue();
      for (std::set<std::string>::const_iterator iter =
               voice.event_types.begin();
           iter != voice.event_types.end();
           ++iter) {
        event_types->Append(Value::CreateStringValue(*iter));
      }
      // If the extension sends end events, the controller will handle
      // queueing and send interrupted and cancelled events.
      if (voice.event_types.find(constants::kEventTypeEnd) !=
          voice.event_types.end()) {
        event_types->Append(
            Value::CreateStringValue(constants::kEventTypeCancelled));
        event_types->Append(Value::CreateStringValue(
            constants::kEventTypeInterrupted));
      }

      result_voice->Set(constants::kEventTypesKey, event_types);
      result_voices->Append(result_voice);
    }
  }
}

bool GetMatchingExtensionVoice(
    Utterance* utterance,
    const Extension** matching_extension,
    size_t* voice_index) {
  // This will only happen during unit testing. Otherwise, an utterance
  // will always have an associated profile.
  if (!utterance->profile())
    return false;

  ExtensionService* service = utterance->profile()->GetExtensionService();
  DCHECK(service);
  ExtensionEventRouter* event_router =
      utterance->profile()->GetExtensionEventRouter();
  DCHECK(event_router);

  *matching_extension = NULL;
  *voice_index = -1;
  const ExtensionSet* extensions = service->extensions();
  ExtensionSet::const_iterator iter;
  for (iter = extensions->begin(); iter != extensions->end(); ++iter) {
    const Extension* extension = *iter;

    if (!event_router->ExtensionHasEventListener(
            extension->id(), events::kOnSpeak) ||
        !event_router->ExtensionHasEventListener(
            extension->id(), events::kOnStop)) {
      continue;
    }

    if (!utterance->extension_id().empty() &&
        utterance->extension_id() != extension->id()) {
      continue;
    }

    const std::vector<Extension::TtsVoice>& tts_voices =
        extension->tts_voices();
    for (size_t i = 0; i < tts_voices.size(); ++i) {
      const Extension::TtsVoice& voice = tts_voices[i];
      if (!voice.voice_name.empty() &&
          !utterance->voice_name().empty() &&
          voice.voice_name != utterance->voice_name()) {
        continue;
      }
      if (!voice.lang.empty() &&
          !utterance->lang().empty() &&
          voice.lang != utterance->lang()) {
        continue;
      }
      if (!voice.gender.empty() &&
          !utterance->gender().empty() &&
          voice.gender != utterance->gender()) {
        continue;
      }
      if (utterance->required_event_types().size() > 0) {
        bool has_all_required_event_types = true;
        for (std::set<std::string>::const_iterator iter =
                 utterance->required_event_types().begin();
             iter != utterance->required_event_types().end();
             ++iter) {
          if (voice.event_types.find(*iter) == voice.event_types.end()) {
            has_all_required_event_types = false;
            break;
          }
        }
        if (!has_all_required_event_types)
          continue;
      }

      *matching_extension = extension;
      *voice_index = i;
      return true;
    }
  }

  return false;
}

void ExtensionTtsEngineSpeak(Utterance* utterance,
                             const Extension* extension,
                             size_t voice_index) {
  // See if the engine supports the "end" event; if so, we can keep the
  // utterance around and track it. If not, we're finished with this
  // utterance now.
  const std::set<std::string> event_types =
      extension->tts_voices()[voice_index].event_types;
  bool sends_end_event =
      (event_types.find(constants::kEventTypeEnd) != event_types.end());

  ListValue args;
  args.Set(0, Value::CreateStringValue(utterance->text()));

  // Pass through most options to the speech engine, but remove some
  // that are handled internally.
  DictionaryValue* options = static_cast<DictionaryValue*>(
      utterance->options()->DeepCopy());
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

  args.Set(1, options);
  args.Set(2, Value::CreateIntegerValue(utterance->id()));
  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  utterance->profile()->GetExtensionEventRouter()->DispatchEventToExtension(
      extension->id(),
      events::kOnSpeak,
      json_args,
      utterance->profile(),
      GURL());
}

void ExtensionTtsEngineStop(Utterance* utterance) {
  utterance->profile()->GetExtensionEventRouter()->DispatchEventToExtension(
      utterance->extension_id(),
      events::kOnStop,
      "[]",
      utterance->profile(),
      GURL());
}

bool ExtensionTtsEngineSendTtsEventFunction::RunImpl() {
  int utterance_id;
  std::string error_message;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &utterance_id));

  DictionaryValue* event;
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
  for (size_t i = 0; i < extension->tts_voices().size(); i++) {
    const Extension::TtsVoice& voice = extension->tts_voices()[i];
    if (voice.event_types.find(event_type) != voice.event_types.end()) {
      event_type_allowed = true;
      break;
    }
  }
  if (!event_type_allowed) {
    error_ = constants::kErrorUndeclaredEventType;
    return false;
  }

  ExtensionTtsController* controller = ExtensionTtsController::GetInstance();
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
  } else {
    EXTENSION_FUNCTION_VALIDATE(false);
  }

  return true;
}
