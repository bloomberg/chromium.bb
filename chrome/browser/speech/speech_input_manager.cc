// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_input_manager.h"

#include "app/l10n_util.h"
#include "base/ref_counted.h"
#include "base/singleton.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/speech/speech_input_bubble_controller.h"
#include "chrome/browser/speech/speech_recognizer.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "grit/generated_resources.h"
#include "media/audio/audio_manager.h"
#include <map>

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
                                const gfx::Rect& element_rect);
  virtual void CancelRecognition(int caller_id);
  virtual void StopRecording(int caller_id);

  // SpeechRecognizer::Delegate methods.
  virtual void SetRecognitionResult(int caller_id, bool error,
                                    const string16& value);
  virtual void DidCompleteRecording(int caller_id);
  virtual void DidCompleteRecognition(int caller_id);
  virtual void OnRecognizerError(int caller_id,
                                 SpeechRecognizer::ErrorCode error);
  virtual void DidCompleteEnvironmentEstimation(int caller_id);

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
  friend struct DefaultSingletonTraits<SpeechInputManagerImpl>;
  SpeechInputManagerImpl();
  virtual ~SpeechInputManagerImpl();

  bool HasPendingRequest(int caller_id) const;
  SpeechInputManagerDelegate* GetDelegate(int caller_id) const;

  void CancelRecognitionAndInformDelegate(int caller_id);

  // Starts/restarts recognition for an existing request.
  void StartRecognitionForRequest(int caller_id);

  SpeechInputManagerDelegate* delegate_;
  typedef std::map<int, SpeechInputRequest> SpeechRecognizerMap;
  SpeechRecognizerMap requests_;
  int recording_caller_id_;
  scoped_refptr<SpeechInputBubbleController> bubble_controller_;
};

SpeechInputManager* SpeechInputManager::Get() {
  return Singleton<SpeechInputManagerImpl>::get();
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
    const gfx::Rect& element_rect) {
  DCHECK(!HasPendingRequest(caller_id));

  bubble_controller_->CreateBubble(caller_id, render_process_id, render_view_id,
                                   element_rect);

  SpeechInputRequest* request = &requests_[caller_id];
  request->delegate = delegate;
  request->recognizer = new SpeechRecognizer(this, caller_id);
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

void SpeechInputManagerImpl::StopRecording(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  requests_[caller_id].recognizer->StopRecording();
}

void SpeechInputManagerImpl::SetRecognitionResult(int caller_id,
                                                  bool error,
                                                  const string16& value) {
  DCHECK(HasPendingRequest(caller_id));
  GetDelegate(caller_id)->SetRecognitionResult(caller_id,
                                               (error ? string16() : value));
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

void SpeechInputManagerImpl::CancelRecognitionAndInformDelegate(int caller_id) {
  SpeechInputManagerDelegate* cur_delegate = GetDelegate(caller_id);
  CancelRecognition(caller_id);
  cur_delegate->DidCompleteRecording(caller_id);
  cur_delegate->DidCompleteRecognition(caller_id);
}

void SpeechInputManagerImpl::InfoBubbleButtonClicked(
    int caller_id, SpeechInputBubble::Button button) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
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
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
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
