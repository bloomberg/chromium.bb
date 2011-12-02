// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_TRACK_OUTPUT_ANDROID_H_
#define MEDIA_AUDIO_AUDIO_TRACK_OUTPUT_ANDROID_H_

#include <jni.h>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"

class AudioManagerAndroid;

// Implements PCM audio output support for Android using the AudioTrack API.
class AudioTrackOutputStream : public AudioOutputStream {
 public:
  enum Status {
    IDLE,
    OPENED,
    PLAYING,
    INVALID
  };

  explicit AudioTrackOutputStream(const AudioParameters& params);

  virtual ~AudioTrackOutputStream();

  // Implementation of AudioOutputStream.
  virtual bool Open() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Start(AudioSourceCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;

  static AudioOutputStream* MakeStream(const AudioParameters& params);

 private:
  // Helper methods to invoke Java methods on |j_audio_track_|.
  void CallVoidMethod(std::string method_name);

  // Get the value of static field.
  jint GetStaticIntField(std::string class_name, std::string field_name);

  // Feed more data to AudioTrack.
  void FillAudioBufferTask();

  AudioSourceCallback* source_callback_;
  AudioParameters params_;

  class StreamBuffer;
  scoped_ptr<StreamBuffer> data_buffer_;
  Status status_;
  double volume_;
  int buffer_size_;

  // Java AudioTrack class and instance.
  jclass j_class_;
  jobject j_audio_track_;

  base::RepeatingTimer<AudioTrackOutputStream> timer_;

  DISALLOW_COPY_AND_ASSIGN(AudioTrackOutputStream);
};

#endif  // MEDIA_AUDIO_AUDIO_TRACK_OUTPUT_ANDROID_H_
