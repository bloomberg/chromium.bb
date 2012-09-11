// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <math.h>

#include "base/memory/singleton.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_platform.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {
const char kNotSupportedError[] =
    "Native speech synthesis not supported on this platform.";

// Speech dispatcher exports.
// The following types come from the libspeechd-dev package/libspeechd.h.
typedef enum {
  SPD_MODE_SINGLE = 0,
  SPD_MODE_THREADED = 1
} SPDConnectionMode;

typedef enum {
  SPD_IMPORTANT = 1,
  SPD_MESSAGE = 2,
  SPD_TEXT = 3,
  SPD_NOTIFICATION = 4,
  SPD_PROGRESS = 5
} SPDPriority;

typedef enum {
  SPD_EVENT_BEGIN,
  SPD_EVENT_END,
  SPD_EVENT_CANCEL,
  SPD_EVENT_PAUSE,
  SPD_EVENT_RESUME,
  SPD_EVENT_INDEX_MARK
} SPDNotificationType;

typedef enum {
  SPD_BEGIN = 1,
  SPD_END = 2,
  SPD_INDEX_MARKS = 4,
  SPD_CANCEL = 8,
  SPD_PAUSE = 16,
  SPD_RESUME = 32
} SPDNotification;

typedef void (*SPDCallback)(
    size_t msg_id, size_t client_id, SPDNotificationType state);
typedef void (*SPDCallbackIM)(size_t msg_id,
                              size_t client_id,
                              SPDNotificationType state,
                              char* index_mark);

typedef struct {
  /* PUBLIC */
  SPDCallback callback_begin;
  SPDCallback callback_end;
  SPDCallback callback_cancel;
  SPDCallback callback_pause;
  SPDCallback callback_resume;
  SPDCallbackIM callback_im;

  /* PRIVATE */
  int socket;
  FILE* stream;
  SPDConnectionMode mode;

  pthread_mutex_t* ssip_mutex;

  pthread_t* events_thread;
  pthread_mutex_t* comm_mutex;
  pthread_cond_t* cond_reply_ready;
  pthread_mutex_t* mutex_reply_ready;
  pthread_cond_t* cond_reply_ack;
  pthread_mutex_t* mutex_reply_ack;

  char* reply;
} SPDConnection;

typedef SPDConnection* (*spd_open_func)(const char* client_name,
    const char* connection_name,
    const char* user_name,
    SPDConnectionMode mode);
typedef int (*spd_say_func)(SPDConnection* connection,
                            SPDPriority priority,
                            const char* text);
typedef int (*spd_stop_func)(SPDConnection* connection);
typedef void (*spd_close_func)(SPDConnection* connection);
typedef int (*spd_set_notification_on_func)(SPDConnection* connection,
                                            SPDNotification notification);
typedef int (*spd_set_voice_rate_func)(SPDConnection* connection, int rate);
typedef int (*spd_set_voice_pitch_func)(SPDConnection* connection, int pitch);
};

class SpeechDispatcherWrapper {
 public:
  static SPDNotificationType current_notification_;

  SpeechDispatcherWrapper();
  ~SpeechDispatcherWrapper();

  bool Speak(const char* text);
  bool IsSpeaking();
  bool StopSpeaking();
  void SetRate(int rate);
  void SetPitch(int pitch);

  // Resets the connection with speech dispatcher.
  void Reset();

  // States whether Speech Dispatcher loaded successfully.
  bool loaded() {
    return loaded_;
  }

 private:
  static void NotificationCallback(size_t msg_id,
                                   size_t client_id,
                                   SPDNotificationType type);

  static void IndexMarkCallback(size_t msg_id,
                                size_t client_id,
                                SPDNotificationType state,
                                char* index_mark);

  // Interface bindings.
  spd_open_func spd_open;
  spd_say_func spd_say;
  spd_stop_func spd_stop;
  spd_close_func spd_close;
  spd_set_notification_on_func spd_set_notification_on;
  spd_set_voice_rate_func spd_set_voice_rate;
  spd_set_voice_pitch_func spd_set_voice_pitch;

  bool loaded_;
  void* library_;
  SPDConnection* conn_;
  DISALLOW_COPY_AND_ASSIGN(SpeechDispatcherWrapper);
};

// static
SPDNotificationType SpeechDispatcherWrapper::current_notification_ =
    SPD_EVENT_END;

class ExtensionTtsPlatformImplLinux : public ExtensionTtsPlatformImpl {
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
  static ExtensionTtsPlatformImplLinux* GetInstance();

 private:
  ExtensionTtsPlatformImplLinux(): utterance_id_(0) {}
  virtual ~ExtensionTtsPlatformImplLinux() {}

  SpeechDispatcherWrapper spd_;

  // These apply to the current utterance only.
  std::string utterance_;
  int utterance_id_;

  friend struct DefaultSingletonTraits<ExtensionTtsPlatformImplLinux>;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTtsPlatformImplLinux);
};

SpeechDispatcherWrapper::SpeechDispatcherWrapper() : loaded_(false) {
  library_ = dlopen("libspeechd.so", RTLD_LAZY);
  if (!library_)
    return;

  spd_open = reinterpret_cast<spd_open_func>(dlsym(library_, "spd_open"));
  if (!spd_open)
    return;

  spd_say = reinterpret_cast<spd_say_func>(dlsym(library_, "spd_say"));
  if (!spd_say)
    return;

  spd_stop = reinterpret_cast<spd_stop_func>(dlsym(library_, "spd_stop"));
  if (!spd_stop)
    return;

  spd_close = reinterpret_cast<spd_close_func>(dlsym(library_, "spd_close"));
  if (!spd_close)
    return;

  conn_ = spd_open("chrome", "extension_api", NULL, SPD_MODE_THREADED);
  if (!conn_)
    return;

  spd_set_notification_on = reinterpret_cast<spd_set_notification_on_func>(
      dlsym(library_, "spd_set_notification_on"));
  if (!spd_set_notification_on)
    return;

  spd_set_voice_rate = reinterpret_cast<spd_set_voice_rate_func>(
      dlsym(library_, "spd_set_voice_rate"));
  if (!spd_set_voice_rate)
    return;

  spd_set_voice_pitch = reinterpret_cast<spd_set_voice_pitch_func>(
      dlsym(library_, "spd_set_voice_pitch"));
  if (!spd_set_voice_pitch)
    return;

  // Register callbacks for all events.
  conn_->callback_begin =
    conn_->callback_end =
    conn_->callback_cancel =
    conn_->callback_pause =
    conn_->callback_resume =
    &SpeechDispatcherWrapper::NotificationCallback;

  conn_->callback_im = &SpeechDispatcherWrapper::IndexMarkCallback;

  spd_set_notification_on(conn_, SPD_BEGIN);
  spd_set_notification_on(conn_, SPD_END);
  spd_set_notification_on(conn_, SPD_CANCEL);
  spd_set_notification_on(conn_, SPD_PAUSE);
  spd_set_notification_on(conn_, SPD_RESUME);

  loaded_ = true;
}

SpeechDispatcherWrapper::~SpeechDispatcherWrapper() {
  if (conn_) {
    spd_close(conn_);
    conn_ = NULL;
  }

  if (library_) {
    dlclose(library_);
    library_ = NULL;
  }
}
bool SpeechDispatcherWrapper::Speak(const char* text) {
  if (!loaded())
    return false;
  if (spd_say(conn_, SPD_TEXT, text) == -1) {
    Reset();
    return false;
  }
  return true;
}

bool SpeechDispatcherWrapper::IsSpeaking() {
  return SpeechDispatcherWrapper::current_notification_ == SPD_EVENT_BEGIN;
}

bool SpeechDispatcherWrapper::StopSpeaking() {
  if (!loaded())
    return false;
  if (spd_stop(conn_) == -1) {
    Reset();
    return false;
  }
  return true;
}
void SpeechDispatcherWrapper::SetRate(int rate) {
  spd_set_voice_rate(conn_, rate);
}

void SpeechDispatcherWrapper::SetPitch(int pitch) {
  spd_set_voice_pitch(conn_, pitch);
}

// Resets the connection with speech dispatcher.
void SpeechDispatcherWrapper::Reset() {
  if (conn_)
    spd_close(conn_);
  conn_ = spd_open("chrome", "extension_api", NULL, SPD_MODE_THREADED);
}

// static
void SpeechDispatcherWrapper::NotificationCallback(
    size_t msg_id, size_t client_id, SPDNotificationType type) {
  // We run Speech Dispatcher in threaded mode, so these callbacks should always
  // be in a separate thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    SpeechDispatcherWrapper::current_notification_ = type;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&ExtensionTtsPlatformImplLinux::OnSpeechEvent,
        base::Unretained(ExtensionTtsPlatformImplLinux::GetInstance()),
        type));
  }
}

// static
void SpeechDispatcherWrapper::IndexMarkCallback(size_t msg_id,
    size_t client_id,
    SPDNotificationType state,
    char* index_mark) {
  // TODO(dtseng): index_mark appears to specify an index type supplied by a
  // client. Need to explore how this is used before hooking it up with existing
  // word, sentence events.
  // We run Speech Dispatcher in threaded mode, so these callbacks should always
  // be in a separate thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    SpeechDispatcherWrapper::current_notification_ = state;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&ExtensionTtsPlatformImplLinux::OnSpeechEvent,
        base::Unretained(ExtensionTtsPlatformImplLinux::GetInstance()),
        state));
  }
}

bool ExtensionTtsPlatformImplLinux::PlatformImplAvailable() {
  return spd_.loaded();
}

bool ExtensionTtsPlatformImplLinux::Speak(
                   int utterance_id,
                   const std::string& utterance,
                   const std::string& lang,
                   const UtteranceContinuousParameters& params) {
  if (!spd_.loaded()) {
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
  spd_.SetRate(100 * log10(rate) / log10(3));
  spd_.SetPitch(100 * log10(pitch) / log10(3));

  utterance_ = utterance;
  utterance_id_ = utterance_id;

  spd_.Speak(utterance.c_str());
  return true;
}

bool ExtensionTtsPlatformImplLinux::StopSpeaking() {
  return spd_.StopSpeaking();
}

bool ExtensionTtsPlatformImplLinux::IsSpeaking() {
  return spd_.IsSpeaking();
}

bool ExtensionTtsPlatformImplLinux::SendsEvent(TtsEventType event_type) {
  return (event_type == TTS_EVENT_START ||
          event_type == TTS_EVENT_END ||
          event_type == TTS_EVENT_CANCELLED ||
          event_type == TTS_EVENT_MARKER);
}

void ExtensionTtsPlatformImplLinux::OnSpeechEvent(SPDNotificationType type) {
  ExtensionTtsController* controller = ExtensionTtsController::GetInstance();
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
ExtensionTtsPlatformImplLinux* ExtensionTtsPlatformImplLinux::GetInstance() {
  return Singleton<ExtensionTtsPlatformImplLinux>::get();
}

// static
ExtensionTtsPlatformImpl* ExtensionTtsPlatformImpl::GetInstance() {
  return ExtensionTtsPlatformImplLinux::GetInstance();
}
