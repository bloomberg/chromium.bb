// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_input_manager.h"

#include "content/browser/browser_thread.h"
#include "media/audio/audio_manager.h"

namespace speech_input {

SpeechInputManager::SpeechInputManager()
    : can_report_metrics_(false),
      censor_results_(true),
      recording_caller_id_(0) {
}

SpeechInputManager::~SpeechInputManager() {
  while (requests_.begin() != requests_.end())
    CancelRecognition(requests_.begin()->first);
}

bool SpeechInputManager::HasPendingRequest(int caller_id) const {
  return requests_.find(caller_id) != requests_.end();
}

SpeechInputManagerDelegate* SpeechInputManager::GetDelegate(
    int caller_id) const {
  return requests_.find(caller_id)->second.delegate;
}

void SpeechInputManager::ShowAudioInputSettings() {
  // Since AudioManager::ShowAudioInputSettings can potentially launch external
  // processes, do that in the FILE thread to not block the calling threads.
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableFunction(&SpeechInputManager::ShowAudioInputSettings));
    return;
  }

  DCHECK(AudioManager::GetAudioManager()->CanShowAudioInputSettings());
  if (AudioManager::GetAudioManager()->CanShowAudioInputSettings())
    AudioManager::GetAudioManager()->ShowAudioInputSettings();
}

void SpeechInputManager::StartRecognition(
    SpeechInputManagerDelegate* delegate,
    int caller_id,
    int render_process_id,
    int render_view_id,
    const gfx::Rect& element_rect,
    const std::string& language,
    const std::string& grammar,
    const std::string& origin_url) {
  DCHECK(!HasPendingRequest(caller_id));

  ShowRecognitionRequested(
      caller_id, render_process_id, render_view_id, element_rect);
  GetRequestInfo(&can_report_metrics_, &request_info_);

  SpeechInputRequest* request = &requests_[caller_id];
  request->delegate = delegate;
  request->recognizer = new SpeechRecognizer(
      this, caller_id, language, grammar, censor_results(),
      request_info_, can_report_metrics_ ? origin_url : "");
  request->is_active = false;

  StartRecognitionForRequest(caller_id);
}

void SpeechInputManager::StartRecognitionForRequest(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));

  // If we are currently recording audio for another caller, abort that cleanly.
  if (recording_caller_id_)
    CancelRecognitionAndInformDelegate(recording_caller_id_);

  if (!AudioManager::GetAudioManager()->HasAudioInputDevices()) {
    ShowNoMicError(caller_id);
  } else {
    recording_caller_id_ = caller_id;
    requests_[caller_id].is_active = true;
    requests_[caller_id].recognizer->StartRecording();
    ShowWarmUp(caller_id);
  }
}

void SpeechInputManager::CancelRecognition(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  if (requests_[caller_id].is_active)
    requests_[caller_id].recognizer->CancelRecognition();
  requests_.erase(caller_id);
  if (recording_caller_id_ == caller_id)
    recording_caller_id_ = 0;
  DoClose(caller_id);
}

void SpeechInputManager::CancelAllRequestsWithDelegate(
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

void SpeechInputManager::StopRecording(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  requests_[caller_id].recognizer->StopRecording();
}

void SpeechInputManager::SetRecognitionResult(
    int caller_id, bool error, const SpeechInputResultArray& result) {
  DCHECK(HasPendingRequest(caller_id));
  GetDelegate(caller_id)->SetRecognitionResult(caller_id, result);
}

void SpeechInputManager::DidCompleteRecording(int caller_id) {
  DCHECK(recording_caller_id_ == caller_id);
  DCHECK(HasPendingRequest(caller_id));
  recording_caller_id_ = 0;
  GetDelegate(caller_id)->DidCompleteRecording(caller_id);
  ShowRecognizing(caller_id);
}

void SpeechInputManager::DidCompleteRecognition(int caller_id) {
  GetDelegate(caller_id)->DidCompleteRecognition(caller_id);
  requests_.erase(caller_id);
  DoClose(caller_id);
}

void SpeechInputManager::OnRecognizerError(
    int caller_id, SpeechRecognizer::ErrorCode error) {
  if (caller_id == recording_caller_id_)
    recording_caller_id_ = 0;
  requests_[caller_id].is_active = false;
  ShowRecognizerError(caller_id, error);
}

void SpeechInputManager::DidStartReceivingAudio(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  DCHECK(recording_caller_id_ == caller_id);
  ShowRecording(caller_id);
}

void SpeechInputManager::DidCompleteEnvironmentEstimation(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  DCHECK(recording_caller_id_ == caller_id);
}

void SpeechInputManager::SetInputVolume(int caller_id, float volume,
                                        float noise_volume) {
  DCHECK(HasPendingRequest(caller_id));
  DCHECK_EQ(recording_caller_id_, caller_id);
  ShowInputVolume(caller_id, volume, noise_volume);
}

void SpeechInputManager::CancelRecognitionAndInformDelegate(
    int caller_id) {
  SpeechInputManagerDelegate* cur_delegate = GetDelegate(caller_id);
  CancelRecognition(caller_id);
  cur_delegate->DidCompleteRecording(caller_id);
  cur_delegate->DidCompleteRecognition(caller_id);
}

void SpeechInputManager::OnFocusChanged(int caller_id) {
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

SpeechInputManager::SpeechInputRequest::SpeechInputRequest()
    : delegate(NULL),
      is_active(false) {
}

SpeechInputManager::SpeechInputRequest::~SpeechInputRequest() {
}

}  // namespace speech_input
