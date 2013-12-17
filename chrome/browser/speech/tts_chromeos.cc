// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/tts_platform.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"

namespace {

// Trigger installing high-quality speech after this many utterances
// have been spoken in one session.
const int kHighQualitySpeechUtteranceThreshold = 100;

}  // anonymous namespace

// Chrome OS doesn't have native TTS, instead it includes a built-in
// component extension that provides speech synthesis. This class monitors
// use of this component extension and triggers installing a higher-quality
// speech synthesis extension if a certain number of utterances are spoken
// in a single session.
class TtsPlatformImplChromeOs
    : public TtsPlatformImpl {
 public:
  // TtsPlatformImpl overrides:
  virtual bool PlatformImplAvailable() OVERRIDE {
    return false;
  }

  virtual bool Speak(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const VoiceData& voice,
      const UtteranceContinuousParameters& params) OVERRIDE {
    return false;
  }

  virtual bool StopSpeaking() OVERRIDE { return false; }
  virtual void Pause() OVERRIDE {}
  virtual void Resume() OVERRIDE {}
  virtual bool IsSpeaking() OVERRIDE { return false; }
  virtual void GetVoices(std::vector<VoiceData>* out_voices) OVERRIDE {}

  virtual void WillSpeakUtteranceWithVoice(
      const Utterance* utterance,
      const VoiceData& voice_data) OVERRIDE;

  // Get the single instance of this class.
  static TtsPlatformImplChromeOs* GetInstance();

 private:
  TtsPlatformImplChromeOs();
  virtual ~TtsPlatformImplChromeOs() {}

  friend struct DefaultSingletonTraits<TtsPlatformImplChromeOs>;

  // A count of the number of utterances spoken for each language
  // using the built-in speech synthesis. When enough utterances have
  // been spoken in a single session, automatically enable install
  // the high-quality speech synthesis extension for that language.
  base::hash_map<std::string, int> lang_utterance_count_;

  DISALLOW_COPY_AND_ASSIGN(TtsPlatformImplChromeOs);
};

TtsPlatformImplChromeOs::TtsPlatformImplChromeOs() {
}

void TtsPlatformImplChromeOs::WillSpeakUtteranceWithVoice(
    const Utterance* utterance,
    const VoiceData& voice_data) {
  CHECK(utterance);
  CHECK(utterance->profile());

  if (utterance->profile()->IsOffTheRecord())
    return;

  if (voice_data.extension_id != extension_misc::kSpeechSynthesisExtensionId)
    return;

  lang_utterance_count_[voice_data.lang]++;
  if (lang_utterance_count_[voice_data.lang] !=
          kHighQualitySpeechUtteranceThreshold) {
    return;
  }

  // Add this language to the list that are allowed to install a
  // component extension for high-quality speech synthesis, overriding
  // the lower-quality one.
  ListPrefUpdate updater(utterance->profile()->GetPrefs(),
                         prefs::kHighQualitySpeechSynthesisLanguages);
  updater->AppendIfNotPresent(new base::StringValue(voice_data.lang));
}

// static
TtsPlatformImpl* TtsPlatformImpl::GetInstance() {
  return TtsPlatformImplChromeOs::GetInstance();
}

// static
TtsPlatformImplChromeOs*
TtsPlatformImplChromeOs::GetInstance() {
  return Singleton<TtsPlatformImplChromeOs>::get();
}
