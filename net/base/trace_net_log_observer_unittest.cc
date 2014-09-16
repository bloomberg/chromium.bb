// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/trace_net_log_observer.h"

#include <string>
#include <vector>

#include "base/debug/trace_event.h"
#include "base/debug/trace_event_impl.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "net/base/capturing_net_log.h"
#include "net/base/net_log.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::debug::TraceLog;

namespace net {

namespace {

struct TraceEntryInfo {
  std::string category;
  std::string id;
  std::string phase;
  std::string name;
  std::string source_type;
};

TraceEntryInfo GetTraceEntryInfoFromValue(const base::DictionaryValue& value) {
  TraceEntryInfo info;
  EXPECT_TRUE(value.GetString("cat", &info.category));
  EXPECT_TRUE(value.GetString("id", &info.id));
  EXPECT_TRUE(value.GetString("ph", &info.phase));
  EXPECT_TRUE(value.GetString("name", &info.name));
  EXPECT_TRUE(value.GetString("args.source_type", &info.source_type));

  return info;
}

class TraceNetLogObserverTest : public testing::Test {
 public:
  TraceNetLogObserverTest() {
    TraceLog* tracelog = TraceLog::GetInstance();
    DCHECK(tracelog);
    DCHECK(!tracelog->IsEnabled());
    trace_buffer_.SetOutputCallback(json_output_.GetCallback());
    trace_net_log_observer_.reset(new TraceNetLogObserver());
    trace_events_.reset(new base::ListValue());
  }

  virtual ~TraceNetLogObserverTest() {
    DCHECK(!TraceLog::GetInstance()->IsEnabled());
  }

  void OnTraceDataCollected(
      base::RunLoop* run_loop,
      const scoped_refptr<base::RefCountedString>& events_str,
      bool has_more_events) {
    DCHECK(trace_events_->empty());
    trace_buffer_.Start();
    trace_buffer_.AddFragment(events_str->data());
    trace_buffer_.Finish();

    scoped_ptr<base::Value> trace_value;
    trace_value.reset(base::JSONReader::Read(
        json_output_.json_output,
        base::JSON_PARSE_RFC | base::JSON_DETACHABLE_CHILDREN));

    ASSERT_TRUE(trace_value) << json_output_.json_output;
    base::ListValue* trace_events = NULL;
    ASSERT_TRUE(trace_value->GetAsList(&trace_events));

    trace_events_ = FilterNetLogTraceEvents(*trace_events);

    if (!has_more_events)
      run_loop->Quit();
  }

  static void EnableTraceLog() {
    TraceLog::GetInstance()->SetEnabled(base::debug::CategoryFilter("netlog"),
                                        TraceLog::RECORDING_MODE,
                                        base::debug::TraceOptions());
  }

  void EndTraceAndFlush() {
    base::RunLoop run_loop;
    TraceLog::GetInstance()->SetDisabled();
    TraceLog::GetInstance()->Flush(
        base::Bind(&TraceNetLogObserverTest::OnTraceDataCollected,
                   base::Unretained(this),
                   base::Unretained(&run_loop)));
    run_loop.Run();
  }

  void set_trace_net_log_observer(TraceNetLogObserver* trace_net_log_observer) {
    trace_net_log_observer_.reset(trace_net_log_observer);
  }

  static scoped_ptr<base::ListValue> FilterNetLogTraceEvents(
      const base::ListValue& trace_events) {
    scoped_ptr<base::ListValue> filtered_trace_events(new base::ListValue());
    for (size_t i = 0; i < trace_events.GetSize(); i++) {
      const base::DictionaryValue* dict = NULL;
      if (!trace_events.GetDictionary(i, &dict)) {
        ADD_FAILURE() << "Unexpected non-dictionary event in trace_events";
        continue;
      }
      std::string category;
      if (!dict->GetString("cat", &category)) {
        ADD_FAILURE()
            << "Unexpected item without a category field in trace_events";
        continue;
      }
      if (category != "netlog")
        continue;
      filtered_trace_events->Append(dict->DeepCopy());
    }
    return filtered_trace_events.Pass();
  }

  base::ListValue* trace_events() const {
    return trace_events_.get();
  }

  CapturingNetLog* net_log() {
    return &net_log_;
  }

  TraceNetLogObserver* trace_net_log_observer() const {
    return trace_net_log_observer_.get();
  }

 private:
  scoped_ptr<base::ListValue> trace_events_;
  base::debug::TraceResultBuffer trace_buffer_;
  base::debug::TraceResultBuffer::SimpleOutput json_output_;
  CapturingNetLog net_log_;
  scoped_ptr<TraceNetLogObserver> trace_net_log_observer_;
};

TEST_F(TraceNetLogObserverTest, TracingNotEnabled) {
  trace_net_log_observer()->WatchForTraceStart(net_log());
  net_log()->AddGlobalEntry(NetLog::TYPE_REQUEST_ALIVE);

  EndTraceAndFlush();
  trace_net_log_observer()->StopWatchForTraceStart();

  EXPECT_EQ(0u, trace_events()->GetSize());
}

TEST_F(TraceNetLogObserverTest, TraceEventCaptured) {
  CapturingNetLog::CapturedEntryList entries;
  net_log()->GetEntries(&entries);
  EXPECT_TRUE(entries.empty());

  trace_net_log_observer()->WatchForTraceStart(net_log());
  EnableTraceLog();
  BoundNetLog bound_net_log =
      BoundNetLog::Make(net_log(), net::NetLog::SOURCE_NONE);
  net_log()->AddGlobalEntry(NetLog::TYPE_CANCELLED);
  bound_net_log.BeginEvent(NetLog::TYPE_URL_REQUEST_START_JOB);
  bound_net_log.EndEvent(NetLog::TYPE_REQUEST_ALIVE);

  net_log()->GetEntries(&entries);
  EXPECT_EQ(3u, entries.size());
  EndTraceAndFlush();
  trace_net_log_observer()->StopWatchForTraceStart();
  EXPECT_EQ(3u, trace_events()->GetSize());
  const base::DictionaryValue* item1 = NULL;
  ASSERT_TRUE(trace_events()->GetDictionary(0, &item1));
  const base::DictionaryValue* item2 = NULL;
  ASSERT_TRUE(trace_events()->GetDictionary(1, &item2));
  const base::DictionaryValue* item3 = NULL;
  ASSERT_TRUE(trace_events()->GetDictionary(2, &item3));

  TraceEntryInfo actual_item1 = GetTraceEntryInfoFromValue(*item1);
  TraceEntryInfo actual_item2 = GetTraceEntryInfoFromValue(*item2);
  TraceEntryInfo actual_item3 = GetTraceEntryInfoFromValue(*item3);
  EXPECT_EQ("netlog", actual_item1.category);
  EXPECT_EQ(base::StringPrintf("0x%d", entries[0].source.id), actual_item1.id);
  EXPECT_EQ(std::string(1, TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT),
            actual_item1.phase);
  EXPECT_EQ(NetLog::EventTypeToString(NetLog::TYPE_CANCELLED),
            actual_item1.name);
  EXPECT_EQ(NetLog::SourceTypeToString(entries[0].source.type),
            actual_item1.source_type);

  EXPECT_EQ("netlog", actual_item2.category);
  EXPECT_EQ(base::StringPrintf("0x%d", entries[1].source.id), actual_item2.id);
  EXPECT_EQ(std::string(1, TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN),
            actual_item2.phase);
  EXPECT_EQ(NetLog::EventTypeToString(NetLog::TYPE_URL_REQUEST_START_JOB),
            actual_item2.name);
  EXPECT_EQ(NetLog::SourceTypeToString(entries[1].source.type),
            actual_item2.source_type);

  EXPECT_EQ("netlog", actual_item3.category);
  EXPECT_EQ(base::StringPrintf("0x%d", entries[2].source.id), actual_item3.id);
  EXPECT_EQ(std::string(1, TRACE_EVENT_PHASE_NESTABLE_ASYNC_END),
            actual_item3.phase);
  EXPECT_EQ(NetLog::EventTypeToString(NetLog::TYPE_REQUEST_ALIVE),
            actual_item3.name);
  EXPECT_EQ(NetLog::SourceTypeToString(entries[2].source.type),
            actual_item3.source_type);
}

TEST_F(TraceNetLogObserverTest, EnableAndDisableTracing) {
  trace_net_log_observer()->WatchForTraceStart(net_log());
  EnableTraceLog();
  net_log()->AddGlobalEntry(NetLog::TYPE_CANCELLED);
  TraceLog::GetInstance()->SetDisabled();
  net_log()->AddGlobalEntry(NetLog::TYPE_REQUEST_ALIVE);
  EnableTraceLog();
  net_log()->AddGlobalEntry(NetLog::TYPE_URL_REQUEST_START_JOB);

  EndTraceAndFlush();
  trace_net_log_observer()->StopWatchForTraceStart();

  CapturingNetLog::CapturedEntryList entries;
  net_log()->GetEntries(&entries);
  EXPECT_EQ(3u, entries.size());
  EXPECT_EQ(2u, trace_events()->GetSize());
  const base::DictionaryValue* item1 = NULL;
  ASSERT_TRUE(trace_events()->GetDictionary(0, &item1));
  const base::DictionaryValue* item2 = NULL;
  ASSERT_TRUE(trace_events()->GetDictionary(1, &item2));

  TraceEntryInfo actual_item1 = GetTraceEntryInfoFromValue(*item1);
  TraceEntryInfo actual_item2 = GetTraceEntryInfoFromValue(*item2);
  EXPECT_EQ("netlog", actual_item1.category);
  EXPECT_EQ(base::StringPrintf("0x%d", entries[0].source.id), actual_item1.id);
  EXPECT_EQ(std::string(1, TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT),
            actual_item1.phase);
  EXPECT_EQ(NetLog::EventTypeToString(NetLog::TYPE_CANCELLED),
            actual_item1.name);
  EXPECT_EQ(NetLog::SourceTypeToString(entries[0].source.type),
            actual_item1.source_type);

  EXPECT_EQ("netlog", actual_item2.category);
  EXPECT_EQ(base::StringPrintf("0x%d", entries[2].source.id), actual_item2.id);
  EXPECT_EQ(std::string(1, TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT),
            actual_item2.phase);
  EXPECT_EQ(NetLog::EventTypeToString(NetLog::TYPE_URL_REQUEST_START_JOB),
            actual_item2.name);
  EXPECT_EQ(NetLog::SourceTypeToString(entries[2].source.type),
            actual_item2.source_type);
}

TEST_F(TraceNetLogObserverTest, DestroyObserverWhileTracing) {
  trace_net_log_observer()->WatchForTraceStart(net_log());
  EnableTraceLog();
  net_log()->AddGlobalEntry(NetLog::TYPE_CANCELLED);
  trace_net_log_observer()->StopWatchForTraceStart();
  set_trace_net_log_observer(NULL);
  net_log()->AddGlobalEntry(NetLog::TYPE_REQUEST_ALIVE);

  EndTraceAndFlush();

  CapturingNetLog::CapturedEntryList entries;
  net_log()->GetEntries(&entries);
  EXPECT_EQ(2u, entries.size());
  EXPECT_EQ(1u, trace_events()->GetSize());

  const base::DictionaryValue* item1 = NULL;
  ASSERT_TRUE(trace_events()->GetDictionary(0, &item1));

  TraceEntryInfo actual_item1 = GetTraceEntryInfoFromValue(*item1);
  EXPECT_EQ("netlog", actual_item1.category);
  EXPECT_EQ(base::StringPrintf("0x%d", entries[0].source.id), actual_item1.id);
  EXPECT_EQ(std::string(1, TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT),
            actual_item1.phase);
  EXPECT_EQ(NetLog::EventTypeToString(NetLog::TYPE_CANCELLED),
            actual_item1.name);
  EXPECT_EQ(NetLog::SourceTypeToString(entries[0].source.type),
            actual_item1.source_type);
}

TEST_F(TraceNetLogObserverTest, DestroyObserverWhileNotTracing) {
  trace_net_log_observer()->WatchForTraceStart(net_log());
  net_log()->AddGlobalEntry(NetLog::TYPE_CANCELLED);
  trace_net_log_observer()->StopWatchForTraceStart();
  set_trace_net_log_observer(NULL);
  net_log()->AddGlobalEntry(NetLog::TYPE_REQUEST_ALIVE);
  net_log()->AddGlobalEntry(NetLog::TYPE_URL_REQUEST_START_JOB);

  EndTraceAndFlush();

  CapturingNetLog::CapturedEntryList entries;
  net_log()->GetEntries(&entries);
  EXPECT_EQ(3u, entries.size());
  EXPECT_EQ(0u, trace_events()->GetSize());
}

TEST_F(TraceNetLogObserverTest, CreateObserverAfterTracingStarts) {
  set_trace_net_log_observer(NULL);
  EnableTraceLog();
  set_trace_net_log_observer(new TraceNetLogObserver());
  trace_net_log_observer()->WatchForTraceStart(net_log());
  net_log()->AddGlobalEntry(NetLog::TYPE_CANCELLED);
  trace_net_log_observer()->StopWatchForTraceStart();
  net_log()->AddGlobalEntry(NetLog::TYPE_REQUEST_ALIVE);
  net_log()->AddGlobalEntry(NetLog::TYPE_URL_REQUEST_START_JOB);

  EndTraceAndFlush();

  CapturingNetLog::CapturedEntryList entries;
  net_log()->GetEntries(&entries);
  EXPECT_EQ(3u, entries.size());
  EXPECT_EQ(0u, trace_events()->GetSize());
}

TEST_F(TraceNetLogObserverTest, EventsWithAndWithoutParameters) {
  trace_net_log_observer()->WatchForTraceStart(net_log());
  EnableTraceLog();
  NetLog::ParametersCallback net_log_callback;
  std::string param = "bar";
  net_log_callback = NetLog::StringCallback("foo", &param);

  net_log()->AddGlobalEntry(NetLog::TYPE_CANCELLED, net_log_callback);
  net_log()->AddGlobalEntry(NetLog::TYPE_REQUEST_ALIVE);

  EndTraceAndFlush();
  trace_net_log_observer()->StopWatchForTraceStart();

  CapturingNetLog::CapturedEntryList entries;
  net_log()->GetEntries(&entries);
  EXPECT_EQ(2u, entries.size());
  EXPECT_EQ(2u, trace_events()->GetSize());
  const base::DictionaryValue* item1 = NULL;
  ASSERT_TRUE(trace_events()->GetDictionary(0, &item1));
  const base::DictionaryValue* item2 = NULL;
  ASSERT_TRUE(trace_events()->GetDictionary(1, &item2));

  TraceEntryInfo actual_item1 = GetTraceEntryInfoFromValue(*item1);
  TraceEntryInfo actual_item2 = GetTraceEntryInfoFromValue(*item2);
  EXPECT_EQ("netlog", actual_item1.category);
  EXPECT_EQ(base::StringPrintf("0x%d", entries[0].source.id), actual_item1.id);
  EXPECT_EQ(std::string(1, TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT),
            actual_item1.phase);
  EXPECT_EQ(NetLog::EventTypeToString(NetLog::TYPE_CANCELLED),
            actual_item1.name);
  EXPECT_EQ(NetLog::SourceTypeToString(entries[0].source.type),
            actual_item1.source_type);

  EXPECT_EQ("netlog", actual_item2.category);
  EXPECT_EQ(base::StringPrintf("0x%d", entries[1].source.id), actual_item2.id);
  EXPECT_EQ(std::string(1, TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT),
            actual_item2.phase);
  EXPECT_EQ(NetLog::EventTypeToString(NetLog::TYPE_REQUEST_ALIVE),
            actual_item2.name);
  EXPECT_EQ(NetLog::SourceTypeToString(entries[1].source.type),
            actual_item2.source_type);

  std::string item1_params;
  std::string item2_params;
  EXPECT_TRUE(item1->GetString("args.params.foo", &item1_params));
  EXPECT_EQ("bar", item1_params);

  EXPECT_TRUE(item2->GetString("args.params", &item2_params));
  EXPECT_TRUE(item2_params.empty());
}

}  // namespace

}  // namespace net
