// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognizer.h"

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "third_party/speex/include/speex/speex.h"

using media::AudioInputController;
using std::list;
using std::string;

namespace {
const char* const kDefaultSpeechRecognitionUrl =
    "http://www.google.com/speech-api/v1/recognize?lang=en-us&client=chromium";
const char* const kContentTypeSpeex =
    "audio/x-speex-with-header-byte; rate=16000";
const int kAudioSampleRate = 16000;
const int kSpeexEncodingQuality = 8;
const int kMaxSpeexFrameLength = 110;  // (44kbps rate sampled at 32kHz).

// Since the frame length gets written out as a byte in the encoded packet,
// make sure it is within the byte range.
COMPILE_ASSERT(kMaxSpeexFrameLength <= 0xFF, invalidLength);

const int kAudioPacketIntervalMs = 100;  // Record 100ms long audio packets.
const int kNumAudioChannels = 1;  // Speech is recorded as mono.
const int kNumBitsPerAudioSample = 16;
}  // namespace

namespace speech_input {

// Provides a simple interface to encode raw audio using the Speex codec.
class SpeexEncoder {
 public:
  SpeexEncoder();
  ~SpeexEncoder();

  int samples_per_frame() const { return samples_per_frame_; }

  // Encodes each frame of raw audio in |raw_samples| and adds the
  // encoded frames as a set of strings to the |encoded_frames| list.
  // Ownership of the newly added strings is transferred to the caller.
  void Encode(const string& raw_samples,
              std::list<std::string*>* encoded_frames);

 private:
  SpeexBits bits_;
  void* encoder_state_;
  int samples_per_frame_;
  char encoded_frame_data_[kMaxSpeexFrameLength + 1];  // +1 for the frame size.
};

SpeexEncoder::SpeexEncoder() {
  speex_bits_init(&bits_);
  encoder_state_ = speex_encoder_init(&speex_wb_mode);
  DCHECK(encoder_state_);
  speex_encoder_ctl(encoder_state_, SPEEX_GET_FRAME_SIZE, &samples_per_frame_);
  DCHECK(samples_per_frame_ > 0);
  int quality = kSpeexEncodingQuality;
  speex_encoder_ctl(encoder_state_, SPEEX_SET_QUALITY, &quality);
  int vbr = 1;
  speex_encoder_ctl(encoder_state_, SPEEX_SET_VBR, &vbr);
}

SpeexEncoder::~SpeexEncoder() {
  speex_bits_destroy(&bits_);
  speex_encoder_destroy(encoder_state_);
}

void SpeexEncoder::Encode(const string& raw_samples,
                          std::list<std::string*>* encoded_frames) {
  const short* samples = reinterpret_cast<const short*>(raw_samples.data());
  DCHECK((raw_samples.length() % sizeof(short)) == 0);
  int num_samples = raw_samples.length() / sizeof(short);

  // Drop incomplete frames, typically those which come in when recording stops.
  num_samples -= (num_samples % samples_per_frame_);
  for (int i = 0; i < num_samples; i += samples_per_frame_) {
    speex_bits_reset(&bits_);
    speex_encode_int(encoder_state_, const_cast<spx_int16_t*>(samples + i),
                     &bits_);

    // Encode the frame and place the size of the frame as the first byte. This
    // is the packet format for MIME type x-speex-with-header-byte.
    int frame_length = speex_bits_write(&bits_, encoded_frame_data_ + 1,
                                        kMaxSpeexFrameLength);
    encoded_frame_data_[0] = static_cast<char>(frame_length);
    encoded_frames->push_back(new string(encoded_frame_data_,
                                         frame_length + 1));
  }
}

SpeechRecognizer::SpeechRecognizer(Delegate* delegate,
                                   const SpeechInputCallerId& caller_id)
    : delegate_(delegate),
      caller_id_(caller_id),
      encoder_(new SpeexEncoder()) {
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

  int samples_per_packet = (kAudioSampleRate * kAudioPacketIntervalMs) / 1000;
  DCHECK((samples_per_packet % encoder_->samples_per_frame()) == 0);
  audio_controller_ = AudioInputController::Create(this,
      AudioManager::AUDIO_PCM_LINEAR, kNumAudioChannels,
      kAudioSampleRate, kNumBitsPerAudioSample,
      samples_per_packet);
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
  delegate_->DidCompleteRecording(caller_id_);

  // If we haven't got any audio yet end the recognition sequence here.
  if (audio_buffers_.empty()) {
    // Guard against the delegate freeing us until we finish our job.
    scoped_refptr<SpeechRecognizer> me(this);
    delegate_->DidCompleteRecognition(caller_id_);
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
  request_->Send(kContentTypeSpeex, data);
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
  delegate_->DidCompleteRecording(caller_id_);
  delegate_->DidCompleteRecognition(caller_id_);
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

  encoder_->Encode(*data, &audio_buffers_);
  delete data;

  // TODO(satish): Once we have streaming POST, start sending the data received
  // here as POST chunks.
}

void SpeechRecognizer::SetRecognitionResult(bool error, const string16& value) {
  delegate_->SetRecognitionResult(caller_id_, error, value);

  // Guard against the delegate freeing us until we finish our job.
  scoped_refptr<SpeechRecognizer> me(this);
  delegate_->DidCompleteRecognition(caller_id_);
}

}  // namespace speech_input
