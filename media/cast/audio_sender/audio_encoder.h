// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_AUDIO_SENDER_AUDIO_ENCODER_H_
#define MEDIA_CAST_AUDIO_SENDER_AUDIO_ENCODER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"

namespace base {
class TimeTicks;
}

namespace media {
class AudioBus;
}

namespace media {
namespace cast {

class AudioEncoder : public base::RefCountedThreadSafe<AudioEncoder> {
 public:
  typedef base::Callback<void(scoped_ptr<EncodedAudioFrame>,
                              const base::TimeTicks&)> FrameEncodedCallback;

  AudioEncoder(const scoped_refptr<CastEnvironment>& cast_environment,
               const AudioSenderConfig& audio_config,
               const FrameEncodedCallback& frame_encoded_callback);

  // The |audio_bus| must be valid until the |done_callback| is called.
  // The callback is called from the main cast thread as soon as the encoder is
  // done with |audio_bus|; it does not mean that the encoded data has been
  // sent out.
  void InsertAudio(const AudioBus* audio_bus,
                   const base::TimeTicks& recorded_time,
                   const base::Closure& done_callback);

 protected:
  virtual ~AudioEncoder();

 private:
  friend class base::RefCountedThreadSafe<AudioEncoder>;

  class ImplBase;
  class OpusImpl;
  class Pcm16Impl;

  // Invokes |impl_|'s encode method on the AUDIO_ENCODER thread while holding
  // a ref-count on AudioEncoder.
  void EncodeAudio(const AudioBus* audio_bus,
                   const base::TimeTicks& recorded_time,
                   const base::Closure& done_callback);

  const scoped_refptr<CastEnvironment> cast_environment_;
  scoped_ptr<ImplBase> impl_;

  // Used to ensure only one thread invokes InsertAudio().
  base::ThreadChecker insert_thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AudioEncoder);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_AUDIO_SENDER_AUDIO_ENCODER_H_
