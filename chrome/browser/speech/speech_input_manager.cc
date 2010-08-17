// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_input_manager.h"

#include "base/ref_counted.h"
#include <map>

namespace speech_input {

class SpeechInputManagerImpl : public SpeechInputManager,
                               public SpeechRecognizerDelegate {
public:
  explicit SpeechInputManagerImpl(SpeechInputManagerDelegate* delegate);
  virtual ~SpeechInputManagerImpl();

  // SpeechInputManager methods.
  void StartRecognition(const SpeechInputCallerId& caller_id);
  void CancelRecognition(const SpeechInputCallerId& caller_id);
  void StopRecording(const SpeechInputCallerId& caller_id);

  // SpeechRecognizer::Delegate methods.
  void SetRecognitionResult(const SpeechInputCallerId& caller_id, bool error,
                            const string16& value);
  void DidCompleteRecording(const SpeechInputCallerId& caller_id);
  void DidCompleteRecognition(const SpeechInputCallerId& caller_id);

private:
  SpeechInputManagerDelegate* delegate_;
  typedef std::map<SpeechInputCallerId,
                   scoped_refptr<SpeechRecognizer> > SpeechRecognizerMap;
  SpeechRecognizerMap recognizers_;
  SpeechInputCallerId recording_caller_id_;
};

SpeechInputManager* SpeechInputManager::Create(
    SpeechInputManagerDelegate* delegate) {
  return new SpeechInputManagerImpl(delegate);
}

SpeechInputManagerImpl::SpeechInputManagerImpl(
    SpeechInputManagerDelegate* delegate)
    : delegate_(delegate),
      recording_caller_id_(0, 0) {
}

SpeechInputManagerImpl::~SpeechInputManagerImpl() {
  while (recognizers_.begin() != recognizers_.end())
    CancelRecognition(recognizers_.begin()->first);
}

void SpeechInputManagerImpl::StartRecognition(
    const SpeechInputCallerId& caller_id) {
  // Make sure we are not already doing recognition for this render view.
  DCHECK(recognizers_.find(caller_id) == recognizers_.end());
  if (recognizers_.find(caller_id) != recognizers_.end())
    return;

  // If we are currently recording audio for another caller, abort that cleanly.
  if (recording_caller_id_.first != 0) {
    SpeechInputCallerId active_caller = recording_caller_id_;
    CancelRecognition(active_caller);
    DidCompleteRecording(active_caller);
    DidCompleteRecognition(active_caller);
  }

  recording_caller_id_ = caller_id;
  recognizers_[caller_id] = new SpeechRecognizer(this, caller_id);
  recognizers_[caller_id]->StartRecording();
}

void SpeechInputManagerImpl::CancelRecognition(
    const SpeechInputCallerId& caller_id) {
  DCHECK(recognizers_.find(caller_id) != recognizers_.end());
  if (recognizers_.find(caller_id) != recognizers_.end()) {
    recognizers_[caller_id]->CancelRecognition();
    recognizers_.erase(caller_id);
  }
}

void SpeechInputManagerImpl::StopRecording(
    const SpeechInputCallerId& caller_id) {
  DCHECK(recognizers_.find(caller_id) != recognizers_.end());
  if (recognizers_.find(caller_id) != recognizers_.end()) {
    recognizers_[caller_id]->StopRecording();
  }
}

void SpeechInputManagerImpl::SetRecognitionResult(
    const SpeechInputCallerId& caller_id, bool error, const string16& value) {
  delegate_->SetRecognitionResult(caller_id, (error ? string16() : value));
}

void SpeechInputManagerImpl::DidCompleteRecording(
    const SpeechInputCallerId& caller_id) {
  DCHECK(recording_caller_id_ == caller_id);
  recording_caller_id_.first = 0;
  delegate_->DidCompleteRecording(caller_id);
}

void SpeechInputManagerImpl::DidCompleteRecognition(
    const SpeechInputCallerId& caller_id) {
  recognizers_.erase(caller_id);
  delegate_->DidCompleteRecognition(caller_id);
}

}  // namespace speech_input
