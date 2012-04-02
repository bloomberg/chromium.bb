// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_track_output_android.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;

static const int kTimerIntervalInMilliseconds = 50;

class AudioTrackOutputStream::StreamBuffer {
 public:
  explicit StreamBuffer(uint32 buffer_size);

  uint32 ReadStream(uint8* dest, uint32 max_size);
  void ResetBuffer(uint32 data_size);
  uint8* GetWritableBuffer();
  const uint8* ReadBuffer();
  void AdvancePosition(uint32 advance);

  uint32 buffer_size() { return buffer_size_; }
  uint32 data_len() { return data_size_ - current_; }

 private:
  scoped_array<uint8> buffer_;
  uint32 buffer_size_;
  uint32 data_size_;
  uint32 current_;

  DISALLOW_COPY_AND_ASSIGN(StreamBuffer);
};

AudioTrackOutputStream::StreamBuffer::StreamBuffer(uint32 buffer_size)
    : buffer_(new uint8[buffer_size]),
      buffer_size_(buffer_size),
      data_size_(0),
      current_(0) {
}

uint32 AudioTrackOutputStream::StreamBuffer::ReadStream(uint8* dest,
                                                        uint32 max_size) {
  uint32 copy_size = data_len() < max_size ? data_len() : max_size;
  memcpy(dest, buffer_.get() + current_, copy_size);
  current_ += copy_size;
  return copy_size;
}

void AudioTrackOutputStream::StreamBuffer::ResetBuffer(uint32 data_size) {
  CHECK_LE(data_size, buffer_size_);
  data_size_ = data_size;
  current_ = 0;
}

uint8* AudioTrackOutputStream::StreamBuffer::GetWritableBuffer() {
  return buffer_.get();
}

const uint8* AudioTrackOutputStream::StreamBuffer::ReadBuffer() {
  return buffer_.get() + current_;
}

void AudioTrackOutputStream::StreamBuffer::AdvancePosition(uint32 advance) {
  current_ += advance;
  CHECK(current_ <= data_size_);
}

AudioTrackOutputStream::AudioTrackOutputStream(const AudioParameters& params)
    : source_callback_(NULL),
      params_(params.format,
              params.sample_rate,
              params.bits_per_sample,
              params.samples_per_packet,
              params.channels),
      status_(IDLE),
      volume_(0),
      buffer_size_(0),
      j_class_(NULL),
      j_audio_track_(NULL) {
  data_buffer_.reset(
      new AudioTrackOutputStream::StreamBuffer(params.GetPacketSize()));
}

AudioTrackOutputStream::~AudioTrackOutputStream() {
  Close();

  if (j_class_ && j_audio_track_) {
    JNIEnv* env = AttachCurrentThread();
    CHECK(env);
    env->DeleteGlobalRef(j_audio_track_);
    j_audio_track_ = NULL;
    env->DeleteGlobalRef(j_class_);
    j_class_ = NULL;
  }
}

bool AudioTrackOutputStream::Open() {
  if (!params_.IsValid())
    return false;

  if (status_ == OPENED)
    return true;
  else if (status_ != IDLE)
    return false;

  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jclass cls = env->FindClass("android/media/AudioTrack");
  CHECK(cls);
  j_class_ = static_cast<jclass>(env->NewGlobalRef(cls));
  env->DeleteLocalRef(cls);

  jint channels;
  if (params_.channels == 1)
    channels = GetStaticIntField("AudioFormat", "CHANNEL_OUT_MONO");
  else if (params_.channels == 2)
    channels = GetStaticIntField("AudioFormat", "CHANNEL_OUT_STEREO");
  else if (params_.channels == 4)
    channels = GetStaticIntField("AudioFormat", "CHANNEL_OUT_QUAD");
  else
    return false;

  jint bits_per_sample;
  if (params_.bits_per_sample == 16)
    bits_per_sample = GetStaticIntField("AudioFormat", "ENCODING_PCM_16BIT");
  else if (params_.bits_per_sample == 8)
    bits_per_sample = GetStaticIntField("AudioFormat", "ENCODING_PCM_8BIT");
  else
    return false;

  jmethodID min_method = env->GetStaticMethodID(j_class_, "getMinBufferSize",
                                                "(III)I");
  CHECK(min_method);

  int min_buffer_size = env->CallStaticIntMethod(
      j_class_, min_method, static_cast<jint>(params_.sample_rate),
      channels, bits_per_sample);

  if (params_.GetPacketSize() < min_buffer_size)
    return false;
  buffer_size_ = params_.GetPacketSize();

  jmethodID constructor = env->GetMethodID(j_class_, "<init>", "(IIIIII)V");
  CHECK(constructor);

  jobject tmp = env->NewObject(
      j_class_, constructor,
      GetStaticIntField("AudioManager", "STREAM_MUSIC"),
      static_cast<jint>(params_.sample_rate), channels, bits_per_sample,
      static_cast<jint>(buffer_size_),
      GetStaticIntField("AudioTrack", "MODE_STREAM"));

  CHECK(tmp);
  j_audio_track_ = env->NewGlobalRef(tmp);
  env->DeleteLocalRef(tmp);

  status_ = OPENED;
  return true;
}

void AudioTrackOutputStream::Close() {
  if (!j_audio_track_)
    return;

  Stop();
  CallVoidMethod("flush");
  status_ = INVALID;
}

void AudioTrackOutputStream::Start(AudioSourceCallback* callback) {
  if (status_ != OPENED)
    return;
  if (!j_audio_track_)
    return;

  source_callback_ = callback;
  data_buffer_->ResetBuffer(0);

  FillAudioBufferTask();
  CallVoidMethod("play");
  status_ = PLAYING;

  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kTimerIntervalInMilliseconds),
      this, &AudioTrackOutputStream::FillAudioBufferTask);
}

void AudioTrackOutputStream::Stop() {
  if (!j_audio_track_)
    return;

  if (status_ == PLAYING) {
    timer_.Stop();
    CallVoidMethod("stop");
    status_ = OPENED;
  }
}

void AudioTrackOutputStream::SetVolume(double volume) {
  volume_ = volume;

  if (!j_audio_track_)
    return;

  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = env->GetMethodID(j_class_, "setStereoVolume", "(FF)I");
  CHECK(method);
  env->CallIntMethod(j_audio_track_, method, static_cast<jfloat>(volume),
                     static_cast<jfloat>(volume));
  CheckException(env);
}

void AudioTrackOutputStream::GetVolume(double* volume) {
  if (volume)
    *volume = volume_;
}

// static
AudioOutputStream* AudioTrackOutputStream::MakeStream(
    const AudioParameters& params) {
  if (params.IsValid())
    return new AudioTrackOutputStream(params);

  return NULL;
}

void AudioTrackOutputStream::CallVoidMethod(std::string method_name) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jmethodID method = env->GetMethodID(j_class_, method_name.c_str(), "()V");
  CHECK(method);
  env->CallVoidMethod(j_audio_track_, method);
  CheckException(env);
}

jint AudioTrackOutputStream::GetStaticIntField(std::string class_name,
                                               std::string field_name) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  class_name.insert(0, "android/media/");
  jclass cls = env->FindClass(class_name.c_str());
  CHECK(cls);

  jfieldID field = env->GetStaticFieldID(cls, field_name.c_str(), "I");
  CHECK(field);
  jint int_field = env->GetStaticIntField(cls, field);
  env->DeleteLocalRef(cls);
  return int_field;
}

void AudioTrackOutputStream::FillAudioBufferTask() {
  if (status_ != PLAYING)
    return;

  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  jmethodID method = env->GetMethodID(j_class_, "getPlaybackHeadPosition",
                                      "()I");
  CHECK(method);

  int64 position = env->CallIntMethod(j_audio_track_, method);
  CheckException(env);

  // Calculate how many bytes we can fill in.
  position *= params_.sample_rate * params_.bits_per_sample *
      params_.channels / 8;
  position %= buffer_size_;

  int need_buffer = static_cast<int>(buffer_size_ - position);
  CHECK(need_buffer >= 0 && need_buffer <= buffer_size_);

  if (!need_buffer)
    return;

  // Fill the internal buffer first.
  if (!data_buffer_->data_len()) {
    uint32 src_data_size = source_callback_->OnMoreData(
        this,
        data_buffer_->GetWritableBuffer(),
        data_buffer_->buffer_size(),
        AudioBuffersState());
    data_buffer_->ResetBuffer(src_data_size);
  }
  need_buffer = std::min(need_buffer,
                         static_cast<int>(data_buffer_->data_len()));

  // Prepare a Java array that contains the samples.
  jbyteArray buf = env->NewByteArray(need_buffer);
  env->SetByteArrayRegion(
      buf, 0, need_buffer,
      reinterpret_cast<const jbyte*>(data_buffer_->ReadBuffer()));
  data_buffer_->AdvancePosition(need_buffer);

  // Invoke method to submit samples.
  method = env->GetMethodID(j_class_, "write", "([BII)I");
  env->CallIntMethod(j_audio_track_, method, buf, static_cast<jint>(0),
                     static_cast<jint>(need_buffer));
  CheckException(env);
  env->DeleteLocalRef(buf);
}
