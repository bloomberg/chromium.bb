// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/heap_profiler_allocation_context.h"

#include <cstring>

#include "base/hash.h"
#include "base/macros.h"

namespace base {
namespace trace_event {

// Constructor that does not initialize members.
AllocationContext::AllocationContext() {}

// static
AllocationContext AllocationContext::Empty() {
  AllocationContext ctx;

  for (size_t i = 0; i < arraysize(ctx.backtrace.frames); i++)
    ctx.backtrace.frames[i] = nullptr;

  ctx.type_id = 0;

  return ctx;
}

bool operator==(const Backtrace& lhs, const Backtrace& rhs) {
  // Pointer equality of the stack frames is assumed, so instead of doing a deep
  // string comparison on all of the frames, a |memcmp| suffices.
  return std::memcmp(lhs.frames, rhs.frames, sizeof(lhs.frames)) == 0;
}

bool operator==(const AllocationContext& lhs, const AllocationContext& rhs) {
  return (lhs.backtrace == rhs.backtrace) && (lhs.type_id == rhs.type_id);
}

}  // namespace trace_event
}  // namespace base

namespace BASE_HASH_NAMESPACE {
using base::trace_event::AllocationContext;
using base::trace_event::Backtrace;

size_t hash<Backtrace>::operator()(const Backtrace& backtrace) const {
  return base::SuperFastHash(reinterpret_cast<const char*>(backtrace.frames),
                             sizeof(backtrace.frames));
}

size_t hash<AllocationContext>::operator()(const AllocationContext& ctx) const {
  size_t ctx_hash = hash<Backtrace>()(ctx.backtrace);

  // Multiply one side to break the commutativity of +. Multiplication with a
  // number coprime to |numeric_limits<size_t>::max() + 1| is bijective so
  // randomness is preserved. The type ID is assumed to be distributed randomly
  // already so there is no need to hash it.
  return (ctx_hash * 3) + static_cast<size_t>(ctx.type_id);
}

}  // BASE_HASH_NAMESPACE
