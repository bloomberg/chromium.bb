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

SpdyRstStreamIR::SpdyRstStreamIR(SpdyStreamId stream_id,
                                 SpdyRstStreamStatus status,
                                 base::StringPiece description)
    : SpdyFrameWithStreamIdIR(stream_id),
      description_(description) {
  set_status(status);
}

SpdyRstStreamIR::~SpdyRstStreamIR() {}

void SpdyRstStreamIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitRstStream(*this);
}

SpdySettingsIR::SpdySettingsIR()
    : clear_settings_(false),
      is_ack_(false) {}

SpdySettingsIR::~SpdySettingsIR() {}

void SpdySettingsIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitSettings(*this);
}

void SpdyPingIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitPing(*this);
}

SpdyGoAwayIR::SpdyGoAwayIR(SpdyStreamId last_good_stream_id,
                           SpdyGoAwayStatus status,
                           const base::StringPiece& description)
    : description_(description) {
      set_last_good_stream_id(last_good_stream_id);
  set_status(status);
}

SpdyGoAwayIR::~SpdyGoAwayIR() {}

const base::StringPiece& SpdyGoAwayIR::description() const {
  return description_;
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

void SpdyBlockedIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitBlocked(*this);
}

void SpdyPushPromiseIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitPushPromise(*this);
}

void SpdyContinuationIR::Visit(SpdyFrameVisitor* visitor) const {
  return visitor->VisitContinuation(*this);
}

}  // namespace net
