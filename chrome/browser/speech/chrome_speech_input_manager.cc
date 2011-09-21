// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/chrome_speech_input_manager.h"

#include <string>

#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "media/audio/audio_manager.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "chrome/installer/util/wmi.h"
#endif

namespace speech_input {

// Asynchronously fetches the PC and audio hardware/driver info if
// the user has opted into UMA. This information is sent with speech input
// requests to the server for identifying and improving quality issues with
// specific device configurations.
class ChromeSpeechInputManager::OptionalRequestInfo
    : public base::RefCountedThreadSafe<OptionalRequestInfo> {
 public:
  OptionalRequestInfo() : can_report_metrics_(false) {}

  void Refresh() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    // UMA opt-in can be checked only from the UI thread, so switch to that.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &OptionalRequestInfo::CheckUMAAndGetHardwareInfo));
  }

  void CheckUMAAndGetHardwareInfo() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (g_browser_process->local_state()->GetBoolean(
        prefs::kMetricsReportingEnabled)) {
      // Access potentially slow OS calls from the FILE thread.
      BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(this, &OptionalRequestInfo::GetHardwareInfo));
    }
  }

  void GetHardwareInfo() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    base::AutoLock lock(lock_);
    can_report_metrics_ = true;
#if defined(OS_WIN)
    value_ = UTF16ToUTF8(
        installer::WMIComputerSystem::GetModel() + L"|" +
        AudioManager::GetAudioManager()->GetAudioInputDeviceModel());
#else  // defined(OS_WIN)
    value_ = UTF16ToUTF8(
        AudioManager::GetAudioManager()->GetAudioInputDeviceModel());
#endif  // defined(OS_WIN)
  }

  std::string value() {
    base::AutoLock lock(lock_);
    return value_;
  }

  bool can_report_metrics() {
    base::AutoLock lock(lock_);
    return can_report_metrics_;
  }

 private:
  friend class base::RefCountedThreadSafe<OptionalRequestInfo>;

  ~OptionalRequestInfo() {}

  base::Lock lock_;
  std::string value_;
  bool can_report_metrics_;

  DISALLOW_COPY_AND_ASSIGN(OptionalRequestInfo);
};

ChromeSpeechInputManager::SpeechInputRequest::SpeechInputRequest()
    : delegate(NULL),
      is_active(false) {
}

ChromeSpeechInputManager::SpeechInputRequest::~SpeechInputRequest() {
}

ChromeSpeechInputManager* ChromeSpeechInputManager::GetInstance() {
  return Singleton<ChromeSpeechInputManager>::get();
}

ChromeSpeechInputManager::ChromeSpeechInputManager()
    : bubble_controller_(new SpeechInputBubbleController(
          ALLOW_THIS_IN_INITIALIZER_LIST(this))) {
}

ChromeSpeechInputManager::~ChromeSpeechInputManager() {
}

void ChromeSpeechInputManager::ShowRecognitionRequested(
    int caller_id,
    int render_process_id,
    int render_view_id,
    const gfx::Rect& element_rect) {
  bubble_controller_->CreateBubble(caller_id, render_process_id,
                                   render_view_id, element_rect);
}

void ChromeSpeechInputManager::GetRequestInfo(
    bool* can_report_metrics, std::string* request_info) {
  if (!optional_request_info_.get()) {
    optional_request_info_ = new OptionalRequestInfo();
    // Since hardware info is optional with speech input requests, we start an
    // asynchronous fetch here and move on with recording audio. This first
    // speech input request would send an empty string for hardware info and
    // subsequent requests may have the hardware info available if the fetch
    // completed before them. This way we don't end up stalling the user with
    // a long wait and disk seeks when they click on a UI element and start
    // speaking.
    optional_request_info_->Refresh();
  }
  *can_report_metrics = optional_request_info_->can_report_metrics();
  *request_info = optional_request_info_->value();
}

void ChromeSpeechInputManager::ShowWarmUp(int caller_id) {
  bubble_controller_->SetBubbleWarmUpMode(caller_id);
}

void ChromeSpeechInputManager::ShowRecognizing(int caller_id) {
  bubble_controller_->SetBubbleRecognizingMode(caller_id);
}

void ChromeSpeechInputManager::ShowRecording(int caller_id) {
  bubble_controller_->SetBubbleRecordingMode(caller_id);
}

void ChromeSpeechInputManager::ShowInputVolume(
    int caller_id, float volume, float noise_volume) {
  bubble_controller_->SetBubbleInputVolume(caller_id, volume, noise_volume);
}

void ChromeSpeechInputManager::ShowNoMicError(int caller_id) {
  bubble_controller_->SetBubbleMessage(
      caller_id, l10n_util::GetStringUTF16(IDS_SPEECH_INPUT_NO_MIC));
}

void ChromeSpeechInputManager::ShowRecognizerError(
    int caller_id, SpeechRecognizer::ErrorCode error) {
  struct ErrorMessageMapEntry {
    SpeechRecognizer::ErrorCode error;
    int message_id;
  };
  ErrorMessageMapEntry error_message_map[] = {
    {
      SpeechRecognizer::RECOGNIZER_ERROR_CAPTURE, IDS_SPEECH_INPUT_MIC_ERROR
    }, {
      SpeechRecognizer::RECOGNIZER_ERROR_NO_SPEECH, IDS_SPEECH_INPUT_NO_SPEECH
    }, {
      SpeechRecognizer::RECOGNIZER_ERROR_NO_RESULTS, IDS_SPEECH_INPUT_NO_RESULTS
    }, {
      SpeechRecognizer::RECOGNIZER_ERROR_NETWORK, IDS_SPEECH_INPUT_NET_ERROR
    }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(error_message_map); ++i) {
    if (error_message_map[i].error == error) {
      bubble_controller_->SetBubbleMessage(
          caller_id,
          l10n_util::GetStringUTF16(error_message_map[i].message_id));
      return;
    }
  }

  NOTREACHED() << "unknown error " << error;
}

void ChromeSpeechInputManager::DoClose(int caller_id) {
  bubble_controller_->CloseBubble(caller_id);
}

void ChromeSpeechInputManager::InfoBubbleButtonClicked(
    int caller_id, SpeechInputBubble::Button button) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Ignore if the caller id was not in our active recognizers list because the
  // user might have clicked more than once, or recognition could have been
  // cancelled due to other reasons before the user click was processed.
  if (!HasPendingRequest(caller_id))
    return;

  if (button == SpeechInputBubble::BUTTON_CANCEL) {
    CancelRecognitionAndInformDelegate(caller_id);
  } else if (button == SpeechInputBubble::BUTTON_TRY_AGAIN) {
    StartRecognitionForRequest(caller_id);
  }
}

void ChromeSpeechInputManager::InfoBubbleFocusChanged(int caller_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  OnFocusChanged(caller_id);
}

}  // namespace speech_input
