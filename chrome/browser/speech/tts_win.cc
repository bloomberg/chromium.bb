// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <sapi.h>

#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/scoped_comptr.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/speech/tts_platform.h"

class TtsPlatformImplWin : public TtsPlatformImpl {
 public:
  virtual bool PlatformImplAvailable() {
    return true;
  }

  virtual bool Speak(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const UtteranceContinuousParameters& params);

  virtual bool StopSpeaking();

  virtual bool IsSpeaking();

  virtual bool SendsEvent(TtsEventType event_type);

  // Get the single instance of this class.
  static TtsPlatformImplWin* GetInstance();

  static void __stdcall SpeechEventCallback(WPARAM w_param, LPARAM l_param);

 private:
  TtsPlatformImplWin();
  virtual ~TtsPlatformImplWin() {}

  void OnSpeechEvent();

  base::win::ScopedComPtr<ISpVoice> speech_synthesizer_;

  // These apply to the current utterance only.
  std::wstring utterance_;
  int utterance_id_;
  int prefix_len_;
  ULONG stream_number_;
  int char_position_;

  friend struct DefaultSingletonTraits<TtsPlatformImplWin>;

  DISALLOW_COPY_AND_ASSIGN(TtsPlatformImplWin);
};

// static
TtsPlatformImpl* TtsPlatformImpl::GetInstance() {
  return TtsPlatformImplWin::GetInstance();
}

bool TtsPlatformImplWin::Speak(
    int utterance_id,
    const std::string& src_utterance,
    const std::string& lang,
    const UtteranceContinuousParameters& params) {
  std::wstring prefix;
  std::wstring suffix;

  if (!speech_synthesizer_.get())
    return false;

  // TODO(dmazzoni): support languages other than the default: crbug.com/88059

  if (params.rate >= 0.0) {
    // Map our multiplicative range of 0.1x to 10.0x onto Microsoft's
    // linear range of -10 to 10:
    //   0.1 -> -10
    //   1.0 -> 0
    //  10.0 -> 10
    speech_synthesizer_->SetRate(static_cast<int32>(10 * log10(params.rate)));
  }

  if (params.pitch >= 0.0) {
    // The TTS api allows a range of -10 to 10 for speech pitch.
    // TODO(dtseng): cleanup if we ever use any other properties that
    // require xml.
    std::wstring pitch_value =
        base::IntToString16(static_cast<int>(params.pitch * 10 - 10));
    prefix = L"<pitch absmiddle=\"" + pitch_value + L"\">";
    suffix = L"</pitch>";
  }

  if (params.volume >= 0.0) {
    // The TTS api allows a range of 0 to 100 for speech volume.
    speech_synthesizer_->SetVolume(static_cast<uint16>(params.volume * 100));
  }

  // TODO(dmazzoni): convert SSML to SAPI xml. http://crbug.com/88072

  utterance_ = UTF8ToWide(src_utterance);
  utterance_id_ = utterance_id;
  char_position_ = 0;
  std::wstring merged_utterance = prefix + utterance_ + suffix;
  prefix_len_ = prefix.size();

  HRESULT result = speech_synthesizer_->Speak(
      merged_utterance.c_str(),
      SPF_ASYNC,
      &stream_number_);
  return (result == S_OK);
}

bool TtsPlatformImplWin::StopSpeaking() {
  if (speech_synthesizer_.get()) {
    // Clear the stream number so that any further events relating to this
    // utterance are ignored.
    stream_number_ = 0;

    if (IsSpeaking()) {
      // Stop speech by speaking the empty string with the purge flag.
      speech_synthesizer_->Speak(L"", SPF_ASYNC | SPF_PURGEBEFORESPEAK, NULL);
    }
  }
  return true;
}

bool TtsPlatformImplWin::IsSpeaking() {
  if (speech_synthesizer_.get()) {
    SPVOICESTATUS status;
    HRESULT result = speech_synthesizer_->GetStatus(&status, NULL);
    if (result == S_OK) {
      if (status.dwRunningState == 0 ||  // 0 == waiting to speak
          status.dwRunningState == SPRS_IS_SPEAKING) {
        return true;
      }
    }
  }
  return false;
}

bool TtsPlatformImplWin::SendsEvent(TtsEventType event_type) {
  return (event_type == TTS_EVENT_START ||
          event_type == TTS_EVENT_END ||
          event_type == TTS_EVENT_MARKER ||
          event_type == TTS_EVENT_WORD ||
          event_type == TTS_EVENT_SENTENCE);
}

void TtsPlatformImplWin::OnSpeechEvent() {
  TtsController* controller = TtsController::GetInstance();
  SPEVENT event;
  while (S_OK == speech_synthesizer_->GetEvents(1, &event, NULL)) {
    if (event.ulStreamNum != stream_number_)
      continue;

    switch (event.eEventId) {
    case SPEI_START_INPUT_STREAM:
      controller->OnTtsEvent(
          utterance_id_, TTS_EVENT_START, 0, std::string());
      break;
    case SPEI_END_INPUT_STREAM:
      char_position_ = utterance_.size();
      controller->OnTtsEvent(
          utterance_id_, TTS_EVENT_END, char_position_, std::string());
      break;
    case SPEI_TTS_BOOKMARK:
      controller->OnTtsEvent(
          utterance_id_, TTS_EVENT_MARKER, char_position_, std::string());
      break;
    case SPEI_WORD_BOUNDARY:
      char_position_ = static_cast<ULONG>(event.lParam) - prefix_len_;
      controller->OnTtsEvent(
          utterance_id_, TTS_EVENT_WORD, char_position_,
          std::string());
      break;
    case SPEI_SENTENCE_BOUNDARY:
      char_position_ = static_cast<ULONG>(event.lParam) - prefix_len_;
      controller->OnTtsEvent(
          utterance_id_, TTS_EVENT_SENTENCE, char_position_,
          std::string());
      break;
    }
  }
}

TtsPlatformImplWin::TtsPlatformImplWin()
  : utterance_id_(0),
    prefix_len_(0),
    stream_number_(0),
    char_position_(0) {
  speech_synthesizer_.CreateInstance(CLSID_SpVoice);
  if (speech_synthesizer_.get()) {
    ULONGLONG event_mask =
        SPFEI(SPEI_START_INPUT_STREAM) |
        SPFEI(SPEI_TTS_BOOKMARK) |
        SPFEI(SPEI_WORD_BOUNDARY) |
        SPFEI(SPEI_SENTENCE_BOUNDARY) |
        SPFEI(SPEI_END_INPUT_STREAM);
    speech_synthesizer_->SetInterest(event_mask, event_mask);
    speech_synthesizer_->SetNotifyCallbackFunction(
        TtsPlatformImplWin::SpeechEventCallback, 0, 0);
  }
}

// static
TtsPlatformImplWin* TtsPlatformImplWin::GetInstance() {
  return Singleton<TtsPlatformImplWin,
                   LeakySingletonTraits<TtsPlatformImplWin> >::get();
}

// static
void TtsPlatformImplWin::SpeechEventCallback(
    WPARAM w_param, LPARAM l_param) {
  GetInstance()->OnSpeechEvent();
}
