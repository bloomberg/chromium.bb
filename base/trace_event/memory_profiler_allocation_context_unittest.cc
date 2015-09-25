// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_profiler_allocation_context.h"
#include "base/trace_event/trace_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

// Define all strings once, because the pseudo stack requires pointer equality,
// and string interning is unreliable.
const char kCupcake[] = "Cupcake";
const char kDonut[] = "Donut";
const char kEclair[] = "Eclair";
const char kFroyo[] = "Froyo";
const char kGingerbread[] = "Gingerbread";
const char kHoneycomb[] = "Honeycomb";

// Asserts that the fixed-size array |expected_stack| matches the pseudo
// stack. Syntax note: |const StackFrame (&expected_stack)[N]| is the syntax
// for "expected_stack is a reference to a const fixed-size array of StackFrame
// of length N".
template <size_t N>
void AssertPseudoStackEquals(const StackFrame(&expected_stack)[N]) {
  auto pseudo_stack = AllocationContextTracker::GetPseudoStackForTesting();
  auto actual = pseudo_stack->top();
  auto actual_bottom = pseudo_stack->bottom();
  auto expected = expected_stack;
  auto expected_bottom = expected_stack + N;

  // Note that this requires the pointers to be equal, this is not doing a deep
  // string comparison.
  for (; actual != actual_bottom && expected != expected_bottom;
       actual++, expected++)
    ASSERT_EQ(*expected, *actual);

  // Ensure that the height of the stacks is the same.
  ASSERT_EQ(actual, actual_bottom);
  ASSERT_EQ(expected, expected_bottom);
}

void AssertPseudoStackEmpty() {
  auto pseudo_stack = AllocationContextTracker::GetPseudoStackForTesting();
  ASSERT_EQ(pseudo_stack->top(), pseudo_stack->bottom());
}

class AllocationContextTest : public testing::Test {
 public:
  void EnableTracing() {
    TraceConfig config("");
    TraceLog::GetInstance()->SetEnabled(config, TraceLog::RECORDING_MODE);
    AllocationContextTracker::SetCaptureEnabled(true);
  }

  void DisableTracing() {
    AllocationContextTracker::SetCaptureEnabled(false);
    TraceLog::GetInstance()->SetDisabled();
  }
};

TEST_F(AllocationContextTest, PseudoStackScopedTrace) {
  StackFrame c = kCupcake;
  StackFrame d = kDonut;
  StackFrame e = kEclair;
  StackFrame f = kFroyo;

  EnableTracing();
  AssertPseudoStackEmpty();

  {
    TRACE_EVENT0("Testing", kCupcake);
    StackFrame frame_c[] = {c};
    AssertPseudoStackEquals(frame_c);

    {
      TRACE_EVENT0("Testing", kDonut);
      StackFrame frame_dc[] = {d, c};
      AssertPseudoStackEquals(frame_dc);
    }

    AssertPseudoStackEquals(frame_c);

    {
      TRACE_EVENT0("Testing", kEclair);
      StackFrame frame_ec[] = {e, c};
      AssertPseudoStackEquals(frame_ec);
    }

    AssertPseudoStackEquals(frame_c);
  }

  AssertPseudoStackEmpty();

  {
    TRACE_EVENT0("Testing", kFroyo);
    StackFrame frame_f[] = {f};
    AssertPseudoStackEquals(frame_f);
  }

  AssertPseudoStackEmpty();
  DisableTracing();
}

TEST_F(AllocationContextTest, PseudoStackBeginEndTrace) {
  StackFrame c = kCupcake;
  StackFrame d = kDonut;
  StackFrame e = kEclair;
  StackFrame f = kFroyo;

  StackFrame frame_c[] = {c};
  StackFrame frame_dc[] = {d, c};
  StackFrame frame_ec[] = {e, c};
  StackFrame frame_f[] = {f};

  EnableTracing();
  AssertPseudoStackEmpty();

  TRACE_EVENT_BEGIN0("Testing", kCupcake);
  AssertPseudoStackEquals(frame_c);

  TRACE_EVENT_BEGIN0("Testing", kDonut);
  AssertPseudoStackEquals(frame_dc);
  TRACE_EVENT_END0("Testing", kDonut);

  AssertPseudoStackEquals(frame_c);

  TRACE_EVENT_BEGIN0("Testing", kEclair);
  AssertPseudoStackEquals(frame_ec);
  TRACE_EVENT_END0("Testing", kEclair);

  AssertPseudoStackEquals(frame_c);
  TRACE_EVENT_END0("Testing", kCupcake);

  AssertPseudoStackEmpty();

  TRACE_EVENT_BEGIN0("Testing", kFroyo);
  AssertPseudoStackEquals(frame_f);
  TRACE_EVENT_END0("Testing", kFroyo);

  AssertPseudoStackEmpty();
  DisableTracing();
}

TEST_F(AllocationContextTest, PseudoStackMixedTrace) {
  StackFrame c = kCupcake;
  StackFrame d = kDonut;
  StackFrame e = kEclair;
  StackFrame f = kFroyo;

  StackFrame frame_c[] = {c};
  StackFrame frame_dc[] = {d, c};
  StackFrame frame_e[] = {e};
  StackFrame frame_fe[] = {f, e};

  EnableTracing();
  AssertPseudoStackEmpty();

  TRACE_EVENT_BEGIN0("Testing", kCupcake);
  AssertPseudoStackEquals(frame_c);

  {
    TRACE_EVENT0("Testing", kDonut);
    AssertPseudoStackEquals(frame_dc);
  }

  AssertPseudoStackEquals(frame_c);
  TRACE_EVENT_END0("Testing", kCupcake);
  AssertPseudoStackEmpty();

  {
    TRACE_EVENT0("Testing", kEclair);
    AssertPseudoStackEquals(frame_e);

    TRACE_EVENT_BEGIN0("Testing", kFroyo);
    AssertPseudoStackEquals(frame_fe);
    TRACE_EVENT_END0("Testing", kFroyo);
    AssertPseudoStackEquals(frame_e);
  }

  AssertPseudoStackEmpty();
  DisableTracing();
}

TEST_F(AllocationContextTest, PseudoStackEnableWithEventInScope) {
  StackFrame h = kHoneycomb;

  {
    TRACE_EVENT0("Testing", kGingerbread);
    EnableTracing();
    AssertPseudoStackEmpty();

    {
      TRACE_EVENT0("Testing", kHoneycomb);
      StackFrame frame_h[] = {h};
      AssertPseudoStackEquals(frame_h);
    }

    AssertPseudoStackEmpty();

    // The pop at the end of this scope for the 'Gingerbread' frame must not
    // cause a stack underflow.
  }
  AssertPseudoStackEmpty();
  DisableTracing();
}

}  // namespace trace_event
}  // namespace base
