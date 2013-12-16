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
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/rtcp/rtcp.h"  // RtcpCastMessage
#include "media/cast/rtp_receiver/rtp_receiver_defines.h"  // RtpCastHeader

namespace crypto {
  class Encryptor;
}

namespace media {
namespace cast {

class AudioDecoder;
class Framer;
class LocalRtpAudioData;
class LocalRtpAudioFeedback;
class PacedPacketSender;
class RtpReceiver;
class RtpReceiverStatistics;

struct DecodedAudioCallbackData {
  DecodedAudioCallbackData();
  ~DecodedAudioCallbackData();
  int number_of_10ms_blocks;
  int desired_frequency;
  AudioFrameDecodedCallback callback;
};

// This class is not thread safe. Should only be called from the Main cast
// thread.
class AudioReceiver : public base::NonThreadSafe,
                      public base::SupportsWeakPtr<AudioReceiver> {
 public:
  AudioReceiver(scoped_refptr<CastEnvironment> cast_environment,
                const AudioReceiverConfig& audio_config,
                PacedPacketSender* const packet_sender);

  virtual ~AudioReceiver();

  // Extract a raw audio frame from the cast receiver.
  // Actual decoding will be preformed on a designated audio_decoder thread.
  void GetRawAudioFrame(int number_of_10ms_blocks,
                        int desired_frequency,
                        const AudioFrameDecodedCallback& callback);

  // Extract an encoded audio frame from the cast receiver.
  void GetEncodedAudioFrame(const AudioFrameEncodedCallback& callback);

  // Should only be called from the main cast thread.
  void IncomingPacket(const uint8* packet, size_t length,
                      const base::Closure callback);

 protected:
  void IncomingParsedRtpPacket(const uint8* payload_data,
                               size_t payload_size,
                               const RtpCastHeader& rtp_header);
 private:
  friend class LocalRtpAudioData;
  friend class LocalRtpAudioFeedback;

  void CastFeedback(const RtcpCastMessage& cast_message);

  // Time to pull out the audio even though we are missing data.
  void PlayoutTimeout();

  bool PostEncodedAudioFrame(const AudioFrameEncodedCallback& callback,
                             uint32 rtp_timestamp,
                             bool next_frame,
                             scoped_ptr<EncodedAudioFrame>* encoded_frame);

  // Actual decoding implementation - should be called under the audio decoder
  // thread.
  void DecodeAudioFrameThread(int number_of_10ms_blocks,
                              int desired_frequency,
                              const AudioFrameDecodedCallback callback);
  void ReturnDecodedFrameWithPlayoutDelay(
      scoped_ptr<PcmAudioFrame> audio_frame, uint32 rtp_timestamp,
      const AudioFrameDecodedCallback callback);

  // Return the playout time based on the current time and rtp timestamp.
  base::TimeTicks GetPlayoutTime(base::TimeTicks now, uint32 rtp_timestamp);

  void InitializeTimers();

  // Decrypts the data within the |audio_frame| and replaces the data with the
  // decrypted string.
  bool DecryptAudioFrame(scoped_ptr<EncodedAudioFrame>* audio_frame);

  // Schedule the next RTCP report.
  void ScheduleNextRtcpReport();

  // Actually send the next RTCP report.
  void SendNextRtcpReport();

  // Schedule timing for the next cast message.
  void ScheduleNextCastMessage();

  // Actually send the next cast message.
  void SendNextCastMessage();

  scoped_refptr<CastEnvironment> cast_environment_;
  base::WeakPtrFactory<AudioReceiver> weak_factory_;

  const AudioCodec codec_;
  const int frequency_;
  base::TimeDelta target_delay_delta_;
  scoped_ptr<Framer> audio_buffer_;
  scoped_ptr<AudioDecoder> audio_decoder_;
  scoped_ptr<LocalRtpAudioData> incoming_payload_callback_;
  scoped_ptr<LocalRtpAudioFeedback> incoming_payload_feedback_;
  scoped_ptr<RtpReceiver> rtp_receiver_;
  scoped_ptr<Rtcp> rtcp_;
  scoped_ptr<RtpReceiverStatistics> rtp_audio_receiver_statistics_;
  base::TimeDelta time_offset_;
  base::TimeTicks time_first_incoming_packet_;
  uint32 first_incoming_rtp_timestamp_;
  scoped_ptr<crypto::Encryptor> decryptor_;
  std::string iv_mask_;

  std::list<AudioFrameEncodedCallback> queued_encoded_callbacks_;
  std::list<DecodedAudioCallbackData> queued_decoded_callbacks_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_AUDIO_RECEIVER_AUDIO_RECEIVER_H_
