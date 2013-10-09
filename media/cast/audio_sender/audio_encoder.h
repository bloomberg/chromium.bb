// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_AUDIO_SENDER_AUDIO_ENCODER_H_
#define MEDIA_CAST_AUDIO_SENDER_AUDIO_ENCODER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/rtp_sender/rtp_sender.h"

namespace webrtc {
class AudioCodingModule;
}

namespace media {
namespace cast {

class WebrtEncodedDataCallback;

// Thread safe class.
// It should be called from the main cast thread; however that is not required.
class AudioEncoder : public base::RefCountedThreadSafe<AudioEncoder> {
 public:
  typedef base::Callback<void(scoped_ptr<EncodedAudioFrame>,
                              const base::TimeTicks&)> FrameEncodedCallback;

  AudioEncoder(scoped_refptr<CastEnvironment> cast_environment,
               const AudioSenderConfig& audio_config);

  // The audio_frame must be valid until the closure callback is called.
  // The closure callback is called from the main cast thread as soon as
  // the encoder is done with the frame; it does not mean that the encoded frame
  // has been sent out.
  void InsertRawAudioFrame(const PcmAudioFrame* audio_frame,
                           const base::TimeTicks& recorded_time,
                           const FrameEncodedCallback& frame_encoded_callback,
                           const base::Closure callback);

 protected:
  virtual ~AudioEncoder();

 private:
  friend class base::RefCountedThreadSafe<AudioEncoder>;

  void EncodeAudioFrameThread(
      const PcmAudioFrame* audio_frame,
      const base::TimeTicks& recorded_time,
      const FrameEncodedCallback& frame_encoded_callback,
      const base::Closure release_callback);

  scoped_refptr<CastEnvironment> cast_environment_;
  scoped_ptr<webrtc::AudioCodingModule> audio_encoder_;
  scoped_ptr<WebrtEncodedDataCallback> webrtc_encoder_callback_;
  uint32 timestamp_;

  DISALLOW_COPY_AND_ASSIGN(AudioEncoder);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_AUDIO_SENDER_AUDIO_ENCODER_H_
