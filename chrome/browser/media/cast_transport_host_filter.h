// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CAST_TRANSPORT_HOST_FILTER_H_
#define CHROME_BROWSER_MEDIA_CAST_TRANSPORT_HOST_FILTER_H_

#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/default_tick_clock.h"
#include "chrome/common/cast_messages.h"
#include "content/public/browser/browser_message_filter.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/net/cast_transport_sender.h"

namespace content {
class PowerSaveBlocker;
}  // namespace content

namespace cast {

class CastTransportHostFilter : public content::BrowserMessageFilter {
 public:
  CastTransportHostFilter();
 private:
  virtual ~CastTransportHostFilter();

  void NotifyStatusChange(
      int32 channel_id,
      media::cast::CastTransportStatus result);
  void SendRawEvents(
      int32 channel_id,
      const std::vector<media::cast::PacketEvent>& packet_events,
      const std::vector<media::cast::FrameEvent>& frame_events);
  void SendRtt(int32 channel_id, uint32 ssrc, base::TimeDelta rtt);
  void SendCastMessage(int32 channel_id,
                       uint32 ssrc,
                       const media::cast::RtcpCastMessage& cast_message);

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Forwarding functions.
  void OnInitializeAudio(
      int32 channel_id,
      const media::cast::CastTransportRtpConfig& config);
  void OnInitializeVideo(
      int32 channel_id,
      const media::cast::CastTransportRtpConfig& config);
  void OnInsertFrame(
      int32 channel_id,
      uint32 ssrc,
      const media::cast::EncodedFrame& frame);
  void OnSendSenderReport(
      int32 channel_id,
      uint32 ssrc,
      base::TimeTicks current_time,
      uint32 current_time_as_rtp_timestamp);
  void OnCancelSendingFrames(int32 channel_id, uint32 ssrc,
                             const std::vector<uint32>& frame_ids);
  void OnResendFrameForKickstart(int32 channel_id, uint32 ssrc,
                                 uint32 frame_id);
  void OnNew(
      int32 channel_id,
      const net::IPEndPoint& remote_end_point,
      const base::DictionaryValue& options);
  void OnDelete(int32 channel_id);

  IDMap<media::cast::CastTransportSender, IDMapOwnPointer> id_map_;

  // Clock used by Cast transport.
  base::DefaultTickClock clock_;

  // While |id_map_| is non-empty, hold an instance of
  // content::PowerSaveBlocker.  This prevents Chrome from being suspended while
  // remoting content.
  scoped_ptr<content::PowerSaveBlocker> power_save_blocker_;

  base::WeakPtrFactory<CastTransportHostFilter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastTransportHostFilter);
};

}  // namespace cast

#endif  // CHROME_BROWSER_MEDIA_CAST_TRANSPORT_HOST_FILTER_H_
