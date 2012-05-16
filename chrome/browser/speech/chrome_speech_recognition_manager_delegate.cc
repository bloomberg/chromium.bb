// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/chrome_speech_recognition_manager_delegate.h"

#include <string>

#include "base/bind.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/chrome_view_type.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_result.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "chrome/installer/util/wmi.h"
#endif

using content::BrowserThread;
using content::SpeechRecognitionManager;

namespace {
const int kNoActiveBubble =
    content::SpeechRecognitionManager::kSessionIDInvalid;
}  // namespace

namespace speech {

// Asynchronously fetches the PC and audio hardware/driver info if
// the user has opted into UMA. This information is sent with speech input
// requests to the server for identifying and improving quality issues with
// specific device configurations.
class ChromeSpeechRecognitionManagerDelegate::OptionalRequestInfo
    : public base::RefCountedThreadSafe<OptionalRequestInfo> {
 public:
  OptionalRequestInfo() : can_report_metrics_(false) {
  }

  void Refresh() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    // UMA opt-in can be checked only from the UI thread, so switch to that.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&OptionalRequestInfo::CheckUMAAndGetHardwareInfo, this));
  }

  void CheckUMAAndGetHardwareInfo() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (g_browser_process->local_state()->GetBoolean(
        prefs::kMetricsReportingEnabled)) {
      // Access potentially slow OS calls from the FILE thread.
      BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
          base::Bind(&OptionalRequestInfo::GetHardwareInfo, this));
    }
  }

  void GetHardwareInfo() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    base::AutoLock lock(lock_);
    can_report_metrics_ = true;
    string16 device_model =
        SpeechRecognitionManager::GetInstance()->GetAudioInputDeviceModel();
#if defined(OS_WIN)
    value_ = UTF16ToUTF8(
        installer::WMIComputerSystem::GetModel() + L"|" + device_model);
#else  // defined(OS_WIN)
    value_ = UTF16ToUTF8(device_model);
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

ChromeSpeechRecognitionManagerDelegate::ChromeSpeechRecognitionManagerDelegate()
    : bubble_controller_(new SpeechRecognitionBubbleController(
          ALLOW_THIS_IN_INITIALIZER_LIST(this))),
      active_bubble_session_id_(kNoActiveBubble) {
}

ChromeSpeechRecognitionManagerDelegate::
    ~ChromeSpeechRecognitionManagerDelegate() {
}

void ChromeSpeechRecognitionManagerDelegate::InfoBubbleButtonClicked(
    int session_id, SpeechRecognitionBubble::Button button) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(active_bubble_session_id_, session_id);

  if (button == SpeechRecognitionBubble::BUTTON_CANCEL) {
    bubble_controller_->CloseBubble(session_id);
    last_session_config_.reset();
    active_bubble_session_id_ = kNoActiveBubble;

    // We can safely call AbortSession even if the session has already ended,
    // the manager's public methods are reliable and will handle it properly.
    SpeechRecognitionManager::GetInstance()->AbortSession(session_id);
  } else if (button == SpeechRecognitionBubble::BUTTON_TRY_AGAIN) {
    bubble_controller_->CloseBubble(session_id);
    active_bubble_session_id_ = kNoActiveBubble;
    RestartLastSession();
  }
}

void ChromeSpeechRecognitionManagerDelegate::InfoBubbleFocusChanged(
    int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(active_bubble_session_id_, session_id);

  bubble_controller_->CloseBubble(session_id);
  last_session_config_.reset();
  active_bubble_session_id_ = kNoActiveBubble;

  // If the user clicks outside the bubble while capturing audio we abort the
  // session. Otherwise, i.e. audio capture is ended and we are just waiting for
  // results, this activity is carried silently in background.
  if (SpeechRecognitionManager::GetInstance()->IsCapturingAudio())
    SpeechRecognitionManager::GetInstance()->AbortSession(session_id);
}

void ChromeSpeechRecognitionManagerDelegate::RestartLastSession() {
  DCHECK(last_session_config_.get());
  SpeechRecognitionManager* manager = SpeechRecognitionManager::GetInstance();
  const int new_session_id = manager->CreateSession(*last_session_config_);
  DCHECK_NE(new_session_id, kNoActiveBubble);
  last_session_config_.reset();
  manager->StartSession(new_session_id);
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionStart(
    int session_id) {
  // Copy the configuration of the session (for the "try again" button).
  last_session_config_.reset(new content::SpeechRecognitionSessionConfig(
      SpeechRecognitionManager::GetInstance()->GetSessionConfig(session_id)));

  // Create and show the bubble.
  DCHECK_EQ(active_bubble_session_id_, kNoActiveBubble);
  active_bubble_session_id_ = session_id;
  const content::SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);
  bubble_controller_->CreateBubble(session_id,
                                   context.render_process_id,
                                   context.render_view_id,
                                   context.element_rect);
  // TODO(primiano) Why not create directly the bubble in warmup mode?
  bubble_controller_->SetBubbleWarmUpMode(session_id);
}

void ChromeSpeechRecognitionManagerDelegate::OnAudioStart(int session_id) {
  bubble_controller_->SetBubbleRecordingMode(session_id);
}

void ChromeSpeechRecognitionManagerDelegate::OnEnvironmentEstimationComplete(
    int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::OnSoundStart(int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::OnSoundEnd(int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::OnAudioEnd(int session_id) {
  // OnAudioEnd can be also raised after an abort, when the bubble has already
  // been closed.
  if (active_bubble_session_id_ == session_id)
    bubble_controller_->SetBubbleRecognizingMode(session_id);
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionResult(
    int session_id, const content::SpeechRecognitionResult& result) {
  // A result can be dispatched when the bubble is not visible anymore (e.g.,
  // lost focus while waiting for a result, thus continuing in background).
  if (active_bubble_session_id_ == session_id) {
    bubble_controller_->CloseBubble(session_id);
    last_session_config_.reset();
    active_bubble_session_id_ = kNoActiveBubble;
  }
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionError(
    int session_id, const content::SpeechRecognitionError& error) {
  // An error can be dispatched when the bubble is not visible anymore.
  if (active_bubble_session_id_ != session_id)
    return;

  int error_message_id = 0;
  switch (error.code) {
    case content::SPEECH_RECOGNITION_ERROR_AUDIO:
      switch (error.details) {
        case content::SPEECH_AUDIO_ERROR_DETAILS_NO_MIC:
          error_message_id = IDS_SPEECH_INPUT_NO_MIC;
          break;
        case content::SPEECH_AUDIO_ERROR_DETAILS_IN_USE:
          error_message_id = IDS_SPEECH_INPUT_MIC_IN_USE;
          break;
        default:
          error_message_id = IDS_SPEECH_INPUT_MIC_ERROR;
          break;
      }
      break;
    case content::SPEECH_RECOGNITION_ERROR_ABORTED:
      error_message_id = IDS_SPEECH_INPUT_ABORTED;
      break;
    case content::SPEECH_RECOGNITION_ERROR_NO_SPEECH:
      error_message_id = IDS_SPEECH_INPUT_NO_SPEECH;
      break;
    case content::SPEECH_RECOGNITION_ERROR_NO_MATCH:
      error_message_id = IDS_SPEECH_INPUT_NO_RESULTS;
      break;
    case content::SPEECH_RECOGNITION_ERROR_NETWORK:
      error_message_id = IDS_SPEECH_INPUT_NET_ERROR;
      break;
    default:
      NOTREACHED() << "unknown error " << error.code;
      return;
  }
  bubble_controller_->SetBubbleMessage(
      session_id, l10n_util::GetStringUTF16(error_message_id));
}

void ChromeSpeechRecognitionManagerDelegate::OnAudioLevelsChange(
    int session_id, float volume, float noise_volume) {
  if (active_bubble_session_id_ == session_id)
    bubble_controller_->SetBubbleInputVolume(session_id, volume, noise_volume);
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionEnd(int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::GetDiagnosticInformation(
    bool* can_report_metrics,
    std::string* hardware_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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
  *hardware_info = optional_request_info_->value();
}

void ChromeSpeechRecognitionManagerDelegate::CheckRecognitionIsAllowed(
    int session_id,
    base::Callback<void(int session_id, bool is_allowed)> callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  const content::SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);

  // The check must be performed in the UI thread. We defer it posting to
  // CheckRenderViewType, which will issue the callback on our behalf.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&CheckRenderViewType,
                                     session_id,
                                     callback,
                                     context.render_process_id,
                                     context.render_view_id));
}

content::SpeechRecognitionEventListener*
ChromeSpeechRecognitionManagerDelegate::GetEventListener() {
  return this;
}

void ChromeSpeechRecognitionManagerDelegate::CheckRenderViewType(
    int session_id,
    base::Callback<void(int session_id, bool is_allowed)> callback,
    int render_process_id,
    int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const content::RenderViewHost* render_view_host =
      content::RenderViewHost::FromID(render_process_id, render_view_id);

  // For host delegates other than VIEW_TYPE_TAB_CONTENTS we can't reliably show
  // a popup, including the speech input bubble. In these cases for privacy
  // reasons we don't want to start recording if the user can't be properly
  // notified. An example of this is trying to show the speech input bubble
  // within an extension popup: http://crbug.com/92083. In these situations the
  // speech input extension API should be used instead.

  const bool allowed = (render_view_host != NULL &&
                        render_view_host->GetDelegate() != NULL &&
                        render_view_host->GetDelegate()->GetRenderViewType() ==
                            chrome::VIEW_TYPE_TAB_CONTENTS);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(callback, session_id, allowed));
}

}  // namespace speech
