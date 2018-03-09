// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_PARAMETERS_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_PARAMETERS_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebRTCRtpParameters.h"
#include "third_party/webrtc/api/rtpparameters.h"

namespace content {

CONTENT_EXPORT blink::WebRTCRtpEncodingParameters
GetWebRTCRtpEncodingParameters(
    const webrtc::RtpEncodingParameters& encoding_parameters);

CONTENT_EXPORT blink::WebRTCRtpHeaderExtensionParameters
GetWebRTCRtpHeaderExtensionParameters(
    const webrtc::RtpHeaderExtensionParameters& extension_parameters);

CONTENT_EXPORT blink::WebRTCRtcpParameters GetWebRTCRtcpParameters();

CONTENT_EXPORT blink::WebRTCRtpCodecParameters GetWebRTCRtpCodecParameters(
    const webrtc::RtpCodecParameters& codec_parameters);

CONTENT_EXPORT blink::WebRTCRtpParameters GetWebRTCRtpParameters(
    const webrtc::RtpParameters& parameters);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_RTC_RTP_PARAMETERS_H_
