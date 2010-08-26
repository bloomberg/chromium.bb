// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_input_manager.h"

#include "base/ref_counted.h"
#include "base/singleton.h"
#include <map>

namespace speech_input {

class SpeechInputManagerImpl : public SpeechInputManager,
                               public SpeechRecognizerDelegate {
 public:
  // SpeechInputManager methods.
  virtual void StartRecognition(SpeechInputManagerDelegate* delegate,
                                int caller_id);
  virtual void CancelRecognition(int caller_id);
  virtual void StopRecording(int caller_id);

  // SpeechRecognizer::Delegate methods.
  virtual void SetRecognitionResult(int caller_id, bool error,
                                    const string16& value);
  virtual void DidCompleteRecording(int caller_id);
  virtual void DidCompleteRecognition(int caller_id);

 private:
  // Private constructor to enforce singleton.
  friend struct DefaultSingletonTraits<SpeechInputManagerImpl>;
  SpeechInputManagerImpl();
  virtual ~SpeechInputManagerImpl();

  bool HasPendingRequest(int caller_id) const;
  SpeechRecognizer* GetRecognizer(int caller_id) const;
  SpeechInputManagerDelegate* GetDelegate(int caller_id) const;

  void CancelRecognitionAndInformDelegate(int caller_id);

  SpeechInputManagerDelegate* delegate_;
  typedef std::pair<SpeechInputManagerDelegate*,
                    scoped_refptr<SpeechRecognizer> > SpeechInputRequest;
  typedef std::map<int, SpeechInputRequest> SpeechRecognizerMap;
  SpeechRecognizerMap requests_;
  int recording_caller_id_;
};

SpeechInputManager* SpeechInputManager::Get() {
  return Singleton<SpeechInputManagerImpl>::get();
}

SpeechInputManagerImpl::SpeechInputManagerImpl()
    : recording_caller_id_(0) {
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
  return requests_.find(caller_id)->second.first;
}

SpeechRecognizer* SpeechInputManagerImpl::GetRecognizer(int caller_id) const {
  return requests_.find(caller_id)->second.second;
}

void SpeechInputManagerImpl::StartRecognition(
    SpeechInputManagerDelegate* delegate,
    int caller_id) {
  DCHECK(!HasPendingRequest(caller_id));

  // If we are currently recording audio for another caller, abort that cleanly.
  if (recording_caller_id_)
    CancelRecognitionAndInformDelegate(recording_caller_id_);

  recording_caller_id_ = caller_id;
  scoped_refptr<SpeechRecognizer> recognizer(new SpeechRecognizer(this,
                                                                  caller_id));
  requests_[caller_id] = std::make_pair(delegate, recognizer);
  recognizer->StartRecording();
}

void SpeechInputManagerImpl::CancelRecognition(int caller_id) {
  DCHECK(HasPendingRequest(caller_id));
  GetRecognizer(caller_id)->CancelRecognition();
  requests_.erase(caller_id);
  if (recording_caller_id_ == caller_id)
    recording_caller_id_ = 0;
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
}

void SpeechInputManagerImpl::DidCompleteRecognition(int caller_id) {
  GetDelegate(caller_id)->DidCompleteRecognition(caller_id);
  requests_.erase(caller_id);
}

void SpeechInputManagerImpl::CancelRecognitionAndInformDelegate(int caller_id) {
  SpeechInputManagerDelegate* cur_delegate = GetDelegate(caller_id);
  CancelRecognition(caller_id);
  cur_delegate->DidCompleteRecording(caller_id);
  cur_delegate->DidCompleteRecognition(caller_id);
}

}  // namespace speech_input
