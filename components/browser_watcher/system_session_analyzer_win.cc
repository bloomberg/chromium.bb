// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/system_session_analyzer_win.h"

#include <windows.h>
#include <winevt.h>

#include <utility>

#include "base/macros.h"
#include "base/time/time.h"

namespace browser_watcher {

namespace {

// The name of the log channel to query.
const wchar_t kChannelName[] = L"System";

// Event ids of system startup / shutdown events. These were obtained from
// inspection of the System log in Event Viewer on Windows 10:
//   - id 6005: "The Event log service was started."
//   - id 6006: "The Event log service was stopped."
//   - id 6008: "The previous system shutdown at <time> on <date> was
//               unexpected."
const uint16_t kIdSessionStart = 6005U;
const uint16_t kIdSessionEnd = 6006U;
const uint16_t kIdSessionEndUnclean = 6008U;

// An XPATH expression to query for system startup / shutdown events. The query
// is expected to retrieve exactly one event for each startup (kIdSessionStart)
// and one event for each shutdown (either kIdSessionEnd or
// kIdSessionEndUnclean).
const wchar_t kSessionEventsQuery[] =
    L"*[System[Provider[@Name='eventlog']"
    L" and (EventID=6005 or EventID=6006 or EventID=6008)]]";

// XPath expressions to attributes of interest.
const wchar_t kEventIdPath[] = L"Event/System/EventID";
const wchar_t kEventTimePath[] = L"Event/System/TimeCreated/@SystemTime";

// The timeout to use for calls to ::EvtNext.
const uint32_t kTimeoutMs = 5000;

struct EvtHandleCloser {
  using pointer = EVT_HANDLE;
  void operator()(EVT_HANDLE handle) const {
    if (handle)
      ::EvtClose(handle);
  }
};

using EvtHandle = std::unique_ptr<EVT_HANDLE, EvtHandleCloser>;

base::Time ULLFileTimeToTime(ULONGLONG time_ulonglong) {
  // Copy low / high parts as FILETIME is not always 64bit aligned.
  ULARGE_INTEGER time;
  time.QuadPart = time_ulonglong;
  FILETIME ft;
  ft.dwLowDateTime = time.LowPart;
  ft.dwHighDateTime = time.HighPart;

  return base::Time::FromFileTime(ft);
}

// Create a render context (i.e. specify attributes of interest).
EvtHandle CreateRenderContext() {
  LPCWSTR value_paths[] = {kEventIdPath, kEventTimePath};
  const DWORD kValueCnt = arraysize(value_paths);

  EVT_HANDLE context = NULL;
  context =
      ::EvtCreateRenderContext(kValueCnt, value_paths, EvtRenderContextValues);
  if (!context)
    DLOG(ERROR) << "Failed to create render context.";

  return EvtHandle(context);
}

bool GetEventInfo(EVT_HANDLE context,
                  EVT_HANDLE event,
                  SystemSessionAnalyzer::EventInfo* info) {
  DCHECK(context);
  DCHECK(event);
  DCHECK(info);

  // Retrieve attributes of interest from the event. We expect the context to
  // specify the retrieval of two attributes (event id and event time), each
  // with a specific type.
  const DWORD kAttributeCnt = 2U;
  std::vector<EVT_VARIANT> buffer(kAttributeCnt);
  DWORD buffer_size = kAttributeCnt * sizeof(EVT_VARIANT);
  DWORD buffer_used = 0U;
  DWORD retrieved_attribute_cnt = 0U;
  if (!::EvtRender(context, event, EvtRenderEventValues, buffer_size,
                   buffer.data(), &buffer_used, &retrieved_attribute_cnt)) {
    DLOG(ERROR) << "Failed to render the event.";
    return false;
  }

  // Validate the count and types of the retrieved attributes.
  if ((retrieved_attribute_cnt != kAttributeCnt) ||
      (buffer[0].Type != EvtVarTypeUInt16) ||
      (buffer[1].Type != EvtVarTypeFileTime)) {
    return false;
  }

  info->event_id = buffer[0].UInt16Val;
  info->event_time = ULLFileTimeToTime(buffer[1].FileTimeVal);

  return true;
}

}  // namespace

SystemSessionAnalyzer::SystemSessionAnalyzer(uint32_t session_cnt)
    : session_cnt_(session_cnt) {}

SystemSessionAnalyzer::~SystemSessionAnalyzer() {}

SystemSessionAnalyzer::Status SystemSessionAnalyzer::IsSessionUnclean(
    base::Time timestamp) {
  if (!initialized_) {
    DCHECK(!init_success_);
    init_success_ = Initialize();
    initialized_ = true;
  }
  if (!init_success_)
    return FAILED;
  if (timestamp < coverage_start_)
    return OUTSIDE_RANGE;

  // Get the first session starting after the timestamp.
  std::map<base::Time, base::TimeDelta>::const_iterator it =
      unclean_sessions_.upper_bound(timestamp);
  if (it == unclean_sessions_.begin())
    return CLEAN;  // No prior unclean session.

  // Get the previous session and see if it encompasses the timestamp.
  --it;
  bool is_spanned = (timestamp - it->first) <= it->second;
  return is_spanned ? UNCLEAN : CLEAN;
}

bool SystemSessionAnalyzer::FetchEvents(
    std::vector<EventInfo>* event_infos) const {
  DCHECK(event_infos);

  // Query for the events. Note: requesting events from newest to oldest.
  EVT_HANDLE query_raw =
      ::EvtQuery(nullptr, kChannelName, kSessionEventsQuery,
                 EvtQueryChannelPath | EvtQueryReverseDirection);
  if (!query_raw) {
    DLOG(ERROR) << "Event query failed.";
    return false;
  }
  EvtHandle query(query_raw);

  // Retrieve events: 2 events per session, plus the current session's start.
  DWORD desired_event_cnt = 2U * session_cnt_ + 1U;
  std::vector<EVT_HANDLE> events_raw(desired_event_cnt, NULL);
  DWORD event_cnt = 0U;
  BOOL success = ::EvtNext(query.get(), desired_event_cnt, events_raw.data(),
                           kTimeoutMs, 0, &event_cnt);

  // Ensure handles get closed. The MSDN sample seems to imply handles may need
  // to be closed event in if EvtNext failed.
  std::vector<EvtHandle> events(desired_event_cnt);
  for (size_t i = 0; i < event_cnt; ++i)
    events[i].reset(events_raw[i]);

  if (!success) {
    DLOG(ERROR) << "Failed to retrieve events.";
    return false;
  }

  // Extract information from the events.
  EvtHandle render_context = CreateRenderContext();
  if (!render_context.get())
    return false;

  std::vector<EventInfo> event_infos_tmp;
  event_infos_tmp.reserve(event_cnt);

  EventInfo info = {};
  for (size_t i = 0; i < event_cnt; ++i) {
    if (!GetEventInfo(render_context.get(), events[i].get(), &info))
      return false;
    event_infos_tmp.push_back(info);
  }

  event_infos->swap(event_infos_tmp);
  return true;
}

bool SystemSessionAnalyzer::Initialize() {
  std::vector<SystemSessionAnalyzer::EventInfo> events;
  if (!FetchEvents(&events))
    return false;

  // Validate the number and ordering of events (newest to oldest). The
  // expectation is a (start / [unclean]shutdown) pair of events for each
  // session, as well as an additional event for the current session's start.
  size_t event_cnt = events.size();
  if ((event_cnt % 2) == 0)
    return false;  // Even number is unexpected.
  if (event_cnt < 3)
    return false;  // Not enough data for a single session.
  for (size_t i = 1; i < event_cnt; ++i) {
    if (events[i - 1].event_time < events[i].event_time)
      return false;
  }

  // Step through (start / shutdown) event pairs, validating the types of events
  // and recording unclean sessions.
  std::map<base::Time, base::TimeDelta> unclean_sessions;
  size_t start = 2U;
  size_t end = 1U;
  for (; start < event_cnt && end < event_cnt; start += 2, end += 2) {
    uint16_t start_id = events[start].event_id;
    uint16_t end_id = events[end].event_id;
    if (start_id != kIdSessionStart)
      return false;  // Unexpected event type.
    if (end_id != kIdSessionEnd && end_id != kIdSessionEndUnclean)
      return false;  // Unexpected event type.

    if (end_id == kIdSessionEnd)
      continue;  // This is a clean session.

    unclean_sessions.insert(
        std::make_pair(events[start].event_time,
                       events[end].event_time - events[start].event_time));
  }

  unclean_sessions_.swap(unclean_sessions);
  DCHECK_GT(event_cnt, 0U);
  coverage_start_ = events[event_cnt - 1].event_time;

  return true;
}

}  // namespace browser_watcher
