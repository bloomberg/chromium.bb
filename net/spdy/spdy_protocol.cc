// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_protocol.h"

namespace net {

SpdyFrameWithNameValueBlockIR::SpdyFrameWithNameValueBlockIR(
    SpdyStreamId stream_id) : SpdyFrameWithFinIR(stream_id) {}

SpdyFrameWithNameValueBlockIR::~SpdyFrameWithNameValueBlockIR() {}

SpdyDataIR::SpdyDataIR(SpdyStreamId stream_id, const base::StringPiece& data)
    : SpdyFrameWithFinIR(stream_id),
      pad_low_(false),
      pad_high_(false),
      padding_payload_len_(0) {
  SetDataDeep(data);
}

SpdyDataIR::SpdyDataIR(SpdyStreamId stream_id)
    : SpdyFrameWithFinIR(stream_id),
      pad_low_(false),
      pad_high_(false),
      padding_payload_len_(0) {}

SpdyDataIR::~SpdyDataIR() {}

bool SpdyConstants::IsValidFrameType(SpdyMajorVersion version,
                                     int frame_type_field) {
  switch (version) {
    case SPDY2:
    case SPDY3:
      // SYN_STREAM is the first valid frame.
      if (frame_type_field < SerializeFrameType(version, SYN_STREAM)) {
        return false;
      }

      // WINDOW_UPDATE is the last valid frame.
      if (frame_type_field > SerializeFrameType(version, WINDOW_UPDATE)) {
        return false;
      }

      // The valid range is non-contiguous.
      if (frame_type_field == NOOP) {
        return false;
      }

      return true;
    case SPDY4:
      // DATA is the first valid frame.
      if (frame_type_field < SerializeFrameType(version, DATA)) {
        return false;
      }

      // BLOCKED is the last valid frame.
      if (frame_type_field > SerializeFrameType(version, BLOCKED)) {
        return false;
      }

      return true;
  }

  LOG(DFATAL) << "Unhandled SPDY version " << version;
  return false;
}

SpdyFrameType SpdyConstants::ParseFrameType(SpdyMajorVersion version,
                                            int frame_type_field) {
  switch (version) {
    case SPDY2:
    case SPDY3:
      switch (frame_type_field) {
        case 1:
          return SYN_STREAM;
        case 2:
          return SYN_REPLY;
        case 3:
          return RST_STREAM;
        case 4:
          return SETTINGS;
        case 6:
          return PING;
        case 7:
          return GOAWAY;
        case 8:
          return HEADERS;
        case 9:
          return WINDOW_UPDATE;
      }
      break;
    case SPDY4:
      switch (frame_type_field) {
        case 0:
          return DATA;
        case 1:
          return HEADERS;
        // TODO(hkhalil): Add PRIORITY.
        case 3:
          return RST_STREAM;
        case 4:
          return SETTINGS;
        case 5:
          return PUSH_PROMISE;
        case 6:
          return PING;
        case 7:
          return GOAWAY;
        case 8:
          return WINDOW_UPDATE;
        case 9:
          return CONTINUATION;
        case 10:
          return BLOCKED;
      }
      break;
  }

  LOG(DFATAL) << "Unhandled frame type " << frame_type_field;
  return DATA;
}

int SpdyConstants::SerializeFrameType(SpdyMajorVersion version,
                                      SpdyFrameType frame_type) {
  switch (version) {
    case SPDY2:
    case SPDY3:
      switch (frame_type) {
        case SYN_STREAM:
          return 1;
        case SYN_REPLY:
          return 2;
        case RST_STREAM:
          return 3;
        case SETTINGS:
          return 4;
        case PING:
          return 6;
        case GOAWAY:
          return 7;
        case HEADERS:
          return 8;
        case WINDOW_UPDATE:
          return 9;
        default:
          LOG(DFATAL) << "Serializing unhandled frame type " << frame_type;
          return -1;
      }
    case SPDY4:
      switch (frame_type) {
        case DATA:
          return 0;
        case HEADERS:
          return 1;
        // TODO(hkhalil): Add PRIORITY.
        case RST_STREAM:
          return 3;
        case SETTINGS:
          return 4;
        case PUSH_PROMISE:
          return 5;
        case PING:
          return 6;
        case GOAWAY:
          return 7;
        case WINDOW_UPDATE:
          return 8;
        case CONTINUATION:
          return 9;
        case BLOCKED:
          return 10;
        default:
          LOG(DFATAL) << "Serializing unhandled frame type " << frame_type;
          return -1;
      }
  }

  LOG(DFATAL) << "Unhandled SPDY version " << version;
  return -1;
}

bool SpdyConstants::IsValidSettingId(SpdyMajorVersion version,
                                     int setting_id_field) {
  switch (version) {
    case SPDY2:
    case SPDY3:
      // UPLOAD_BANDWIDTH is the first valid setting id.
      if (setting_id_field <
          SerializeSettingId(version, SETTINGS_UPLOAD_BANDWIDTH)) {
        return false;
      }

      // INITIAL_WINDOW_SIZE is the last valid setting id.
      if (setting_id_field >
          SerializeSettingId(version, SETTINGS_INITIAL_WINDOW_SIZE)) {
        return false;
      }

      return true;
    case SPDY4:
      // HEADER_TABLE_SIZE is the first valid setting id.
      if (setting_id_field <
          SerializeSettingId(version, SETTINGS_HEADER_TABLE_SIZE)) {
        return false;
      }

      // INITIAL_WINDOW_SIZE is the last valid setting id.
      if (setting_id_field >
          SerializeSettingId(version, SETTINGS_INITIAL_WINDOW_SIZE)) {
        return false;
      }

      return true;
  }

  LOG(DFATAL) << "Unhandled SPDY version " << version;
  return false;
}

SpdySettingsIds SpdyConstants::ParseSettingId(SpdyMajorVersion version,
                                              int setting_id_field) {
  switch (version) {
    case SPDY2:
    case SPDY3:
      switch (setting_id_field) {
        case 1:
          return SETTINGS_UPLOAD_BANDWIDTH;
        case 2:
          return SETTINGS_DOWNLOAD_BANDWIDTH;
        case 3:
          return SETTINGS_ROUND_TRIP_TIME;
        case 4:
          return SETTINGS_MAX_CONCURRENT_STREAMS;
        case 5:
          return SETTINGS_CURRENT_CWND;
        case 6:
          return SETTINGS_DOWNLOAD_RETRANS_RATE;
        case 7:
          return SETTINGS_INITIAL_WINDOW_SIZE;
      }
      break;
    case SPDY4:
      switch (setting_id_field) {
        case 1:
          return SETTINGS_HEADER_TABLE_SIZE;
        case 2:
          return SETTINGS_ENABLE_PUSH;
        case 3:
          return SETTINGS_MAX_CONCURRENT_STREAMS;
        case 4:
          return SETTINGS_INITIAL_WINDOW_SIZE;
      }
      break;
  }

  LOG(DFATAL) << "Unhandled setting ID " << setting_id_field;
  return SETTINGS_UPLOAD_BANDWIDTH;
}

int SpdyConstants::SerializeSettingId(SpdyMajorVersion version,
                                       SpdySettingsIds id) {
  switch (version) {
    case SPDY2:
    case SPDY3:
      switch (id) {
        case SETTINGS_UPLOAD_BANDWIDTH:
          return 1;
        case SETTINGS_DOWNLOAD_BANDWIDTH:
          return 2;
        case SETTINGS_ROUND_TRIP_TIME:
          return 3;
        case SETTINGS_MAX_CONCURRENT_STREAMS:
          return 4;
        case SETTINGS_CURRENT_CWND:
          return 5;
        case SETTINGS_DOWNLOAD_RETRANS_RATE:
          return 6;
        case SETTINGS_INITIAL_WINDOW_SIZE:
          return 7;
        default:
          LOG(DFATAL) << "Serializing unhandled setting id " << id;
          return -1;
      }
    case SPDY4:
      switch (id) {
        case SETTINGS_HEADER_TABLE_SIZE:
          return 1;
        case SETTINGS_ENABLE_PUSH:
          return 2;
        case SETTINGS_MAX_CONCURRENT_STREAMS:
          return 3;
        case SETTINGS_INITIAL_WINDOW_SIZE:
          return 4;
        default:
          LOG(DFATAL) << "Serializing unhandled setting id " << id;
          return -1;
      }
  }
  LOG(DFATAL) << "Unhandled SPDY version " << version;
  return -1;
}

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
