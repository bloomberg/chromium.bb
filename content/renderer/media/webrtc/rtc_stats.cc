// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_stats.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/webrtc/api/rtcstats.h"

namespace content {

RTCStatsReport::RTCStatsReport(
    const scoped_refptr<const webrtc::RTCStatsReport>& stats_report)
    : stats_report_(stats_report),
      it_(stats_report_->begin()),
      end_(stats_report_->end()) {
  DCHECK(stats_report_);
}

RTCStatsReport::~RTCStatsReport() {
}

std::unique_ptr<blink::WebRTCStats> RTCStatsReport::next() {
  if (it_ == end_)
    return std::unique_ptr<blink::WebRTCStats>();
  const webrtc::RTCStats& next = *it_;
  ++it_;
  return std::unique_ptr<blink::WebRTCStats>(
      new RTCStats(stats_report_, &next));
}

RTCStats::RTCStats(
    const scoped_refptr<const webrtc::RTCStatsReport>& stats_owner,
    const webrtc::RTCStats* stats)
    : stats_owner_(stats_owner),
      stats_(stats),
      stats_members_(stats->Members()) {
  DCHECK(stats_owner_);
  DCHECK(stats_);
  DCHECK(stats_owner_->Get(stats_->id()));
}

RTCStats::~RTCStats() {
}

blink::WebString RTCStats::id() const {
  return blink::WebString::fromUTF8(stats_->id());
}

blink::WebString RTCStats::type() const {
  return blink::WebString::fromUTF8(stats_->type());
}

double RTCStats::timestamp() const {
  return stats_->timestamp_us() / static_cast<double>(
      base::Time::kMicrosecondsPerMillisecond);
}

size_t RTCStats::membersCount() const {
  return stats_members_.size();
}

std::unique_ptr<blink::WebRTCStatsMember> RTCStats::getMember(size_t i) const {
  DCHECK_LT(i, stats_members_.size());
  return std::unique_ptr<blink::WebRTCStatsMember>(
      new RTCStatsMember(stats_owner_, stats_members_[i]));
}

RTCStatsMember::RTCStatsMember(
    const scoped_refptr<const webrtc::RTCStatsReport>& stats_owner,
    const webrtc::RTCStatsMemberInterface* member)
    : stats_owner_(stats_owner),
      member_(member) {
  DCHECK(stats_owner_);
  DCHECK(member_);
}

RTCStatsMember::~RTCStatsMember() {
}

blink::WebString RTCStatsMember::name() const {
  return blink::WebString::fromUTF8(member_->name());
}

blink::WebRTCStatsMemberType RTCStatsMember::type() const {
  switch (member_->type()) {
    case webrtc::RTCStatsMemberInterface::kInt32:
      return blink::WebRTCStatsMemberTypeInt32;
    case webrtc::RTCStatsMemberInterface::kUint32:
      return blink::WebRTCStatsMemberTypeUint32;
    case webrtc::RTCStatsMemberInterface::kInt64:
      return blink::WebRTCStatsMemberTypeInt64;
    case webrtc::RTCStatsMemberInterface::kUint64:
      return blink::WebRTCStatsMemberTypeUint64;
    case webrtc::RTCStatsMemberInterface::kDouble:
      return blink::WebRTCStatsMemberTypeDouble;
    case webrtc::RTCStatsMemberInterface::kStaticString:
    case webrtc::RTCStatsMemberInterface::kString:
      return blink::WebRTCStatsMemberTypeString;
    case webrtc::RTCStatsMemberInterface::kSequenceInt32:
      return blink::WebRTCStatsMemberTypeSequenceInt32;
    case webrtc::RTCStatsMemberInterface::kSequenceUint32:
      return blink::WebRTCStatsMemberTypeSequenceUint32;
    case webrtc::RTCStatsMemberInterface::kSequenceInt64:
      return blink::WebRTCStatsMemberTypeSequenceInt64;
    case webrtc::RTCStatsMemberInterface::kSequenceUint64:
      return blink::WebRTCStatsMemberTypeSequenceUint64;
    case webrtc::RTCStatsMemberInterface::kSequenceDouble:
      return blink::WebRTCStatsMemberTypeSequenceDouble;
    case webrtc::RTCStatsMemberInterface::kSequenceStaticString:
    case webrtc::RTCStatsMemberInterface::kSequenceString:
      return blink::WebRTCStatsMemberTypeSequenceString;
    default:
      NOTREACHED();
      return blink::WebRTCStatsMemberTypeSequenceInt32;
  }
}

int32_t RTCStatsMember::valueInt32() const {
  return *member_->cast_to<webrtc::RTCStatsMember<int32_t>>();
}

uint32_t RTCStatsMember::valueUint32() const {
  return *member_->cast_to<webrtc::RTCStatsMember<uint32_t>>();
}

int64_t RTCStatsMember::valueInt64() const {
  return *member_->cast_to<webrtc::RTCStatsMember<int64_t>>();
}

uint64_t RTCStatsMember::valueUint64() const {
  return *member_->cast_to<webrtc::RTCStatsMember<uint64_t>>();
}

double RTCStatsMember::valueDouble() const {
  return *member_->cast_to<webrtc::RTCStatsMember<double>>();
}

blink::WebString RTCStatsMember::valueString() const {
  switch (member_->type()) {
    case webrtc::RTCStatsMemberInterface::kStaticString:
      return blink::WebString::fromUTF8(
          *member_->cast_to<webrtc::RTCStatsMember<const char*>>());
    case webrtc::RTCStatsMemberInterface::kString:
      return blink::WebString::fromUTF8(
          *member_->cast_to<webrtc::RTCStatsMember<std::string>>());
    default:
      NOTREACHED();
      return blink::WebString();
  }
}

blink::WebVector<int32_t> RTCStatsMember::valueSequenceInt32() const {
  return blink::WebVector<int32_t>(
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<int32_t>>>());
}

blink::WebVector<uint32_t> RTCStatsMember::valueSequenceUint32() const {
  return blink::WebVector<uint32_t>(
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<uint32_t>>>());
}

blink::WebVector<int64_t> RTCStatsMember::valueSequenceInt64() const {
  return blink::WebVector<int64_t>(
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<int64_t>>>());
}

blink::WebVector<uint64_t> RTCStatsMember::valueSequenceUint64() const {
  return blink::WebVector<uint64_t>(
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<uint64_t>>>());
}

blink::WebVector<double> RTCStatsMember::valueSequenceDouble() const {
  return blink::WebVector<double>(
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<double>>>());
}

blink::WebVector<blink::WebString> RTCStatsMember::valueSequenceString() const {
  switch (member_->type()) {
    case webrtc::RTCStatsMemberInterface::kStaticString:
      {
        const std::vector<const char*>& sequence =
            *member_->cast_to<
                webrtc::RTCStatsMember<std::vector<const char*>>>();
        blink::WebVector<blink::WebString> web_sequence(sequence.size());
        for (size_t i = 0; i < sequence.size(); ++i)
          web_sequence[i] = blink::WebString::fromUTF8(sequence[i]);
        return web_sequence;
      }
    case webrtc::RTCStatsMemberInterface::kString:
      {
        const std::vector<std::string>& sequence =
            *member_->cast_to<
                webrtc::RTCStatsMember<std::vector<std::string>>>();
        blink::WebVector<blink::WebString> web_sequence(sequence.size());
        for (size_t i = 0; i < sequence.size(); ++i)
          web_sequence[i] = blink::WebString::fromUTF8(sequence[i]);
        return web_sequence;
      }
    default:
      NOTREACHED();
      return blink::WebVector<blink::WebString>();
  }
}

}  // namespace content
