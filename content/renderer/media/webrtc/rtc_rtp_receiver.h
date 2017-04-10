// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_RECEIVER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_RECEIVER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
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

  RTCRtpReceiver(webrtc::RtpReceiverInterface* webrtc_rtp_receiver,
                 const blink::WebMediaStreamTrack& web_track);
  ~RTCRtpReceiver() override;

  uintptr_t Id() const override;

  const blink::WebMediaStreamTrack& Track() const override;
  const webrtc::MediaStreamTrackInterface& webrtc_track() const;

 private:
  const scoped_refptr<webrtc::RtpReceiverInterface> webrtc_rtp_receiver_;
  const blink::WebMediaStreamTrack web_track_;

  DISALLOW_COPY_AND_ASSIGN(RTCRtpReceiver);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_RECEIVER_H_
