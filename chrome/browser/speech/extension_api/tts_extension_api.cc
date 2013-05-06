// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_extension_api.h"

#include <string>

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_constants.h"
#include "chrome/browser/speech/tts_controller.h"
#include "ui/base/l10n/l10n_util.h"

namespace constants = tts_extension_api_constants;

namespace events {
const char kOnEvent[] = "tts.onEvent";
};  // namespace events

namespace {

const char *TtsEventTypeToString(TtsEventType event_type) {
  switch (event_type) {
  case TTS_EVENT_START:
    return constants::kEventTypeStart;
  case TTS_EVENT_END:
    return constants::kEventTypeEnd;
  case TTS_EVENT_WORD:
    return constants::kEventTypeWord;
  case TTS_EVENT_SENTENCE:
    return constants::kEventTypeSentence;
  case TTS_EVENT_MARKER:
    return constants::kEventTypeMarker;
  case TTS_EVENT_INTERRUPTED:
    return constants::kEventTypeInterrupted;
  case TTS_EVENT_CANCELLED:
    return constants::kEventTypeCancelled;
  case TTS_EVENT_ERROR:
    return constants::kEventTypeError;
  default:
    NOTREACHED();
    return "";
  }
}

}  // anonymous namespace

namespace extensions {

// One of these is constructed for each utterance, and deleted
// when the utterance gets any final event.
class TtsExtensionEventHandler : public UtteranceEventDelegate {
 public:
  virtual void OnTtsEvent(Utterance* utterance,
                          TtsEventType event_type,
                          int char_index,
                          const std::string& error_message) OVERRIDE;
};

void TtsExtensionEventHandler::OnTtsEvent(Utterance* utterance,
                                          TtsEventType event_type,
                                          int char_index,
                                          const std::string& error_message) {
  if (utterance->src_id() < 0)
    return;

  std::string event_type_string = TtsEventTypeToString(event_type);
  const std::set<std::string>& desired_event_types =
      utterance->desired_event_types();
  if (desired_event_types.size() > 0 &&
      desired_event_types.find(event_type_string) ==
      desired_event_types.end()) {
    return;
  }

  scoped_ptr<DictionaryValue> details(new DictionaryValue());
  if (char_index >= 0)
    details->SetInteger(constants::kCharIndexKey, char_index);
  details->SetString(constants::kEventTypeKey, event_type_string);
  if (event_type == TTS_EVENT_ERROR) {
    details->SetString(constants::kErrorMessageKey, error_message);
  }
  details->SetInteger(constants::kSrcIdKey, utterance->src_id());
  details->SetBoolean(constants::kIsFinalEventKey, utterance->finished());

  scoped_ptr<ListValue> arguments(new ListValue());
  arguments->Set(0, details.release());

  scoped_ptr<extensions::Event> event(
      new extensions::Event(events::kOnEvent, arguments.Pass()));
  event->restrict_to_profile = utterance->profile();
  event->event_url = utterance->src_url();
  extensions::ExtensionSystem::Get(utterance->profile())->event_router()->
      DispatchEventToExtension(utterance->src_extension_id(), event.Pass());

  if (utterance->finished())
    delete this;
}


bool TtsSpeakFunction::RunImpl() {
  std::string text;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &text));
  if (text.size() > 32768) {
    error_ = constants::kErrorUtteranceTooLong;
    return false;
  }

  scoped_ptr<DictionaryValue> options(new DictionaryValue());
  if (args_->GetSize() >= 2) {
    DictionaryValue* temp_options = NULL;
    if (args_->GetDictionary(1, &temp_options))
      options.reset(temp_options->DeepCopy());
  }

  std::string voice_name;
  if (options->HasKey(constants::kVoiceNameKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        options->GetString(constants::kVoiceNameKey, &voice_name));
  }

  std::string lang;
  if (options->HasKey(constants::kLangKey))
    EXTENSION_FUNCTION_VALIDATE(options->GetString(constants::kLangKey, &lang));
  if (!lang.empty() && !l10n_util::IsValidLocaleSyntax(lang)) {
    error_ = constants::kErrorInvalidLang;
    return false;
  }

  std::string gender;
  if (options->HasKey(constants::kGenderKey))
    EXTENSION_FUNCTION_VALIDATE(
        options->GetString(constants::kGenderKey, &gender));
  if (!gender.empty() &&
      gender != constants::kGenderFemale &&
      gender != constants::kGenderMale) {
    error_ = constants::kErrorInvalidGender;
    return false;
  }

  double rate = 1.0;
  if (options->HasKey(constants::kRateKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        options->GetDouble(constants::kRateKey, &rate));
    if (rate < 0.1 || rate > 10.0) {
      error_ = constants::kErrorInvalidRate;
      return false;
    }
  }

  double pitch = 1.0;
  if (options->HasKey(constants::kPitchKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        options->GetDouble(constants::kPitchKey, &pitch));
    if (pitch < 0.0 || pitch > 2.0) {
      error_ = constants::kErrorInvalidPitch;
      return false;
    }
  }

  double volume = 1.0;
  if (options->HasKey(constants::kVolumeKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        options->GetDouble(constants::kVolumeKey, &volume));
    if (volume < 0.0 || volume > 1.0) {
      error_ = constants::kErrorInvalidVolume;
      return false;
    }
  }

  bool can_enqueue = false;
  if (options->HasKey(constants::kEnqueueKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        options->GetBoolean(constants::kEnqueueKey, &can_enqueue));
  }

  std::set<std::string> required_event_types;
  if (options->HasKey(constants::kRequiredEventTypesKey)) {
    ListValue* list;
    EXTENSION_FUNCTION_VALIDATE(
        options->GetList(constants::kRequiredEventTypesKey, &list));
    for (size_t i = 0; i < list->GetSize(); ++i) {
      std::string event_type;
      if (!list->GetString(i, &event_type))
        required_event_types.insert(event_type);
    }
  }

  std::set<std::string> desired_event_types;
  if (options->HasKey(constants::kDesiredEventTypesKey)) {
    ListValue* list;
    EXTENSION_FUNCTION_VALIDATE(
        options->GetList(constants::kDesiredEventTypesKey, &list));
    for (size_t i = 0; i < list->GetSize(); ++i) {
      std::string event_type;
      if (!list->GetString(i, &event_type))
        desired_event_types.insert(event_type);
    }
  }

  std::string voice_extension_id;
  if (options->HasKey(constants::kExtensionIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        options->GetString(constants::kExtensionIdKey, &voice_extension_id));
  }

  int src_id = -1;
  if (options->HasKey(constants::kSrcIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        options->GetInteger(constants::kSrcIdKey, &src_id));
  }

  // If we got this far, the arguments were all in the valid format, so
  // send the success response to the callback now - this ensures that
  // the callback response always arrives before events, which makes
  // the behavior more predictable and easier to write unit tests for too.
  SendResponse(true);

  UtteranceContinuousParameters continuous_params;
  continuous_params.rate = rate;
  continuous_params.pitch = pitch;
  continuous_params.volume = volume;

  Utterance* utterance = new Utterance(profile());
  utterance->set_text(text);
  utterance->set_voice_name(voice_name);
  utterance->set_src_extension_id(extension_id());
  utterance->set_src_id(src_id);
  utterance->set_src_url(source_url());
  utterance->set_lang(lang);
  utterance->set_gender(gender);
  utterance->set_continuous_parameters(continuous_params);
  utterance->set_can_enqueue(can_enqueue);
  utterance->set_required_event_types(required_event_types);
  utterance->set_desired_event_types(desired_event_types);
  utterance->set_extension_id(voice_extension_id);
  utterance->set_options(options.get());
  utterance->set_event_delegate(new TtsExtensionEventHandler());

  TtsController* controller = TtsController::GetInstance();
  controller->SpeakOrEnqueue(utterance);
  return true;
}

bool TtsStopSpeakingFunction::RunImpl() {
  TtsController::GetInstance()->Stop();
  return true;
}

bool TtsIsSpeakingFunction::RunImpl() {
  SetResult(Value::CreateBooleanValue(
      TtsController::GetInstance()->IsSpeaking()));
  return true;
}

bool TtsGetVoicesFunction::RunImpl() {
  std::vector<VoiceData> voices;
  TtsController::GetInstance()->GetVoices(profile(), &voices);

  scoped_ptr<ListValue> result_voices(new ListValue());
  for (size_t i = 0; i < voices.size(); ++i) {
    const VoiceData& voice = voices[i];
    DictionaryValue* result_voice = new DictionaryValue();
    result_voice->SetString(constants::kVoiceNameKey, voice.name);
    if (!voice.lang.empty())
      result_voice->SetString(constants::kLangKey, voice.lang);
    if (!voice.gender.empty())
      result_voice->SetString(constants::kGenderKey, voice.gender);
    if (!voice.extension_id.empty())
      result_voice->SetString(constants::kExtensionIdKey, voice.extension_id);

    ListValue* event_types = new ListValue();
    for (size_t j = 0; j < voice.events.size(); ++j)
      event_types->Append(Value::CreateStringValue(voice.events[j]));
    result_voice->Set(constants::kEventTypesKey, event_types);

    result_voices->Append(result_voice);
  }

  SetResult(result_voices.release());
  return true;
}

// static
TtsAPI* TtsAPI::Get(Profile* profile) {
  return ProfileKeyedAPIFactory<TtsAPI>::GetForProfile(profile);
}

TtsAPI::TtsAPI(Profile* profile) {
  ExtensionFunctionRegistry* registry =
      ExtensionFunctionRegistry::GetInstance();
  registry->RegisterFunction<ExtensionTtsEngineSendTtsEventFunction>();
  registry->RegisterFunction<TtsGetVoicesFunction>();
  registry->RegisterFunction<TtsIsSpeakingFunction>();
  registry->RegisterFunction<TtsSpeakFunction>();
  registry->RegisterFunction<TtsStopSpeakingFunction>();
}

TtsAPI::~TtsAPI() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<TtsAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

ProfileKeyedAPIFactory<TtsAPI>* TtsAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
