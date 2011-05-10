// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Trace events are for tracking application performance.
//
// Events are issued against categories. Whereas LOG's
// categories are statically defined, TRACE categories are created
// implicitly with a string. For example:
//   GPU_TRACE_EVENT_INSTANT0("MY_SUBSYSTEM", "SomeImportantEvent")
//
// Events can be INSTANT, or can be pairs of BEGIN and END:
//   GPU_TRACE_EVENT_BEGIN0("MY_SUBSYSTEM", "SomethingCostly")
//   doSomethingCostly()
//   GPU_TRACE_EVENT_END0("MY_SUBSYSTEM", "SomethingCostly")
//
// A common use case is to trace entire function scopes. This
// issues a trace BEGIN and END automatically:
//   void doSomethingCostly() {
//     GPU_TRACE_EVENT0("MY_SUBSYSTEM", "doSomethingCostly");
//     ...
//   }
//
// Additional parameters can be associated with an event:
//   void doSomethingCostly2(int howMuch) {
//     GPU_TRACE_EVENT1("MY_SUBSYSTEM", "doSomethingCostly",
//         "howMuch", StringPrintf("%i", howMuch).c_str());
//     ...
//   }
//
// The trace system will automatically add to this information the
// current process id, thread id, a timestamp down to the
// microsecond, as well as the file and line number of the calling location.
//
// By default, trace collection is compiled in, but turned off at runtime.
// Collecting trace data is the responsibility of the embedding
// application. In Chrome's case, navigating to about:gpu will turn on
// tracing and display data collected across all active processes.
//

#ifndef GPU_TRACE_EVENT_H_
#define GPU_TRACE_EVENT_H_
#pragma once

#if defined(__native_client__)

// Native Client needs to avoid pulling in base/ headers,
// so stub out the tracing code at compile time.
#define GPU_TRACE_EVENT0(x0, x1) { }
#define GPU_TRACE_EVENT1(x0, x1, x2, x3) { }
#define GPU_TRACE_EVENT2(x0, x1, x2, x3, x4, x5) { }
#define GPU_TRACE_EVENT_INSTANT0(x0, x1) { }
#define GPU_TRACE_EVENT_INSTANT1(x0, x1, x2, x3) { }
#define GPU_TRACE_EVENT_INSTANT2(x0, x1, x2, x3, x4, x5) { }
#define GPU_TRACE_BEGIN0(x0, x1) { }
#define GPU_TRACE_BEGIN1(x0, x1, x2, x3) { }
#define GPU_TRACE_BEGIN2(x0, x1, x2, x3, x4, x5) { }
#define GPU_TRACE_END0(x0, x1) { }
#define GPU_TRACE_END1(x0, x1, x2, x3) { }
#define GPU_TRACE_END2(x0, x1, x2, x3, x4, x5) { }

#else

#include "build/build_config.h"

#include <string>

#include "base/callback_old.h"
#include "base/memory/scoped_ptr.h"
#include "base/atomicops.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/callback.h"
#include "base/string_util.h"
#include <vector>


// Implementation detail: trace event macros create temporary variables
// to keep instrumentation overhead low. These macros give each temporary
// variable a unique name based on the line number to prevent name collissions.
#define GPU_TRACE_EVENT_UNIQUE_IDENTIFIER3(a,b) a##b
#define GPU_TRACE_EVENT_UNIQUE_IDENTIFIER2(a,b) \
  GPU_TRACE_EVENT_UNIQUE_IDENTIFIER3(a,b)
#define GPU_TRACE_EVENT_UNIQUE_IDENTIFIER(name_prefix) \
  GPU_TRACE_EVENT_UNIQUE_IDENTIFIER2(name_prefix, __LINE__)

// Records a pair of begin and end events called "name" for the current
// scope, with 0, 1 or 2 associated arguments. If the category is not
// enabled, then this does nothing.
#define GPU_TRACE_EVENT0(category, name) \
  GPU_TRACE_EVENT1(category, name, NULL, 0)
#define GPU_TRACE_EVENT1(category, name, arg1name, arg1val) \
  GPU_TRACE_EVENT2(category, name, arg1name, arg1val, NULL, 0)
#define GPU_TRACE_EVENT2(category, name, arg1name, arg1val, arg2name, arg2val) \
  static gpu::TraceCategory* \
      GPU_TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic) = \
      gpu::TraceLog::GetInstance()->GetCategory(category); \
  if (base::subtle::Acquire_Load(\
      &(GPU_TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic))->enabled_)) { \
    gpu::TraceLog::GetInstance()->AddTraceEvent( \
        gpu::GPU_TRACE_EVENT_PHASE_BEGIN, \
        __FILE__, __LINE__, \
        GPU_TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic), \
        name, \
        arg1name, arg1val, \
        arg2name, arg2val); \
  } \
  gpu::internal::TraceEndOnScopeClose __profileScope ## __LINE ( \
      __FILE__, __LINE__, \
      GPU_TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic), name);

// Records a single event called "name" immediately, with 0, 1 or 2
// associated arguments. If the category is not enabled, then this
// does nothing.
#define GPU_TRACE_EVENT_INSTANT0(category, name) \
  GPU_TRACE_EVENT_INSTANT1(category, name, NULL, 0)
#define GPU_TRACE_EVENT_INSTANT1(category, name, arg1name, arg1val) \
  GPU_TRACE_EVENT_INSTANT2(category, name, arg1name, arg1val, NULL, 0)
#define GPU_TRACE_EVENT_INSTANT2(category, name, arg1name, arg1val, \
    arg2name, arg2val) \
  static gpu::TraceCategory* \
      GPU_TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic) = \
      gpu::TraceLog::GetInstance()->GetCategory(category); \
  if (base::subtle::Acquire_Load( \
      &(GPU_TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic))->enabled_)) { \
    gpu::TraceLog::GetInstance()->AddTraceEvent( \
        gpu::GPU_TRACE_EVENT_PHASE_INSTANT, \
        __FILE__, __LINE__, \
        GPU_TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic), \
        name, \
        arg1name, arg1val, \
        arg2name, arg2val); \
  }

// Records a single BEGIN event called "name" immediately, with 0, 1 or 2
// associated arguments. If the category is not enabled, then this
// does nothing.
#define GPU_TRACE_EVENT_BEGIN0(category, name) \
  GPU_TRACE_EVENT_BEGIN1(category, name, NULL, 0)
#define GPU_TRACE_EVENT_BEGIN1(category, name, arg1name, arg1val) \
  GPU_TRACE_EVENT_BEGIN2(category, name, arg1name, arg1val, NULL, 0)
#define GPU_TRACE_EVENT_BEGIN2(category, name, arg1name, arg1val, \
    arg2name, arg2val) \
  static gpu::TraceCategory* \
      GPU_TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic) = \
      gpu::TraceLog::GetInstance()->GetCategory(category); \
  if (base::subtle::Acquire_Load( \
      &(GPU_TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic))->enabled_)) { \
    gpu::TraceLog::GetInstance()->AddTraceEvent( \
        gpu::GPU_TRACE_EVENT_PHASE_BEGIN, \
        __FILE__, __LINE__, \
        GPU_TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic), \
        name, \
        arg1name, arg1val, \
        arg2name, arg2val); \
  }

// Records a single END event for "name" immediately. If the category
// is not enabled, then this does nothing.
#define GPU_TRACE_EVENT_END0(category, name) \
  static gpu::TraceCategory* \
      GPU_TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic) = \
      gpu::TraceLog::GetInstance()->GetCategory(category); \
  if (base::subtle::Acquire_Load( \
      &(GPU_TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic))->enabled_)) {  \
    gpu::TraceLog::GetInstance()->AddTraceEvent( \
        gpu::GPU_TRACE_EVENT_PHASE_END, \
        __FILE__, __LINE__, \
        GPU_TRACE_EVENT_UNIQUE_IDENTIFIER(catstatic), \
        name, \
        NULL, 0, \
        NULL, 0); \
  }


namespace gpu {

// Categories allow enabling/disabling of streams of trace events
// Don't manipulate the category object directly, as this may lead
// to threading issues. Use the TraceLog methods instead.
class TraceCategory {
 public:
  TraceCategory();
  ~TraceCategory();

  void set(const char* name, bool enabled) {
    name_ = name;
    base::subtle::NoBarrier_Store(&enabled_,
                                  static_cast<base::subtle::Atomic32>(enabled));
  }

  const char* name() const { return name_; }

  // NEVER read these directly, let the macros do it for you
  volatile base::subtle::Atomic32 enabled_;
 protected:
  const char* name_;
};

#define TRACE_MAX_NUM_ARGS 2

enum TraceEventPhase {
  GPU_TRACE_EVENT_PHASE_BEGIN,
  GPU_TRACE_EVENT_PHASE_END,
  GPU_TRACE_EVENT_PHASE_INSTANT
};

// Simple union of values. This is much lighter weight than base::Value, which
// requires dynamic allocation and a vtable. To keep the trace runtime overhead
// low, we want constant size storage here.
class TraceValue {
 public:
  enum Type {
    TRACE_TYPE_UNDEFINED,
    TRACE_TYPE_BOOL,
    TRACE_TYPE_UINT,
    TRACE_TYPE_INT,
    TRACE_TYPE_DOUBLE,
    TRACE_TYPE_POINTER,
    TRACE_TYPE_STRING
  };

  TraceValue() : type_(TRACE_TYPE_UNDEFINED) {
    value_.as_uint = 0ull;
  }
  TraceValue(bool rhs) : type_(TRACE_TYPE_BOOL) {
    value_.as_uint = 0ull; // zero all bits
    value_.as_bool = rhs;
  }
  TraceValue(uint64 rhs) : type_(TRACE_TYPE_UINT) {
    value_.as_uint = rhs;
  }
  TraceValue(uint32 rhs) : type_(TRACE_TYPE_UINT) {
    value_.as_uint = rhs;
  }
  TraceValue(uint16 rhs) : type_(TRACE_TYPE_UINT) {
    value_.as_uint = rhs;
  }
  TraceValue(uint8 rhs) : type_(TRACE_TYPE_UINT) {
    value_.as_uint = rhs;
  }
  TraceValue(int64 rhs) : type_(TRACE_TYPE_INT) {
    value_.as_int = rhs;
  }
  TraceValue(int32 rhs) : type_(TRACE_TYPE_INT) {
    value_.as_int = rhs;
  }
  TraceValue(int16 rhs) : type_(TRACE_TYPE_INT) {
    value_.as_int = rhs;
  }
  TraceValue(int8 rhs) : type_(TRACE_TYPE_INT) {
    value_.as_int = rhs;
  }
  TraceValue(double rhs) : type_(TRACE_TYPE_DOUBLE) {
    value_.as_double = rhs;
  }
  TraceValue(const void* rhs) : type_(TRACE_TYPE_POINTER) {
    value_.as_uint = 0ull; // zero all bits
    value_.as_pointer = rhs;
  }
  explicit TraceValue(const char* rhs) : type_(TRACE_TYPE_STRING) {
    value_.as_uint = 0ull; // zero all bits
    value_.as_string = base::strdup(rhs);
  }
  TraceValue(const TraceValue& rhs) : type_(TRACE_TYPE_UNDEFINED) {
    operator=(rhs);
  }
  ~TraceValue() {
    Destroy();
  }

  TraceValue& operator=(const TraceValue& rhs);
  bool operator==(const TraceValue& rhs) const;
  bool operator!=(const TraceValue& rhs) const {
    return !operator==(rhs);
  }

  void Destroy();

  void AppendAsJSON(std::string* out) const;

  Type type() const {
    return type_;
  }
  uint64 as_uint() const {
    return value_.as_uint;
  }
  bool as_bool() const {
    return value_.as_bool;
  }
  int64 as_int() const {
    return value_.as_int;
  }
  double as_double() const {
    return value_.as_double;
  }
  const void* as_pointer() const {
    return value_.as_pointer;
  }
  const char* as_string() const {
    return value_.as_string;
  }

 private:
  union Value {
    bool as_bool;
    uint64 as_uint;
    int64 as_int;
    double as_double;
    const void* as_pointer;
    char* as_string;
  };

  Type type_;
  Value value_;
};

// Output records are "Events" and can be obtained via the
// OutputCallback whenever the logging system decides to flush. This
// can happen at any time, on any thread, or you can programatically
// force it to happen.
struct TraceEvent {
  static void AppendAsJSON(std::string* out,
      const std::vector<TraceEvent>& events,
      size_t start,
      size_t count);
  TraceEvent();
  ~TraceEvent();
  void AppendAsJSON(std::string* out) const;


  unsigned long processId;
  unsigned long threadId;
  base::TimeTicks timestamp;
  TraceEventPhase phase;
  TraceCategory* category;
  const char* name;
  const char* argNames[TRACE_MAX_NUM_ARGS];
  TraceValue argValues[TRACE_MAX_NUM_ARGS];
};


class TraceLog {
 public:
  static TraceLog* GetInstance();

  // Global enable of tracing. Currently enables all categories or not.
  // TODO(nduca) Replaced with an Enable/DisableCategory() that
  // implicitly controls the global logging state.
  void SetEnabled(bool enabled);

  float GetBufferPercentFull() const;

  // When enough events are collected, they are handed (in bulk) to
  // the output callback. If no callback is set, the output will be
  // silently dropped.
  typedef Callback1<const std::string& /* json_events */>::Type OutputCallback;
  void SetOutputCallback(OutputCallback* cb);

  // The trace buffer does not flush dynamically, so when it fills up,
  // subsequent trace events will be dropped. This callback is generated when
  // the trace buffer is full.
  typedef Callback0::Type BufferFullCallback;
  void SetBufferFullCallback(BufferFullCallback* cb);

  // Forwards data collected by a child process to the registered
  //  output callback.
  void AddRemotelyCollectedData(const std::string& json_events);

  // Flushes all logged data to the callback.
  void Flush();

  // Called by GPU_TRACE_EVENT* macros, don't call this directly.
  TraceCategory* GetCategory(const char* name);

  // Called by GPU_TRACE_EVENT* macros, don't call this directly.
  void AddTraceEvent(TraceEventPhase phase,
      const char* file, int line,
      TraceCategory* category,
      const char* name,
      const char* arg1name, TraceValue arg1val,
      const char* arg2name, TraceValue arg2val);

 private:
  // This allows constructor and destructor to be private and usable only
  // by the Singleton class.
  friend struct StaticMemorySingletonTraits<TraceLog>;

  TraceLog();
  ~TraceLog();
  void FlushWithLockAlreadyHeld();

  // TODO(nduca): switch to per-thread trace buffers to reduce thread
  // synchronization.
  base::Lock lock_;
  bool enabled_;
  scoped_ptr<OutputCallback> output_callback_;
  scoped_ptr<BufferFullCallback> buffer_full_callback_;
  std::vector<TraceEvent> logged_events_;

  DISALLOW_COPY_AND_ASSIGN(TraceLog);
};

namespace internal {

// Used by GPU_TRACE_EVENTx macro. Do not use directly.
class TraceEndOnScopeClose {
 public:
  TraceEndOnScopeClose(const char* file, int line,
                       TraceCategory* category,
                       const char* name)
      : file_(file)
      , line_(line)
      , category_(category)
      , name_(name) { }

  ~TraceEndOnScopeClose() {
    if (base::subtle::Acquire_Load(&category_->enabled_))
      gpu::TraceLog::GetInstance()->AddTraceEvent(
          gpu::GPU_TRACE_EVENT_PHASE_END,
          file_, line_,
          category_,
          name_,
          NULL, 0, NULL, 0);
  }

 private:
  const char* file_;
  int line_;
  TraceCategory* category_;
  const char* name_;
};

}  // namespace internal

}  // namespace gpu
#endif  // __native_client__
#endif  // GPU_TRACE_EVENT_H_
