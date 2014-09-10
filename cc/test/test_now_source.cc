// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <string>

#include "cc/test/test_now_source.h"

namespace cc {

// TestNowSource::Constructors
scoped_refptr<TestNowSource> TestNowSource::Create() {
  return make_scoped_refptr(new TestNowSource());
}

scoped_refptr<TestNowSource> TestNowSource::Create(base::TimeTicks initial) {
  return make_scoped_refptr(new TestNowSource(initial));
}

scoped_refptr<TestNowSource> TestNowSource::Create(int64_t initial) {
  return make_scoped_refptr(new TestNowSource(initial));
}

TestNowSource::TestNowSource()
    : initial_(base::TimeTicks::FromInternalValue(10000)), now_() {
  Reset();
}

TestNowSource::TestNowSource(base::TimeTicks initial)
    : initial_(initial), now_() {
  Reset();
}

TestNowSource::TestNowSource(int64_t initial)
    : initial_(base::TimeTicks::FromInternalValue(initial)), now_() {
  Reset();
}

TestNowSource::~TestNowSource() {
}

// TestNowSource actual functionality
void TestNowSource::Reset() {
  TRACE_EVENT_INSTANT2("cc",
                       "TestNowSource::Reset",
                       TRACE_EVENT_SCOPE_THREAD,
                       "previous",
                       now_,
                       "initial",
                       initial_);
  now_ = initial_;
}

base::TimeTicks TestNowSource::Now() const {
  return now_;
}

void TestNowSource::SetNow(base::TimeTicks time) {
  TRACE_EVENT_INSTANT2("cc",
                       "TestNowSource::SetNow",
                       TRACE_EVENT_SCOPE_THREAD,
                       "previous",
                       now_,
                       "new",
                       time);
  DCHECK(time >= now_);  // Time should always go forward.
  now_ = time;
}

void TestNowSource::AdvanceNow(base::TimeDelta period) {
  TRACE_EVENT_INSTANT2("cc",
                       "TestNowSource::AdvanceNow",
                       TRACE_EVENT_SCOPE_THREAD,
                       "previous",
                       now_,
                       "by",
                       period.ToInternalValue());
  DCHECK(now_ != kAbsoluteMaxNow);
  DCHECK(period >= base::TimeDelta());  // Time should always go forward.
  now_ += period;
}

const base::TimeTicks TestNowSource::kAbsoluteMaxNow =
    base::TimeTicks::FromInternalValue(std::numeric_limits<int64_t>::max());

// TestNowSource::Convenience functions
void TestNowSource::AdvanceNowMicroseconds(int64_t period_in_microseconds) {
  AdvanceNow(base::TimeDelta::FromMicroseconds(period_in_microseconds));
}
void TestNowSource::SetNowMicroseconds(int64_t time_in_microseconds) {
  SetNow(base::TimeTicks::FromInternalValue(time_in_microseconds));
}

// TestNowSource::Tracing functions
void TestNowSource::AsValueInto(base::debug::TracedValue* state) const {
  state->SetInteger("now_in_microseconds", now_.ToInternalValue());
}

scoped_refptr<base::debug::ConvertableToTraceFormat> TestNowSource::AsValue()
    const {
  scoped_refptr<base::debug::TracedValue> state =
      new base::debug::TracedValue();
  AsValueInto(state.get());
  return state;
}

// TestNowSource::Pretty printing functions
std::string TestNowSource::ToString() const {
  std::string output("TestNowSource(");
  AsValue()->AppendAsTraceFormat(&output);
  output += ")";
  return output;
}

::std::ostream& operator<<(::std::ostream& os,
                           const scoped_refptr<TestNowSource>& src) {
  os << src->ToString();
  return os;
}
void PrintTo(const scoped_refptr<TestNowSource>& src, ::std::ostream* os) {
  *os << src;
}

}  // namespace cc
