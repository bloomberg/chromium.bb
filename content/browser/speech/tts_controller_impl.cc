// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/tts_controller_impl.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "third_party/blink/public/platform/web_speech_synthesis_constants.h"

namespace content {

// A value to be used to indicate that there is no char index available.
const int kInvalidCharIndex = -1;

// Returns true if this event type is one that indicates an utterance
// is finished and can be destroyed.
bool IsFinalTtsEventType(TtsEventType event_type) {
  return (event_type == TTS_EVENT_END || event_type == TTS_EVENT_INTERRUPTED ||
          event_type == TTS_EVENT_CANCELLED || event_type == TTS_EVENT_ERROR);
}

//
// UtteranceContinuousParameters
//

UtteranceContinuousParameters::UtteranceContinuousParameters()
    : rate(blink::kWebSpeechSynthesisDoublePrefNotSet),
      pitch(blink::kWebSpeechSynthesisDoublePrefNotSet),
      volume(blink::kWebSpeechSynthesisDoublePrefNotSet) {}

//
// VoiceData
//

VoiceData::VoiceData() : remote(false), native(false) {}

VoiceData::VoiceData(const VoiceData& other) = default;

VoiceData::~VoiceData() {}

//
// Utterance
//

// static
int Utterance::next_utterance_id_ = 0;

Utterance::Utterance(BrowserContext* browser_context)
    : browser_context_(browser_context),
      id_(next_utterance_id_++),
      src_id_(-1),
      can_enqueue_(false),
      char_index_(0),
      finished_(false) {
  options_.reset(new base::DictionaryValue());
}

Utterance::~Utterance() {
  // It's an error if an Utterance is destructed without being finished,
  // unless |browser_context_| is nullptr because it's a unit test.
  DCHECK(finished_ || !browser_context_);
}

void Utterance::OnTtsEvent(TtsEventType event_type,
                           int char_index,
                           const std::string& error_message) {
  if (char_index >= 0)
    char_index_ = char_index;
  if (IsFinalTtsEventType(event_type))
    finished_ = true;

  if (event_delegate_)
    event_delegate_->OnTtsEvent(this, event_type, char_index, error_message);
  if (finished_)
    event_delegate_ = nullptr;
}

void Utterance::Finish() {
  finished_ = true;
}

void Utterance::set_options(const base::Value* options) {
  options_.reset(options->DeepCopy());
}

TtsController* TtsController::GetInstance() {
  return TtsControllerImpl::GetInstance();
}

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

//
// TtsControllerImpl
//

// static
TtsControllerImpl* TtsControllerImpl::GetInstance() {
  return base::Singleton<TtsControllerImpl>::get();
}

TtsControllerImpl::TtsControllerImpl()
    : delegate_(nullptr),
      current_utterance_(nullptr),
      paused_(false),
      tts_platform_(nullptr) {}

TtsControllerImpl::~TtsControllerImpl() {
  if (current_utterance_) {
    current_utterance_->Finish();
    delete current_utterance_;
  }

  // Clear any queued utterances too.
  ClearUtteranceQueue(false);  // Don't sent events.
}

void TtsControllerImpl::SpeakOrEnqueue(Utterance* utterance) {
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

void TtsControllerImpl::Stop() {
  base::RecordAction(base::UserMetricsAction("TextToSpeech.Stop"));

  paused_ = false;
  if (current_utterance_ && !current_utterance_->engine_id().empty()) {
    if (GetTtsControllerDelegate()->GetTtsEngineDelegate())
      GetTtsControllerDelegate()->GetTtsEngineDelegate()->Stop(
          current_utterance_);
  } else {
    GetTtsPlatform()->ClearError();
    GetTtsPlatform()->StopSpeaking();
  }

  if (current_utterance_)
    current_utterance_->OnTtsEvent(TTS_EVENT_INTERRUPTED, kInvalidCharIndex,
                                   std::string());
  FinishCurrentUtterance();
  ClearUtteranceQueue(true);  // Send events.
}

void TtsControllerImpl::Pause() {
  base::RecordAction(base::UserMetricsAction("TextToSpeech.Pause"));

  paused_ = true;
  if (current_utterance_ && !current_utterance_->engine_id().empty()) {
    if (GetTtsControllerDelegate()->GetTtsEngineDelegate())
      GetTtsControllerDelegate()->GetTtsEngineDelegate()->Pause(
          current_utterance_);
  } else if (current_utterance_) {
    GetTtsPlatform()->ClearError();
    GetTtsPlatform()->Pause();
  }
}

void TtsControllerImpl::Resume() {
  base::RecordAction(base::UserMetricsAction("TextToSpeech.Resume"));

  paused_ = false;
  if (current_utterance_ && !current_utterance_->engine_id().empty()) {
    if (GetTtsControllerDelegate()->GetTtsEngineDelegate())
      GetTtsControllerDelegate()->GetTtsEngineDelegate()->Resume(
          current_utterance_);
  } else if (current_utterance_) {
    GetTtsPlatform()->ClearError();
    GetTtsPlatform()->Resume();
  } else {
    SpeakNextUtterance();
  }
}

void TtsControllerImpl::OnTtsEvent(int utterance_id,
                                   TtsEventType event_type,
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
    case TTS_EVENT_START:
      metric = UMATextToSpeechEvent::START;
      break;
    case TTS_EVENT_END:
      metric = UMATextToSpeechEvent::END;
      break;
    case TTS_EVENT_WORD:
      metric = UMATextToSpeechEvent::WORD;
      break;
    case TTS_EVENT_SENTENCE:
      metric = UMATextToSpeechEvent::SENTENCE;
      break;
    case TTS_EVENT_MARKER:
      metric = UMATextToSpeechEvent::MARKER;
      break;
    case TTS_EVENT_INTERRUPTED:
      metric = UMATextToSpeechEvent::INTERRUPTED;
      break;
    case TTS_EVENT_CANCELLED:
      metric = UMATextToSpeechEvent::CANCELLED;
      break;
    case TTS_EVENT_ERROR:
      metric = UMATextToSpeechEvent::SPEECH_ERROR;
      break;
    case TTS_EVENT_PAUSE:
      metric = UMATextToSpeechEvent::PAUSE;
      break;
    case TTS_EVENT_RESUME:
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

void TtsControllerImpl::GetVoices(BrowserContext* browser_context,
                                  std::vector<VoiceData>* out_voices) {
  TtsPlatform* tts_platform = GetTtsPlatform();
  if (tts_platform) {
    // Ensure we have all built-in voices loaded. This is a no-op if already
    // loaded.
    tts_platform->LoadBuiltInTtsEngine(browser_context);
    if (tts_platform->PlatformImplAvailable())
      tts_platform->GetVoices(out_voices);
  }

  if (browser_context && GetTtsControllerDelegate()->GetTtsEngineDelegate())
    GetTtsControllerDelegate()->GetTtsEngineDelegate()->GetVoices(
        browser_context, out_voices);
}

bool TtsControllerImpl::IsSpeaking() {
  return current_utterance_ != nullptr || GetTtsPlatform()->IsSpeaking();
}

void TtsControllerImpl::VoicesChanged() {
  // Existence of platform tts indicates explicit requests to tts. Since
  // |VoicesChanged| can occur implicitly, only send if needed.
  for (auto& delegate : voices_changed_delegates_)
    delegate.OnVoicesChanged();
}

void TtsControllerImpl::AddVoicesChangedDelegate(
    VoicesChangedDelegate* delegate) {
  voices_changed_delegates_.AddObserver(delegate);
}

void TtsControllerImpl::RemoveVoicesChangedDelegate(
    VoicesChangedDelegate* delegate) {
  voices_changed_delegates_.RemoveObserver(delegate);
}

void TtsControllerImpl::RemoveUtteranceEventDelegate(
    UtteranceEventDelegate* delegate) {
  // First clear any pending utterances with this delegate.
  base::queue<Utterance*> old_queue = utterance_queue_;
  utterance_queue_ = base::queue<Utterance*>();
  while (!old_queue.empty()) {
    Utterance* utterance = old_queue.front();
    old_queue.pop();
    if (utterance->event_delegate() != delegate)
      utterance_queue_.push(utterance);
    else
      delete utterance;
  }

  if (current_utterance_ && current_utterance_->event_delegate() == delegate) {
    current_utterance_->set_event_delegate(nullptr);
    if (!current_utterance_->engine_id().empty()) {
      if (GetTtsControllerDelegate()->GetTtsEngineDelegate())
        GetTtsControllerDelegate()->GetTtsEngineDelegate()->Stop(
            current_utterance_);
    } else {
      GetTtsPlatform()->ClearError();
      GetTtsPlatform()->StopSpeaking();
    }

    FinishCurrentUtterance();
    if (!paused_)
      SpeakNextUtterance();
  }
}

void TtsControllerImpl::SetTtsEngineDelegate(TtsEngineDelegate* delegate) {
  if (!GetTtsControllerDelegate())
    return;

  GetTtsControllerDelegate()->SetTtsEngineDelegate(delegate);
}

TtsEngineDelegate* TtsControllerImpl::GetTtsEngineDelegate() {
  if (!GetTtsControllerDelegate())
    return nullptr;

  return GetTtsControllerDelegate()->GetTtsEngineDelegate();
}

void TtsControllerImpl::SetTtsPlatform(TtsPlatform* tts_platform) {
  tts_platform_ = tts_platform;
}

int TtsControllerImpl::QueueSize() {
  return static_cast<int>(utterance_queue_.size());
}

TtsPlatform* TtsControllerImpl::GetTtsPlatform() {
  if (!tts_platform_)
    tts_platform_ = TtsPlatform::GetInstance();
  return tts_platform_;
}

void TtsControllerImpl::SpeakNow(Utterance* utterance) {
  if (!GetTtsControllerDelegate())
    return;

  // Ensure we have all built-in voices loaded. This is a no-op if already
  // loaded.
  bool loaded_built_in =
      GetTtsPlatform()->LoadBuiltInTtsEngine(utterance->browser_context());

  // Get all available voices and try to find a matching voice.
  std::vector<VoiceData> voices;
  GetVoices(utterance->browser_context(), &voices);

  // Get the best matching voice. If nothing matches, just set "native"
  // to true because that might trigger deferred loading of native voices.
  // TODO(katie): Move most of the GetMatchingVoice logic into content/ and
  // use the TTS controller delegate to get chrome-specific info as needed.
  int index = GetTtsControllerDelegate()->GetMatchingVoice(utterance, voices);
  VoiceData voice;
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
    DCHECK(!voice.engine_id.empty());
    current_utterance_ = utterance;
    utterance->set_engine_id(voice.engine_id);
    if (GetTtsControllerDelegate()->GetTtsEngineDelegate())
      GetTtsControllerDelegate()->GetTtsEngineDelegate()->Speak(utterance,
                                                                voice);
    bool sends_end_event =
        voice.events.find(TTS_EVENT_END) != voice.events.end();
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
      utterance->OnTtsEvent(TTS_EVENT_ERROR, kInvalidCharIndex,
                            GetTtsPlatform()->GetError());
      delete utterance;
      return;
    }
  }
}

void TtsControllerImpl::ClearUtteranceQueue(bool send_events) {
  while (!utterance_queue_.empty()) {
    Utterance* utterance = utterance_queue_.front();
    utterance_queue_.pop();
    if (send_events)
      utterance->OnTtsEvent(TTS_EVENT_CANCELLED, kInvalidCharIndex,
                            std::string());
    else
      utterance->Finish();
    delete utterance;
  }
}

void TtsControllerImpl::FinishCurrentUtterance() {
  if (current_utterance_) {
    if (!current_utterance_->finished())
      current_utterance_->OnTtsEvent(TTS_EVENT_INTERRUPTED, kInvalidCharIndex,
                                     std::string());
    delete current_utterance_;
    current_utterance_ = nullptr;
  }
}

void TtsControllerImpl::SpeakNextUtterance() {
  if (paused_)
    return;

  // Start speaking the next utterance in the queue.  Keep trying in case
  // one fails but there are still more in the queue to try.
  while (!utterance_queue_.empty() && !current_utterance_) {
    Utterance* utterance = utterance_queue_.front();
    utterance_queue_.pop();
    SpeakNow(utterance);
  }
}

void TtsControllerImpl::UpdateUtteranceDefaults(Utterance* utterance) {
  double rate = utterance->continuous_parameters().rate;
  double pitch = utterance->continuous_parameters().pitch;
  double volume = utterance->continuous_parameters().volume;
#if defined(OS_CHROMEOS)
  GetTtsControllerDelegate()->UpdateUtteranceDefaultsFromPrefs(utterance, &rate,
                                                               &pitch, &volume);
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

TtsControllerDelegate* TtsControllerImpl::GetTtsControllerDelegate() {
  if (delegate_)
    return delegate_;
  if (GetContentClient() && GetContentClient()->browser()) {
    delegate_ = GetContentClient()->browser()->GetTtsControllerDelegate();
    return delegate_;
  }
  return nullptr;
}

}  // namespace content
