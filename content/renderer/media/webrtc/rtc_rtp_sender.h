// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_SENDER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_SENDER_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebRTCRtpSender.h"
#include "third_party/webrtc/api/rtpsenderinterface.h"

namespace content {

// Used to surface |webrtc::RtpSenderInterface| to blink. Multiple
// |RTCRtpSender|s could reference the same webrtc receiver; |id| is the value
// of the pointer to the webrtc sender.
class CONTENT_EXPORT RTCRtpSender : public blink::WebRTCRtpSender {
 public:
  static uintptr_t getId(const webrtc::RtpSenderInterface* webrtc_rtp_sender);

  RTCRtpSender(webrtc::RtpSenderInterface* webrtc_rtp_sender,
               std::unique_ptr<blink::WebMediaStreamTrack> web_track);
  ~RTCRtpSender() override;

  uintptr_t Id() const override;
  const blink::WebMediaStreamTrack* Track() const override;

  const webrtc::MediaStreamTrackInterface* webrtc_track() const;

 private:
  const scoped_refptr<webrtc::RtpSenderInterface> webrtc_rtp_sender_;
  std::unique_ptr<blink::WebMediaStreamTrack> web_track_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_SENDER_H_
