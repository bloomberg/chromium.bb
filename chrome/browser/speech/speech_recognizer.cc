// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognizer.h"

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/common/net/url_request_context_getter.h"

using media::AudioInputController;
using std::list;
using std::string;

namespace {
const char* kDefaultSpeechRecognitionUrl =
    "http://www.google.com/speech-api/v1/recognize?lang=en-us&client=chromium";
const int kAudioPacketIntervalMs = 100;  // Record 100ms long audio packets.
const int kNumAudioChannels = 1;  // Speech is recorded as mono.
const int kNumBitsPerAudioSample = 16;
}  // namespace

namespace speech_input {

SpeechRecognizer::SpeechRecognizer(Delegate* delegate, int render_view_id)
    : delegate_(delegate),
      render_view_id_(render_view_id) {
}

SpeechRecognizer::~SpeechRecognizer() {
  // Recording should have stopped earlier due to the endpointer or
  // |StopRecording| being called.
  DCHECK(!audio_controller_.get());
  DCHECK(!request_.get() || !request_->HasPendingRequest());
  DCHECK(audio_buffers_.empty());
}

bool SpeechRecognizer::StartRecording() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(!audio_controller_.get());
  DCHECK(!request_.get() || !request_->HasPendingRequest());

  audio_controller_ = AudioInputController::Create(this,
      AudioManager::AUDIO_PCM_LINEAR, kNumAudioChannels,
      AudioManager::kTelephoneSampleRate, kNumBitsPerAudioSample,
      (AudioManager::kTelephoneSampleRate * kAudioPacketIntervalMs) / 1000);
  DCHECK(audio_controller_.get());
  LOG(INFO) << "SpeechRecognizer starting record.";
  audio_controller_->Record();

  return true;
}

void SpeechRecognizer::CancelRecognition() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(audio_controller_.get() || request_.get());

  // Stop recording if required.
  if (audio_controller_.get()) {
    LOG(INFO) << "SpeechRecognizer stopping record.";
    audio_controller_->Close();
    audio_controller_ = NULL;  // Releases the ref ptr.
  }

  LOG(INFO) << "SpeechRecognizer canceling recognition.";
  ReleaseAudioBuffers();
  request_.reset();
}

void SpeechRecognizer::StopRecording() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  // If audio recording has already stopped and we are in recognition phase,
  // silently ignore any more calls to stop recording.
  if (!audio_controller_.get())
    return;

  LOG(INFO) << "SpeechRecognizer stopping record.";
  audio_controller_->Close();
  audio_controller_ = NULL;  // Releases the ref ptr.
  delegate_->DidCompleteRecording(render_view_id_);

  // If we haven't got any audio yet end the recognition sequence here.
  if (audio_buffers_.empty()) {
    // Guard against the delegate freeing us until we finish our job.
    scoped_refptr<SpeechRecognizer> me(this);
    delegate_->DidCompleteRecognition(render_view_id_);
    return;
  }

  // We now have recorded audio in our buffers, so start a recognition request.
  // Since the http request takes a single string as POST data, allocate
  // one and copy over bytes from the audio buffers to the string.
  int audio_buffer_length = 0;
  for (AudioBufferQueue::iterator it = audio_buffers_.begin();
       it != audio_buffers_.end(); it++) {
    audio_buffer_length += (*it)->length();
  }
  string data;
  data.reserve(audio_buffer_length);
  for (AudioBufferQueue::iterator it = audio_buffers_.begin();
       it != audio_buffers_.end(); it++) {
    data.append(*(*it));
  }
  DCHECK(!request_.get());
  request_.reset(new SpeechRecognitionRequest(
      Profile::GetDefaultRequestContext(),
      GURL(kDefaultSpeechRecognitionUrl),
      this));
  request_->Send(data);
  ReleaseAudioBuffers();  // No need to keep the audio anymore.
}

void SpeechRecognizer::ReleaseAudioBuffers() {
  for (AudioBufferQueue::iterator it = audio_buffers_.begin();
       it != audio_buffers_.end(); it++)
    delete *it;
  audio_buffers_.clear();
}

// Invoked in the audio thread.
void SpeechRecognizer::OnError(AudioInputController* controller,
                               int error_code) {
  ChromeThread::PostTask(ChromeThread::IO, FROM_HERE,
                         NewRunnableMethod(this,
                                           &SpeechRecognizer::HandleOnError,
                                           error_code));
}

void SpeechRecognizer::HandleOnError(int error_code) {
  LOG(WARNING) << "SpeechRecognizer::HandleOnError, code=" << error_code;

  // Check if we are still recording before canceling recognition, as
  // recording might have been stopped after this error was posted to the queue
  // by |OnError|.
  if (!audio_controller_.get())
    return;

  CancelRecognition();
  delegate_->DidCompleteRecording(render_view_id_);
  delegate_->DidCompleteRecognition(render_view_id_);
}

void SpeechRecognizer::OnData(AudioInputController* controller,
                              const uint8* data, uint32 size) {
  if (size == 0)  // This could happen when recording stops and is normal.
    return;

  string* str_data = new string(reinterpret_cast<const char*>(data), size);
  ChromeThread::PostTask(ChromeThread::IO, FROM_HERE,
                         NewRunnableMethod(this,
                                           &SpeechRecognizer::HandleOnData,
                                           str_data));
}

void SpeechRecognizer::HandleOnData(string* data) {
  // Check if we are still recording and if not discard this buffer, as
  // recording might have been stopped after this buffer was posted to the queue
  // by |OnData|.
  if (!audio_controller_.get()) {
    delete data;
    return;
  }

  // TODO(satish): Once we have streaming POST, start sending the data received
  // here as POST chunks.
  audio_buffers_.push_back(data);
}

void SpeechRecognizer::SetRecognitionResult(bool error, const string16& value) {
  delegate_->SetRecognitionResult(render_view_id_, error, value);

  // Guard against the delegate freeing us until we finish our job.
  scoped_refptr<SpeechRecognizer> me(this);
  delegate_->DidCompleteRecognition(render_view_id_);
}

}  // namespace speech_input
