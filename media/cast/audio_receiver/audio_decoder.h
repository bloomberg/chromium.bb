// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_AUDIO_RECEIVER_AUDIO_DECODER_H_
#define MEDIA_CAST_AUDIO_RECEIVER_AUDIO_DECODER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_thread.h"
#include "media/cast/rtp_common/rtp_defines.h"

namespace webrtc {
class AudioCodingModule;
}

namespace media {
namespace cast {

// Thread safe class.
// It should be called from the main cast thread; however that is not required.
class AudioDecoder : public base::RefCountedThreadSafe<AudioDecoder> {
 public:
  explicit AudioDecoder(scoped_refptr<CastThread> cast_thread,
                        const AudioReceiverConfig& audio_config);

  virtual ~AudioDecoder();

  // Extract a raw audio frame from the decoder.
  // Set the number of desired 10ms blocks and frequency.
  bool GetRawAudioFrame(int number_of_10ms_blocks,
                        int desired_frequency,
                        PcmAudioFrame* audio_frame,
                        uint32* rtp_timestamp);

  // Insert an RTP packet to the decoder.
  void IncomingParsedRtpPacket(const uint8* payload_data,
                               int payload_size,
                               const RtpCastHeader& rtp_header);

 private:
  // Can't use scoped_ptr due to protected constructor within webrtc.
  webrtc::AudioCodingModule* audio_decoder_;
  bool have_received_packets_;
  scoped_refptr<CastThread> cast_thread_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoder);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_AUDIO_RECEIVER_AUDIO_DECODER_H_