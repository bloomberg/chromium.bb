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

void SpdyDataIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitData(*this);
}

void SpdySynStreamIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitSynStream(*this);
}

void SpdySynReplyIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitSynReply(*this);
}

void SpdyRstStreamIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitRstStream(*this);
}

SpdySettingsIR::SpdySettingsIR() : clear_settings_(false) {}

SpdySettingsIR::~SpdySettingsIR() {}

void SpdySettingsIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitSettings(*this);
}

void SpdyPingIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitPing(*this);
}

void SpdyGoAwayIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitGoAway(*this);
}

void SpdyHeadersIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitHeaders(*this);
}

void SpdyWindowUpdateIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitWindowUpdate(*this);
}

SpdyCredentialIR::SpdyCredentialIR(int16 slot) {
  set_slot(slot);
}

SpdyCredentialIR::~SpdyCredentialIR() {}

void SpdyCredentialIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitCredential(*this);
}

void SpdyBlockedIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitBlocked(*this);
}

void SpdyPushPromiseIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitPushPromise(*this);
}

}  // namespace net
