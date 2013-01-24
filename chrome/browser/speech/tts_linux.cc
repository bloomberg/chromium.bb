// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "base/memory/singleton.h"
#include "chrome/browser/speech/tts_platform.h"
#include "content/public/browser/browser_thread.h"

#include "library_loaders/libspeechd.h"

using content::BrowserThread;

namespace {

const char kNotSupportedError[] =
    "Native speech synthesis not supported on this platform.";

}  // namespace

class TtsPlatformImplLinux : public TtsPlatformImpl {
 public:
  virtual bool PlatformImplAvailable();
  virtual bool Speak(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const UtteranceContinuousParameters& params);
  virtual bool StopSpeaking();
  virtual bool IsSpeaking();
  virtual bool SendsEvent(TtsEventType event_type);
  void OnSpeechEvent(SPDNotificationType type);

  // Get the single instance of this class.
  static TtsPlatformImplLinux* GetInstance();

 private:
  TtsPlatformImplLinux();
  virtual ~TtsPlatformImplLinux();

  // Resets the connection with speech dispatcher.
  void Reset();

  static void NotificationCallback(size_t msg_id,
                                   size_t client_id,
                                   SPDNotificationType type);

  static void IndexMarkCallback(size_t msg_id,
                                size_t client_id,
                                SPDNotificationType state,
                                char* index_mark);

  static SPDNotificationType current_notification_;

  LibSpeechdLoader libspeechd_loader_;
  SPDConnection* conn_;

  // These apply to the current utterance only.
  std::string utterance_;
  int utterance_id_;

  friend struct DefaultSingletonTraits<TtsPlatformImplLinux>;

  DISALLOW_COPY_AND_ASSIGN(TtsPlatformImplLinux);
};

// static
SPDNotificationType TtsPlatformImplLinux::current_notification_ =
    SPD_EVENT_END;

TtsPlatformImplLinux::TtsPlatformImplLinux()
    : utterance_id_(0) {
  if (!libspeechd_loader_.Load("libspeechd.so.2"))
    return;

  conn_ = libspeechd_loader_.spd_open(
      "chrome", "extension_api", NULL, SPD_MODE_THREADED);
  if (!conn_)
    return;

  // Register callbacks for all events.
  conn_->callback_begin =
    conn_->callback_end =
    conn_->callback_cancel =
    conn_->callback_pause =
    conn_->callback_resume =
    &NotificationCallback;

  conn_->callback_im = &IndexMarkCallback;

  libspeechd_loader_.spd_set_notification_on(conn_, SPD_BEGIN);
  libspeechd_loader_.spd_set_notification_on(conn_, SPD_END);
  libspeechd_loader_.spd_set_notification_on(conn_, SPD_CANCEL);
  libspeechd_loader_.spd_set_notification_on(conn_, SPD_PAUSE);
  libspeechd_loader_.spd_set_notification_on(conn_, SPD_RESUME);
}

TtsPlatformImplLinux::~TtsPlatformImplLinux() {
  if (conn_) {
    libspeechd_loader_.spd_close(conn_);
    conn_ = NULL;
  }
}

void TtsPlatformImplLinux::Reset() {
  if (conn_)
    libspeechd_loader_.spd_close(conn_);
  conn_ = libspeechd_loader_.spd_open(
      "chrome", "extension_api", NULL, SPD_MODE_THREADED);
}

bool TtsPlatformImplLinux::PlatformImplAvailable() {
  return libspeechd_loader_.loaded() && (conn_ != NULL);
}

bool TtsPlatformImplLinux::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const UtteranceContinuousParameters& params) {
  if (!PlatformImplAvailable()) {
    error_ = kNotSupportedError;
    return false;
  }

  // Speech dispatcher's speech params are around 3x at either limit.
  float rate = params.rate > 3 ? 3 : params.rate;
  rate = params.rate < 0.334 ? 0.334 : rate;
  float pitch = params.pitch > 3 ? 3 : params.pitch;
  pitch = params.pitch < 0.334 ? 0.334 : pitch;

  // Map our multiplicative range to Speech Dispatcher's linear range.
  // .334 = -100.
  // 3 = 100.
  libspeechd_loader_.spd_set_voice_rate(conn_, 100 * log10(rate) / log10(3));
  libspeechd_loader_.spd_set_voice_pitch(conn_, 100 * log10(pitch) / log10(3));

  utterance_ = utterance;
  utterance_id_ = utterance_id;

  if (libspeechd_loader_.spd_say(conn_, SPD_TEXT, utterance.c_str()) == -1) {
    Reset();
    return false;
  }
  return true;
}

bool TtsPlatformImplLinux::StopSpeaking() {
  if (!PlatformImplAvailable())
    return false;
  if (libspeechd_loader_.spd_stop(conn_) == -1) {
    Reset();
    return false;
  }
  return true;
}

bool TtsPlatformImplLinux::IsSpeaking() {
  return current_notification_ == SPD_EVENT_BEGIN;
}

bool TtsPlatformImplLinux::SendsEvent(TtsEventType event_type) {
  return (event_type == TTS_EVENT_START ||
          event_type == TTS_EVENT_END ||
          event_type == TTS_EVENT_CANCELLED ||
          event_type == TTS_EVENT_MARKER);
}

void TtsPlatformImplLinux::OnSpeechEvent(SPDNotificationType type) {
  TtsController* controller = TtsController::GetInstance();
  switch (type) {
  case SPD_EVENT_BEGIN:
  case SPD_EVENT_RESUME:
    controller->OnTtsEvent(utterance_id_, TTS_EVENT_START, 0, std::string());
    break;
  case SPD_EVENT_END:
  case SPD_EVENT_PAUSE:
    controller->OnTtsEvent(
        utterance_id_, TTS_EVENT_END, utterance_.size(), std::string());
    break;
  case SPD_EVENT_CANCEL:
    controller->OnTtsEvent(
        utterance_id_, TTS_EVENT_CANCELLED, 0, std::string());
    break;
  case SPD_EVENT_INDEX_MARK:
    controller->OnTtsEvent(utterance_id_, TTS_EVENT_MARKER, 0, std::string());
    break;
  }
}

// static
void TtsPlatformImplLinux::NotificationCallback(
    size_t msg_id, size_t client_id, SPDNotificationType type) {
  // We run Speech Dispatcher in threaded mode, so these callbacks should always
  // be in a separate thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    current_notification_ = type;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TtsPlatformImplLinux::OnSpeechEvent,
        base::Unretained(TtsPlatformImplLinux::GetInstance()),
        type));
  }
}

// static
void TtsPlatformImplLinux::IndexMarkCallback(size_t msg_id,
                                                      size_t client_id,
                                                      SPDNotificationType state,
                                                      char* index_mark) {
  // TODO(dtseng): index_mark appears to specify an index type supplied by a
  // client. Need to explore how this is used before hooking it up with existing
  // word, sentence events.
  // We run Speech Dispatcher in threaded mode, so these callbacks should always
  // be in a separate thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    current_notification_ = state;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TtsPlatformImplLinux::OnSpeechEvent,
        base::Unretained(TtsPlatformImplLinux::GetInstance()),
        state));
  }
}

// static
TtsPlatformImplLinux* TtsPlatformImplLinux::GetInstance() {
  return Singleton<TtsPlatformImplLinux,
                   LeakySingletonTraits<TtsPlatformImplLinux> >::get();
}

// static
TtsPlatformImpl* TtsPlatformImpl::GetInstance() {
  return TtsPlatformImplLinux::GetInstance();
}
