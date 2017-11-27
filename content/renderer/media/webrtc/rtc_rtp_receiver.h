// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_RECEIVER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_RECEIVER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_adapter_map.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter_map.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebRTCRtpReceiver.h"
#include "third_party/webrtc/api/mediastreaminterface.h"
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
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> webrtc_receiver,
      std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
          track_adapter,
      std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
          stream_adapter_refs);
  ~RTCRtpReceiver() override;

  // Creates a shallow copy of the receiver, representing the same underlying
  // webrtc receiver as the original.
  std::unique_ptr<RTCRtpReceiver> ShallowCopy() const;

  uintptr_t Id() const override;
  const blink::WebMediaStreamTrack& Track() const override;
  blink::WebVector<blink::WebMediaStream> Streams() const override;
  blink::WebVector<std::unique_ptr<blink::WebRTCRtpContributingSource>>
  GetSources() override;

  webrtc::RtpReceiverInterface* webrtc_receiver() const;
  const webrtc::MediaStreamTrackInterface& webrtc_track() const;
  bool HasStream(const webrtc::MediaStreamInterface* webrtc_stream) const;
  std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
  StreamAdapterRefs() const;

 private:
  const rtc::scoped_refptr<webrtc::RtpReceiverInterface> webrtc_receiver_;

  // The track adapter is the glue between blink and webrtc layer tracks.
  // Keeping a reference to the adapter ensures it is not disposed, as is
  // required as long as the webrtc layer track is in use by the receiver.
  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_adapter_;
  // Similarly, references needs to be kept to the stream adapters of streams
  // associated with the receiver.
  std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
      stream_adapter_refs_;

  DISALLOW_COPY_AND_ASSIGN(RTCRtpReceiver);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_RECEIVER_H_
