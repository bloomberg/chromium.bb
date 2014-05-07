// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_flow_controller.h"

#include "base/basictypes.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_protocol.h"

namespace net {

#define ENDPOINT (is_server_ ? "Server: " : " Client: ")

QuicFlowController::QuicFlowController(QuicVersion version,
                                       QuicStreamId id,
                                       bool is_server,
                                       uint64 send_window_offset,
                                       uint64 receive_window_offset,
                                       uint64 max_receive_window)
      : id_(id),
        is_enabled_(true),
        is_server_(is_server),
        bytes_consumed_(0),
        bytes_buffered_(0),
        bytes_sent_(0),
        send_window_offset_(send_window_offset),
        receive_window_offset_(receive_window_offset),
        max_receive_window_(max_receive_window),
        last_blocked_send_window_offset_(0) {
  DVLOG(1) << ENDPOINT << "Created flow controller for stream " << id_
           << ", setting initial receive window offset to: "
           << receive_window_offset_
           << ", max receive window to: "
           << max_receive_window_
           << ", setting send window offset to: " << send_window_offset_;
  if (version < QUIC_VERSION_17) {
    DVLOG(1) << ENDPOINT << "Disabling QuicFlowController for stream " << id_
             << ", QUIC version " << version;
    Disable();
  }
}

void QuicFlowController::AddBytesConsumed(uint64 bytes_consumed) {
  if (!IsEnabled()) {
    return;
  }

  bytes_consumed_ += bytes_consumed;
  DVLOG(1) << ENDPOINT << "Stream " << id_ << " consumed: " << bytes_consumed_;
}

void QuicFlowController::AddBytesBuffered(uint64 bytes_buffered) {
  if (!IsEnabled()) {
    return;
  }

  bytes_buffered_ += bytes_buffered;
  DVLOG(1) << ENDPOINT << "Stream " << id_ << " buffered: " << bytes_buffered_;
}

void QuicFlowController::RemoveBytesBuffered(uint64 bytes_buffered) {
  if (!IsEnabled()) {
    return;
  }

  if (bytes_buffered_ < bytes_buffered) {
    LOG(DFATAL) << "Trying to remove " << bytes_buffered << " bytes, when only "
                << bytes_buffered_ << " bytes are buffered";
    bytes_buffered_ = 0;
    return;
  }

  bytes_buffered_ -= bytes_buffered;
  DVLOG(1) << ENDPOINT << "Stream " << id_ << " buffered: " << bytes_buffered_;
}

void QuicFlowController::AddBytesSent(uint64 bytes_sent) {
  if (!IsEnabled()) {
    return;
  }

  if (bytes_sent_ + bytes_sent > send_window_offset_) {
    LOG(DFATAL) << ENDPOINT << "Stream " << id_ << " Trying to send an extra "
                << bytes_sent << " bytes, when bytes_sent = " << bytes_sent_
                << ", and send_window_offset_ = " << send_window_offset_;
    bytes_sent_ = send_window_offset_;
    return;
  }

  bytes_sent_ += bytes_sent;
  DVLOG(1) << ENDPOINT << "Stream " << id_ << " sent: " << bytes_sent_;
}

bool QuicFlowController::FlowControlViolation() {
  if (!IsEnabled()) {
    return false;
  }

  if (receive_window_offset_ < TotalReceivedBytes()) {
    LOG(ERROR)
        << ENDPOINT << "Flow control violation on stream " << id_
        << ", receive window: " << receive_window_offset_
        << ", bytes received: " << TotalReceivedBytes();

    return true;
  }
  return false;
}

void QuicFlowController::MaybeSendWindowUpdate(QuicConnection* connection) {
  if (!IsEnabled()) {
    return;
  }

  // Send WindowUpdate to increase receive window if
  // (receive window offset - consumed bytes) < (max window / 2).
  // This is behaviour copied from SPDY.
  size_t consumed_window = receive_window_offset_ - bytes_consumed_;
  size_t threshold = (max_receive_window_ / 2);

  if (consumed_window < threshold) {
    // Update our receive window.
    receive_window_offset_ += (max_receive_window_ - consumed_window);

    DVLOG(1) << ENDPOINT << "Sending WindowUpdate frame for stream " << id_
             << ", consumed bytes: " << bytes_consumed_
             << ", consumed window: " << consumed_window
             << ", and threshold: " << threshold
             << ", and max recvw: " << max_receive_window_
             << ". New receive window offset is: " << receive_window_offset_;

    // Inform the peer of our new receive window.
    connection->SendWindowUpdate(id_, receive_window_offset_);
  }
}

void QuicFlowController::MaybeSendBlocked(QuicConnection* connection) {
  if (!IsEnabled()) {
    return;
  }

  if (SendWindowSize() == 0 &&
      last_blocked_send_window_offset_ < send_window_offset_) {
    DVLOG(1) << ENDPOINT << "Stream " << id_ << " is flow control blocked. "
             << "Send window: " << SendWindowSize()
             << ", bytes sent: " << bytes_sent_
             << ", send limit: " << send_window_offset_;
    // The entire send_window has been consumed, we are now flow control
    // blocked.
    connection->SendBlocked(id_);

    // Keep track of when we last sent a BLOCKED frame so that we only send one
    // at a given send offset.
    last_blocked_send_window_offset_ = send_window_offset_;
  }
}

bool QuicFlowController::UpdateSendWindowOffset(uint64 new_send_window_offset) {
  if (!IsEnabled()) {
    return false;
  }

  // Only update if send window has increased.
  if (new_send_window_offset <= send_window_offset_) {
    return false;
  }

  DVLOG(1) << ENDPOINT << "UpdateSendWindowOffset for stream " << id_
           << " with new offset " << new_send_window_offset
           << " , current offset: " << send_window_offset_;

  send_window_offset_ = new_send_window_offset;
  return true;
}

void QuicFlowController::Disable() {
  is_enabled_ = false;
}

bool QuicFlowController::IsEnabled() const {
  bool connection_flow_control_enabled =
      (id_ == kConnectionLevelId &&
       FLAGS_enable_quic_connection_flow_control);
  bool stream_flow_control_enabled =
      (id_ != kConnectionLevelId &&
       FLAGS_enable_quic_stream_flow_control_2);
  return (connection_flow_control_enabled || stream_flow_control_enabled) &&
         is_enabled_;
}

bool QuicFlowController::IsBlocked() const {
  return IsEnabled() && SendWindowSize() == 0;
}

uint64 QuicFlowController::SendWindowSize() const {
  if (bytes_sent_ > send_window_offset_) {
    return 0;
  }
  return send_window_offset_ - bytes_sent_;
}

uint64 QuicFlowController::TotalReceivedBytes() const {
  return bytes_consumed_ + bytes_buffered_;
}

}  // namespace net
