// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_AUDIO_RECEIVER_AUDIO_DECODER_H_
#define MEDIA_CAST_AUDIO_RECEIVER_AUDIO_DECODER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/cast/cast_config.h"
#include "media/cast/rtp_common/rtp_defines.h"

namespace webrtc {
class AudioCodingModule;
}

namespace media {
namespace cast {

// Thread safe class.
class AudioDecoder : public base::RefCountedThreadSafe<AudioDecoder> {
 public:
  explicit AudioDecoder(const AudioReceiverConfig& audio_config);

  // Extract a raw audio frame from the decoder.
  // Set the number of desired 10ms blocks and frequency.
  // Should be called from the cast audio decoder thread; however that is not
  // required.
  bool GetRawAudioFrame(int number_of_10ms_blocks,
                        int desired_frequency,
                        PcmAudioFrame* audio_frame,
                        uint32* rtp_timestamp);

  // Insert an RTP packet to the decoder.
  // Should be called from the main cast thread; however that is not required.
  void IncomingParsedRtpPacket(const uint8* payload_data,
                               size_t payload_size,
                               const RtpCastHeader& rtp_header);

 protected:
  virtual ~AudioDecoder();

 private:
  friend class base::RefCountedThreadSafe<AudioDecoder>;

  scoped_ptr<webrtc::AudioCodingModule> audio_decoder_;
  bool have_received_packets_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoder);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_AUDIO_RECEIVER_AUDIO_DECODER_H_
