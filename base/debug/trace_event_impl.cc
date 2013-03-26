// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/debug/leak_annotations.h"
#include "base/debug/trace_event.h"
#include "base/format_macros.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/synchronization/waitable_event.h"
#include "base/sys_info.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_local.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"

#if defined(OS_WIN)
#include "base/debug/trace_event_win.h"
#endif

class DeleteTraceLogForTesting {
 public:
  static void Delete() {
    Singleton<base::debug::TraceLog,
              StaticMemorySingletonTraits<base::debug::TraceLog> >::OnExit(0);
  }
};

// The thread buckets for the sampling profiler.
BASE_EXPORT TRACE_EVENT_API_ATOMIC_WORD g_trace_state0;
BASE_EXPORT TRACE_EVENT_API_ATOMIC_WORD g_trace_state1;
BASE_EXPORT TRACE_EVENT_API_ATOMIC_WORD g_trace_state2;

namespace base {
namespace debug {

// Controls the number of trace events we will buffer in-memory
// before throwing them away.
const size_t kTraceEventBufferSize = 500000;
const size_t kTraceEventBatchSize = 1000;
const size_t kTraceEventInitialBufferSize = 1024;

#define TRACE_EVENT_MAX_CATEGORIES 100

namespace {

// Parallel arrays g_categories and g_category_enabled are separate so that
// a pointer to a member of g_category_enabled can be easily converted to an
// index into g_categories. This allows macros to deal only with char enabled
// pointers from g_category_enabled, and we can convert internally to determine
// the category name from the char enabled pointer.
const char* g_categories[TRACE_EVENT_MAX_CATEGORIES] = {
  "tracing already shutdown",
  "tracing categories exhausted; must increase TRACE_EVENT_MAX_CATEGORIES",
  "__metadata",
};

// The enabled flag is char instead of bool so that the API can be used from C.
unsigned char g_category_enabled[TRACE_EVENT_MAX_CATEGORIES] = { 0 };
const int g_category_already_shutdown = 0;
const int g_category_categories_exhausted = 1;
const int g_category_metadata = 2;
const int g_num_builtin_categories = 3;
int g_category_index = g_num_builtin_categories; // Skip default categories.

// The name of the current thread. This is used to decide if the current
// thread name has changed. We combine all the seen thread names into the
// output name for the thread.
LazyInstance<ThreadLocalPointer<const char> >::Leaky
    g_current_thread_name = LAZY_INSTANCE_INITIALIZER;

const char kRecordUntilFull[] = "record-until-full";
const char kRecordContinuously[] = "record-continuously";

size_t NextIndex(size_t index) {
  index++;
  if (index >= kTraceEventBufferSize)
    index = 0;
  return index;
}

}  // namespace

class TraceBufferRingBuffer : public TraceBuffer {
 public:
  TraceBufferRingBuffer()
      : unused_event_index_(0),
        oldest_event_index_(0) {
    logged_events_.reserve(kTraceEventInitialBufferSize);
  }

  ~TraceBufferRingBuffer() {}

  void AddEvent(const TraceEvent& event) OVERRIDE {
    if (unused_event_index_ < Size())
      logged_events_[unused_event_index_] = event;
    else
      logged_events_.push_back(event);

    unused_event_index_ = NextIndex(unused_event_index_);
    if (unused_event_index_ == oldest_event_index_) {
      oldest_event_index_ = NextIndex(oldest_event_index_);
    }
  }

  bool HasMoreEvents() const OVERRIDE {
    return oldest_event_index_ != unused_event_index_;
  }

  const TraceEvent& NextEvent() OVERRIDE {
    DCHECK(HasMoreEvents());

    size_t next = oldest_event_index_;
    oldest_event_index_ = NextIndex(oldest_event_index_);
    return GetEventAt(next);
  }

  bool IsFull() const OVERRIDE {
    return false;
  }

  size_t CountEnabledByName(const unsigned char* category,
                            const std::string& event_name) const OVERRIDE {
    size_t notify_count = 0;
    size_t index = oldest_event_index_;
    while (index != unused_event_index_) {
      const TraceEvent& event = GetEventAt(index);
      if (category == event.category_enabled() &&
          strcmp(event_name.c_str(), event.name()) == 0) {
        ++notify_count;
      }
      index = NextIndex(index);
    }
    return notify_count;
  }

  const TraceEvent& GetEventAt(size_t index) const OVERRIDE {
    DCHECK(index < logged_events_.size());
    return logged_events_[index];
  }

  size_t Size() const OVERRIDE {
    return logged_events_.size();
  }

 private:
  size_t unused_event_index_;
  size_t oldest_event_index_;
  std::vector<TraceEvent> logged_events_;

  DISALLOW_COPY_AND_ASSIGN(TraceBufferRingBuffer);
};

class TraceBufferVector : public TraceBuffer {
 public:
  TraceBufferVector() : current_iteration_index_(0) {
    logged_events_.reserve(kTraceEventInitialBufferSize);
  }

  ~TraceBufferVector() {
  }

  void AddEvent(const TraceEvent& event) OVERRIDE {
    // Note, we have two callers which need to be handled. The first is
    // AddTraceEventWithThreadIdAndTimestamp() which checks Size() and does an
    // early exit if full. The second is AddThreadNameMetadataEvents().
    // We can not DECHECK(!IsFull()) because we have to add the metadata
    // events even if the buffer is full.
    logged_events_.push_back(event);
  }

  bool HasMoreEvents() const OVERRIDE {
    return current_iteration_index_ < Size();
  }

  const TraceEvent& NextEvent() OVERRIDE {
    DCHECK(HasMoreEvents());
    return GetEventAt(current_iteration_index_++);
  }

  bool IsFull() const OVERRIDE {
    return Size() >= kTraceEventBufferSize;
  }

  size_t CountEnabledByName(const unsigned char* category,
                            const std::string& event_name) const OVERRIDE {
    size_t notify_count = 0;
    for (size_t i = 0; i < Size(); i++) {
      const TraceEvent& event = GetEventAt(i);
      if (category == event.category_enabled() &&
          strcmp(event_name.c_str(), event.name()) == 0) {
        ++notify_count;
      }
    }
    return notify_count;
  }

  const TraceEvent& GetEventAt(size_t index) const OVERRIDE {
    DCHECK(index < logged_events_.size());
    return logged_events_[index];
  }

  size_t Size() const OVERRIDE {
    return logged_events_.size();
  }

 private:
  size_t current_iteration_index_;
  std::vector<TraceEvent> logged_events_;

  DISALLOW_COPY_AND_ASSIGN(TraceBufferVector);
};

////////////////////////////////////////////////////////////////////////////////
//
// TraceEvent
//
////////////////////////////////////////////////////////////////////////////////

namespace {

size_t GetAllocLength(const char* str) { return str ? strlen(str) + 1 : 0; }

// Copies |*member| into |*buffer|, sets |*member| to point to this new
// location, and then advances |*buffer| by the amount written.
void CopyTraceEventParameter(char** buffer,
                             const char** member,
                             const char* end) {
  if (*member) {
    size_t written = strlcpy(*buffer, *member, end - *buffer) + 1;
    DCHECK_LE(static_cast<int>(written), end - *buffer);
    *member = *buffer;
    *buffer += written;
  }
}

}  // namespace

TraceEvent::TraceEvent()
    : id_(0u),
      category_enabled_(NULL),
      name_(NULL),
      thread_id_(0),
      phase_(TRACE_EVENT_PHASE_BEGIN),
      flags_(0) {
  arg_names_[0] = NULL;
  arg_names_[1] = NULL;
  memset(arg_values_, 0, sizeof(arg_values_));
}

TraceEvent::TraceEvent(int thread_id,
                       TimeTicks timestamp,
                       char phase,
                       const unsigned char* category_enabled,
                       const char* name,
                       unsigned long long id,
                       int num_args,
                       const char** arg_names,
                       const unsigned char* arg_types,
                       const unsigned long long* arg_values,
                       unsigned char flags)
    : timestamp_(timestamp),
      id_(id),
      category_enabled_(category_enabled),
      name_(name),
      thread_id_(thread_id),
      phase_(phase),
      flags_(flags) {
  // Clamp num_args since it may have been set by a third_party library.
  num_args = (num_args > kTraceMaxNumArgs) ? kTraceMaxNumArgs : num_args;
  int i = 0;
  for (; i < num_args; ++i) {
    arg_names_[i] = arg_names[i];
    arg_values_[i].as_uint = arg_values[i];
    arg_types_[i] = arg_types[i];
  }
  for (; i < kTraceMaxNumArgs; ++i) {
    arg_names_[i] = NULL;
    arg_values_[i].as_uint = 0u;
    arg_types_[i] = TRACE_VALUE_TYPE_UINT;
  }

  bool copy = !!(flags & TRACE_EVENT_FLAG_COPY);
  size_t alloc_size = 0;
  if (copy) {
    alloc_size += GetAllocLength(name);
    for (i = 0; i < num_args; ++i) {
      alloc_size += GetAllocLength(arg_names_[i]);
      if (arg_types_[i] == TRACE_VALUE_TYPE_STRING)
        arg_types_[i] = TRACE_VALUE_TYPE_COPY_STRING;
    }
  }

  bool arg_is_copy[kTraceMaxNumArgs];
  for (i = 0; i < num_args; ++i) {
    // We only take a copy of arg_vals if they are of type COPY_STRING.
    arg_is_copy[i] = (arg_types_[i] == TRACE_VALUE_TYPE_COPY_STRING);
    if (arg_is_copy[i])
      alloc_size += GetAllocLength(arg_values_[i].as_string);
  }

  if (alloc_size) {
    parameter_copy_storage_ = new RefCountedString;
    parameter_copy_storage_->data().resize(alloc_size);
    char* ptr = string_as_array(&parameter_copy_storage_->data());
    const char* end = ptr + alloc_size;
    if (copy) {
      CopyTraceEventParameter(&ptr, &name_, end);
      for (i = 0; i < num_args; ++i)
        CopyTraceEventParameter(&ptr, &arg_names_[i], end);
    }
    for (i = 0; i < num_args; ++i) {
      if (arg_is_copy[i])
        CopyTraceEventParameter(&ptr, &arg_values_[i].as_string, end);
    }
    DCHECK_EQ(end, ptr) << "Overrun by " << ptr - end;
  }
}

TraceEvent::~TraceEvent() {
}

// static
void TraceEvent::AppendValueAsJSON(unsigned char type,
                                   TraceEvent::TraceValue value,
                                   std::string* out) {
  std::string::size_type start_pos;
  switch (type) {
    case TRACE_VALUE_TYPE_BOOL:
      *out += value.as_bool ? "true" : "false";
      break;
    case TRACE_VALUE_TYPE_UINT:
      StringAppendF(out, "%" PRIu64, static_cast<uint64>(value.as_uint));
      break;
    case TRACE_VALUE_TYPE_INT:
      StringAppendF(out, "%" PRId64, static_cast<int64>(value.as_int));
      break;
    case TRACE_VALUE_TYPE_DOUBLE:
      StringAppendF(out, "%f", value.as_double);
      break;
    case TRACE_VALUE_TYPE_POINTER:
      // JSON only supports double and int numbers.
      // So as not to lose bits from a 64-bit pointer, output as a hex string.
      StringAppendF(out, "\"%" PRIx64 "\"", static_cast<uint64>(
                                     reinterpret_cast<intptr_t>(
                                     value.as_pointer)));
      break;
    case TRACE_VALUE_TYPE_STRING:
    case TRACE_VALUE_TYPE_COPY_STRING:
      *out += "\"";
      start_pos = out->size();
      *out += value.as_string ? value.as_string : "NULL";
      // insert backslash before special characters for proper json format.
      while ((start_pos = out->find_first_of("\\\"", start_pos)) !=
             std::string::npos) {
        out->insert(start_pos, 1, '\\');
        // skip inserted escape character and following character.
        start_pos += 2;
      }
      *out += "\"";
      break;
    default:
      NOTREACHED() << "Don't know how to print this value";
      break;
  }
}

void TraceEvent::AppendAsJSON(std::string* out) const {
  int64 time_int64 = timestamp_.ToInternalValue();
  int process_id = TraceLog::GetInstance()->process_id();
  // Category name checked at category creation time.
  DCHECK(!strchr(name_, '"'));
  StringAppendF(out,
      "{\"cat\":\"%s\",\"pid\":%i,\"tid\":%i,\"ts\":%" PRId64 ","
      "\"ph\":\"%c\",\"name\":\"%s\",\"args\":{",
      TraceLog::GetCategoryName(category_enabled_),
      process_id,
      thread_id_,
      time_int64,
      phase_,
      name_);

  // Output argument names and values, stop at first NULL argument name.
  for (int i = 0; i < kTraceMaxNumArgs && arg_names_[i]; ++i) {
    if (i > 0)
      *out += ",";
    *out += "\"";
    *out += arg_names_[i];
    *out += "\":";
    AppendValueAsJSON(arg_types_[i], arg_values_[i], out);
  }
  *out += "}";

  // If id_ is set, print it out as a hex string so we don't loose any
  // bits (it might be a 64-bit pointer).
  if (flags_ & TRACE_EVENT_FLAG_HAS_ID)
    StringAppendF(out, ",\"id\":\"%" PRIx64 "\"", static_cast<uint64>(id_));

  // Instant events also output their scope.
  if (phase_ == TRACE_EVENT_PHASE_INSTANT) {
    char scope = '?';
    switch (flags_ & TRACE_EVENT_FLAG_SCOPE_MASK) {
      case TRACE_EVENT_SCOPE_GLOBAL:
        scope = TRACE_EVENT_SCOPE_NAME_GLOBAL;
        break;

      case TRACE_EVENT_SCOPE_PROCESS:
        scope = TRACE_EVENT_SCOPE_NAME_PROCESS;
        break;

      case TRACE_EVENT_SCOPE_THREAD:
        scope = TRACE_EVENT_SCOPE_NAME_THREAD;
        break;
    }
    StringAppendF(out, ",\"s\":\"%c\"", scope);
  }

  *out += "}";
}

////////////////////////////////////////////////////////////////////////////////
//
// TraceResultBuffer
//
////////////////////////////////////////////////////////////////////////////////

TraceResultBuffer::OutputCallback
    TraceResultBuffer::SimpleOutput::GetCallback() {
  return Bind(&SimpleOutput::Append, Unretained(this));
}

void TraceResultBuffer::SimpleOutput::Append(
    const std::string& json_trace_output) {
  json_output += json_trace_output;
}

TraceResultBuffer::TraceResultBuffer() : append_comma_(false) {
}

TraceResultBuffer::~TraceResultBuffer() {
}

void TraceResultBuffer::SetOutputCallback(
    const OutputCallback& json_chunk_callback) {
  output_callback_ = json_chunk_callback;
}

void TraceResultBuffer::Start() {
  append_comma_ = false;
  output_callback_.Run("[");
}

void TraceResultBuffer::AddFragment(const std::string& trace_fragment) {
  if (append_comma_)
    output_callback_.Run(",");
  append_comma_ = true;
  output_callback_.Run(trace_fragment);
}

void TraceResultBuffer::Finish() {
  output_callback_.Run("]");
}

////////////////////////////////////////////////////////////////////////////////
//
// TraceSamplingThread
//
////////////////////////////////////////////////////////////////////////////////
class TraceBucketData;
typedef base::Callback<void(TraceBucketData*)> TraceSampleCallback;

class TraceBucketData {
 public:
  TraceBucketData(base::subtle::AtomicWord* bucket,
                  const char* name,
                  TraceSampleCallback callback);
  ~TraceBucketData();

  TRACE_EVENT_API_ATOMIC_WORD* bucket;
  const char* bucket_name;
  TraceSampleCallback callback;
};

// This object must be created on the IO thread.
class TraceSamplingThread : public PlatformThread::Delegate {
 public:
  TraceSamplingThread();
  virtual ~TraceSamplingThread();

  // Implementation of PlatformThread::Delegate:
  virtual void ThreadMain() OVERRIDE;

  static void DefaultSampleCallback(TraceBucketData* bucekt_data);

  void Stop();
  void InstallWaitableEventForSamplingTesting(WaitableEvent* waitable_event);

 private:
  friend class TraceLog;

  void GetSamples();
  // Not thread-safe. Once the ThreadMain has been called, this can no longer
  // be called.
  void RegisterSampleBucket(TRACE_EVENT_API_ATOMIC_WORD* bucket,
                            const char* const name,
                            TraceSampleCallback callback);
  // Splits a combined "category\0name" into the two component parts.
  static void ExtractCategoryAndName(const char* combined,
                                     const char** category,
                                     const char** name);
  std::vector<TraceBucketData> sample_buckets_;
  bool thread_running_;
  scoped_ptr<CancellationFlag> cancellation_flag_;
  scoped_ptr<WaitableEvent> waitable_event_for_testing_;
};


TraceSamplingThread::TraceSamplingThread()
    : thread_running_(false) {
  cancellation_flag_.reset(new CancellationFlag);
}

TraceSamplingThread::~TraceSamplingThread() {
}

void TraceSamplingThread::ThreadMain() {
  PlatformThread::SetName("Sampling Thread");
  thread_running_ = true;
  const int kSamplingFrequencyMicroseconds = 1000;
  while (!cancellation_flag_->IsSet()) {
    PlatformThread::Sleep(
        TimeDelta::FromMicroseconds(kSamplingFrequencyMicroseconds));
    GetSamples();
    if (waitable_event_for_testing_.get())
      waitable_event_for_testing_->Signal();
  }
}

// static
void TraceSamplingThread::DefaultSampleCallback(TraceBucketData* bucket_data) {
  TRACE_EVENT_API_ATOMIC_WORD category_and_name =
      TRACE_EVENT_API_ATOMIC_LOAD(*bucket_data->bucket);
  if (!category_and_name)
    return;
  const char* const combined =
      reinterpret_cast<const char* const>(category_and_name);
  const char* category;
  const char* name;
  ExtractCategoryAndName(combined, &category, &name);
  TRACE_EVENT_API_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_SAMPLE,
                                  TraceLog::GetCategoryEnabled(category),
                                  name,
                                  0,
                                  0,
                                  NULL,
                                  NULL,
                                  NULL,
                                  0);
}

void TraceSamplingThread::GetSamples() {
  for (size_t i = 0; i < sample_buckets_.size(); ++i) {
    TraceBucketData* bucket_data = &sample_buckets_[i];
    bucket_data->callback.Run(bucket_data);
  }
}

void TraceSamplingThread::RegisterSampleBucket(
    TRACE_EVENT_API_ATOMIC_WORD* bucket,
    const char* const name,
    TraceSampleCallback callback) {
  DCHECK(!thread_running_);
  sample_buckets_.push_back(TraceBucketData(bucket, name, callback));
}

// static
void TraceSamplingThread::ExtractCategoryAndName(const char* combined,
                                                 const char** category,
                                                 const char** name) {
  *category = combined;
  *name = &combined[strlen(combined) + 1];
}

void TraceSamplingThread::Stop() {
  cancellation_flag_->Set();
}

void TraceSamplingThread::InstallWaitableEventForSamplingTesting(
    WaitableEvent* waitable_event) {
  waitable_event_for_testing_.reset(waitable_event);
}


TraceBucketData::TraceBucketData(base::subtle::AtomicWord* bucket,
                                 const char* name,
                                 TraceSampleCallback callback)
    : bucket(bucket),
      bucket_name(name),
      callback(callback) {
}

TraceBucketData::~TraceBucketData() {
}

////////////////////////////////////////////////////////////////////////////////
//
// TraceLog
//
////////////////////////////////////////////////////////////////////////////////

TraceLog::NotificationHelper::NotificationHelper(TraceLog* trace_log)
    : trace_log_(trace_log),
      notification_(0) {
}

TraceLog::NotificationHelper::~NotificationHelper() {
}

void TraceLog::NotificationHelper::AddNotificationWhileLocked(
    int notification) {
  if (trace_log_->notification_callback_.is_null())
    return;
  if (notification_ == 0)
    callback_copy_ = trace_log_->notification_callback_;
  notification_ |= notification;
}

void TraceLog::NotificationHelper::SendNotificationIfAny() {
  if (notification_)
    callback_copy_.Run(notification_);
}

// static
TraceLog* TraceLog::GetInstance() {
  return Singleton<TraceLog, StaticMemorySingletonTraits<TraceLog> >::get();
}

// static
// Note, if you add more options here you also need to update:
// content/browser/devtools/devtools_tracing_handler:TraceOptionsFromString
TraceLog::Options TraceLog::TraceOptionsFromString(const std::string& options) {
  std::vector<std::string> split;
  base::SplitString(options, ',', &split);
  int ret = 0;
  for (std::vector<std::string>::iterator iter = split.begin();
       iter != split.end();
       ++iter) {
    if (*iter == kRecordUntilFull) {
      ret |= RECORD_UNTIL_FULL;
    } else if (*iter == kRecordContinuously) {
      ret |= RECORD_CONTINUOUSLY;
    } else {
      NOTREACHED();  // Unknown option provided.
    }
  }
  if (!(ret & RECORD_UNTIL_FULL) && !(ret & RECORD_CONTINUOUSLY))
    ret |= RECORD_UNTIL_FULL;  // Default when no options are specified.

  return static_cast<Options>(ret);
}

TraceLog::TraceLog()
    : enable_count_(0),
      logged_events_(NULL),
      dispatching_to_observer_list_(false),
      watch_category_(NULL),
      trace_options_(RECORD_UNTIL_FULL),
      sampling_thread_handle_(0) {
  // Trace is enabled or disabled on one thread while other threads are
  // accessing the enabled flag. We don't care whether edge-case events are
  // traced or not, so we allow races on the enabled flag to keep the trace
  // macros fast.
  // TODO(jbates): ANNOTATE_BENIGN_RACE_SIZED crashes windows TSAN bots:
  // ANNOTATE_BENIGN_RACE_SIZED(g_category_enabled, sizeof(g_category_enabled),
  //                            "trace_event category enabled");
  for (int i = 0; i < TRACE_EVENT_MAX_CATEGORIES; ++i) {
    ANNOTATE_BENIGN_RACE(&g_category_enabled[i],
                         "trace_event category enabled");
  }
#if defined(OS_NACL)  // NaCl shouldn't expose the process id.
  SetProcessID(0);
#else
  SetProcessID(static_cast<int>(GetCurrentProcId()));
#endif

  logged_events_.reset(GetTraceBuffer());
}

TraceLog::~TraceLog() {
}

const unsigned char* TraceLog::GetCategoryEnabled(const char* name) {
  TraceLog* tracelog = GetInstance();
  if (!tracelog) {
    DCHECK(!g_category_enabled[g_category_already_shutdown]);
    return &g_category_enabled[g_category_already_shutdown];
  }
  return tracelog->GetCategoryEnabledInternal(name);
}

const char* TraceLog::GetCategoryName(const unsigned char* category_enabled) {
  // Calculate the index of the category by finding category_enabled in
  // g_category_enabled array.
  uintptr_t category_begin = reinterpret_cast<uintptr_t>(g_category_enabled);
  uintptr_t category_ptr = reinterpret_cast<uintptr_t>(category_enabled);
  DCHECK(category_ptr >= category_begin &&
         category_ptr < reinterpret_cast<uintptr_t>(g_category_enabled +
                                               TRACE_EVENT_MAX_CATEGORIES)) <<
      "out of bounds category pointer";
  uintptr_t category_index =
      (category_ptr - category_begin) / sizeof(g_category_enabled[0]);
  return g_categories[category_index];
}

static void EnableMatchingCategory(int category_index,
                                   const std::vector<std::string>& patterns,
                                   unsigned char matched_value,
                                   unsigned char unmatched_value) {
  std::vector<std::string>::const_iterator ci = patterns.begin();
  bool is_match = false;
  for (; ci != patterns.end(); ++ci) {
    is_match = MatchPattern(g_categories[category_index], ci->c_str());
    if (is_match)
      break;
  }
  g_category_enabled[category_index] = is_match ?
      matched_value : unmatched_value;
}

// Enable/disable each category based on the category filters in |patterns|.
// If the category name matches one of the patterns, its enabled status is set
// to |matched_value|. Otherwise its enabled status is set to |unmatched_value|.
static void EnableMatchingCategories(const std::vector<std::string>& patterns,
                                     unsigned char matched_value,
                                     unsigned char unmatched_value) {
  for (int i = 0; i < g_category_index; i++)
    EnableMatchingCategory(i, patterns, matched_value, unmatched_value);
}

const unsigned char* TraceLog::GetCategoryEnabledInternal(const char* name) {
  AutoLock lock(lock_);
  DCHECK(!strchr(name, '"')) << "Category names may not contain double quote";

  unsigned char* category_enabled = NULL;
  // Search for pre-existing category matching this name
  for (int i = 0; i < g_category_index; i++) {
    if (strcmp(g_categories[i], name) == 0) {
      category_enabled = &g_category_enabled[i];
      break;
    }
  }

  if (!category_enabled) {
    // Create a new category
    DCHECK(g_category_index < TRACE_EVENT_MAX_CATEGORIES) <<
        "must increase TRACE_EVENT_MAX_CATEGORIES";
    if (g_category_index < TRACE_EVENT_MAX_CATEGORIES) {
      int new_index = g_category_index++;
      // Don't hold on to the name pointer, so that we can create categories
      // with strings not known at compile time (this is required by
      // SetWatchEvent).
      const char* new_name = strdup(name);
      ANNOTATE_LEAKING_OBJECT_PTR(new_name);
      g_categories[new_index] = new_name;
      DCHECK(!g_category_enabled[new_index]);
      if (enable_count_) {
        // Note that if both included and excluded_categories are empty, the
        // else clause below excludes nothing, thereby enabling this category.
        if (!included_categories_.empty()) {
          EnableMatchingCategory(new_index, included_categories_,
                                 CATEGORY_ENABLED, 0);
        } else {
          EnableMatchingCategory(new_index, excluded_categories_,
                                 0, CATEGORY_ENABLED);
        }
      } else {
        g_category_enabled[new_index] = 0;
      }
      category_enabled = &g_category_enabled[new_index];
    } else {
      category_enabled = &g_category_enabled[g_category_categories_exhausted];
    }
  }
#if defined(OS_ANDROID)
  ApplyATraceEnabledFlag(category_enabled);
#endif
  return category_enabled;
}

void TraceLog::GetKnownCategories(std::vector<std::string>* categories) {
  AutoLock lock(lock_);
  for (int i = g_num_builtin_categories; i < g_category_index; i++)
    categories->push_back(g_categories[i]);
}

void TraceLog::SetEnabled(const std::vector<std::string>& included_categories,
                          const std::vector<std::string>& excluded_categories,
                          Options options) {
  AutoLock lock(lock_);

  if (enable_count_++ > 0) {
    if (options != trace_options_) {
      DLOG(ERROR) << "Attemting to re-enable tracing with a different "
                  << "set of options.";
    }

    // Tracing is already enabled, so just merge in enabled categories.
    // We only expand the set of enabled categories upon nested SetEnable().
    if (!included_categories_.empty() && !included_categories.empty()) {
      included_categories_.insert(included_categories_.end(),
                                  included_categories.begin(),
                                  included_categories.end());
      EnableMatchingCategories(included_categories_, CATEGORY_ENABLED, 0);
    } else {
      // If either old or new included categories are empty, allow all events.
      included_categories_.clear();
      excluded_categories_.clear();
      EnableMatchingCategories(excluded_categories_, 0, CATEGORY_ENABLED);
    }
    return;
  }

  if (options != trace_options_) {
    trace_options_ = options;
    logged_events_.reset(GetTraceBuffer());
  }

  if (dispatching_to_observer_list_) {
    DLOG(ERROR) <<
        "Cannot manipulate TraceLog::Enabled state from an observer.";
    return;
  }

  dispatching_to_observer_list_ = true;
  FOR_EACH_OBSERVER(EnabledStateChangedObserver, enabled_state_observer_list_,
                    OnTraceLogWillEnable());
  dispatching_to_observer_list_ = false;

  included_categories_ = included_categories;
  excluded_categories_ = excluded_categories;
  // Note that if both included and excluded_categories are empty, the else
  // clause below excludes nothing, thereby enabling all categories.
  if (!included_categories_.empty())
    EnableMatchingCategories(included_categories_, CATEGORY_ENABLED, 0);
  else
    EnableMatchingCategories(excluded_categories_, 0, CATEGORY_ENABLED);

  if (options & ENABLE_SAMPLING) {
    sampling_thread_.reset(new TraceSamplingThread);
    sampling_thread_->RegisterSampleBucket(
        &g_trace_state0,
        "bucket0",
        Bind(&TraceSamplingThread::DefaultSampleCallback));
    sampling_thread_->RegisterSampleBucket(
        &g_trace_state1,
        "bucket1",
        Bind(&TraceSamplingThread::DefaultSampleCallback));
    sampling_thread_->RegisterSampleBucket(
        &g_trace_state2,
        "bucket2",
        Bind(&TraceSamplingThread::DefaultSampleCallback));
    if (!PlatformThread::Create(
          0, sampling_thread_.get(), &sampling_thread_handle_)) {
      DCHECK(false) << "failed to create thread";
    }
  }
}

void TraceLog::SetEnabled(const std::string& categories, Options options) {
  std::vector<std::string> included, excluded;
  // Tokenize list of categories, delimited by ','.
  StringTokenizer tokens(categories, ",");
  while (tokens.GetNext()) {
    bool is_included = true;
    std::string category = tokens.token();
    // Excluded categories start with '-'.
    if (category.at(0) == '-') {
      // Remove '-' from category string.
      category = category.substr(1);
      is_included = false;
    }
    if (is_included)
      included.push_back(category);
    else
      excluded.push_back(category);
  }
  SetEnabled(included, excluded, options);
}

void TraceLog::GetEnabledTraceCategories(
    std::vector<std::string>* included_out,
    std::vector<std::string>* excluded_out) {
  AutoLock lock(lock_);
  if (enable_count_) {
    *included_out = included_categories_;
    *excluded_out = excluded_categories_;
  }
}

void TraceLog::SetDisabled() {
  AutoLock lock(lock_);
  DCHECK(enable_count_ > 0);
  if (--enable_count_ != 0)
    return;

  if (dispatching_to_observer_list_) {
    DLOG(ERROR)
        << "Cannot manipulate TraceLog::Enabled state from an observer.";
    return;
  }

  if (sampling_thread_.get()) {
    // Stop the sampling thread.
    sampling_thread_->Stop();
    lock_.Release();
    PlatformThread::Join(sampling_thread_handle_);
    lock_.Acquire();
    sampling_thread_handle_ = 0;
    sampling_thread_.reset();
  }

  dispatching_to_observer_list_ = true;
  FOR_EACH_OBSERVER(EnabledStateChangedObserver,
                    enabled_state_observer_list_,
                    OnTraceLogWillDisable());
  dispatching_to_observer_list_ = false;

  included_categories_.clear();
  excluded_categories_.clear();
  watch_category_ = NULL;
  watch_event_name_ = "";
  for (int i = 0; i < g_category_index; i++)
    g_category_enabled[i] = 0;
  AddThreadNameMetadataEvents();
}

void TraceLog::SetEnabled(bool enabled, Options options) {
  if (enabled)
    SetEnabled(std::vector<std::string>(), std::vector<std::string>(), options);
  else
    SetDisabled();
}

void TraceLog::AddEnabledStateObserver(EnabledStateChangedObserver* listener) {
  enabled_state_observer_list_.AddObserver(listener);
}

void TraceLog::RemoveEnabledStateObserver(
    EnabledStateChangedObserver* listener) {
  enabled_state_observer_list_.RemoveObserver(listener);
}

float TraceLog::GetBufferPercentFull() const {
  return (float)((double)logged_events_->Size()/(double)kTraceEventBufferSize);
}

void TraceLog::SetNotificationCallback(
    const TraceLog::NotificationCallback& cb) {
  AutoLock lock(lock_);
  notification_callback_ = cb;
}

TraceBuffer* TraceLog::GetTraceBuffer() {
  if (trace_options_ & RECORD_CONTINUOUSLY)
    return new TraceBufferRingBuffer();
  return new TraceBufferVector();
}

void TraceLog::SetEventCallback(EventCallback cb) {
  AutoLock lock(lock_);
  event_callback_ = cb;
};

void TraceLog::Flush(const TraceLog::OutputCallback& cb) {
  scoped_ptr<TraceBuffer> previous_logged_events;
  {
    AutoLock lock(lock_);
    previous_logged_events.swap(logged_events_);
    logged_events_.reset(GetTraceBuffer());
  }  // release lock

  while (previous_logged_events->HasMoreEvents()) {
    scoped_refptr<RefCountedString> json_events_str_ptr =
        new RefCountedString();

    for (size_t i = 0; i < kTraceEventBatchSize; ++i) {
      if (i > 0)
        *(&(json_events_str_ptr->data())) += ",";

      previous_logged_events->NextEvent().AppendAsJSON(
          &(json_events_str_ptr->data()));

      if (!previous_logged_events->HasMoreEvents())
        break;
    }

    cb.Run(json_events_str_ptr);
  }
}

void TraceLog::AddTraceEvent(char phase,
                             const unsigned char* category_enabled,
                             const char* name,
                             unsigned long long id,
                             int num_args,
                             const char** arg_names,
                             const unsigned char* arg_types,
                             const unsigned long long* arg_values,
                             unsigned char flags) {
  int thread_id = static_cast<int>(base::PlatformThread::CurrentId());
  base::TimeTicks now = base::TimeTicks::NowFromSystemTraceTime();
  AddTraceEventWithThreadIdAndTimestamp(phase, category_enabled, name, id,
                                        thread_id, now, num_args, arg_names,
                                        arg_types, arg_values, flags);
}

void TraceLog::AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_enabled,
    const char* name,
    unsigned long long id,
    int thread_id,
    const TimeTicks& timestamp,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const unsigned long long* arg_values,
    unsigned char flags) {
  DCHECK(name);

  if (flags & TRACE_EVENT_FLAG_MANGLE_ID)
    id ^= process_id_hash_;

#if defined(OS_ANDROID)
  SendToATrace(phase, GetCategoryName(category_enabled), name, id,
               num_args, arg_names, arg_types, arg_values, flags);
#endif

  TimeTicks now = timestamp - time_offset_;
  EventCallback event_callback_copy;

  NotificationHelper notifier(this);

  {
    AutoLock lock(lock_);
    if (*category_enabled != CATEGORY_ENABLED)
      return;
    if (logged_events_->IsFull())
      return;

    const char* new_name = ThreadIdNameManager::GetInstance()->
        GetName(thread_id);
    // Check if the thread name has been set or changed since the previous
    // call (if any), but don't bother if the new name is empty. Note this will
    // not detect a thread name change within the same char* buffer address: we
    // favor common case performance over corner case correctness.
    if (new_name != g_current_thread_name.Get().Get() &&
        new_name && *new_name) {
      g_current_thread_name.Get().Set(new_name);

      hash_map<int, std::string>::iterator existing_name =
          thread_names_.find(thread_id);
      if (existing_name == thread_names_.end()) {
        // This is a new thread id, and a new name.
        thread_names_[thread_id] = new_name;
      } else {
        // This is a thread id that we've seen before, but potentially with a
        // new name.
        std::vector<StringPiece> existing_names;
        Tokenize(existing_name->second, ",", &existing_names);
        bool found = std::find(existing_names.begin(),
                               existing_names.end(),
                               new_name) != existing_names.end();
        if (!found) {
          existing_name->second.push_back(',');
          existing_name->second.append(new_name);
        }
      }
    }

    logged_events_->AddEvent(TraceEvent(thread_id,
        now, phase, category_enabled, name, id,
        num_args, arg_names, arg_types, arg_values,
        flags));

    if (logged_events_->IsFull())
      notifier.AddNotificationWhileLocked(TRACE_BUFFER_FULL);

    if (watch_category_ == category_enabled && watch_event_name_ == name)
      notifier.AddNotificationWhileLocked(EVENT_WATCH_NOTIFICATION);

    event_callback_copy = event_callback_;
  }  // release lock

  notifier.SendNotificationIfAny();
  if (event_callback_copy != NULL) {
    event_callback_copy(phase, category_enabled, name, id,
        num_args, arg_names, arg_types, arg_values,
        flags);
  }
}

void TraceLog::AddTraceEventEtw(char phase,
                                const char* name,
                                const void* id,
                                const char* extra) {
#if defined(OS_WIN)
  TraceEventETWProvider::Trace(name, phase, id, extra);
#endif
  INTERNAL_TRACE_EVENT_ADD(phase, "ETW Trace Event", name,
                           TRACE_EVENT_FLAG_COPY, "id", id, "extra", extra);
}

void TraceLog::AddTraceEventEtw(char phase,
                                const char* name,
                                const void* id,
                                const std::string& extra)
{
#if defined(OS_WIN)
  TraceEventETWProvider::Trace(name, phase, id, extra);
#endif
  INTERNAL_TRACE_EVENT_ADD(phase, "ETW Trace Event", name,
                           TRACE_EVENT_FLAG_COPY, "id", id, "extra", extra);
}

void TraceLog::SetWatchEvent(const std::string& category_name,
                             const std::string& event_name) {
  const unsigned char* category = GetCategoryEnabled(category_name.c_str());
  size_t notify_count = 0;
  {
    AutoLock lock(lock_);
    watch_category_ = category;
    watch_event_name_ = event_name;

    // First, search existing events for watch event because we want to catch
    // it even if it has already occurred.
    notify_count = logged_events_->CountEnabledByName(category, event_name);
  }  // release lock

  // Send notification for each event found.
  for (size_t i = 0; i < notify_count; ++i) {
    NotificationHelper notifier(this);
    lock_.Acquire();
    notifier.AddNotificationWhileLocked(EVENT_WATCH_NOTIFICATION);
    lock_.Release();
    notifier.SendNotificationIfAny();
  }
}

void TraceLog::CancelWatchEvent() {
  AutoLock lock(lock_);
  watch_category_ = NULL;
  watch_event_name_ = "";
}

void TraceLog::AddThreadNameMetadataEvents() {
  lock_.AssertAcquired();
  for(hash_map<int, std::string>::iterator it = thread_names_.begin();
      it != thread_names_.end();
      it++) {
    if (!it->second.empty()) {
      int num_args = 1;
      const char* arg_name = "name";
      unsigned char arg_type;
      unsigned long long arg_value;
      trace_event_internal::SetTraceValue(it->second, &arg_type, &arg_value);
      logged_events_->AddEvent(TraceEvent(it->first,
          TimeTicks(), TRACE_EVENT_PHASE_METADATA,
          &g_category_enabled[g_category_metadata],
          "thread_name", trace_event_internal::kNoEventId,
          num_args, &arg_name, &arg_type, &arg_value,
          TRACE_EVENT_FLAG_NONE));
    }
  }
}

void TraceLog::InstallWaitableEventForSamplingTesting(
    WaitableEvent* waitable_event) {
  sampling_thread_->InstallWaitableEventForSamplingTesting(waitable_event);
}

void TraceLog::DeleteForTesting() {
  DeleteTraceLogForTesting::Delete();
}

void TraceLog::Resurrect() {
  StaticMemorySingletonTraits<TraceLog>::Resurrect();
}

void TraceLog::SetProcessID(int process_id) {
  process_id_ = process_id;
  // Create a FNV hash from the process ID for XORing.
  // See http://isthe.com/chongo/tech/comp/fnv/ for algorithm details.
  unsigned long long offset_basis = 14695981039346656037ull;
  unsigned long long fnv_prime = 1099511628211ull;
  unsigned long long pid = static_cast<unsigned long long>(process_id_);
  process_id_hash_ = (offset_basis ^ pid) * fnv_prime;
}

void TraceLog::SetTimeOffset(TimeDelta offset) {
  time_offset_ = offset;
}

}  // namespace debug
}  // namespace base

namespace trace_event_internal {

ScopedTrace::ScopedTrace(
    TRACE_EVENT_API_ATOMIC_WORD* event_uid, const char* name) {
  category_enabled_ =
    reinterpret_cast<const unsigned char*>(TRACE_EVENT_API_ATOMIC_LOAD(
        *event_uid));
  if (!category_enabled_) {
    category_enabled_ = TRACE_EVENT_API_GET_CATEGORY_ENABLED("gpu");
    TRACE_EVENT_API_ATOMIC_STORE(
        *event_uid,
        reinterpret_cast<TRACE_EVENT_API_ATOMIC_WORD>(category_enabled_));
  }
  if (*category_enabled_) {
    name_ = name;
    TRACE_EVENT_API_ADD_TRACE_EVENT(
        TRACE_EVENT_PHASE_BEGIN,    // phase
        category_enabled_,          // category enabled
        name,                       // name
        0,                          // id
        0,                          // num_args
        NULL,                       // arg_names
        NULL,                       // arg_types
        NULL,                       // arg_values
        TRACE_EVENT_FLAG_NONE);     // flags
  } else {
    category_enabled_ = NULL;
  }
}

ScopedTrace::~ScopedTrace() {
  if (category_enabled_ && *category_enabled_) {
    TRACE_EVENT_API_ADD_TRACE_EVENT(
        TRACE_EVENT_PHASE_END,   // phase
        category_enabled_,       // category enabled
        name_,                   // name
        0,                       // id
        0,                       // num_args
        NULL,                    // arg_names
        NULL,                    // arg_types
        NULL,                    // arg_values
        TRACE_EVENT_FLAG_NONE);  // flags
  }
}

}  // namespace trace_event_internal

