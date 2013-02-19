// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_protocol.h"

namespace net {

SpdyFrameWithNameValueBlockIR::SpdyFrameWithNameValueBlockIR(
    SpdyStreamId stream_id) : SpdyFrameWithFinIR(stream_id) {}

SpdyFrameWithNameValueBlockIR::~SpdyFrameWithNameValueBlockIR() {}

SpdyDataIR::SpdyDataIR(SpdyStreamId stream_id, const base::StringPiece& data)
    : SpdyFrameWithFinIR(stream_id) {
  SetDataDeep(data);
}

SpdyDataIR::SpdyDataIR(SpdyStreamId stream_id)
    : SpdyFrameWithFinIR(stream_id) {}

SpdyDataIR::~SpdyDataIR() {}

SpdySettingsIR::SpdySettingsIR() : clear_settings_(false) {}

SpdySettingsIR::~SpdySettingsIR() {}

SpdyCredentialIR::SpdyCredentialIR(int16 slot) {
  set_slot(slot);
}

SpdyCredentialIR::~SpdyCredentialIR() {}

}  // namespace
