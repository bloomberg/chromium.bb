// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/rendering_stats.h"

namespace cc {

RenderingStats::TimeDeltaList::TimeDeltaList() {
}

RenderingStats::TimeDeltaList::~TimeDeltaList() {
}

void RenderingStats::TimeDeltaList::Append(base::TimeDelta value) {
  values.push_back(value);
}

void RenderingStats::TimeDeltaList::AddToTracedValue(
    base::debug::TracedValue* list_value) const {
  for (const auto& value : values) {
    list_value->AppendDouble(value.InMillisecondsF());
  }
}

void RenderingStats::TimeDeltaList::Add(const TimeDeltaList& other) {
  values.insert(values.end(), other.values.begin(), other.values.end());
}

base::TimeDelta RenderingStats::TimeDeltaList::GetLastTimeDelta() const {
  return values.empty() ? base::TimeDelta() : values.back();
}

RenderingStats::RenderingStats()
    : frame_count(0),
      visible_content_area(0),
      approximated_visible_content_area(0) {
}

RenderingStats::~RenderingStats() {
}

scoped_refptr<base::debug::ConvertableToTraceFormat>
RenderingStats::AsTraceableData() const {
  scoped_refptr<base::debug::TracedValue> record_data =
      new base::debug::TracedValue();
  record_data->SetInteger("frame_count", frame_count);
  record_data->SetInteger("visible_content_area", visible_content_area);
  record_data->SetInteger("approximated_visible_content_area",
                          approximated_visible_content_area);
  record_data->BeginArray("draw_duration_ms");
  draw_duration.AddToTracedValue(record_data.get());
  record_data->EndArray();

  record_data->BeginArray("draw_duration_estimate_ms");
  draw_duration_estimate.AddToTracedValue(record_data.get());
  record_data->EndArray();

  record_data->BeginArray("begin_main_frame_to_commit_duration_ms");
  begin_main_frame_to_commit_duration.AddToTracedValue(record_data.get());
  record_data->EndArray();

  record_data->BeginArray("begin_main_frame_to_commit_duration_estimate_ms");
  begin_main_frame_to_commit_duration_estimate.AddToTracedValue(
      record_data.get());
  record_data->EndArray();

  record_data->BeginArray("commit_to_activate_duration_ms");
  commit_to_activate_duration.AddToTracedValue(record_data.get());
  record_data->EndArray();

  record_data->BeginArray("commit_to_activate_duration_estimate_ms");
  commit_to_activate_duration_estimate.AddToTracedValue(record_data.get());
  record_data->EndArray();
  return record_data;
}

void RenderingStats::Add(const RenderingStats& other) {
  frame_count += other.frame_count;
  visible_content_area += other.visible_content_area;
  approximated_visible_content_area += other.approximated_visible_content_area;

  draw_duration.Add(other.draw_duration);
  draw_duration_estimate.Add(other.draw_duration_estimate);
  begin_main_frame_to_commit_duration.Add(
      other.begin_main_frame_to_commit_duration);
  begin_main_frame_to_commit_duration_estimate.Add(
      other.begin_main_frame_to_commit_duration_estimate);
  commit_to_activate_duration.Add(other.commit_to_activate_duration);
  commit_to_activate_duration_estimate.Add(
      other.commit_to_activate_duration_estimate);
}

}  // namespace cc
