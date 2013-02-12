// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_protocol.h"

namespace net {

SpdyFrameWithNameValueBlockIR::SpdyFrameWithNameValueBlockIR(
    SpdyStreamId stream_id) : SpdyFrameWithFinIR(stream_id) {}

SpdyFrameWithNameValueBlockIR::~SpdyFrameWithNameValueBlockIR() {}

SpdySettingsIR::SpdySettingsIR() : clear_settings_(false) {}

SpdySettingsIR::~SpdySettingsIR() {}

SpdyCredentialIR::SpdyCredentialIR(int16 slot) {
  set_slot(slot);
}

SpdyCredentialIR::~SpdyCredentialIR() {}

}  // namespace
