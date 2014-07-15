// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CAST_TRANSPORT_HOST_FILTER_H_
#define CHROME_BROWSER_MEDIA_CAST_TRANSPORT_HOST_FILTER_H_

#include "base/id_map.h"
#include "base/time/default_tick_clock.h"
#include "chrome/common/cast_messages.h"
#include "content/public/browser/browser_message_filter.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/net/cast_transport_sender.h"

namespace cast {

class CastTransportHostFilter : public content::BrowserMessageFilter {
 public:
  CastTransportHostFilter();
 private:
  virtual ~CastTransportHostFilter();

  void NotifyStatusChange(
      int32 channel_id,
      media::cast::CastTransportStatus result);
  void ReceivedPacket(
      int32 channel_id,
      scoped_ptr<media::cast::Packet> result);
  void RawEvents(int32 channel_id,
                 const std::vector<media::cast::PacketEvent>& packet_events);

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Forwarding functions.
  void OnInitializeAudio(
      int32 channel_id,
      const media::cast::CastTransportRtpConfig& config);
  void OnInitializeVideo(
      int32 channel_id,
      const media::cast::CastTransportRtpConfig& config);
  void OnInsertCodedAudioFrame(
      int32 channel_id,
      const media::cast::EncodedFrame& audio_frame);
  void OnInsertCodedVideoFrame(
      int32 channel_id,
      const media::cast::EncodedFrame& video_frame);
  void OnSendRtcpFromRtpSender(
      int32 channel_id,
      const media::cast::SendRtcpFromRtpSenderData& data,
      const media::cast::RtcpDlrrReportBlock& dlrr);
  void OnResendPackets(
      int32 channel_id,
      bool is_audio,
      const media::cast::MissingFramesAndPacketsMap& missing_packets,
      bool cancel_rtx_if_not_in_list,
      base::TimeDelta dedupe_window);
  void OnNew(
      int32 channel_id,
      const net::IPEndPoint& remote_end_point);
  void OnDelete(int32 channel_id);

  IDMap<media::cast::CastTransportSender, IDMapOwnPointer> id_map_;

  // Clock used by Cast transport.
  base::DefaultTickClock clock_;

  DISALLOW_COPY_AND_ASSIGN(CastTransportHostFilter);
};

}  // namespace cast

#endif  // CHROME_BROWSER_MEDIA_CAST_TRANSPORT_HOST_FILTER_H_
