// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CAST_CONFIG_H_
#define MEDIA_CAST_CAST_CONFIG_H_

#include <list>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/cast/cast_defines.h"
#include "media/cast/net/cast_transport_config.h"

namespace media {
class VideoEncodeAccelerator;

namespace cast {

// TODO(miu): Merge AudioSenderConfig and VideoSenderConfig and make their
// naming/documentation consistent with FrameReceiverConfig.
struct AudioSenderConfig {
  AudioSenderConfig();
  ~AudioSenderConfig();

  // Identifier referring to the sender, used by the receiver.
  uint32 ssrc;

  // The receiver's SSRC identifier.
  uint32 incoming_feedback_ssrc;

  int rtcp_interval;

  // The total amount of time between a frame's capture/recording on the sender
  // and its playback on the receiver (i.e., shown to a user).  This is fixed as
  // a value large enough to give the system sufficient time to encode,
  // transmit/retransmit, receive, decode, and render; given its run-time
  // environment (sender/receiver hardware performance, network conditions,
  // etc.).
  base::TimeDelta target_playout_delay;

  // RTP payload type enum: Specifies the type/encoding of frame data.
  int rtp_payload_type;

  bool use_external_encoder;
  int frequency;
  int channels;
  int bitrate;  // Set to <= 0 for "auto variable bitrate" (libopus knows best).
  Codec codec;

  // The AES crypto key and initialization vector.  Each of these strings
  // contains the data in binary form, of size kAesKeySize.  If they are empty
  // strings, crypto is not being used.
  std::string aes_key;
  std::string aes_iv_mask;
};

struct VideoSenderConfig {
  VideoSenderConfig();
  ~VideoSenderConfig();

  // Identifier referring to the sender, used by the receiver.
  uint32 ssrc;

  // The receiver's SSRC identifier.
  uint32 incoming_feedback_ssrc;  // TODO(miu): Rename to receiver_ssrc.

  int rtcp_interval;

  // The total amount of time between a frame's capture/recording on the sender
  // and its playback on the receiver (i.e., shown to a user).  This is fixed as
  // a value large enough to give the system sufficient time to encode,
  // transmit/retransmit, receive, decode, and render; given its run-time
  // environment (sender/receiver hardware performance, network conditions,
  // etc.).
  base::TimeDelta target_playout_delay;

  // RTP payload type enum: Specifies the type/encoding of frame data.
  int rtp_payload_type;

  bool use_external_encoder;
  int width;  // Incoming frames will be scaled to this size.
  int height;

  float congestion_control_back_off;
  int max_bitrate;
  int min_bitrate;
  int start_bitrate;
  int max_qp;
  int min_qp;
  int max_frame_rate;
  int max_number_of_video_buffers_used;  // Max value depend on codec.
  Codec codec;
  int number_of_encode_threads;

  // The AES crypto key and initialization vector.  Each of these strings
  // contains the data in binary form, of size kAesKeySize.  If they are empty
  // strings, crypto is not being used.
  std::string aes_key;
  std::string aes_iv_mask;
};

// TODO(miu): Naming and minor type changes are badly needed in a later CL.
struct FrameReceiverConfig {
  FrameReceiverConfig();
  ~FrameReceiverConfig();

  // The receiver's SSRC identifier.
  uint32 feedback_ssrc;  // TODO(miu): Rename to receiver_ssrc for clarity.

  // The sender's SSRC identifier.
  uint32 incoming_ssrc;  // TODO(miu): Rename to sender_ssrc for clarity.

  // Mean interval (in milliseconds) between RTCP reports.
  // TODO(miu): Remove this since it's never not kDefaultRtcpIntervalMs.
  int rtcp_interval;

  // The total amount of time between a frame's capture/recording on the sender
  // and its playback on the receiver (i.e., shown to a user).  This is fixed as
  // a value large enough to give the system sufficient time to encode,
  // transmit/retransmit, receive, decode, and render; given its run-time
  // environment (sender/receiver hardware performance, network conditions,
  // etc.).
  int rtp_max_delay_ms;  // TODO(miu): Change to TimeDelta target_playout_delay.

  // RTP payload type enum: Specifies the type/encoding of frame data.
  int rtp_payload_type;

  // RTP timebase: The number of RTP units advanced per one second.  For audio,
  // this is the sampling rate.  For video, by convention, this is 90 kHz.
  int frequency;  // TODO(miu): Rename to rtp_timebase for clarity.

  // Number of channels.  For audio, this is normally 2.  For video, this must
  // be 1 as Cast does not have support for stereoscopic video.
  int channels;

  // The target frame rate.  For audio, this is normally 100 (i.e., frames have
  // a duration of 10ms each).  For video, this is normally 30, but any frame
  // rate is supported.
  int max_frame_rate;  // TODO(miu): Rename to target_frame_rate.

  // Codec used for the compression of signal data.
  // TODO(miu): Merge the AudioCodec and VideoCodec enums into one so this union
  // is not necessary.
  Codec codec;

  // The AES crypto key and initialization vector.  Each of these strings
  // contains the data in binary form, of size kAesKeySize.  If they are empty
  // strings, crypto is not being used.
  std::string aes_key;
  std::string aes_iv_mask;
};

// Import from media::cast.

typedef Packet Packet;
typedef PacketList PacketList;

typedef base::Callback<void(CastInitializationStatus)>
    CastInitializationCallback;

typedef base::Callback<void(scoped_refptr<base::SingleThreadTaskRunner>,
                            scoped_ptr<media::VideoEncodeAccelerator>)>
    ReceiveVideoEncodeAcceleratorCallback;
typedef base::Callback<void(const ReceiveVideoEncodeAcceleratorCallback&)>
    CreateVideoEncodeAcceleratorCallback;

typedef base::Callback<void(scoped_ptr<base::SharedMemory>)>
    ReceiveVideoEncodeMemoryCallback;
typedef base::Callback<void(size_t size,
                            const ReceiveVideoEncodeMemoryCallback&)>
    CreateVideoEncodeMemoryCallback;

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_CONFIG_H_
