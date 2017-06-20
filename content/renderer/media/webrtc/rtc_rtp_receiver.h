// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_RECEIVER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_RECEIVER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter_map.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebRTCRtpReceiver.h"
#include "third_party/webrtc/api/rtpreceiverinterface.h"

namespace content {

// Used to surface |webrtc::RtpReceiverInterface| to blink. Multiple
// |RTCRtpReceiver|s could reference the same webrtc receiver; |id| is the value
// of the pointer to the webrtc receiver.
class CONTENT_EXPORT RTCRtpReceiver : public blink::WebRTCRtpReceiver {
 public:
  static uintptr_t getId(
      const webrtc::RtpReceiverInterface* webrtc_rtp_receiver);

  RTCRtpReceiver(
      scoped_refptr<webrtc::RtpReceiverInterface> webrtc_rtp_receiver,
      std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
          track_adapter);
  ~RTCRtpReceiver() override;

  uintptr_t Id() const override;
  const blink::WebMediaStreamTrack& Track() const override;
  blink::WebVector<std::unique_ptr<blink::WebRTCRtpContributingSource>>
  GetSources() override;

  const webrtc::MediaStreamTrackInterface& webrtc_track() const;

 private:
  const scoped_refptr<webrtc::RtpReceiverInterface> webrtc_rtp_receiver_;
  // The track adapter is the glue between blink and webrtc layer tracks.
  // Keeping a reference to the adapter ensures it is not disposed, as is
  // required as long as the webrtc layer track is in use by the receiver.
  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_adapter_;

  DISALLOW_COPY_AND_ASSIGN(RTCRtpReceiver);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_RECEIVER_H_
