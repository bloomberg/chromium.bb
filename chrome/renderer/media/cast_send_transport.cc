// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_send_transport.h"

#include "base/logging.h"
#include "chrome/renderer/media/cast_session.h"
#include "chrome/renderer/media/cast_udp_transport.h"

CastCodecSpecificParams::CastCodecSpecificParams() {
}

CastCodecSpecificParams::~CastCodecSpecificParams() {
}

CastRtpPayloadParams::CastRtpPayloadParams()
    : payload_type(0),
      ssrc(0),
      clock_rate(0),
      max_bitrate(0),
      min_bitrate(0),
      channels(0),
      width(0),
      height(0) {
}

CastRtpPayloadParams::~CastRtpPayloadParams() {
}

CastRtpCaps::CastRtpCaps() {
}

CastRtpCaps::~CastRtpCaps() {
}

CastSendTransport::CastSendTransport(CastUdpTransport* udp_transport)
    : cast_session_(udp_transport->cast_session()) {
}

CastSendTransport::~CastSendTransport() {
}

CastRtpCaps CastSendTransport::GetCaps() {
  NOTIMPLEMENTED();
  return CastRtpCaps();
}

CastRtpParams CastSendTransport::GetParams() {
  NOTIMPLEMENTED();
  return CastRtpParams();
}

CastRtpParams CastSendTransport::CreateParams(
    const CastRtpCaps& remote_caps) {
  NOTIMPLEMENTED();
  return CastRtpParams();
}

void CastSendTransport::Start(const CastRtpParams& params) {
  NOTIMPLEMENTED();
}

void CastSendTransport::Stop() {
  NOTIMPLEMENTED();
}
