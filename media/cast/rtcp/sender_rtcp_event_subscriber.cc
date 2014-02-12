// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtcp/sender_rtcp_event_subscriber.h"

#include <utility>

#include "base/logging.h"
#include "media/cast/rtcp/rtcp_defines.h"

namespace media {
namespace cast {

SenderRtcpEventSubscriber::SenderRtcpEventSubscriber(
    const size_t max_size_to_retain)
    : max_size_to_retain_(max_size_to_retain) {
  DCHECK(max_size_to_retain_ > 0u);
}

SenderRtcpEventSubscriber::~SenderRtcpEventSubscriber() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void SenderRtcpEventSubscriber::OnReceiveFrameEvent(
    const FrameEvent& frame_event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (frame_event.type != kVideoFrameCaptured &&
      frame_event.type != kVideoFrameSentToEncoder &&
      frame_event.type != kVideoFrameEncoded) {
    // Not interested in other events.
    return;
  }

  RtcpEventMap::iterator it = rtcp_events_.find(frame_event.rtp_timestamp);
  if (it == rtcp_events_.end()) {
    // We have not stored this frame (RTP timestamp) in our map.
    RtcpEvent rtcp_event;
    rtcp_event.type = frame_event.type;
    rtcp_event.timestamp = frame_event.timestamp;

    // Do not need to fill out rtcp_event.delay_delta or rtcp_event.packet_id
    // as they are not set in frame events we are interested in.
    rtcp_events_.insert(std::make_pair(frame_event.rtp_timestamp, rtcp_event));

    TruncateMapIfNeeded();
  } else {
    // We already have this frame (RTP timestamp) in our map.
    // Only update events that are later in the chain.
    // This is due to that events can be reordered on the wire.
    if (frame_event.type == kVideoFrameCaptured) {
      return;  // First event in chain can not be late by definition.
    }

    if (it->second.type == kVideoFrameEncoded) {
      return;  // Last event in chain should not be updated.
    }

    // Update existing entry.
    it->second.type = frame_event.type;
  }

  DCHECK(rtcp_events_.size() <= max_size_to_retain_);
}

void SenderRtcpEventSubscriber::OnReceivePacketEvent(
    const PacketEvent& packet_event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Do nothing as RTP sender is not interested in packet events for RTCP.
}

void SenderRtcpEventSubscriber::OnReceiveGenericEvent(
    const GenericEvent& generic_event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Do nothing as RTP sender is not interested in generic events for RTCP.
}

void SenderRtcpEventSubscriber::GetRtcpEventsAndReset(
    RtcpEventMap* rtcp_events) {
  DCHECK(thread_checker_.CalledOnValidThread());
  rtcp_events->swap(rtcp_events_);
  rtcp_events_.clear();
}

void SenderRtcpEventSubscriber::TruncateMapIfNeeded() {
  // If map size has exceeded |max_size_to_retain_|, remove entry with
  // the smallest RTP timestamp.
  if (rtcp_events_.size() > max_size_to_retain_) {
    DVLOG(2) << "RTCP event map exceeded size limit; "
             << "removing oldest entry";
    // This is fine since we only insert elements one at a time.
    rtcp_events_.erase(rtcp_events_.begin());
  }
}

}  // namespace cast
}  // namespace media
