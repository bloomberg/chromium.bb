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
  virtual void RecognitionCancelled(int caller_id);
  virtual void SpeechInputFocusChanged(int caller_id);

 private:
  struct SpeechInputRequest {
    SpeechInputManagerDelegate* delegate;
    scoped_refptr<SpeechRecognizer> recognizer;
    int render_process_id;
    int render_view_id;
    gfx::Rect element_rect;
  };

  // Private constructor to enforce singleton.
  friend struct DefaultSingletonTraits<SpeechInputManagerImpl>;
  SpeechInputManagerImpl();
  virtual ~SpeechInputManagerImpl();

  bool HasPendingRequest(int caller_id) const;
  SpeechRecognizer* GetRecognizer(int caller_id) const;
  SpeechInputManagerDelegate* GetDelegate(int caller_id) const;

  void CancelRecognitionAndInformDelegate(int caller_id);

  // Shows an error message string as a simple alert info bar for the tab
  // identified by |render_process_id| and |render_view_id|.
  void ShowErrorMessage(int render_process_id, int render_view_id,
                        const string16& message);
  static void ShowErrorMessageInUIThread(
      int render_process_id, int render_view_id,
      const string16& message);

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

SpeechRecognizer* SpeechInputManagerImpl::GetRecognizer(int caller_id) const {
  return requests_.find(caller_id)->second.recognizer;
}

void SpeechInputManagerImpl::StartRecognition(
    SpeechInputManagerDelegate* delegate,
    int caller_id,
    int render_process_id,
    int render_view_id,
    const gfx::Rect& element_rect) {
  DCHECK(!HasPendingRequest(caller_id));

  // If we are currently recording audio for another caller, abort that cleanly.
  if (recording_caller_id_)
    CancelRecognitionAndInformDelegate(recording_caller_id_);

  if (!AudioManager::GetAudioManager()->HasAudioInputDevices()) {
    ShowErrorMessage(render_process_id, render_view_id,
                     l10n_util::GetStringUTF16(IDS_SPEECH_INPUT_NO_MIC));
    delegate->DidCompleteRecognition(caller_id);
    return;
  }

  recording_caller_id_ = caller_id;
  SpeechInputRequest request;
  request.delegate = delegate;
  request.recognizer = new SpeechRecognizer(this, caller_id);
  request.render_process_id = render_process_id;
  request.render_view_id = render_view_id;
  request.element_rect = element_rect;
  requests_[caller_id] = request;
  request.recognizer->StartRecording();
}

void SpeechInputManagerImpl::CancelRecognition(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  GetRecognizer(caller_id)->CancelRecognition();
  requests_.erase(caller_id);
  if (recording_caller_id_ == caller_id)
    recording_caller_id_ = 0;
  bubble_controller_->CloseBubble(caller_id);
}

void SpeechInputManagerImpl::StopRecording(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  GetRecognizer(caller_id)->StopRecording();
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
  bubble_controller_->SetBubbleToRecognizingMode(caller_id);
}

void SpeechInputManagerImpl::DidCompleteRecognition(int caller_id) {
  GetDelegate(caller_id)->DidCompleteRecognition(caller_id);
  requests_.erase(caller_id);
  bubble_controller_->CloseBubble(caller_id);
}

void SpeechInputManagerImpl::OnRecognizerError(
    int caller_id, SpeechRecognizer::ErrorCode error) {
  // TODO(satish): Show specific messages for the various error codes and allow
  // user to retry without ending the recognition session if possible.
  const SpeechInputRequest& request = requests_[caller_id];
  ShowErrorMessage(request.render_process_id, request.render_view_id,
                   l10n_util::GetStringUTF16(IDS_SPEECH_INPUT_ERROR));
}

void SpeechInputManagerImpl::DidCompleteEnvironmentEstimation(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  DCHECK(recording_caller_id_ == caller_id);

  // Speech recognizer has gathered enough background audio so we can ask the
  // user to start speaking.
  const SpeechInputRequest& request = requests_[caller_id];
  bubble_controller_->CreateBubble(caller_id, request.render_process_id,
                                   request.render_view_id,
                                   request.element_rect);
}

void SpeechInputManagerImpl::CancelRecognitionAndInformDelegate(int caller_id) {
  SpeechInputManagerDelegate* cur_delegate = GetDelegate(caller_id);
  CancelRecognition(caller_id);
  cur_delegate->DidCompleteRecording(caller_id);
  cur_delegate->DidCompleteRecognition(caller_id);
}

void SpeechInputManagerImpl::RecognitionCancelled(int caller_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  // Ignore if the caller id was not in our active recognizers list because the
  // user might have clicked more than once, or recognition could have been
  // cancelled due to other reasons before the user click was processed.
  if (HasPendingRequest(caller_id))
    CancelRecognitionAndInformDelegate(caller_id);
}

void SpeechInputManagerImpl::SpeechInputFocusChanged(int caller_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  // Ignore if the caller id was not in our active recognizers list because the
  // user might have clicked more than once, or recognition could have been
  // ended due to other reasons before the user click was processed.
  if (HasPendingRequest(caller_id)) {
    // If this is an ongoing recording, abort it since user has switched focus.
    // Otherwise recognition has started and keep that going so user can start
    // speaking to another element while this gets the results in parallel.
    if (recording_caller_id_ == caller_id)
      CancelRecognitionAndInformDelegate(caller_id);
  }
}

void SpeechInputManagerImpl::ShowErrorMessage(
    int render_process_id, int render_view_id,
    const string16& message) {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableFunction(&SpeechInputManagerImpl::ShowErrorMessageInUIThread,
                          render_process_id, render_view_id, message));
}

void SpeechInputManagerImpl::ShowErrorMessageInUIThread(
    int render_process_id, int render_view_id,
    const string16& message) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  TabContents* tab = tab_util::GetTabContentsByID(render_process_id,
                                                  render_view_id);
  if (tab)  // Check in case the tab was closed before we got this request.
    tab->AddInfoBar(new SimpleAlertInfoBarDelegate(tab, message, NULL, true));
}

}  // namespace speech_input
