// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_tracing_model.h"

#include "base/json/json_reader.h"
#include "base/strings/string_split.h"
#include "base/trace_event/common/trace_event_common.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_event.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_event_matcher.h"

namespace arc {

namespace {

constexpr char kAndroidCategory[] = "android";
constexpr char kTracingMarkWrite[] = ": tracing_mark_write: ";
constexpr int kTracingMarkWriteLength = sizeof(kTracingMarkWrite) - 1;
constexpr char kTraceEventClockSync[] = "trace_event_clock_sync: ";
constexpr int kTraceEventClockSyncLength = sizeof(kTraceEventClockSync) - 1;

// Helper function that converts a portion of string to uint32_t value. |pos|
// specifies the position in string to parse. |end_char| specifies expected
// character at the end. \0 means parse to the end. Returns the position of
// the next character after parsed digits or std::string::npos in case parsing
// failed. This is performance oriented and main idea is to avoid extra memory
// allocations due to sub-string extractions.
size_t ParseUint32(const std::string& str,
                   size_t pos,
                   char end_char,
                   uint32_t* res) {
  *res = 0;
  const size_t len = str.length();
  while (true) {
    const char& c = str[pos];
    if (c != ' ')
      break;
    if (++pos == len)
      return std::string::npos;
  }

  while (true) {
    const char& c = str[pos];
    if (c < '0' || c > '9')
      return std::string::npos;
    const uint32_t prev = *res;
    *res = *res * 10 + c - '0';
    if (prev != (*res - c + '0') / 10)
      return std::string::npos;  // overflow
    if (++pos == len || str[pos] == end_char)
      break;
  }

  return pos;
}

std::vector<std::unique_ptr<ArcTracingEventMatcher>> BuildSelector(
    const std::string& query) {
  std::vector<std::unique_ptr<ArcTracingEventMatcher>> result;
  for (const std::string& segment : base::SplitString(
           query, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    result.emplace_back(std::make_unique<ArcTracingEventMatcher>(segment));
  }
  return result;
}

void SelectRecursively(
    size_t level,
    const ArcTracingEvent* event,
    const std::vector<std::unique_ptr<ArcTracingEventMatcher>>& selector,
    ArcTracingModel::TracingEventPtrs* collector) {
  if (level >= selector.size())
    return;
  if (!selector[level]->Match(*event))
    return;
  if (level == selector.size() - 1) {
    // Last segment
    collector->push_back(event);
  } else {
    for (const auto& child : event->children())
      SelectRecursively(level + 1, child.get(), selector, collector);
  }
}

}  // namespace

ArcTracingModel::ArcTracingModel() = default;

ArcTracingModel::~ArcTracingModel() = default;

bool ArcTracingModel::Build(const std::string& data) {
  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(data);
  if (!value) {
    LOG(ERROR) << "Cannot parse trace data";
    return false;
  }

  base::DictionaryValue* dictionary;
  if (!value->GetAsDictionary(&dictionary)) {
    LOG(ERROR) << "Trace data is not dictionary";
    return false;
  }

  std::string sys_traces;
  if (dictionary->GetString("systemTraceEvents", &sys_traces)) {
    if (!ConvertSysTraces(sys_traces)) {
      LOG(ERROR) << "Failed to convert systrace data";
      return false;
    }
  }

  base::ListValue* events;
  if (!dictionary->GetList("traceEvents", &events)) {
    LOG(ERROR) << "No trace events";
    return false;
  }

  if (!ProcessEvent(events)) {
    LOG(ERROR) << "No trace events";
    return false;
  }

  return true;
}

ArcTracingModel::TracingEventPtrs ArcTracingModel::Select(
    const std::string query) const {
  ArcTracingModel::TracingEventPtrs collector;
  const std::vector<std::unique_ptr<ArcTracingEventMatcher>> selector =
      BuildSelector(query);
  for (const auto& thread_events : per_thread_events_) {
    for (const auto& root : thread_events.second)
      SelectRecursively(0, root.get(), selector, &collector);
  }
  for (const auto& group_events : group_events_) {
    for (const auto& root : group_events.second)
      SelectRecursively(0, root.get(), selector, &collector);
  }
  return collector;
}

ArcTracingModel::TracingEventPtrs ArcTracingModel::Select(
    const ArcTracingEvent* event,
    const std::string query) const {
  ArcTracingModel::TracingEventPtrs collector;
  for (const auto& child : event->children())
    SelectRecursively(0, child.get(), BuildSelector(query), &collector);
  return collector;
}

bool ArcTracingModel::ProcessEvent(base::ListValue* events) {
  base::ListValue::iterator it = events->begin();
  while (it != events->end()) {
    std::unique_ptr<base::Value> event_data;
    // Take ownership.
    it = events->Erase(it, &event_data);
    if (!event_data->is_dict()) {
      LOG(ERROR) << "Event is not a dictionary";
      return false;
    }

    std::unique_ptr<ArcTracingEvent> event =
        std::make_unique<ArcTracingEvent>(std::move(event_data));
    switch (event->GetPhase()) {
      case TRACE_EVENT_PHASE_METADATA:
      case TRACE_EVENT_PHASE_COMPLETE:
      case TRACE_EVENT_PHASE_COUNTER:
      case TRACE_EVENT_PHASE_ASYNC_BEGIN:
      case TRACE_EVENT_PHASE_ASYNC_STEP_INTO:
      case TRACE_EVENT_PHASE_ASYNC_END:
        break;
      default:
        // Ignore at this moment. They are not currently used.
        continue;
    }

    if (!event->Validate()) {
      LOG(ERROR) << "Invalid event found " << event->ToString();
      return false;
    }

    switch (event->GetPhase()) {
      case TRACE_EVENT_PHASE_METADATA:
        metadata_events_.push_back(std::move(event));
        break;
      case TRACE_EVENT_PHASE_ASYNC_BEGIN:
      case TRACE_EVENT_PHASE_ASYNC_STEP_INTO:
      case TRACE_EVENT_PHASE_ASYNC_END:
        group_events_[event->GetId()].push_back(std::move(event));
        break;
      case TRACE_EVENT_PHASE_COMPLETE:
      case TRACE_EVENT_PHASE_COUNTER:
        if (!AddToThread(std::move(event))) {
          LOG(ERROR) << "Cannot add event to threads " << event->ToString();
          return false;
        }
        break;
      default:
        NOTREACHED();
        return false;
    }
  }

  // Sort group events based on timestamp. They are asynchronous and may come in
  // random order.
  for (auto& gr : group_events_) {
    std::sort(gr.second.begin(), gr.second.end(),
              [](std::unique_ptr<ArcTracingEvent>& a,
                 std::unique_ptr<ArcTracingEvent>& b) {
                return a->GetTimestamp() < b->GetTimestamp();
              });
  }

  return true;
}

bool ArcTracingModel::ConvertSysTraces(const std::string& sys_traces) {
  size_t new_line_pos = 0;
  // To keep in correct order of creation. This converts pair of 'B' and 'E'
  // events to the completed event, 'X'.
  TracingEvents converted_events;
  std::map<uint32_t, std::vector<ArcTracingEvent*>>
      per_thread_pending_events_stack;
  while (true) {
    // Get end of line.
    size_t end_line_pos = sys_traces.find('\n', new_line_pos);
    if (end_line_pos == std::string::npos)
      break;

    const std::string line =
        sys_traces.substr(new_line_pos, end_line_pos - new_line_pos);
    new_line_pos = end_line_pos + 1;

    // Skip comments and empty lines
    if (line.empty() || line[0] == '#')
      continue;

    // Trace event has following format.
    //            TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
    //               | |       |   ||||       |         |
    // Until TIMESTAMP we have fixed position for elements.
    if (line.length() < 35 || line[16] != '-' || line[22] != ' ' ||
        line[28] != ' ' || line[33] != ' ') {
      LOG(ERROR) << "Cannot recognize trace event: " << line;
      return false;
    }

    uint32_t tid;
    if (ParseUint32(line, 17, ' ', &tid) == std::string::npos) {
      LOG(ERROR) << "Cannot parse tid in trace event: " << line;
      return false;
    }
    uint32_t timestamp_high;
    uint32_t timestamp_low;
    const size_t pos_dot = ParseUint32(line, 34, '.', &timestamp_high);
    if (pos_dot == std::string::npos) {
      LOG(ERROR) << "Cannot parse timestamp in trace event: " << line;
      return false;
    }
    const size_t separator_position =
        ParseUint32(line, pos_dot + 1, ':', &timestamp_low);
    if (separator_position == std::string::npos) {
      LOG(ERROR) << "Cannot parse timestamp in trace event: " << line;
      return false;
    }

    const size_t event_position = separator_position + kTracingMarkWriteLength;
    // Skip not relevant information. We only have interest what was written
    // from Android using tracing marker.
    if (event_position + 2 >= line.length() ||
        strncmp(&line[separator_position], kTracingMarkWrite,
                kTracingMarkWriteLength)) {
      continue;
    }

    if (event_position + kTraceEventClockSyncLength < line.length() &&
        !strncmp(&line[event_position], kTraceEventClockSync,
                 kTraceEventClockSyncLength)) {
      // Ignore this service message.
      continue;
    }

    if (line[event_position + 1] != '|') {
      LOG(ERROR) << "Cannot recognize trace marker event: " << line;
      return false;
    }

    const char phase = line[event_position];
    const double timestamp = 1000000L * timestamp_high + timestamp_low;

    uint32_t pid;
    switch (phase) {
      case TRACE_EVENT_PHASE_BEGIN:
      case TRACE_EVENT_PHASE_COUNTER: {
        const size_t name_pos =
            ParseUint32(line, event_position + 2, '|', &pid);
        if (name_pos == std::string::npos) {
          LOG(ERROR) << "Cannot parse pid of trace event: " << line;
          return false;
        }
        const std::string name = line.substr(name_pos + 1);
        std::unique_ptr<ArcTracingEvent> event =
            std::make_unique<ArcTracingEvent>(
                std::make_unique<base::DictionaryValue>());
        event->SetPid(pid);
        event->SetTid(tid);
        event->SetTimestamp(timestamp);
        event->SetCategory(kAndroidCategory);
        event->SetName(name);
        if (phase == TRACE_EVENT_PHASE_BEGIN)
          per_thread_pending_events_stack[tid].push_back(event.get());
        else
          event->SetPhase(TRACE_EVENT_PHASE_COUNTER);
        converted_events.push_back(std::move(event));
      } break;
      case TRACE_EVENT_PHASE_END: {
        // Beginning event may not exist.
        if (per_thread_pending_events_stack[tid].empty())
          continue;
        if (ParseUint32(line, event_position + 2, '\0', &pid) ==
            std::string::npos) {
          LOG(ERROR) << "Cannot parse pid of trace event: " << line;
          return false;
        }
        ArcTracingEvent* completed_event =
            per_thread_pending_events_stack[tid].back();
        per_thread_pending_events_stack[tid].pop_back();
        completed_event->SetPhase(TRACE_EVENT_PHASE_COMPLETE);
        completed_event->SetDuration(timestamp -
                                     completed_event->GetTimestamp());
      } break;
      default:
        LOG(ERROR) << "Unsupported type of trace event: " << line;
        return false;
    }
  }
  // Close all pending tracing event, assuming last event is 0 duration.
  for (auto& pending_events : per_thread_pending_events_stack) {
    if (pending_events.second.empty())
      continue;
    const double last_timestamp = pending_events.second.back()->GetTimestamp();
    for (ArcTracingEvent* pending_event : pending_events.second) {
      pending_event->SetDuration(last_timestamp -
                                 pending_event->GetTimestamp());
      pending_event->SetPhase(TRACE_EVENT_PHASE_COMPLETE);
    }
  }

  // Now put events to the thread models.
  for (auto& converted_event : converted_events) {
    if (!AddToThread(std::move(converted_event))) {
      LOG(ERROR) << "Cannot add systrace event to threads "
                 << converted_event->ToString();
      return false;
    }
  }

  return true;
}

bool ArcTracingModel::AddToThread(std::unique_ptr<ArcTracingEvent> event) {
  const uint64_t full_id = event->GetPid() * 0x100000000L + event->GetTid();
  std::vector<std::unique_ptr<ArcTracingEvent>>& thread_roots =
      per_thread_events_[full_id];
  if (thread_roots.empty() || thread_roots.back()->ClassifyPositionOf(*event) ==
                                  ArcTracingEvent::Position::kAfter) {
    // First event for the thread or event is after already existing last root
    // event. Add as a new root.
    thread_roots.push_back(std::move(event));
    return true;
  }

  return thread_roots.back()->AppendChild(std::move(event));
}

void ArcTracingModel::Dump(std::ostream& stream) const {
  for (auto& gr : group_events_) {
    for (const auto& event : gr.second)
      event->Dump("", stream);
  }

  for (const auto& gr : per_thread_events_) {
    for (const auto& event : gr.second)
      event->Dump("", stream);
  }
}

}  // namespace arc
