// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/websourcebuffer_impl.h"

#include "base/float_util.h"
#include "media/filters/chunk_demuxer.h"

namespace content {

static base::TimeDelta DoubleToTimeDelta(double time) {
  DCHECK(!base::IsNaN(time));
  DCHECK_GE(time, 0);
  if (time == std::numeric_limits<double>::infinity())
    return media::kInfiniteDuration();

  // Don't use base::TimeDelta::Max() here, as we want the largest finite time
  // delta.
  base::TimeDelta max_time = base::TimeDelta::FromInternalValue(kint64max - 1);
  double max_time_in_seconds = max_time.InSecondsF();

  if (time >= max_time_in_seconds)
    return max_time;

  return base::TimeDelta::FromMicroseconds(
      time * base::Time::kMicrosecondsPerSecond);
}

WebSourceBufferImpl::WebSourceBufferImpl(
    const std::string& id, media::ChunkDemuxer* demuxer)
    : id_(id),
      demuxer_(demuxer) {
  DCHECK(demuxer_);
}

WebSourceBufferImpl::~WebSourceBufferImpl() {
  DCHECK(!demuxer_) << "Object destroyed w/o removedFromMediaSource() call";
}

bool WebSourceBufferImpl::setMode(WebSourceBuffer::AppendMode mode) {
  bool sequence_mode = false;
  switch (mode) {
    case WebSourceBuffer::AppendModeSegments:
      break;
    case WebSourceBuffer::AppendModeSequence:
      sequence_mode = true;
      break;
  }

  return demuxer_->SetSequenceMode(id_, sequence_mode);
}

blink::WebTimeRanges WebSourceBufferImpl::buffered() {
  media::Ranges<base::TimeDelta> ranges = demuxer_->GetBufferedRanges(id_);
  blink::WebTimeRanges result(ranges.size());
  for (size_t i = 0; i < ranges.size(); i++) {
    result[i].start = ranges.start(i).InSecondsF();
    result[i].end = ranges.end(i).InSecondsF();
  }
  return result;
}

void WebSourceBufferImpl::append(const unsigned char* data, unsigned length) {
  append(data, length, NULL);
}

void WebSourceBufferImpl::append(
    const unsigned char* data,
    unsigned length,
    double* timestamp_offset) {
  demuxer_->AppendData(id_, data, length, timestamp_offset);
}

void WebSourceBufferImpl::abort() {
  demuxer_->Abort(id_);
}

void WebSourceBufferImpl::remove(double start, double end) {
  demuxer_->Remove(id_, DoubleToTimeDelta(start), DoubleToTimeDelta(end));
}

bool WebSourceBufferImpl::setTimestampOffset(double offset) {
  base::TimeDelta time_offset = base::TimeDelta::FromMicroseconds(
      offset * base::Time::kMicrosecondsPerSecond);
  return demuxer_->SetTimestampOffset(id_, time_offset);
}

void WebSourceBufferImpl::setAppendWindowStart(double start) {
  demuxer_->SetAppendWindowStart(id_, DoubleToTimeDelta(start));
}

void WebSourceBufferImpl::setAppendWindowEnd(double end) {
  demuxer_->SetAppendWindowEnd(id_, DoubleToTimeDelta(end));
}

void WebSourceBufferImpl::removedFromMediaSource() {
  demuxer_->RemoveId(id_);
  demuxer_ = NULL;
}

}  // namespace content
