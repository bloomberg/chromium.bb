// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_stats.h"

#include <set>
#include <string>

#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/webrtc/api/stats/rtcstats_objects.h"

namespace content {

namespace {

class RTCStatsWhitelist {
 public:
  RTCStatsWhitelist() {
    whitelisted_stats_types_.insert(webrtc::RTCCertificateStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCCodecStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCDataChannelStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCIceCandidatePairStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCIceCandidateStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCLocalIceCandidateStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCRemoteIceCandidateStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCMediaStreamStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCMediaStreamTrackStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCPeerConnectionStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCRTPStreamStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCInboundRTPStreamStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCOutboundRTPStreamStats::kType);
    whitelisted_stats_types_.insert(webrtc::RTCTransportStats::kType);
  }

  bool IsWhitelisted(const webrtc::RTCStats& stats) {
    return whitelisted_stats_types_.find(stats.type()) !=
           whitelisted_stats_types_.end();
  }

  void WhitelistStatsForTesting(const char* type) {
    whitelisted_stats_types_.insert(type);
  }

 private:
  std::set<std::string> whitelisted_stats_types_;
};

RTCStatsWhitelist* GetStatsWhitelist() {
  static RTCStatsWhitelist* whitelist = new RTCStatsWhitelist();
  return whitelist;
}

bool IsWhitelistedStats(const webrtc::RTCStats& stats) {
  return GetStatsWhitelist()->IsWhitelisted(stats);
}

}  // namespace

RTCStatsReport::RTCStatsReport(
    const scoped_refptr<const webrtc::RTCStatsReport>& stats_report)
    : stats_report_(stats_report),
      it_(stats_report_->begin()),
      end_(stats_report_->end()) {
  DCHECK(stats_report_);
}

RTCStatsReport::~RTCStatsReport() {
}

std::unique_ptr<blink::WebRTCStatsReport> RTCStatsReport::copyHandle() const {
  return std::unique_ptr<blink::WebRTCStatsReport>(
      new RTCStatsReport(stats_report_));
}

std::unique_ptr<blink::WebRTCStats> RTCStatsReport::getStats(
    blink::WebString id) const {
  const webrtc::RTCStats* stats = stats_report_->Get(id.utf8());
  if (!stats || !IsWhitelistedStats(*stats))
    return std::unique_ptr<blink::WebRTCStats>();
  return std::unique_ptr<blink::WebRTCStats>(
      new RTCStats(stats_report_, stats));
}

std::unique_ptr<blink::WebRTCStats> RTCStatsReport::next() {
  while (it_ != end_) {
    const webrtc::RTCStats& next = *it_;
    ++it_;
    if (IsWhitelistedStats(next)) {
      return std::unique_ptr<blink::WebRTCStats>(
          new RTCStats(stats_report_, &next));
    }
  }
  return std::unique_ptr<blink::WebRTCStats>();
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
    case webrtc::RTCStatsMemberInterface::kBool:
      return blink::WebRTCStatsMemberTypeBool;
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
    case webrtc::RTCStatsMemberInterface::kString:
      return blink::WebRTCStatsMemberTypeString;
    case webrtc::RTCStatsMemberInterface::kSequenceBool:
      return blink::WebRTCStatsMemberTypeSequenceBool;
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
    case webrtc::RTCStatsMemberInterface::kSequenceString:
      return blink::WebRTCStatsMemberTypeSequenceString;
    default:
      NOTREACHED();
      return blink::WebRTCStatsMemberTypeSequenceInt32;
  }
}

bool RTCStatsMember::isDefined() const {
  return member_->is_defined();
}

bool RTCStatsMember::valueBool() const {
  DCHECK(isDefined());
  return *member_->cast_to<webrtc::RTCStatsMember<bool>>();
}

int32_t RTCStatsMember::valueInt32() const {
  DCHECK(isDefined());
  return *member_->cast_to<webrtc::RTCStatsMember<int32_t>>();
}

uint32_t RTCStatsMember::valueUint32() const {
  DCHECK(isDefined());
  return *member_->cast_to<webrtc::RTCStatsMember<uint32_t>>();
}

int64_t RTCStatsMember::valueInt64() const {
  DCHECK(isDefined());
  return *member_->cast_to<webrtc::RTCStatsMember<int64_t>>();
}

uint64_t RTCStatsMember::valueUint64() const {
  DCHECK(isDefined());
  return *member_->cast_to<webrtc::RTCStatsMember<uint64_t>>();
}

double RTCStatsMember::valueDouble() const {
  DCHECK(isDefined());
  return *member_->cast_to<webrtc::RTCStatsMember<double>>();
}

blink::WebString RTCStatsMember::valueString() const {
  DCHECK(isDefined());
  return blink::WebString::fromUTF8(
      *member_->cast_to<webrtc::RTCStatsMember<std::string>>());
}

blink::WebVector<int> RTCStatsMember::valueSequenceBool() const {
  DCHECK(isDefined());
  const std::vector<bool>& vector =
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<bool>>>();
  std::vector<int> uint32_vector;
  uint32_vector.reserve(vector.size());
  for (size_t i = 0; i < vector.size(); ++i) {
    uint32_vector.push_back(vector[i] ? 1 : 0);
  }
  return blink::WebVector<int>(uint32_vector);
}

blink::WebVector<int32_t> RTCStatsMember::valueSequenceInt32() const {
  DCHECK(isDefined());
  return blink::WebVector<int32_t>(
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<int32_t>>>());
}

blink::WebVector<uint32_t> RTCStatsMember::valueSequenceUint32() const {
  DCHECK(isDefined());
  return blink::WebVector<uint32_t>(
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<uint32_t>>>());
}

blink::WebVector<int64_t> RTCStatsMember::valueSequenceInt64() const {
  DCHECK(isDefined());
  return blink::WebVector<int64_t>(
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<int64_t>>>());
}

blink::WebVector<uint64_t> RTCStatsMember::valueSequenceUint64() const {
  DCHECK(isDefined());
  return blink::WebVector<uint64_t>(
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<uint64_t>>>());
}

blink::WebVector<double> RTCStatsMember::valueSequenceDouble() const {
  DCHECK(isDefined());
  return blink::WebVector<double>(
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<double>>>());
}

blink::WebVector<blink::WebString> RTCStatsMember::valueSequenceString() const {
  DCHECK(isDefined());
  const std::vector<std::string>& sequence =
      *member_->cast_to<webrtc::RTCStatsMember<std::vector<std::string>>>();
  blink::WebVector<blink::WebString> web_sequence(sequence.size());
  for (size_t i = 0; i < sequence.size(); ++i)
    web_sequence[i] = blink::WebString::fromUTF8(sequence[i]);
  return web_sequence;
}

void WhitelistStatsForTesting(const char* type) {
  GetStatsWhitelist()->WhitelistStatsForTesting(type);
}

}  // namespace content
