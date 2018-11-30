// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_controller_delegate_impl.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/tts_controller.h"
#include "third_party/blink/public/platform/web_speech_synthesis_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// A value to be used to indicate that there is no char index available.
const int kInvalidCharIndex = -1;

#if defined(OS_CHROMEOS)
bool VoiceIdMatches(const std::string& voice_id,
                    const content::VoiceData& voice) {
  if (voice_id.empty() || voice.name.empty() ||
      (voice.extension_id.empty() && !voice.native))
    return false;
  std::unique_ptr<base::DictionaryValue> json =
      base::DictionaryValue::From(base::JSONReader::Read(voice_id));
  std::string default_name;
  std::string default_extension_id;
  json->GetString("name", &default_name);
  json->GetString("extension", &default_extension_id);
  if (voice.native)
    return default_name == voice.name && default_extension_id.empty();
  return default_name == voice.name &&
         default_extension_id == voice.extension_id;
}
#endif  // defined(OS_CHROMEOS)

// IMPORTANT!
// These values are written to logs.  Do not renumber or delete
// existing items; add new entries to the end of the list.
enum class UMATextToSpeechEvent {
  START = 0,
  END = 1,
  WORD = 2,
  SENTENCE = 3,
  MARKER = 4,
  INTERRUPTED = 5,
  CANCELLED = 6,
  SPEECH_ERROR = 7,
  PAUSE = 8,
  RESUME = 9,

  // This must always be the last enum. It's okay for its value to
  // increase, but none of the other enum values may change.
  COUNT
};

}  // namespace

//
// TtsControllerDelegateImpl
//

// static
TtsControllerDelegateImpl* TtsControllerDelegateImpl::GetInstance() {
  return base::Singleton<TtsControllerDelegateImpl>::get();
}

TtsControllerDelegateImpl::TtsControllerDelegateImpl()
    : current_utterance_(nullptr),
      paused_(false),
      tts_platform_(nullptr),
      tts_engine_delegate_(nullptr) {}

TtsControllerDelegateImpl::~TtsControllerDelegateImpl() {
  if (current_utterance_) {
    current_utterance_->Finish();
    delete current_utterance_;
  }

  // Clear any queued utterances too.
  ClearUtteranceQueue(false);  // Don't sent events.
}

void TtsControllerDelegateImpl::SpeakOrEnqueue(content::Utterance* utterance) {
  // If we're paused and we get an utterance that can't be queued,
  // flush the queue but stay in the paused state.
  if (paused_ && !utterance->can_enqueue()) {
    utterance_queue_.push(utterance);
    Stop();
    paused_ = true;
    return;
  }

  if (paused_ || (IsSpeaking() && utterance->can_enqueue())) {
    utterance_queue_.push(utterance);
  } else {
    Stop();
    SpeakNow(utterance);
  }
}

void TtsControllerDelegateImpl::SpeakNow(content::Utterance* utterance) {
  // Ensure we have all built-in voices loaded. This is a no-op if already
  // loaded.
  bool loaded_built_in =
      GetTtsPlatform()->LoadBuiltInTtsExtension(utterance->browser_context());

  // Get all available voices and try to find a matching voice.
  std::vector<content::VoiceData> voices;
  GetVoices(utterance->browser_context(), &voices);

  // Get the best matching voice. If nothing matches, just set "native"
  // to true because that might trigger deferred loading of native voices.
  int index = GetMatchingVoice(utterance, voices);
  content::VoiceData voice;
  if (index >= 0)
    voice = voices[index];
  else
    voice.native = true;

  UpdateUtteranceDefaults(utterance);

  GetTtsPlatform()->WillSpeakUtteranceWithVoice(utterance, voice);

  base::RecordAction(base::UserMetricsAction("TextToSpeech.Speak"));
  UMA_HISTOGRAM_COUNTS_100000("TextToSpeech.Utterance.TextLength",
                              utterance->text().size());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.FromExtensionAPI",
                        !utterance->src_url().is_empty());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasVoiceName",
                        !utterance->voice_name().empty());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasLang",
                        !utterance->lang().empty());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasRate",
                        utterance->continuous_parameters().rate != 1.0);
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasPitch",
                        utterance->continuous_parameters().pitch != 1.0);
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasVolume",
                        utterance->continuous_parameters().volume != 1.0);
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.Native", voice.native);

  if (!voice.native) {
#if !defined(OS_ANDROID)
    DCHECK(!voice.extension_id.empty());
    current_utterance_ = utterance;
    utterance->set_extension_id(voice.extension_id);
    if (tts_engine_delegate_)
      tts_engine_delegate_->Speak(utterance, voice);
    bool sends_end_event =
        voice.events.find(content::TTS_EVENT_END) != voice.events.end();
    if (!sends_end_event) {
      utterance->Finish();
      delete utterance;
      current_utterance_ = nullptr;
      SpeakNextUtterance();
    }
#endif
  } else {
    // It's possible for certain platforms to send start events immediately
    // during |speak|.
    current_utterance_ = utterance;
    GetTtsPlatform()->ClearError();
    bool success = GetTtsPlatform()->Speak(utterance->id(), utterance->text(),
                                           utterance->lang(), voice,
                                           utterance->continuous_parameters());
    if (!success)
      current_utterance_ = nullptr;

    // If the native voice wasn't able to process this speech, see if
    // the browser has built-in TTS that isn't loaded yet.
    if (!success && loaded_built_in) {
      utterance_queue_.push(utterance);
      return;
    }

    if (!success) {
      utterance->OnTtsEvent(content::TTS_EVENT_ERROR, kInvalidCharIndex,
                            GetTtsPlatform()->GetError());
      delete utterance;
      return;
    }
  }
}

void TtsControllerDelegateImpl::Stop() {
  base::RecordAction(base::UserMetricsAction("TextToSpeech.Stop"));

  paused_ = false;
  if (current_utterance_ && !current_utterance_->extension_id().empty()) {
    if (tts_engine_delegate_)
      tts_engine_delegate_->Stop(current_utterance_);
  } else {
    GetTtsPlatform()->ClearError();
    GetTtsPlatform()->StopSpeaking();
  }

  if (current_utterance_)
    current_utterance_->OnTtsEvent(content::TTS_EVENT_INTERRUPTED,
                                   kInvalidCharIndex, std::string());
  FinishCurrentUtterance();
  ClearUtteranceQueue(true);  // Send events.
}

void TtsControllerDelegateImpl::Pause() {
  base::RecordAction(base::UserMetricsAction("TextToSpeech.Pause"));

  paused_ = true;
  if (current_utterance_ && !current_utterance_->extension_id().empty()) {
    if (tts_engine_delegate_)
      tts_engine_delegate_->Pause(current_utterance_);
  } else if (current_utterance_) {
    GetTtsPlatform()->ClearError();
    GetTtsPlatform()->Pause();
  }
}

void TtsControllerDelegateImpl::Resume() {
  base::RecordAction(base::UserMetricsAction("TextToSpeech.Resume"));

  paused_ = false;
  if (current_utterance_ && !current_utterance_->extension_id().empty()) {
    if (tts_engine_delegate_)
      tts_engine_delegate_->Resume(current_utterance_);
  } else if (current_utterance_) {
    GetTtsPlatform()->ClearError();
    GetTtsPlatform()->Resume();
  } else {
    SpeakNextUtterance();
  }
}

void TtsControllerDelegateImpl::OnTtsEvent(int utterance_id,
                                           content::TtsEventType event_type,
                                           int char_index,
                                           const std::string& error_message) {
  // We may sometimes receive completion callbacks "late", after we've
  // already finished the utterance (for example because another utterance
  // interrupted or we got a call to Stop). This is normal and we can
  // safely just ignore these events.
  if (!current_utterance_ || utterance_id != current_utterance_->id()) {
    return;
  }

  UMATextToSpeechEvent metric;
  switch (event_type) {
    case content::TTS_EVENT_START:
      metric = UMATextToSpeechEvent::START;
      break;
    case content::TTS_EVENT_END:
      metric = UMATextToSpeechEvent::END;
      break;
    case content::TTS_EVENT_WORD:
      metric = UMATextToSpeechEvent::WORD;
      break;
    case content::TTS_EVENT_SENTENCE:
      metric = UMATextToSpeechEvent::SENTENCE;
      break;
    case content::TTS_EVENT_MARKER:
      metric = UMATextToSpeechEvent::MARKER;
      break;
    case content::TTS_EVENT_INTERRUPTED:
      metric = UMATextToSpeechEvent::INTERRUPTED;
      break;
    case content::TTS_EVENT_CANCELLED:
      metric = UMATextToSpeechEvent::CANCELLED;
      break;
    case content::TTS_EVENT_ERROR:
      metric = UMATextToSpeechEvent::SPEECH_ERROR;
      break;
    case content::TTS_EVENT_PAUSE:
      metric = UMATextToSpeechEvent::PAUSE;
      break;
    case content::TTS_EVENT_RESUME:
      metric = UMATextToSpeechEvent::RESUME;
      break;
    default:
      NOTREACHED();
      return;
  }
  UMA_HISTOGRAM_ENUMERATION("TextToSpeech.Event", metric,
                            UMATextToSpeechEvent::COUNT);

  current_utterance_->OnTtsEvent(event_type, char_index, error_message);
  if (current_utterance_->finished()) {
    FinishCurrentUtterance();
    SpeakNextUtterance();
  }
}

void TtsControllerDelegateImpl::GetVoices(
    content::BrowserContext* browser_context,
    std::vector<content::VoiceData>* out_voices) {
  content::TtsPlatform* tts_platform = GetTtsPlatform();
  if (tts_platform) {
    // Ensure we have all built-in voices loaded. This is a no-op if already
    // loaded.
    tts_platform->LoadBuiltInTtsExtension(browser_context);
    if (tts_platform->PlatformImplAvailable())
      tts_platform->GetVoices(out_voices);
  }

  if (browser_context && tts_engine_delegate_)
    tts_engine_delegate_->GetVoices(browser_context, out_voices);
}

bool TtsControllerDelegateImpl::IsSpeaking() {
  return current_utterance_ != nullptr || GetTtsPlatform()->IsSpeaking();
}

void TtsControllerDelegateImpl::FinishCurrentUtterance() {
  if (current_utterance_) {
    if (!current_utterance_->finished())
      current_utterance_->OnTtsEvent(content::TTS_EVENT_INTERRUPTED,
                                     kInvalidCharIndex, std::string());
    delete current_utterance_;
    current_utterance_ = nullptr;
  }
}

void TtsControllerDelegateImpl::SpeakNextUtterance() {
  if (paused_)
    return;

  // Start speaking the next utterance in the queue.  Keep trying in case
  // one fails but there are still more in the queue to try.
  while (!utterance_queue_.empty() && !current_utterance_) {
    content::Utterance* utterance = utterance_queue_.front();
    utterance_queue_.pop();
    SpeakNow(utterance);
  }
}

void TtsControllerDelegateImpl::ClearUtteranceQueue(bool send_events) {
  while (!utterance_queue_.empty()) {
    content::Utterance* utterance = utterance_queue_.front();
    utterance_queue_.pop();
    if (send_events)
      utterance->OnTtsEvent(content::TTS_EVENT_CANCELLED, kInvalidCharIndex,
                            std::string());
    else
      utterance->Finish();
    delete utterance;
  }
}

void TtsControllerDelegateImpl::SetTtsPlatform(
    content::TtsPlatform* tts_platform) {
  tts_platform_ = tts_platform;
}

int TtsControllerDelegateImpl::QueueSize() {
  return static_cast<int>(utterance_queue_.size());
}

content::TtsPlatform* TtsControllerDelegateImpl::GetTtsPlatform() {
  if (!tts_platform_)
    tts_platform_ = content::TtsPlatform::GetInstance();
  return tts_platform_;
}

int TtsControllerDelegateImpl::GetMatchingVoice(
    const content::Utterance* utterance,
    std::vector<content::VoiceData>& voices) {
  // Return the index of the voice that best match the utterance parameters.
  //
  // These criteria are considered mandatory - if they're specified, any voice
  // that doesn't match is rejected.
  //
  //   Extension ID
  //   Voice name
  //
  // The other criteria are scored based on how well they match, in
  // this order of precedence:
  //
  //   Utterange language (exact region preferred, then general language code)
  //   App/system language (exact region preferred, then general language code)
  //   Required event types
  //   User-selected preference of voice given the general language code.

  // TODO(gaochun): Replace the global variable g_browser_process with
  // GetContentClient()->browser() to eliminate the dependency of browser
  // once TTS implementation was moved to content.
  std::string app_lang = g_browser_process->GetApplicationLocale();

#if defined(OS_CHROMEOS)
  const PrefService* prefs = GetPrefService(utterance);
  const base::DictionaryValue* lang_to_voice_pref;
  if (prefs) {
    lang_to_voice_pref =
        prefs->GetDictionary(prefs::kTextToSpeechLangToVoiceName);
  }
#endif  // defined(OS_CHROMEOS)

  // Start with a best score of -1, that way even if none of the criteria
  // match, something will be returned if there are any voices.
  int best_score = -1;
  int best_score_index = -1;
  for (size_t i = 0; i < voices.size(); ++i) {
    const content::VoiceData& voice = voices[i];
    int score = 0;

    // If the extension ID is specified, check for an exact match.
    if (!utterance->extension_id().empty() &&
        utterance->extension_id() != voice.extension_id)
      continue;

    // If the voice name is specified, check for an exact match.
    if (!utterance->voice_name().empty() &&
        voice.name != utterance->voice_name())
      continue;

    // Prefer the utterance language.
    if (!voice.lang.empty() && !utterance->lang().empty()) {
      // An exact language match is worth more than a partial match.
      if (voice.lang == utterance->lang()) {
        score += 128;
      } else if (l10n_util::GetLanguage(voice.lang) ==
                 l10n_util::GetLanguage(utterance->lang())) {
        score += 64;
      }
    }

    // Next, prefer required event types.
    if (utterance->required_event_types().size() > 0) {
      bool has_all_required_event_types = true;
      for (auto iter = utterance->required_event_types().begin();
           iter != utterance->required_event_types().end(); ++iter) {
        if (voice.events.find(*iter) == voice.events.end()) {
          has_all_required_event_types = false;
          break;
        }
      }
      if (has_all_required_event_types)
        score += 32;
    }

#if defined(OS_CHROMEOS)
    // Prefer the user's preference voice for the language:
    if (lang_to_voice_pref) {
      // First prefer the user's preference voice for the utterance language,
      // if the utterance language is specified.
      std::string voice_id;
      if (!utterance->lang().empty()) {
        lang_to_voice_pref->GetString(l10n_util::GetLanguage(utterance->lang()),
                                      &voice_id);
        if (VoiceIdMatches(voice_id, voice))
          score += 16;
      }

      // Then prefer the user's preference voice for the system language.
      // This is a lower priority match than the utterance voice.
      voice_id.clear();
      lang_to_voice_pref->GetString(l10n_util::GetLanguage(app_lang),
                                    &voice_id);
      if (VoiceIdMatches(voice_id, voice))
        score += 8;

      // Finally, prefer the user's preference voice for any language. This will
      // pick the default voice if there is no better match for the current
      // system language and utterance language.
      voice_id.clear();
      lang_to_voice_pref->GetString("noLanguageCode", &voice_id);
      if (VoiceIdMatches(voice_id, voice))
        score += 4;
    }
#endif  // defined(OS_CHROMEOS)

    // Finally, prefer system language.
    if (!voice.lang.empty()) {
      if (voice.lang == app_lang) {
        score += 2;
      } else if (l10n_util::GetLanguage(voice.lang) ==
                 l10n_util::GetLanguage(app_lang)) {
        score += 1;
      }
    }

    if (score > best_score) {
      best_score = score;
      best_score_index = i;
    }
  }

  return best_score_index;
}

void TtsControllerDelegateImpl::UpdateUtteranceDefaults(
    content::Utterance* utterance) {
  double rate = utterance->continuous_parameters().rate;
  double pitch = utterance->continuous_parameters().pitch;
  double volume = utterance->continuous_parameters().volume;
#if defined(OS_CHROMEOS)
  // Update pitch, rate and volume from user prefs if not set explicitly
  // on this utterance.
  const PrefService* prefs = GetPrefService(utterance);
  if (rate == blink::kWebSpeechSynthesisDoublePrefNotSet) {
    rate = prefs ? prefs->GetDouble(prefs::kTextToSpeechRate)
                 : blink::kWebSpeechSynthesisDefaultTextToSpeechRate;
  }
  if (pitch == blink::kWebSpeechSynthesisDoublePrefNotSet) {
    pitch = prefs ? prefs->GetDouble(prefs::kTextToSpeechPitch)
                  : blink::kWebSpeechSynthesisDefaultTextToSpeechPitch;
  }
  if (volume == blink::kWebSpeechSynthesisDoublePrefNotSet) {
    volume = prefs ? prefs->GetDouble(prefs::kTextToSpeechVolume)
                   : blink::kWebSpeechSynthesisDefaultTextToSpeechVolume;
  }
#else
  // Update pitch, rate and volume to defaults if not explicity set on
  // this utterance.
  if (rate == blink::kWebSpeechSynthesisDoublePrefNotSet)
    rate = blink::kWebSpeechSynthesisDefaultTextToSpeechRate;
  if (pitch == blink::kWebSpeechSynthesisDoublePrefNotSet)
    pitch = blink::kWebSpeechSynthesisDefaultTextToSpeechPitch;
  if (volume == blink::kWebSpeechSynthesisDoublePrefNotSet)
    volume = blink::kWebSpeechSynthesisDefaultTextToSpeechVolume;
#endif  // defined(OS_CHROMEOS)
  utterance->set_continuous_parameters(rate, pitch, volume);
}

const PrefService* TtsControllerDelegateImpl::GetPrefService(
    const content::Utterance* utterance) {
  const PrefService* prefs = nullptr;
  // The utterance->browser_context() is null in tests.
  if (utterance->browser_context()) {
    const Profile* profile =
        Profile::FromBrowserContext(utterance->browser_context());
    if (profile)
      prefs = profile->GetPrefs();
  }
  return prefs;
}

void TtsControllerDelegateImpl::VoicesChanged() {
  // Existence of platform tts indicates explicit requests to tts. Since
  // |VoicesChanged| can occur implicitly, only send if needed.
  if (!GetTtsPlatform())
    return;

  for (auto& delegate : voices_changed_delegates_)
    delegate.OnVoicesChanged();
}

void TtsControllerDelegateImpl::AddVoicesChangedDelegate(
    content::VoicesChangedDelegate* delegate) {
  voices_changed_delegates_.AddObserver(delegate);
}

void TtsControllerDelegateImpl::RemoveVoicesChangedDelegate(
    content::VoicesChangedDelegate* delegate) {
  voices_changed_delegates_.RemoveObserver(delegate);
}

void TtsControllerDelegateImpl::RemoveUtteranceEventDelegate(
    content::UtteranceEventDelegate* delegate) {
  // First clear any pending utterances with this delegate.
  base::queue<content::Utterance*> old_queue = utterance_queue_;
  utterance_queue_ = base::queue<content::Utterance*>();
  while (!old_queue.empty()) {
    content::Utterance* utterance = old_queue.front();
    old_queue.pop();
    if (utterance->event_delegate() != delegate)
      utterance_queue_.push(utterance);
    else
      delete utterance;
  }

  if (current_utterance_ && current_utterance_->event_delegate() == delegate) {
    current_utterance_->set_event_delegate(nullptr);
    if (!current_utterance_->extension_id().empty()) {
      if (tts_engine_delegate_)
        tts_engine_delegate_->Stop(current_utterance_);
    } else {
      GetTtsPlatform()->ClearError();
      GetTtsPlatform()->StopSpeaking();
    }

    FinishCurrentUtterance();
    if (!paused_)
      SpeakNextUtterance();
  }
}

void TtsControllerDelegateImpl::SetTtsEngineDelegate(
    content::TtsEngineDelegate* delegate) {
  tts_engine_delegate_ = delegate;
}

content::TtsEngineDelegate* TtsControllerDelegateImpl::GetTtsEngineDelegate() {
  return tts_engine_delegate_;
}
