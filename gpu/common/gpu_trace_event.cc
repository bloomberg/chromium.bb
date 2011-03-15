// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/common/gpu_trace_event.h"

#include "base/format_macros.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/time.h"

#define USE_UNRELIABLE_NOW

using namespace base;

namespace gpu {

// Controls the number of trace events we will buffer in-memory
// before flushing them.
#define TRACE_EVENT_BUFFER_SIZE 16384

////////////////////////////////////////////////////////////////////////////////
//
// TraceLog::Category
//
////////////////////////////////////////////////////////////////////////////////
TraceCategory::TraceCategory(const char* name, bool enabled)
    : name_(name) {
  base::subtle::NoBarrier_Store(&enabled_,
      static_cast<base::subtle::Atomic32>(enabled));
}

TraceCategory::~TraceCategory() {
  base::subtle::NoBarrier_Store(&enabled_,
      static_cast<base::subtle::Atomic32>(0));
}

////////////////////////////////////////////////////////////////////////////////
//
// TraceEvent
//
////////////////////////////////////////////////////////////////////////////////

namespace {
const char* GetPhaseStr(TraceEventPhase phase) {
  if (phase == GPU_TRACE_EVENT_PHASE_BEGIN) {
    return "B";
  } else if (phase == GPU_TRACE_EVENT_PHASE_INSTANT) {
    return "I";
  } else if (phase == GPU_TRACE_EVENT_PHASE_END) {
    return "E";
  } else {
    DCHECK(false);
    return "?";
  }
}
}

TraceEvent::TraceEvent() {
}

TraceEvent::~TraceEvent() {
}


void TraceEvent::AppendAsJSON(std::string* out,
    const std::vector<TraceEvent>& events) {
  *out += "[";
  for (size_t i = 0; i < events.size(); ++i) {
    if (i > 0)
      *out += ",";
    events[i].AppendAsJSON(out);
  }
  *out += "]";
}

void TraceEvent::AppendAsJSON(std::string* out) const {
  int nargs = 0;
  for (int i = 0; i < TRACE_MAX_NUM_ARGS; ++i) {
    if (argNames[i] == NULL)
      break;
    nargs += 1;
  }

  const char* phaseStr = GetPhaseStr(phase);
  int64 time_int64 = timestamp.ToInternalValue();
  long long unsigned int time_llui =
      static_cast<long long unsigned int>(time_int64);
  StringAppendF(out,
      "{cat:'%s',pid:%i,tid:%i,ts:0x%llx,ph:'%s',name:'%s',args:{",
      category->name(),
      static_cast<int>(processId),
      static_cast<int>(threadId),
      time_llui,
      phaseStr,
      name);
  for (int  i = 0; i < nargs; ++i) {
    if (i > 0)
      *out += ",";
    *out += argNames[i];
    *out += ":'";
    *out += argValues[i];
    *out += "'";
  }
  *out += "}}";
}

////////////////////////////////////////////////////////////////////////////////
//
// TraceLog
//
////////////////////////////////////////////////////////////////////////////////

// static
TraceLog* TraceLog::GetInstance() {
  return Singleton<TraceLog, StaticMemorySingletonTraits<TraceLog> >::get();
}

TraceLog::TraceLog()
    : enabled_(false)
{
  logged_events_.reserve(1024);
}

TraceLog::~TraceLog() {
}

TraceCategory* TraceLog::GetCategory(const char* name) {
  AutoLock lock(lock_);
  // TODO(nduca): replace with a hash_map.
  for (int i = categories_.size() - 1; i >= 0; i-- ) {
    if (strcmp(categories_[i]->name(), name) == 0)
      return categories_[i];
  }
  TraceCategory* category = new TraceCategory(name, enabled_);
  categories_.push_back(category);
  return category;
}

void TraceLog::SetEnabled(bool enabled) {
  AutoLock lock(lock_);
  if (enabled == enabled_)
    return;
  if (enabled) {
    // Enable all categories.
    enabled_ = true;
    for (size_t i = 0; i < categories_.size(); i++) {
      base::subtle::NoBarrier_Store(&categories_[i]->enabled_,
                                    static_cast<base::subtle::Atomic32>(1));
    }
  } else {
    // Disable all categories.
    for (size_t i = 0; i < categories_.size(); i++) {
      base::subtle::NoBarrier_Store(&categories_[i]->enabled_,
                                    static_cast<base::subtle::Atomic32>(0));
    }
    enabled_ = false;
    FlushWithLockAlreadyHeld();
  }
}

void TraceLog::SetOutputCallback(TraceLog::OutputCallback* cb) {
  AutoLock lock(lock_);
  if (enabled_) {
    FlushWithLockAlreadyHeld();
  }
  output_callback_.reset(cb);
}

void TraceLog::AddRemotelyCollectedData(const std::string& json_events) {
  AutoLock lock(lock_);
  if (output_callback_.get())
    output_callback_->Run(json_events);
}

void TraceLog::Flush() {
  AutoLock lock(lock_);
  FlushWithLockAlreadyHeld();
}

void TraceLog::FlushWithLockAlreadyHeld() {
  if (output_callback_.get() && logged_events_.size()) {
    std::string json_events;
    TraceEvent::AppendAsJSON(&json_events, logged_events_);
    output_callback_->Run(json_events);
  }
  logged_events_.erase(logged_events_.begin(), logged_events_.end());
}

void TraceLog::AddTraceEvent(TraceEventPhase phase,
    const char* file, int line,
    TraceCategory* category,
    const char* name,
    const char* arg1name, const char* arg1val,
    const char* arg2name, const char* arg2val) {
  DCHECK(file && name);
#ifdef USE_UNRELIABLE_NOW
  TimeTicks now = TimeTicks::HighResNow();
#else
  TimeTicks now = TimeTicks::Now();
#endif
  //static_cast<unsigned long>(base::GetCurrentProcId()),
  AutoLock lock(lock_);
  logged_events_.push_back(TraceEvent());
  TraceEvent& event = logged_events_.back();
  event.processId = static_cast<unsigned long>(base::GetCurrentProcId());
  event.threadId = PlatformThread::CurrentId();
  event.timestamp = now;
  event.phase = phase;
  event.category = category;
  event.name = name;
  event.argNames[0] = arg1name;
  event.argValues[0] = arg1name ? arg1val : "";
  event.argNames[1] = arg2name;
  event.argValues[1] = arg2name ? arg2val : "";
  COMPILE_ASSERT(TRACE_MAX_NUM_ARGS == 2, TraceEvent_arc_count_out_of_sync);

  if (logged_events_.size() > TRACE_EVENT_BUFFER_SIZE)
    FlushWithLockAlreadyHeld();
}

}  // namespace gpu
