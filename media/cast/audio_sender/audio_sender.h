// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_AUDIO_SENDER_H_
#define MEDIA_CAST_AUDIO_SENDER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/transport/rtp_sender/rtp_sender.h"

namespace media {
class AudioBus;
}

namespace media {
namespace cast {

class AudioEncoder;
class LocalRtcpAudioSenderFeedback;
class LocalRtpSenderStatistics;

// This class is not thread safe.
// It's only called from the main cast thread.
class AudioSender : public base::NonThreadSafe,
                    public base::SupportsWeakPtr<AudioSender> {
 public:
  AudioSender(scoped_refptr<CastEnvironment> cast_environment,
              const AudioSenderConfig& audio_config,
              transport::CastTransportSender* const transport_sender);

  virtual ~AudioSender();

  CastInitializationStatus InitializationResult() const {
    return cast_initialization_cb_;
  }

  // The |audio_bus| must be valid until the |done_callback| is called.
  // The callback is called from the main cast thread as soon as the encoder is
  // done with |audio_bus|; it does not mean that the encoded data has been
  // sent out.
  void InsertAudio(const AudioBus* audio_bus,
                   const base::TimeTicks& recorded_time,
                   const base::Closure& done_callback);

  // Only called from the main cast thread.
  void IncomingRtcpPacket(scoped_ptr<Packet> packet);

 protected:
  void SendEncodedAudioFrame(
      scoped_ptr<transport::EncodedAudioFrame> audio_frame,
      const base::TimeTicks& recorded_time);

 private:
  friend class LocalRtcpAudioSenderFeedback;

  void SendEncodedAudioFrameToTransport(
      scoped_ptr<transport::EncodedAudioFrame> audio_frame,
      const base::TimeTicks& recorded_time);

  void ResendPackets(
      const MissingFramesAndPacketsMap& missing_frames_and_packets);

  void ResendPacketsOnTransportThread(
      const transport::MissingFramesAndPacketsMap& missing_packets);

  void ScheduleNextRtcpReport();
  void SendRtcpReport();

  void InitializeTimers();

  base::WeakPtrFactory<AudioSender> weak_factory_;

  scoped_refptr<CastEnvironment> cast_environment_;
  transport::CastTransportSender* const transport_sender_;
  scoped_refptr<AudioEncoder> audio_encoder_;
  scoped_ptr<LocalRtpSenderStatistics> rtp_audio_sender_statistics_;
  scoped_ptr<LocalRtcpAudioSenderFeedback> rtcp_feedback_;
  Rtcp rtcp_;
  bool timers_initialized_;
  CastInitializationStatus cast_initialization_cb_;

  DISALLOW_COPY_AND_ASSIGN(AudioSender);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_AUDIO_SENDER_H_
