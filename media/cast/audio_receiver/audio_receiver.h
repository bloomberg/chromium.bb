// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_AUDIO_RECEIVER_AUDIO_RECEIVER_H_
#define MEDIA_CAST_AUDIO_RECEIVER_AUDIO_RECEIVER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/cast_thread.h"
#include "media/cast/rtcp/rtcp.h"  // RtcpCastMessage
#include "media/cast/rtp_common/rtp_defines.h"  // RtpCastHeader

namespace media {
namespace cast {

class AudioDecoder;
class Framer;
class LocalRtpAudioData;
class LocalRtpAudioFeedback;
class PacedPacketSender;
class RtpReceiver;
class RtpReceiverStatistics;

// This class is not thread safe. Should only be called from the Main cast
// thread.
class AudioReceiver : public base::NonThreadSafe,
                      public base::SupportsWeakPtr<AudioReceiver> {
 public:
  AudioReceiver(scoped_refptr<CastThread> cast_thread,
                const AudioReceiverConfig& audio_config,
                PacedPacketSender* const packet_sender);

  virtual ~AudioReceiver();

  // Extract a raw audio frame from the cast receiver.
  // Actual decoding will be preformed on a designated audio_decoder thread.
  void GetRawAudioFrame(int number_of_10ms_blocks,
                        int desired_frequency,
                        const AudioFrameDecodedCallback callback);

  // Extract an encoded audio frame from the cast receiver.
  bool GetEncodedAudioFrame(EncodedAudioFrame* audio_frame,
                            base::TimeTicks* playout_time);

  // Release frame - should be called following a GetCodedAudioFrame call.
  // Should only be called from the main cast thread.
  void ReleaseFrame(uint8 frame_id);

  // Should only be called from the main cast thread.
  void IncomingPacket(const uint8* packet, int length);

  // Only used for testing.
  void set_clock(base::TickClock* clock) {
    clock_ = clock;
    rtcp_->set_clock(clock);
  }

 protected:
  void IncomingParsedRtpPacket(const uint8* payload_data,
                               int payload_size,
                               const RtpCastHeader& rtp_header);
 private:
  friend class LocalRtpAudioData;
  friend class LocalRtpAudioFeedback;

  void CastFeedback(const RtcpCastMessage& cast_message);

  // Actual decoding implementation - should be called under the audio decoder
  // thread.
  void DecodeAudioFrameThread(int number_of_10ms_blocks,
                              int desired_frequency,
                              const AudioFrameDecodedCallback callback);

  // Return the playout time based on the current time and rtp timestamp.
  base::TimeTicks GetPlayoutTime(base::TimeTicks now,
                                uint32 rtp_timestamp);

  // Schedule the next RTCP report.
  void ScheduleNextRtcpReport();

  // Actually send the next RTCP report.
  void SendNextRtcpReport();

  scoped_refptr<CastThread> cast_thread_;
  base::WeakPtrFactory<AudioReceiver> weak_factory_;

  const AudioCodec codec_;
  const uint32 incoming_ssrc_;
  const int frequency_;
  base::TimeDelta target_delay_delta_;
  scoped_ptr<Framer> audio_buffer_;
  scoped_refptr<AudioDecoder> audio_decoder_;
  scoped_ptr<LocalRtpAudioData> incoming_payload_callback_;
  scoped_ptr<LocalRtpAudioFeedback> incoming_payload_feedback_;
  scoped_ptr<RtpReceiver> rtp_receiver_;
  scoped_ptr<Rtcp> rtcp_;
  scoped_ptr<RtpReceiverStatistics> rtp_audio_receiver_statistics_;
  base::TimeDelta time_offset_;

  scoped_ptr<base::TickClock> default_tick_clock_;
  base::TickClock* clock_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_AUDIO_RECEIVER_AUDIO_RECEIVER_H_