// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

#include "build/build_config.h"

#include <string>

#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "base/atomicops.h"
#include "base/singleton.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/callback.h"
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
  GPU_TRACE_EVENT1(category, name, NULL, NULL)
#define GPU_TRACE_EVENT1(category, name, arg1name, arg1val) \
  GPU_TRACE_EVENT2(category, name, arg1name, arg1val, NULL, NULL)
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
  GPU_TRACE_EVENT_INSTANT1(category, name, NULL, NULL)
#define GPU_TRACE_EVENT_INSTANT1(category, name, arg1name, arg1val) \
  GPU_TRACE_EVENT_INSTANT2(category, name, arg1name, arg1val, NULL, NULL)
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
  GPU_TRACE_EVENT_BEGIN1(category, name, NULL, NULL)
#define GPU_TRACE_EVENT_BEGIN1(category, name, arg1name, arg1val) \
  GPU_TRACE_EVENT_BEGIN2(category, name, arg1name, arg1val, NULL, NULL)
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
        arg1name, arg1val, \
        arg2name, arg2val); \
  }


namespace gpu {

// Categories allow enabling/disabling of streams of trace events
// Don't manipulate the category object directly, as this may lead
// to threading issues. Use the TraceLog methods instead.
class TraceCategory {
 public:
  TraceCategory(const char* name, bool enabled);
  ~TraceCategory();

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

// Output records are "Events" and can be obtained via the
// OutputCallback whenever the logging system decides to flush. This
// can happen at any time, on any thread, or you can programatically
// force it to happen.
struct TraceEvent {
  static void AppendAsJSON(std::string* out,
      const std::vector<TraceEvent>& events);
  TraceEvent() { }
  void AppendAsJSON(std::string* out) const;


  unsigned long processId;
  unsigned long threadId;
  base::TimeTicks timestamp;
  TraceEventPhase phase;
  TraceCategory* category;
  const char* name;
  const char* argNames[TRACE_MAX_NUM_ARGS];
  std::string argValues[TRACE_MAX_NUM_ARGS];
};


class TraceLog {
 public:
  static TraceLog* GetInstance();

  // Global enable of tracing. Currently enables all categories or not.
  // TODO(nduca) Replaced with an Enable/DisableCategory() that
  // implicitly controls the global logging state.
  void SetEnabled(bool enabled);

  // When enough events are collected, they are handed (in bulk) to
  // the output callback. If no callback is set, the output will be
  // silently dropped.
  typedef Callback1<const std::string& /* json_events */>::Type OutputCallback;
  void SetOutputCallback(OutputCallback* cb);

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
      const char* arg1name, const char* arg1val,
      const char* arg2name, const char* arg2val);

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
  ScopedVector<TraceCategory> categories_;
  scoped_ptr<OutputCallback> output_callback_;
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
          NULL, NULL, NULL, NULL);
  }

 private:
  const char* file_;
  int line_;
  TraceCategory* category_;
  const char* name_;
};

}  // namespace internal

}  // namespace gpu

#endif  // GPU_TRACE_EVENT_H_
