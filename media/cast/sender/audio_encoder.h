// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_SENDER_AUDIO_ENCODER_H_
#define MEDIA_CAST_SENDER_AUDIO_ENCODER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/audio_bus.h"
#include "media/cast/cast_environment.h"

namespace base {
class TimeTicks;
}

namespace media {
namespace cast {

class AudioEncoder {
 public:
  // Callback to deliver each EncodedFrame, plus the number of audio samples
  // skipped since the last frame.
  typedef base::Callback<void(scoped_ptr<EncodedFrame>, int)>
      FrameEncodedCallback;

  AudioEncoder(const scoped_refptr<CastEnvironment>& cast_environment,
               int num_channels,
               int sampling_rate,
               int bitrate,
               Codec codec,
               const FrameEncodedCallback& frame_encoded_callback);
  virtual ~AudioEncoder();

  CastInitializationStatus InitializationResult() const;

  int GetSamplesPerFrame() const;

  void InsertAudio(scoped_ptr<AudioBus> audio_bus,
                   const base::TimeTicks& recorded_time);

 private:
  class ImplBase;
  class OpusImpl;
  class Pcm16Impl;

  const scoped_refptr<CastEnvironment> cast_environment_;
  scoped_refptr<ImplBase> impl_;

  // Used to ensure only one thread invokes InsertAudio().
  base::ThreadChecker insert_thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AudioEncoder);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_SENDER_AUDIO_ENCODER_H_
