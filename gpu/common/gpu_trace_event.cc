// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
// before throwing them away.
#define TRACE_EVENT_BUFFER_SIZE 500000
#define TRACE_EVENT_BATCH_SIZE 1000

#define TRACE_EVENT_MAX_CATEGORIES 42

static TraceCategory g_categories[TRACE_EVENT_MAX_CATEGORIES];
static int g_category_index = 0;

////////////////////////////////////////////////////////////////////////////////
//
// TraceLog::Category
//
////////////////////////////////////////////////////////////////////////////////
TraceCategory::TraceCategory()
    : name_(NULL) {
  base::subtle::NoBarrier_Store(&enabled_,
      static_cast<base::subtle::Atomic32>(0));
}

TraceCategory::~TraceCategory() {
  base::subtle::NoBarrier_Store(&enabled_,
      static_cast<base::subtle::Atomic32>(0));
}

////////////////////////////////////////////////////////////////////////////////
//
// TraceValue
//
////////////////////////////////////////////////////////////////////////////////

void TraceValue::Destroy() {
  if (type_ == TRACE_TYPE_STRING) {
    free(value_.as_string);
    value_.as_string = NULL;
  }
  type_ = TRACE_TYPE_UNDEFINED;
  value_.as_uint = 0ull;
}

TraceValue& TraceValue::operator=(const TraceValue& rhs) {
  DCHECK(sizeof(value_) == sizeof(uint64));
  Destroy();
  type_ = rhs.type_;
  if (rhs.type_ == TRACE_TYPE_STRING) {
    value_.as_string = base::strdup(rhs.value_.as_string);
  } else {
    // Copy all 64 bits for all other types.
    value_.as_uint = rhs.value_.as_uint;
  }
  return *this;
}

bool TraceValue::operator==(const TraceValue& rhs) const {
  if (type_ != rhs.type())
    return false;
  if (rhs.type_ == TRACE_TYPE_STRING) {
    return (strcmp(value_.as_string, rhs.value_.as_string) == 0);
  } else {
    // Compare all 64 bits for all other types. Unused bits are set to zero
    // by the constructors of types that may be less than 64 bits.
    return (value_.as_uint == rhs.value_.as_uint);
  }
}

void TraceValue::AppendAsJSON(std::string* out) const {
  char temp_string[128];
  std::string::size_type start_pos;
  switch (type_) {
  case TRACE_TYPE_BOOL:
    *out += as_bool()? "true" : "false";
    break;
  case TRACE_TYPE_UINT:
    base::snprintf(temp_string, sizeof(temp_string), "%llu",
                   static_cast<unsigned long long>(as_uint()));
    *out += temp_string;
    break;
  case TRACE_TYPE_INT:
    base::snprintf(temp_string, sizeof(temp_string), "%lld",
                   static_cast<long long>(as_int()));
    *out += temp_string;
    break;
  case TRACE_TYPE_DOUBLE:
    base::snprintf(temp_string, sizeof(temp_string), "%f", as_double());
    *out += temp_string;
    break;
  case TRACE_TYPE_POINTER:
    base::snprintf(temp_string, sizeof(temp_string), "%p", as_pointer());
    *out += temp_string;
    break;
  case TRACE_TYPE_STRING:
    start_pos = out->size();
    *out += as_string();
    // replace " character with '
    while ((start_pos = out->find_first_of('\"', start_pos)) !=
           std::string::npos)
      (*out)[start_pos] = '\'';
    break;
  default:
    break;
  }
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

TraceEvent::TraceEvent()
    : processId(0),
      threadId(0),
      phase(GPU_TRACE_EVENT_PHASE_BEGIN),
      category(NULL),
      name(NULL) {
  memset(&argNames, 0, sizeof(argNames));
}

TraceEvent::~TraceEvent() {
}


void TraceEvent::AppendAsJSON(std::string* out,
    const std::vector<TraceEvent>& events,
    size_t start,
    size_t count) {
  *out += "[";
  for (size_t i = 0; i < count && start + i < events.size(); ++i) {
    if (i > 0)
      *out += ",";
    events[i + start].AppendAsJSON(out);
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
  StringAppendF(out,
      "{\"cat\":\"%s\",\"pid\":%i,\"tid\":%i,\"ts\":%lld,"
      "\"ph\":\"%s\",\"name\":\"%s\",\"args\":{",
      category->name(),
      static_cast<int>(processId),
      static_cast<int>(threadId),
      static_cast<long long>(time_int64),
      phaseStr,
      name);
  for (int  i = 0; i < nargs; ++i) {
    if (i > 0)
      *out += ",";
    *out += "\"";
    *out += argNames[i];
    *out += "\":\"";
    argValues[i].AppendAsJSON(out);
    *out += "\"";
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
  for (int i = 0; i < g_category_index; i++) {
    if (strcmp(g_categories[i].name(), name) == 0)
      return &g_categories[i];
  }
  CHECK(g_category_index < TRACE_EVENT_MAX_CATEGORIES) <<
      "must increase TRACE_EVENT_MAX_CATEGORIES";
  int new_index = g_category_index++;
  g_categories[new_index].set(name, enabled_);
  return &g_categories[new_index];
}

void TraceLog::SetEnabled(bool enabled) {
  AutoLock lock(lock_);
  if (enabled == enabled_)
    return;
  if (enabled) {
    // Enable all categories.
    enabled_ = true;
    for (int i = 0; i < g_category_index; i++) {
      base::subtle::NoBarrier_Store(&g_categories[i].enabled_,
                                    static_cast<base::subtle::Atomic32>(1));
    }
  } else {
    // Disable all categories.
    for (int i = 0; i < g_category_index; i++) {
      base::subtle::NoBarrier_Store(&g_categories[i].enabled_,
                                    static_cast<base::subtle::Atomic32>(0));
    }
    enabled_ = false;
    FlushWithLockAlreadyHeld();
  }
}

float TraceLog::GetBufferPercentFull() const {
  return (float)((double)logged_events_.size()/(double)TRACE_EVENT_BUFFER_SIZE);
}

void TraceLog::SetOutputCallback(TraceLog::OutputCallback* cb) {
  AutoLock lock(lock_);
  if (enabled_) {
    FlushWithLockAlreadyHeld();
  }
  output_callback_.reset(cb);
}

void TraceLog::SetBufferFullCallback(TraceLog::BufferFullCallback* cb) {
  AutoLock lock(lock_);
  buffer_full_callback_.reset(cb);
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
    for (size_t i = 0; i < logged_events_.size(); i += TRACE_EVENT_BATCH_SIZE) {
      std::string json_events;
      TraceEvent::AppendAsJSON(&json_events, logged_events_,
                               i, TRACE_EVENT_BATCH_SIZE);
      output_callback_->Run(json_events);
    }
  }
  logged_events_.erase(logged_events_.begin(), logged_events_.end());
}

void TraceLog::AddTraceEvent(TraceEventPhase phase,
    const char* file, int line,
    TraceCategory* category,
    const char* name,
    const char* arg1name, TraceValue arg1val,
    const char* arg2name, TraceValue arg2val) {
  DCHECK(file && name);
#ifdef USE_UNRELIABLE_NOW
  TimeTicks now = TimeTicks::HighResNow();
#else
  TimeTicks now = TimeTicks::Now();
#endif
  //static_cast<unsigned long>(base::GetCurrentProcId()),
  AutoLock lock(lock_);
  if (logged_events_.size() >= TRACE_EVENT_BUFFER_SIZE)
    return;
  logged_events_.push_back(TraceEvent());
  TraceEvent& event = logged_events_.back();
  event.processId = static_cast<unsigned long>(base::GetCurrentProcId());
  event.threadId = PlatformThread::CurrentId();
  event.timestamp = now;
  event.phase = phase;
  event.category = category;
  event.name = name;
  event.argNames[0] = arg1name;
  event.argValues[0] = arg1val;
  event.argNames[1] = arg2name;
  event.argValues[1] = arg2val;
  COMPILE_ASSERT(TRACE_MAX_NUM_ARGS == 2, TraceEvent_arc_count_out_of_sync);
  if (logged_events_.size() == TRACE_EVENT_BUFFER_SIZE &&
      buffer_full_callback_.get())
    buffer_full_callback_->Run();
}

}  // namespace gpu
