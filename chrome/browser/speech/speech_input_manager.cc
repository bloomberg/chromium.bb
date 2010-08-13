// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_input_manager.h"

#include "base/ref_counted.h"
#include "chrome/browser/speech/speech_recognizer.h"
#include <map>

namespace speech_input {

class SpeechInputManagerImpl : public SpeechInputManager,
                               public SpeechRecognizerDelegate {
public:
  explicit SpeechInputManagerImpl(SpeechInputManagerDelegate* delegate);
  virtual ~SpeechInputManagerImpl();

  // SpeechInputManager methods.
  void StartRecognition(int render_view_id);
  void CancelRecognition(int render_view_id);
  void StopRecording(int render_view_id);

  // SpeechRecognizer::Delegate methods.
  void SetRecognitionResult(int render_view_id, bool error,
                            const string16& value);
  void DidCompleteRecording(int render_view_id);
  void DidCompleteRecognition(int render_view_id);

private:
  SpeechInputManagerDelegate* delegate_;
  typedef std::map<int, scoped_refptr<SpeechRecognizer> > SpeechRecognizerMap;
  SpeechRecognizerMap recognizers_;
  int recording_render_view_id_;
};

SpeechInputManager* SpeechInputManager::Create(
    SpeechInputManagerDelegate* delegate) {
  return new SpeechInputManagerImpl(delegate);
}

SpeechInputManagerImpl::SpeechInputManagerImpl(
    SpeechInputManagerDelegate* delegate)
    : delegate_(delegate),
      recording_render_view_id_(0) {
}

SpeechInputManagerImpl::~SpeechInputManagerImpl() {
  while (recognizers_.begin() != recognizers_.end())
    recognizers_.begin()->second->CancelRecognition();
}

void SpeechInputManagerImpl::StartRecognition(int render_view_id) {
  // Make sure we are not already doing recognition for this render view.
  DCHECK(recognizers_.find(render_view_id) == recognizers_.end());
  if (recognizers_.find(render_view_id) != recognizers_.end())
    return;

  // If we are currently recording audio for another caller, abort that now.
  if (recording_render_view_id_)
    CancelRecognition(recording_render_view_id_);

  recording_render_view_id_ = render_view_id;
  recognizers_[render_view_id] = new SpeechRecognizer(this, render_view_id);
  recognizers_[render_view_id]->StartRecording();
}

void SpeechInputManagerImpl::CancelRecognition(int render_view_id) {
  DCHECK(recognizers_.find(render_view_id) != recognizers_.end());
  if (recognizers_.find(render_view_id) != recognizers_.end()) {
    recognizers_[render_view_id]->CancelRecognition();
  }
}

void SpeechInputManagerImpl::StopRecording(int render_view_id) {
  DCHECK(recognizers_.find(render_view_id) != recognizers_.end());
  if (recognizers_.find(render_view_id) != recognizers_.end()) {
    recognizers_[render_view_id]->StopRecording();
  }
}

void SpeechInputManagerImpl::SetRecognitionResult(int render_view_id,
                                                  bool error,
                                                  const string16& value) {
  delegate_->SetRecognitionResult(render_view_id, (error ? string16() : value));
}

void SpeechInputManagerImpl::DidCompleteRecording(int render_view_id) {
  DCHECK(recording_render_view_id_ == render_view_id);
  recording_render_view_id_ = 0;
  delegate_->DidCompleteRecording(render_view_id);
}

void SpeechInputManagerImpl::DidCompleteRecognition(int render_view_id) {
  recognizers_.erase(render_view_id);
  delegate_->DidCompleteRecognition(render_view_id);
}

}  // namespace speech_input
