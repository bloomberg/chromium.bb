// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/time/time.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"
#include "cc/debug/frame_timing_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

static std::string ToString(
    scoped_ptr<FrameTimingTracker::CompositeTimingSet> timingset) {
  scoped_refptr<base::trace_event::TracedValue> value =
      new base::trace_event::TracedValue();
  value->BeginArray("values");
  for (const auto& it : *timingset) {
    value->BeginDictionary();
    value->SetInteger("rect_id", it.first);
    value->BeginArray("events");
    for (const auto& event : it.second) {
      value->BeginDictionary();
      value->SetInteger("frame_id", event.frame_id);
      value->SetInteger("timestamp", event.timestamp.ToInternalValue());
      value->EndDictionary();
    }
    value->EndArray();
    value->EndDictionary();
  }
  value->EndArray();
  return value->ToString();
}

TEST(FrameTimingTrackerTest, DefaultTrackerIsEmpty) {
  scoped_ptr<FrameTimingTracker> tracker(FrameTimingTracker::Create());
  EXPECT_EQ("{\"values\":[]}", ToString(tracker->GroupCountsByRectId()));
}

TEST(FrameTimingTrackerTest, NoFrameIdsIsEmpty) {
  scoped_ptr<FrameTimingTracker> tracker(FrameTimingTracker::Create());
  std::vector<std::pair<int, int64_t>> ids;
  tracker->SaveTimeStamps(base::TimeTicks::FromInternalValue(100), ids);
  EXPECT_EQ("{\"values\":[]}", ToString(tracker->GroupCountsByRectId()));
}

TEST(FrameTimingTrackerTest, OneFrameId) {
  scoped_ptr<FrameTimingTracker> tracker(FrameTimingTracker::Create());
  std::vector<std::pair<int, int64_t>> ids;
  ids.push_back(std::make_pair(1, 2));
  tracker->SaveTimeStamps(base::TimeTicks::FromInternalValue(100), ids);
  EXPECT_EQ(
      "{\"values\":[{\"events\":["
      "{\"frame_id\":1,\"timestamp\":100}],\"rect_id\":2}]}",
      ToString(tracker->GroupCountsByRectId()));
}

TEST(FrameTimingTrackerTest, UnsortedTimestampsIds) {
  scoped_ptr<FrameTimingTracker> tracker(FrameTimingTracker::Create());
  std::vector<std::pair<int, int64_t>> ids;
  ids.push_back(std::make_pair(1, 2));
  tracker->SaveTimeStamps(base::TimeTicks::FromInternalValue(200), ids);
  tracker->SaveTimeStamps(base::TimeTicks::FromInternalValue(400), ids);
  tracker->SaveTimeStamps(base::TimeTicks::FromInternalValue(100), ids);
  EXPECT_EQ(
      "{\"values\":[{\"events\":["
      "{\"frame_id\":1,\"timestamp\":100},"
      "{\"frame_id\":1,\"timestamp\":200},"
      "{\"frame_id\":1,\"timestamp\":400}],\"rect_id\":2}]}",
      ToString(tracker->GroupCountsByRectId()));
}

TEST(FrameTimingTrackerTest, MultipleFrameIds) {
  scoped_ptr<FrameTimingTracker> tracker(FrameTimingTracker::Create());

  std::vector<std::pair<int, int64_t>> ids200;
  ids200.push_back(std::make_pair(1, 2));
  ids200.push_back(std::make_pair(1, 3));
  tracker->SaveTimeStamps(base::TimeTicks::FromInternalValue(200), ids200);

  std::vector<std::pair<int, int64_t>> ids400;
  ids400.push_back(std::make_pair(2, 2));
  tracker->SaveTimeStamps(base::TimeTicks::FromInternalValue(400), ids400);

  std::vector<std::pair<int, int64_t>> ids100;
  ids100.push_back(std::make_pair(3, 2));
  ids100.push_back(std::make_pair(2, 3));
  ids100.push_back(std::make_pair(3, 4));
  tracker->SaveTimeStamps(base::TimeTicks::FromInternalValue(100), ids100);

  std::string result = ToString(tracker->GroupCountsByRectId());

  EXPECT_EQ(strlen(
                "{\"values\":[{\"events\":["
                "{\"frame_id\":3,\"timestamp\":100},"
                "{\"frame_id\":1,\"timestamp\":200},"
                "{\"frame_id\":2,\"timestamp\":400}],\"rect_id\":2},"
                "{\"events\":["
                "{\"frame_id\":2,\"timestamp\":100},"
                "{\"frame_id\":1,\"timestamp\":200}],\"rect_id\":3},"
                "{\"events\":["
                "{\"frame_id\":3,\"timestamp\":100}],\"rect_id\":4}"
                "]}"),
            result.size());
  EXPECT_NE(std::string::npos,
            result.find(
                "{\"frame_id\":3,\"timestamp\":100},"
                "{\"frame_id\":1,\"timestamp\":200},"
                "{\"frame_id\":2,\"timestamp\":400}],\"rect_id\":2}"));
  EXPECT_NE(std::string::npos,
            result.find(
                "{\"events\":["
                "{\"frame_id\":2,\"timestamp\":100},"
                "{\"frame_id\":1,\"timestamp\":200}],\"rect_id\":3}"));
  EXPECT_NE(std::string::npos,
            result.find(
                "{\"events\":["
                "{\"frame_id\":3,\"timestamp\":100}],\"rect_id\":4}"));
}

}  // namespace
}  // namespace cc
