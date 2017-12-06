// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_SENDER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_SENDER_H_

#include <memory>
#include <vector>

#include "content/common/content_export.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_adapter_map.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter_map.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebRTCRtpSender.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"
#include "third_party/webrtc/api/rtpsenderinterface.h"
#include "third_party/webrtc/rtc_base/scoped_ref_ptr.h"

namespace content {

// Used to surface |webrtc::RtpSenderInterface| to blink. Multiple
// |RTCRtpSender|s could reference the same webrtc sender; |id| is the value
// of the pointer to the webrtc sender.
class CONTENT_EXPORT RTCRtpSender : public blink::WebRTCRtpSender {
 public:
  static uintptr_t getId(const webrtc::RtpSenderInterface* webrtc_sender);

  RTCRtpSender(
      rtc::scoped_refptr<webrtc::RtpSenderInterface> webrtc_sender,
      std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref);
  RTCRtpSender(
      rtc::scoped_refptr<webrtc::RtpSenderInterface> webrtc_sender,
      std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref,
      std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
          stream_refs);
  RTCRtpSender(const RTCRtpSender& other);
  ~RTCRtpSender() override;

  RTCRtpSender& operator=(const RTCRtpSender& other);

  // Creates a shallow copy of the sender, representing the same underlying
  // webrtc sender as the original.
  // TODO(hbos): Remove in favor of constructor. https://crbug.com/790007
  std::unique_ptr<RTCRtpSender> ShallowCopy() const;

  uintptr_t Id() const override;
  blink::WebMediaStreamTrack Track() const override;

  webrtc::RtpSenderInterface* webrtc_sender() const;
  const webrtc::MediaStreamTrackInterface* webrtc_track() const;
  bool RemoveFromPeerConnection(webrtc::PeerConnectionInterface* pc);

 private:
  class RTCRtpSenderInternal;
  scoped_refptr<RTCRtpSenderInternal> internal_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_SENDER_H_
