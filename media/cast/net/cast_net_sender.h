// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is the main interface for the cast net sender. The cast sender handles
// the cast pipeline from encoded frames (both audio and video), to encryption,
// packetization and transport.
// All configurations are done at creation.

#ifndef MEDIA_CAST_NET_CAST_NET_SENDER_H_
#define MEDIA_CAST_NET_CAST_NET_SENDER_H_

#include "base/basictypes.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/tick_clock.h"
#include "media/cast/net/cast_net_defines.h"

namespace media {
class AudioBus;
class VideoFrame;
}

namespace media {
namespace cast {
namespace transport {

class CastNetNotification {
  public:
    enum CastNetStatus {
      UNINITIALIZED,
      INITIALIZED,
      INVALID_CRYPTO_CONFIG,
      SOCKET_ERROR,
      // TODO(mikhal): Add.
    };

 virtual void NotifyStatusChange(CastNetStatus result) = 0;
 virtual ~CastNetNotification() {}
};

// This Class is not thread safe.
// The application should only trigger this class from one thread.
class CastNetSender : public base::NonThreadSafe {
 public:
  static CastNetSender* CreateCastNetSender(
      base::TickClock* clock,
      const CastNetConfig& config,
      CastNetNotification* const notifier,
      scoped_refptr<PacketReceiver> packet_receiver);

  virtual ~CastNetSender() {}
  virtual void InsertCodedAudioFrame(const AudioBus* audio_bus,
                                     const base::TimeTicks& recorded_time) = 0;

  virtual void InsertCodedVideoFrame(const EncodedVideoFrame* video_frame,
                                     const base::TimeTicks& capture_time) = 0;

  virtual void SendRtcpFromRtpSender(
      uint32 packet_type_flags,
      const RtcpSenderInfo& sender_info,
      const RtcpDlrrReportBlock& dlrr,
      const RtcpSenderLogMessage& sender_log) = 0;

  virtual void ResendPackets(
      const MissingFramesAndPacketsMap& missing_packets) = 0;
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_CAST_NET_SENDER_H_
