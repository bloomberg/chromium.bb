// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_input_manager.h"

#include <map>
#include <string>

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/speech/speech_input_bubble_controller.h"
#include "chrome/browser/speech/speech_recognizer.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "media/audio/audio_manager.h"

#if defined(OS_WIN)
#include "chrome/installer/util/wmi.h"
#endif

namespace {

// Asynchronously fetches the PC and audio hardware/driver info if
// the user has opted into UMA. This information is sent with speech input
// requests to the server for identifying and improving quality issues with
// specific device configurations.
class OptionalRequestInfo
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
  base::Lock lock_;
  std::string value_;
  bool can_report_metrics_;

  DISALLOW_COPY_AND_ASSIGN(OptionalRequestInfo);
};

}  // namespace

namespace speech_input {

class SpeechInputManagerImpl : public SpeechInputManager,
                               public SpeechInputBubbleControllerDelegate,
                               public SpeechRecognizerDelegate {
 public:
  // SpeechInputManager methods.
  virtual void StartRecognition(SpeechInputManagerDelegate* delegate,
                                int caller_id,
                                int render_process_id,
                                int render_view_id,
                                const gfx::Rect& element_rect,
                                const std::string& language,
                                const std::string& grammar,
                                const std::string& origin_url);
  virtual void CancelRecognition(int caller_id);
  virtual void StopRecording(int caller_id);
  virtual void CancelAllRequestsWithDelegate(
      SpeechInputManagerDelegate* delegate);

  // SpeechRecognizer::Delegate methods.
  virtual void SetRecognitionResult(int caller_id,
                                    bool error,
                                    const SpeechInputResultArray& result);
  virtual void DidCompleteRecording(int caller_id);
  virtual void DidCompleteRecognition(int caller_id);
  virtual void OnRecognizerError(int caller_id,
                                 SpeechRecognizer::ErrorCode error);
  virtual void DidCompleteEnvironmentEstimation(int caller_id);
  virtual void SetInputVolume(int caller_id, float volume);

  // SpeechInputBubbleController::Delegate methods.
  virtual void InfoBubbleButtonClicked(int caller_id,
                                       SpeechInputBubble::Button button);
  virtual void InfoBubbleFocusChanged(int caller_id);

 private:
  struct SpeechInputRequest {
    SpeechInputManagerDelegate* delegate;
    scoped_refptr<SpeechRecognizer> recognizer;
    bool is_active;  // Set to true when recording or recognition is going on.
  };

  // Private constructor to enforce singleton.
  friend struct base::DefaultLazyInstanceTraits<SpeechInputManagerImpl>;
  SpeechInputManagerImpl();
  virtual ~SpeechInputManagerImpl();

  bool HasPendingRequest(int caller_id) const;
  SpeechInputManagerDelegate* GetDelegate(int caller_id) const;

  void CancelRecognitionAndInformDelegate(int caller_id);

  // Starts/restarts recognition for an existing request.
  void StartRecognitionForRequest(int caller_id);

  typedef std::map<int, SpeechInputRequest> SpeechRecognizerMap;
  SpeechRecognizerMap requests_;
  int recording_caller_id_;
  scoped_refptr<SpeechInputBubbleController> bubble_controller_;
  scoped_refptr<OptionalRequestInfo> optional_request_info_;
};

static ::base::LazyInstance<SpeechInputManagerImpl> g_speech_input_manager_impl(
    base::LINKER_INITIALIZED);

SpeechInputManager* SpeechInputManager::Get() {
  return g_speech_input_manager_impl.Pointer();
}

bool SpeechInputManager::IsFeatureEnabled() {
  bool enabled = true;
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableSpeechInput)) {
    enabled = false;
#if defined(GOOGLE_CHROME_BUILD)
  } else if (!command_line.HasSwitch(switches::kEnableSpeechInput)) {
    // We need to evaluate whether IO is OK here. http://crbug.com/63335.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    // Official Chrome builds have speech input enabled by default only in the
    // dev channel.
    std::string channel = platform_util::GetVersionStringModifier();
    enabled = (channel == "dev");
#endif
  }

  return enabled;
}

SpeechInputManagerImpl::SpeechInputManagerImpl()
    : recording_caller_id_(0),
      bubble_controller_(new SpeechInputBubbleController(
          ALLOW_THIS_IN_INITIALIZER_LIST(this))) {
}

SpeechInputManagerImpl::~SpeechInputManagerImpl() {
  while (requests_.begin() != requests_.end())
    CancelRecognition(requests_.begin()->first);
}

bool SpeechInputManagerImpl::HasPendingRequest(int caller_id) const {
  return requests_.find(caller_id) != requests_.end();
}

SpeechInputManagerDelegate* SpeechInputManagerImpl::GetDelegate(
    int caller_id) const {
  return requests_.find(caller_id)->second.delegate;
}

void SpeechInputManagerImpl::StartRecognition(
    SpeechInputManagerDelegate* delegate,
    int caller_id,
    int render_process_id,
    int render_view_id,
    const gfx::Rect& element_rect,
    const std::string& language,
    const std::string& grammar,
    const std::string& origin_url) {
  DCHECK(!HasPendingRequest(caller_id));

  bubble_controller_->CreateBubble(caller_id, render_process_id, render_view_id,
                                   element_rect);

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

  SpeechInputRequest* request = &requests_[caller_id];
  request->delegate = delegate;
  request->recognizer = new SpeechRecognizer(
      this, caller_id, language, grammar, optional_request_info_->value(),
      optional_request_info_->can_report_metrics() ? origin_url : "");
  request->is_active = false;

  StartRecognitionForRequest(caller_id);
}

void SpeechInputManagerImpl::StartRecognitionForRequest(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));

  // If we are currently recording audio for another caller, abort that cleanly.
  if (recording_caller_id_)
    CancelRecognitionAndInformDelegate(recording_caller_id_);

  if (!AudioManager::GetAudioManager()->HasAudioInputDevices()) {
    bubble_controller_->SetBubbleMessage(
        caller_id, l10n_util::GetStringUTF16(IDS_SPEECH_INPUT_NO_MIC));
  } else {
    recording_caller_id_ = caller_id;
    requests_[caller_id].is_active = true;
    requests_[caller_id].recognizer->StartRecording();
  }
}

void SpeechInputManagerImpl::CancelRecognition(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  if (requests_[caller_id].is_active)
    requests_[caller_id].recognizer->CancelRecognition();
  requests_.erase(caller_id);
  if (recording_caller_id_ == caller_id)
    recording_caller_id_ = 0;
  bubble_controller_->CloseBubble(caller_id);
}

void SpeechInputManagerImpl::CancelAllRequestsWithDelegate(
    SpeechInputManagerDelegate* delegate) {
  SpeechRecognizerMap::iterator it = requests_.begin();
  while (it != requests_.end()) {
    if (it->second.delegate == delegate) {
      CancelRecognition(it->first);
      // This map will have very few elements so it is simpler to restart.
      it = requests_.begin();
    } else {
      ++it;
    }
  }
}

void SpeechInputManagerImpl::StopRecording(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  requests_[caller_id].recognizer->StopRecording();
}

void SpeechInputManagerImpl::SetRecognitionResult(
    int caller_id, bool error, const SpeechInputResultArray& result) {
  DCHECK(HasPendingRequest(caller_id));
  GetDelegate(caller_id)->SetRecognitionResult(caller_id, result);
}

void SpeechInputManagerImpl::DidCompleteRecording(int caller_id) {
  DCHECK(recording_caller_id_ == caller_id);
  DCHECK(HasPendingRequest(caller_id));
  recording_caller_id_ = 0;
  GetDelegate(caller_id)->DidCompleteRecording(caller_id);
  bubble_controller_->SetBubbleRecognizingMode(caller_id);
}

void SpeechInputManagerImpl::DidCompleteRecognition(int caller_id) {
  GetDelegate(caller_id)->DidCompleteRecognition(caller_id);
  requests_.erase(caller_id);
  bubble_controller_->CloseBubble(caller_id);
}

void SpeechInputManagerImpl::OnRecognizerError(
    int caller_id, SpeechRecognizer::ErrorCode error) {
  if (caller_id == recording_caller_id_)
    recording_caller_id_ = 0;

  requests_[caller_id].is_active = false;

  int message_id;
  switch (error) {
    case SpeechRecognizer::RECOGNIZER_ERROR_CAPTURE:
      message_id = IDS_SPEECH_INPUT_ERROR;
      break;
    case SpeechRecognizer::RECOGNIZER_ERROR_NO_SPEECH:
      message_id = IDS_SPEECH_INPUT_NO_SPEECH;
      break;
    case SpeechRecognizer::RECOGNIZER_ERROR_NO_RESULTS:
      message_id = IDS_SPEECH_INPUT_NO_RESULTS;
      break;
    default:
      NOTREACHED() << "unknown error " << error;
      return;
  }
  bubble_controller_->SetBubbleMessage(caller_id,
                                       l10n_util::GetStringUTF16(message_id));
}

void SpeechInputManagerImpl::DidCompleteEnvironmentEstimation(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  DCHECK(recording_caller_id_ == caller_id);

  // Speech recognizer has gathered enough background audio so we can ask the
  // user to start speaking.
  bubble_controller_->SetBubbleRecordingMode(caller_id);
}

void SpeechInputManagerImpl::SetInputVolume(int caller_id, float volume) {
  DCHECK(HasPendingRequest(caller_id));
  DCHECK_EQ(recording_caller_id_, caller_id);

  bubble_controller_->SetBubbleInputVolume(caller_id, volume);
}

void SpeechInputManagerImpl::CancelRecognitionAndInformDelegate(int caller_id) {
  SpeechInputManagerDelegate* cur_delegate = GetDelegate(caller_id);
  CancelRecognition(caller_id);
  cur_delegate->DidCompleteRecording(caller_id);
  cur_delegate->DidCompleteRecognition(caller_id);
}

void SpeechInputManagerImpl::InfoBubbleButtonClicked(
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

void SpeechInputManagerImpl::InfoBubbleFocusChanged(int caller_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Ignore if the caller id was not in our active recognizers list because the
  // user might have clicked more than once, or recognition could have been
  // ended due to other reasons before the user click was processed.
  if (HasPendingRequest(caller_id)) {
    // If this is an ongoing recording or if we were displaying an error message
    // to the user, abort it since user has switched focus. Otherwise
    // recognition has started and keep that going so user can start speaking to
    // another element while this gets the results in parallel.
    if (recording_caller_id_ == caller_id || !requests_[caller_id].is_active) {
      CancelRecognitionAndInformDelegate(caller_id);
    }
  }
}

}  // namespace speech_input
