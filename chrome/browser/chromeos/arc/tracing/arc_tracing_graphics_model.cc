// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_tracing_graphics_model.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_event.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_event_matcher.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_model.h"

namespace arc {

namespace {

using BufferEventType = ArcTracingGraphicsModel::BufferEventType;

constexpr char kUnknownActivity[] = "unknown";

constexpr char kArgumentAppId[] = "app_id";
constexpr char kArgumentBufferId[] = "buffer_id";
constexpr char kArgumentPutOffset[] = "put_offset";

constexpr char kKeyActivity[] = "activity";
constexpr char kKeyAndroid[] = "android";
constexpr char kKeyBuffers[] = "buffers";
constexpr char kKeyChrome[] = "chrome";
constexpr char kKeyDuration[] = "duration";
constexpr char kKeyViews[] = "views";
constexpr char kKeyTaskId[] = "task_id";

constexpr char kAcquireBufferQuery[] =
    "android:onMessageReceived/android:handleMessageInvalidate/"
    "android:latchBuffer/android:updateTexImage/android:acquireBuffer";
// Android PI+
constexpr char kReleaseBufferQueryP[] =
    "android:onMessageReceived/android:handleMessageRefresh/"
    "android:postComposition/android:releaseBuffer";
// Android NYC
constexpr char kReleaseBufferQueryN[] =
    "android:onMessageReceived/android:handleMessageRefresh/"
    "android:releaseBuffer";
constexpr char kDequeueBufferQuery[] = "android:dequeueBuffer";
constexpr char kQueueBufferQuery[] = "android:queueBuffer";

constexpr char kBarrierOrderingSubQuery[] =
    "gpu:CommandBufferProxyImpl::OrderingBarrier";
constexpr char kBufferInUseQuery[] = "exo:BufferInUse";
constexpr char kHandleMessageRefreshQuery[] =
    "android:onMessageReceived/android:handleMessageRefresh";
constexpr char kHandleMessageInvalidateQuery[] =
    "android:onMessageReceived/android:handleMessageInvalidate";
constexpr char kChromeTopEventsQuery[] =
    "viz,benchmark:Graphics.Pipeline.DrawAndSwap";
constexpr char kVsyncQuery0[] = "android:HW_VSYNC_0|0";
constexpr char kVsyncQuery1[] = "android:HW_VSYNC_0|1";

constexpr char kBarrierFlushMatcher[] = "gpu:CommandBufferStub::OnAsyncFlush";

constexpr char kExoSurfaceAttachMatcher[] = "exo:Surface::Attach";
constexpr char kExoBufferProduceResourceMatcher[] =
    "exo:Buffer::ProduceTransferableResource";
constexpr char kExoBufferReleaseContentsMatcher[] =
    "exo:Buffer::ReleaseContents";

constexpr ssize_t kInvalidBufferIndex = -1;

// Helper factory class that produces graphic buffer events from the giving
// |ArcTracingEvent| generic events. Each |ArcTracingEvent| may produce graphics
// event |ArcTracingGraphicsModel::BufferEvent| on start or/and on finish of the
// event |ArcTracingEvent|. This is organized in form of map
// |ArcTracingEventMatcher| to the pair of |BufferEventType| which indicates
// what to generate on start and on finish of the event.
class BufferGraphicsEventMapper {
 public:
  struct MappingRule {
    MappingRule(BufferEventType map_start, BufferEventType map_finish)
        : map_start(map_start), map_finish(map_finish) {}
    BufferEventType map_start;
    BufferEventType map_finish;
  };
  using MappingRules = std::vector<
      std::pair<std::unique_ptr<ArcTracingEventMatcher>, MappingRule>>;

  BufferGraphicsEventMapper() {
    // exo rules
    rules_.emplace_back(std::make_pair(
        std::make_unique<ArcTracingEventMatcher>(kExoSurfaceAttachMatcher),
        MappingRule(BufferEventType::kExoSurfaceAttach,
                    BufferEventType::kNone)));
    rules_.emplace_back(
        std::make_pair(std::make_unique<ArcTracingEventMatcher>(
                           kExoBufferProduceResourceMatcher),
                       MappingRule(BufferEventType::kExoProduceResource,
                                   BufferEventType::kNone)));
    rules_.emplace_back(std::make_pair(
        std::make_unique<ArcTracingEventMatcher>(
            kExoBufferReleaseContentsMatcher),
        MappingRule(BufferEventType::kNone, BufferEventType::kExoReleased)));
    rules_.emplace_back(std::make_pair(
        std::make_unique<ArcTracingEventMatcher>("exo:BufferInUse(step=bound)"),
        MappingRule(BufferEventType::kExoBound, BufferEventType::kNone)));
    rules_.emplace_back(
        std::make_pair(std::make_unique<ArcTracingEventMatcher>(
                           "exo:BufferInUse(step=pending_query)"),
                       MappingRule(BufferEventType::kExoPendingQuery,
                                   BufferEventType::kNone)));

    // gpu rules
    rules_.emplace_back(
        std::make_pair(std::make_unique<ArcTracingEventMatcher>(
                           "gpu:CommandBufferProxyImpl::OrderingBarrier"),
                       MappingRule(BufferEventType::kChromeBarrierOrder,
                                   BufferEventType::kNone)));
    rules_.emplace_back(std::make_pair(
        std::make_unique<ArcTracingEventMatcher>(kBarrierFlushMatcher),
        MappingRule(BufferEventType::kNone,
                    BufferEventType::kChromeBarrierFlush)));

    // android rules
    rules_.emplace_back(std::make_pair(
        std::make_unique<ArcTracingEventMatcher>(kDequeueBufferQuery),
        MappingRule(BufferEventType::kBufferQueueDequeueStart,
                    BufferEventType::kBufferQueueDequeueDone)));
    rules_.emplace_back(std::make_pair(
        std::make_unique<ArcTracingEventMatcher>(kQueueBufferQuery),
        MappingRule(BufferEventType::kBufferQueueQueueStart,
                    BufferEventType::kBufferQueueQueueDone)));
    rules_.emplace_back(std::make_pair(
        std::make_unique<ArcTracingEventMatcher>("android:acquireBuffer"),
        MappingRule(BufferEventType::kBufferQueueAcquire,
                    BufferEventType::kNone)));
    rules_.push_back(std::make_pair(
        std::make_unique<ArcTracingEventMatcher>("android:releaseBuffer"),
        MappingRule(BufferEventType::kNone,
                    BufferEventType::kBufferQueueReleased)));
    rules_.emplace_back(std::make_pair(
        std::make_unique<ArcTracingEventMatcher>(
            "android:handleMessageInvalidate"),
        MappingRule(BufferEventType::kSurfaceFlingerInvalidationStart,
                    BufferEventType::kSurfaceFlingerInvalidationDone)));
    rules_.emplace_back(std::make_pair(
        std::make_unique<ArcTracingEventMatcher>(
            "android:handleMessageRefresh"),
        MappingRule(BufferEventType::kSurfaceFlingerCompositionStart,
                    BufferEventType::kSurfaceFlingerCompositionDone)));
    rules_.push_back(std::make_pair(
        std::make_unique<ArcTracingEventMatcher>(kVsyncQuery0),
        MappingRule(BufferEventType::kVsync, BufferEventType::kNone)));
    rules_.push_back(std::make_pair(
        std::make_unique<ArcTracingEventMatcher>(kVsyncQuery1),
        MappingRule(BufferEventType::kVsync, BufferEventType::kNone)));

    // viz,benchmark rules
    auto matcher = std::make_unique<ArcTracingEventMatcher>(
        "viz,benchmark:Graphics.Pipeline.DrawAndSwap");
    matcher->SetPhase(TRACE_EVENT_PHASE_ASYNC_BEGIN);
    rules_.emplace_back(std::make_pair(
        std::move(matcher),
        MappingRule(BufferEventType::kChromeOSDraw, BufferEventType::kNone)));
    rules_.emplace_back(std::make_pair(
        std::make_unique<ArcTracingEventMatcher>(
            "viz,benchmark:Graphics.Pipeline.DrawAndSwap(step=Draw)"),
        MappingRule(BufferEventType::kNone, BufferEventType::kNone)));
    rules_.emplace_back(std::make_pair(
        std::make_unique<ArcTracingEventMatcher>(
            "viz,benchmark:Graphics.Pipeline.DrawAndSwap(step=Swap)"),
        MappingRule(BufferEventType::kChromeOSSwap, BufferEventType::kNone)));
    rules_.emplace_back(std::make_pair(
        std::make_unique<ArcTracingEventMatcher>(
            "viz,benchmark:Graphics.Pipeline.DrawAndSwap(step=WaitForAck)"),
        MappingRule(BufferEventType::kChromeOSWaitForAck,
                    BufferEventType::kNone)));
    rules_.emplace_back(
        std::make_pair(std::make_unique<ArcTracingEventMatcher>(
                           "viz,benchmark:Graphics.Pipeline.DrawAndSwap(step="
                           "WaitForPresentation)"),
                       MappingRule(BufferEventType::kChromeOSPresentationDone,
                                   BufferEventType::kNone)));
    matcher = std::make_unique<ArcTracingEventMatcher>(
        "viz,benchmark:Graphics.Pipeline.DrawAndSwap");
    matcher->SetPhase(TRACE_EVENT_PHASE_ASYNC_END);
    rules_.emplace_back(std::make_pair(
        std::move(matcher), MappingRule(BufferEventType::kNone,
                                        BufferEventType::kChromeOSSwapDone)));
  }

  ~BufferGraphicsEventMapper() = default;

  void Produce(const ArcTracingEvent& event,
               ArcTracingGraphicsModel::BufferEvents* collector) const {
    for (const auto& rule : rules_) {
      if (!rule.first->Match(event))
        continue;
      if (rule.second.map_start != BufferEventType::kNone) {
        collector->push_back(ArcTracingGraphicsModel::BufferEvent(
            rule.second.map_start, event.GetTimestamp()));
      }
      if (rule.second.map_finish != BufferEventType::kNone) {
        collector->push_back(ArcTracingGraphicsModel::BufferEvent(
            rule.second.map_finish, event.GetEndTimestamp()));
      }
      return;
    }
    LOG(ERROR) << "Unsupported event: " << event.ToString();
  }

 private:
  MappingRules rules_;

  DISALLOW_COPY_AND_ASSIGN(BufferGraphicsEventMapper);
};

BufferGraphicsEventMapper& GetEventMapper() {
  static base::NoDestructor<BufferGraphicsEventMapper> instance;
  return *instance;
}

// Maps particular buffer to its events.
using BufferToEvents =
    std::map<std::string, ArcTracingGraphicsModel::BufferEvents>;

void SortBufferEventsByTimestamp(
    ArcTracingGraphicsModel::BufferEvents* events) {
  std::sort(events->begin(), events->end(),
            [](const ArcTracingGraphicsModel::BufferEvent& a,
               const ArcTracingGraphicsModel::BufferEvent& b) {
              return a.timestamp < b.timestamp;
            });
}

// Extracts buffer id from the surface flinger event. For example:
// android|releaseBuffer
//   android|com.android.vending/com.android.vending.AssetBrowserActivity#0: 2
// Buffer id appears as a child event where name if the combination of the
// current view of the Activity, its index and the number of buffer starting
// from 0. This helps to exactly identify the particular buffer in context of
// Android. Buffer id for this example is
// "com.android.vending/com.android.vending.AssetBrowserActivity#0: 2"
bool ExtractBufferIdFromSurfaceFlingerEvent(const ArcTracingEvent& event,
                                            std::string* id) {
  for (const auto& child : event.children()) {
    if (child->GetPhase() != TRACE_EVENT_PHASE_COMPLETE)
      continue;
    const std::string& name = child->GetName();
    size_t index = name.find(": ");
    if (index == std::string::npos)
      continue;
    index += 2;
    if (index >= name.length())
      continue;
    bool all_digits = true;
    while (index < name.length() && all_digits) {
      all_digits &= (name[index] >= '0' && name[index] <= '9');
      ++index;
    }
    if (!all_digits)
      continue;
    *id = name;
    return true;
  }
  return false;
}

// Extracts the activity name from the buffer id by discarding the buffer id
// and view index. For example, activity name for buffer id
// "com.android.vending/com.android.vending.AssetBrowserActivity#0: 2"
// is "com.android.vending/com.android.vending.AssetBrowserActivity".
// If the activity cannot be extracted then default |kUnknownActivity| is
// returned.
std::string GetActivityFromBufferName(const std::string& android_buffer_name) {
  const size_t position = android_buffer_name.find('#');
  if (position == std::string::npos)
    return kUnknownActivity;
  return android_buffer_name.substr(0, position);
}

// Processes surface flinger events. It selects events using |query| from the
// model. Buffer id is extracted for the each returned event and new events are
// grouped by its buffer id.
void ProcessSurfaceFlingerEvents(const ArcTracingModel& common_model,
                                 const std::string& query,
                                 BufferToEvents* buffer_to_events) {
  const ArcTracingModel::TracingEventPtrs surface_flinger_events =
      common_model.Select(query);
  std::string buffer_id;
  for (const ArcTracingEvent* event : surface_flinger_events) {
    if (!ExtractBufferIdFromSurfaceFlingerEvent(*event, &buffer_id)) {
      LOG(ERROR) << "Failed to get buffer id from surface flinger event";
      continue;
    }
    ArcTracingGraphicsModel::BufferEvents& graphics_events =
        (*buffer_to_events)[buffer_id];
    GetEventMapper().Produce(*event, &graphics_events);
  }
}

// Processes Android events acquireBuffer, releaseBuffer, dequeueBuffer and
// queueBuffer. It returns map buffer id to the list of sorted by timestamp
// events.
BufferToEvents GetSurfaceFlingerEvents(const ArcTracingModel& common_model) {
  BufferToEvents per_buffer_surface_flinger_events;
  ProcessSurfaceFlingerEvents(common_model, kAcquireBufferQuery,
                              &per_buffer_surface_flinger_events);
  ProcessSurfaceFlingerEvents(common_model, kReleaseBufferQueryP,
                              &per_buffer_surface_flinger_events);
  ProcessSurfaceFlingerEvents(common_model, kReleaseBufferQueryN,
                              &per_buffer_surface_flinger_events);
  ProcessSurfaceFlingerEvents(common_model, kQueueBufferQuery,
                              &per_buffer_surface_flinger_events);
  ProcessSurfaceFlingerEvents(common_model, kDequeueBufferQuery,
                              &per_buffer_surface_flinger_events);
  for (auto& buffer : per_buffer_surface_flinger_events)
    SortBufferEventsByTimestamp(&buffer.second);
  return per_buffer_surface_flinger_events;
}

// Processes exo events Surface::Attach and Buffer::ReleaseContents. Each event
// has argument buffer_id that identifies graphics buffer on Chrome side.
// buffer_id is just row pointer to internal class. If |buffer_id_to_task_id| is
// set then it is updated to map buffer id to task id.
void ProcessChromeEvents(const ArcTracingModel& common_model,
                         const std::string& query,
                         BufferToEvents* buffer_to_events,
                         std::map<std::string, int>* buffer_id_to_task_id) {
  const ArcTracingModel::TracingEventPtrs chrome_events =
      common_model.Select(query);
  for (const ArcTracingEvent* event : chrome_events) {
    const std::string buffer_id = event->GetArgAsString(
        kArgumentBufferId, std::string() /* default_value */);
    if (buffer_id.empty()) {
      LOG(ERROR) << "Failed to get buffer id from event: " << event->ToString();
      continue;
    }
    if (buffer_id_to_task_id) {
      const std::string app_id = event->GetArgAsString(
          kArgumentAppId, std::string() /* default_value */);
      if (app_id.empty()) {
        LOG(ERROR) << "Failed to get app id from event: " << event->ToString();
        continue;
      }
      int task_id = -1;
      if (sscanf(app_id.c_str(), "org.chromium.arc.%d", &task_id) != 1 ||
          task_id < 0) {
        LOG(ERROR) << "Failed to parse app id from event: "
                   << event->ToString();
        continue;
      }
      (*buffer_id_to_task_id)[buffer_id] = task_id;
    }
    ArcTracingGraphicsModel::BufferEvents& graphics_events =
        (*buffer_to_events)[buffer_id];
    GetEventMapper().Produce(*event, &graphics_events);
  }
}

std::string RouteToSelector(const std::vector<const ArcTracingEvent*>& route) {
  std::string result;
  for (const ArcTracingEvent* segment : route)
    result = result + "/" + segment->GetCategory() + ":" + segment->GetName();
  return result;
}

void DetermineHierarchy(std::vector<const ArcTracingEvent*>* route,
                        const ArcTracingEvent* event,
                        const ArcTracingEventMatcher& matcher,
                        std::string* out_query) {
  if (!out_query->empty())
    return;

  route->emplace_back(event);

  if (matcher.Match(*event)) {
    *out_query = RouteToSelector(*route);
  } else {
    for (const auto& child : event->children())
      DetermineHierarchy(route, child.get(), matcher, out_query);
  }

  route->pop_back();
}

BufferToEvents GetChromeEvents(
    const ArcTracingModel& common_model,
    std::map<std::string, int>* buffer_id_to_task_id) {
  // The tracing hierarchy may be easy changed any time in Chrome. This makes
  // using static queries fragile and dependent of many external components. To
  // provide the reliable way of requesting the needed information, let scan
  // |common_model| for top level events and determine the hierarchy of
  // interesting events dynamically.
  const ArcTracingModel::TracingEventPtrs top_level_events =
      common_model.Select("toplevel:");
  std::vector<const ArcTracingEvent*> route;
  std::string barrier_flush_query;
  const ArcTracingEventMatcher barrier_flush_matcher(kBarrierFlushMatcher);
  std::string attach_surface_query;
  const ArcTracingEventMatcher attach_surface_matcher(kExoSurfaceAttachMatcher);
  std::string produce_resource_query;
  const ArcTracingEventMatcher produce_resource_matcher(
      kExoBufferProduceResourceMatcher);
  std::string release_contents_query;
  const ArcTracingEventMatcher release_contents_matcher(
      kExoBufferReleaseContentsMatcher);
  for (const ArcTracingEvent* top_level_event : top_level_events) {
    DetermineHierarchy(&route, top_level_event, barrier_flush_matcher,
                       &barrier_flush_query);
    DetermineHierarchy(&route, top_level_event, attach_surface_matcher,
                       &attach_surface_query);
    DetermineHierarchy(&route, top_level_event, produce_resource_matcher,
                       &produce_resource_query);
    DetermineHierarchy(&route, top_level_event, release_contents_matcher,
                       &release_contents_query);
  }

  BufferToEvents per_buffer_chrome_events;
  // Only exo:Surface::Attach has app id argument.
  ProcessChromeEvents(common_model, attach_surface_query,
                      &per_buffer_chrome_events, buffer_id_to_task_id);
  ProcessChromeEvents(common_model, release_contents_query,
                      &per_buffer_chrome_events,
                      nullptr /* buffer_id_to_task_id */);

  // Handle ProduceTransferableResource events. They have extra link to barrier
  // events. Use buffer_id to bind events for the same graphics buffer.
  const ArcTracingModel::TracingEventPtrs produce_resource_events =
      common_model.Select(produce_resource_query);
  std::map<int, std::string> put_offset_to_buffer_id_map;
  for (const ArcTracingEvent* event : produce_resource_events) {
    const std::string buffer_id = event->GetArgAsString(
        kArgumentBufferId, std::string() /* default_value */);
    if (buffer_id.empty()) {
      LOG(ERROR) << "Failed to get buffer id from event: " << event->ToString();
      continue;
    }

    ArcTracingGraphicsModel::BufferEvents& graphics_events =
        per_buffer_chrome_events[buffer_id];
    GetEventMapper().Produce(*event, &graphics_events);

    const ArcTracingModel::TracingEventPtrs ordering_barrier_events =
        common_model.Select(event, kBarrierOrderingSubQuery);
    if (ordering_barrier_events.size() != 1) {
      LOG(ERROR) << "Expected only one " << kBarrierOrderingSubQuery << ". Got "
                 << ordering_barrier_events.size();
      continue;
    }
    const int put_offset = ordering_barrier_events[0]->GetArgAsInteger(
        kArgumentPutOffset, 0 /* default_value */);
    if (!put_offset) {
      LOG(ERROR) << "No " << kArgumentPutOffset
                 << " argument in: " << ordering_barrier_events[0]->ToString();
      continue;
    }
    if (put_offset_to_buffer_id_map.count(put_offset) &&
        put_offset_to_buffer_id_map[put_offset] != buffer_id) {
      LOG(ERROR) << put_offset << " is already mapped to "
                 << put_offset_to_buffer_id_map[put_offset]
                 << ". Skip mapping to " << buffer_id;
      continue;
    }
    put_offset_to_buffer_id_map[put_offset] = buffer_id;
    GetEventMapper().Produce(*ordering_barrier_events[0], &graphics_events);
  }

  // Find associated barrier flush event using put_offset argument.
  const ArcTracingModel::TracingEventPtrs barrier_flush_events =
      common_model.Select(barrier_flush_query);
  for (const ArcTracingEvent* event : barrier_flush_events) {
    const int put_offset =
        event->GetArgAsInteger(kArgumentPutOffset, 0 /* default_value */);
    if (!put_offset_to_buffer_id_map.count(put_offset))
      continue;
    ArcTracingGraphicsModel::BufferEvents& graphics_events =
        per_buffer_chrome_events[put_offset_to_buffer_id_map[put_offset]];
    GetEventMapper().Produce(*event, &graphics_events);
  }

  // Handle BufferInUse async events.
  const ArcTracingModel::TracingEventPtrs buffer_in_use_events =
      common_model.Select(kBufferInUseQuery);
  std::map<std::string, std::string> buffer_in_use_id_to_buffer_id;
  for (const ArcTracingEvent* event : buffer_in_use_events) {
    // Only start event has buffer_id association.
    if (event->GetPhase() != TRACE_EVENT_PHASE_ASYNC_BEGIN)
      continue;
    const std::string id = event->GetId();
    const std::string buffer_id = event->GetArgAsString(
        kArgumentBufferId, std::string() /* default_value */);
    if (buffer_id.empty() || id.empty()) {
      LOG(ERROR) << "Cannot map id to buffer id for event: "
                 << event->ToString();
      continue;
    }
    if (buffer_in_use_id_to_buffer_id.count(id) &&
        buffer_in_use_id_to_buffer_id[id] != buffer_id) {
      LOG(ERROR) << id << " is already mapped to "
                 << buffer_in_use_id_to_buffer_id[id] << ". Skip mapping to "
                 << buffer_id;
      continue;
    }
    buffer_in_use_id_to_buffer_id[id] = buffer_id;
  }

  for (const ArcTracingEvent* event : buffer_in_use_events) {
    if (event->GetPhase() != TRACE_EVENT_PHASE_ASYNC_STEP_INTO)
      continue;
    const std::string id = event->GetId();
    if (!buffer_in_use_id_to_buffer_id.count(id)) {
      LOG(ERROR) << "Found non-mapped event: " << event->ToString();
      continue;
    }
    ArcTracingGraphicsModel::BufferEvents& graphics_events =
        per_buffer_chrome_events[buffer_in_use_id_to_buffer_id[id]];
    GetEventMapper().Produce(*event, &graphics_events);
  }

  for (auto& buffer : per_buffer_chrome_events)
    SortBufferEventsByTimestamp(&buffer.second);

  return per_buffer_chrome_events;
}

// Helper that finds a event of particular type in the list of events |events|
// starting from the index |start_index|. Returns |kInvalidBufferIndex| if event
// cannot be found.
ssize_t FindEvent(const ArcTracingGraphicsModel::BufferEvents& events,
                  BufferEventType type,
                  size_t start_index) {
  for (size_t i = start_index; i < events.size(); ++i) {
    if (events[i].type == type)
      return i;
  }
  return kInvalidBufferIndex;
}

// Helper that finds valid pair of events for acquire/release buffer.
// |kBufferQueueReleased| should go immediately after |kBufferQueueAcquire|
// event with one exception of |kBufferQueueDequeueStart| that is allowed due to
// asynchronous flow of requesting buffers in Android. Returns
// |kInvalidBufferIndex| if such pair cannot be found.
ssize_t FindAcquireReleasePair(
    const ArcTracingGraphicsModel::BufferEvents& events,
    size_t start_index) {
  const ssize_t index_acquire =
      FindEvent(events, BufferEventType::kBufferQueueAcquire, start_index);
  if (index_acquire == kInvalidBufferIndex)
    return kInvalidBufferIndex;

  // kBufferQueueDequeueStart is allowed between kBufferQueueAcquire and
  // kBufferQueueReleased.
  for (size_t i = index_acquire + 1; i < events.size(); ++i) {
    if (events[i].type == BufferEventType::kBufferQueueDequeueStart) {
      continue;
    }
    if (events[i].type == BufferEventType::kBufferQueueReleased) {
      return index_acquire;
    }
    break;
  }
  return kInvalidBufferIndex;
}

// Helper that performs bisection search of event of type |type| in the ordered
// list of events |events|. Found event should have timestamp not later than
// |timestamp|. Returns |kInvalidBufferIndex| in case event is not found.
ssize_t FindNotLaterThan(const ArcTracingGraphicsModel::BufferEvents& events,
                         BufferEventType type,
                         int64_t timestamp) {
  if (events.empty() || events[0].timestamp > timestamp)
    return kInvalidBufferIndex;

  size_t min_range = 0;
  size_t result = events.size() - 1;
  while (events[result].timestamp > timestamp) {
    const size_t next = (result + min_range + 1) / 2;
    if (events[next].timestamp <= timestamp)
      min_range = next;
    else
      result = next - 1;
  }
  for (ssize_t i = result; i >= 0; --i) {
    if (events[i].type == type)
      return i;
  }
  return kInvalidBufferIndex;
}

// Tries to match Android graphics buffer events and Chrome graphics buffer
// events. There is no direct id usable to say if the same buffer is used or
// not. This tests if two set of events potentially belong the same buffer and
// return the maximum number of matching sequences. In case impossible
// combination is found then it returns 0 score. Impossible combination for
// example when we detect Chrome buffer was attached while it was not held by
// Android between |kBufferQueueAcquire| and |kBufferQueueRelease.
// The process of merging buffers continues while we can merge something. At
// each iteration buffers with maximum merge score get merged. Practically,
// having 20+ cycles (assuming 4 buffers in use) is enough to exactly identify
// the same buffer in Chrome and Android. If needed more similar checks can be
// added.
size_t GetMergeScore(
    const ArcTracingGraphicsModel::BufferEvents& surface_flinger_events,
    const ArcTracingGraphicsModel::BufferEvents& chrome_events) {
  ssize_t attach_index = -1;
  ssize_t acquire_index = -1;
  while (true) {
    acquire_index =
        FindAcquireReleasePair(surface_flinger_events, acquire_index + 1);
    if (acquire_index == kInvalidBufferIndex)
      return 0;
    attach_index =
        FindNotLaterThan(chrome_events, BufferEventType::kExoSurfaceAttach,
                         surface_flinger_events[acquire_index + 1].timestamp);
    if (attach_index >= 0)
      break;
  }
  // From here buffers must be in sync. Attach should happen between acquire and
  // release.
  size_t score = 0;
  while (true) {
    const int64_t timestamp_from =
        surface_flinger_events[acquire_index].timestamp;
    const int64_t timestamp_to =
        surface_flinger_events[acquire_index + 1].timestamp;
    const int64_t timestamp = chrome_events[attach_index].timestamp;
    if (timestamp < timestamp_from || timestamp > timestamp_to) {
      return 0;
    }
    acquire_index =
        FindAcquireReleasePair(surface_flinger_events, acquire_index + 2);
    attach_index = FindEvent(chrome_events, BufferEventType::kExoSurfaceAttach,
                             attach_index + 1);
    if (acquire_index == kInvalidBufferIndex ||
        attach_index == kInvalidBufferIndex) {
      break;
    }
    ++score;
  }

  return score;
}

// Helper that performs query in |common_model| and returns sorted list of built
// events.
ArcTracingGraphicsModel::BufferEvents GetEventsFromQuery(
    const ArcTracingModel& common_model,
    const std::string& query) {
  ArcTracingGraphicsModel::BufferEvents collector;
  for (const ArcTracingEvent* event : common_model.Select(query))
    GetEventMapper().Produce(*event, &collector);
  SortBufferEventsByTimestamp(&collector);
  return collector;
}

// Helper that extracts top level Android events, such as refresh, vsync.
ArcTracingGraphicsModel::BufferEvents GetAndroidTopEvents(
    const ArcTracingModel& common_model) {
  ArcTracingGraphicsModel::BufferEvents collector;
  for (const ArcTracingEvent* event :
       common_model.Select(kHandleMessageRefreshQuery)) {
    GetEventMapper().Produce(*event, &collector);
  }
  for (const ArcTracingEvent* event :
       common_model.Select(kHandleMessageInvalidateQuery)) {
    GetEventMapper().Produce(*event, &collector);
  }

  for (const ArcTracingEvent* event : common_model.Select(kVsyncQuery0))
    GetEventMapper().Produce(*event, &collector);
  for (const ArcTracingEvent* event : common_model.Select(kVsyncQuery1))
    GetEventMapper().Produce(*event, &collector);

  SortBufferEventsByTimestamp(&collector);
  return collector;
}

// Helper that serializes events |events| to the |base::ListValue|.
base::ListValue SerializeEvents(
    const ArcTracingGraphicsModel::BufferEvents& events) {
  base::ListValue list;
  for (auto& event : events) {
    base::ListValue event_value;
    event_value.GetList().push_back(base::Value(static_cast<int>(event.type)));
    event_value.GetList().push_back(
        base::Value(static_cast<double>(event.timestamp)));
    list.GetList().push_back(std::move(event_value));
  }
  return list;
}

bool IsInRange(BufferEventType type,
               BufferEventType type_from_inclusive,
               BufferEventType type_to_inclusive) {
  return type >= type_from_inclusive && type <= type_to_inclusive;
}

// Helper that loads events from |base::Value|. Returns true in case events were
// read successfully. Events must be sorted and be known.
bool LoadEvents(const base::Value* value,
                ArcTracingGraphicsModel::BufferEvents* out_events) {
  DCHECK(out_events);
  if (!value || !value->is_list())
    return false;
  int64_t previous_timestamp = 0;
  for (const auto& entry : value->GetList()) {
    if (!entry.is_list() || entry.GetList().size() != 2)
      return false;
    if (!entry.GetList()[0].is_int())
      return false;
    const BufferEventType type =
        static_cast<BufferEventType>(entry.GetList()[0].GetInt());

    if (!IsInRange(type, BufferEventType::kBufferQueueDequeueStart,
                   BufferEventType::kBufferQueueReleased) &&
        !IsInRange(type, BufferEventType::kExoSurfaceAttach,
                   BufferEventType::kExoReleased) &&
        !IsInRange(type, BufferEventType::kChromeBarrierOrder,
                   BufferEventType::kChromeBarrierFlush) &&
        !IsInRange(type, BufferEventType::kVsync,
                   BufferEventType::kSurfaceFlingerCompositionDone) &&
        !IsInRange(type, BufferEventType::kVsync,
                   BufferEventType::kSurfaceFlingerCompositionDone) &&
        !IsInRange(type, BufferEventType::kChromeOSDraw,
                   BufferEventType::kChromeOSSwapDone)) {
      return false;
    }

    if (!entry.GetList()[1].is_double() && !entry.GetList()[1].is_int())
      return false;
    const int timestamp = entry.GetList()[1].GetDouble();
    if (timestamp < previous_timestamp)
      return false;
    out_events->emplace_back(
        ArcTracingGraphicsModel::BufferEvent(type, timestamp));
    previous_timestamp = timestamp;
  }
  return true;
}

}  // namespace

ArcTracingGraphicsModel::BufferEvent::BufferEvent(BufferEventType type,
                                                  int64_t timestamp)
    : type(type), timestamp(timestamp) {}

bool ArcTracingGraphicsModel::BufferEvent::operator==(
    const BufferEvent& other) const {
  return type == other.type && timestamp == other.timestamp;
}

ArcTracingGraphicsModel::ViewId::ViewId(int task_id,
                                        const std::string& activity)
    : task_id(task_id), activity(activity) {}

bool ArcTracingGraphicsModel::ViewId::operator<(const ViewId& other) const {
  if (task_id != other.task_id)
    return task_id < other.task_id;
  return activity.compare(other.activity) < 0;
}

bool ArcTracingGraphicsModel::ViewId::operator==(const ViewId& other) const {
  return task_id == other.task_id && activity == other.activity;
}

ArcTracingGraphicsModel::ArcTracingGraphicsModel() = default;

ArcTracingGraphicsModel::~ArcTracingGraphicsModel() = default;

bool ArcTracingGraphicsModel::Build(const ArcTracingModel& common_model) {
  Reset();

  BufferToEvents per_buffer_surface_flinger_events =
      GetSurfaceFlingerEvents(common_model);
  BufferToEvents per_buffer_chrome_events =
      GetChromeEvents(common_model, &chrome_buffer_id_to_task_id_);

  // Try to merge surface flinger events and Chrome events. See |GetMergeScore|
  // for more details.
  while (true) {
    size_t max_merge_score = 0;
    std::string surface_flinger_buffer_id;
    std::string chrome_buffer_id;
    for (const auto& surface_flinger_buffer :
         per_buffer_surface_flinger_events) {
      for (const auto& chrome_buffer : per_buffer_chrome_events) {
        const size_t merge_score =
            GetMergeScore(surface_flinger_buffer.second, chrome_buffer.second);
        if (merge_score > max_merge_score) {
          max_merge_score = merge_score;
          surface_flinger_buffer_id = surface_flinger_buffer.first;
          chrome_buffer_id = chrome_buffer.first;
        }
      }
    }
    if (!max_merge_score)
      break;  // No more merge candidates.

    const ViewId view_id(GetTaskIdFromBufferName(chrome_buffer_id),
                         GetActivityFromBufferName(surface_flinger_buffer_id));

    std::vector<BufferEvents>& view_buffers = view_buffers_[view_id];
    view_buffers.push_back(std::move(
        per_buffer_surface_flinger_events[surface_flinger_buffer_id]));
    per_buffer_surface_flinger_events.erase(surface_flinger_buffer_id);
    view_buffers.back().insert(
        view_buffers.back().end(),
        per_buffer_chrome_events[chrome_buffer_id].begin(),
        per_buffer_chrome_events[chrome_buffer_id].end());
    per_buffer_chrome_events.erase(chrome_buffer_id);
    SortBufferEventsByTimestamp(&view_buffers.back());
  }

  for (auto& buffer : per_buffer_surface_flinger_events) {
    LOG(WARNING) << "Failed to merge events for buffer: " << buffer.first;
    view_buffers_[ViewId(-1 /* task_id */,
                         GetActivityFromBufferName(buffer.first))]
        .emplace_back(std::move(buffer.second));
  }

  for (auto& buffer : per_buffer_chrome_events) {
    LOG(WARNING) << "Failed to merge events for buffer: " << buffer.first;
    view_buffers_[ViewId(GetTaskIdFromBufferName(buffer.first),
                         kUnknownActivity)]
        .emplace_back(std::move(buffer.second));
  }

  if (view_buffers_.empty()) {
    LOG(ERROR) << "No buffer events";
    return false;
  }

  chrome_top_level_ = GetEventsFromQuery(common_model, kChromeTopEventsQuery);
  if (chrome_top_level_.empty()) {
    LOG(ERROR) << "No Chrome top events";
    return false;
  }

  android_top_level_ = GetAndroidTopEvents(common_model);
  if (android_top_level_.empty()) {
    LOG(ERROR) << "No Android events";
    return false;
  }

  NormalizeTimestamps();

  return true;
}

void ArcTracingGraphicsModel::NormalizeTimestamps() {
  int64_t min = std::numeric_limits<int64_t>::max();
  int64_t max = std::numeric_limits<int64_t>::min();
  for (const auto& view : view_buffers_) {
    for (const auto& buffer : view.second) {
      min = std::min(min, buffer.front().timestamp);
      max = std::max(max, buffer.back().timestamp);
    }
  }

  min = std::min(min, android_top_level_.front().timestamp);
  max = std::max(max, android_top_level_.back().timestamp);

  min = std::min(min, chrome_top_level_.front().timestamp);
  max = std::max(max, chrome_top_level_.back().timestamp);

  duration_ = max - min + 1;

  for (auto& view : view_buffers_) {
    for (auto& buffer : view.second) {
      for (auto& event : buffer)
        event.timestamp -= min;
    }
  }

  for (auto& event : chrome_top_level_)
    event.timestamp -= min;
  for (auto& event : android_top_level_)
    event.timestamp -= min;
}

void ArcTracingGraphicsModel::Reset() {
  chrome_top_level_.clear();
  android_top_level_.clear();
  view_buffers_.clear();
  chrome_buffer_id_to_task_id_.clear();
  duration_ = 0;
}

int ArcTracingGraphicsModel::GetTaskIdFromBufferName(
    const std::string& chrome_buffer_name) const {
  const auto it = chrome_buffer_id_to_task_id_.find(chrome_buffer_name);
  if (it == chrome_buffer_id_to_task_id_.end())
    return -1;
  return it->second;
}

std::unique_ptr<base::DictionaryValue> ArcTracingGraphicsModel::Serialize()
    const {
  std::unique_ptr<base::DictionaryValue> root =
      std::make_unique<base::DictionaryValue>();

  // Views
  base::ListValue view_list;
  for (auto& view : view_buffers_) {
    base::DictionaryValue view_value;
    view_value.SetKey(kKeyActivity, base::Value(view.first.activity));
    view_value.SetKey(kKeyTaskId, base::Value(view.first.task_id));
    base::ListValue buffer_list;
    for (auto& buffer : view.second)
      buffer_list.GetList().push_back(SerializeEvents(buffer));

    view_value.SetKey(kKeyBuffers, std::move(buffer_list));

    view_list.GetList().emplace_back(std::move(view_value));
  }
  root->SetKey(kKeyViews, std::move(view_list));

  // Android top events.
  root->SetKey(kKeyAndroid, SerializeEvents(android_top_level_));

  // Chrome top events
  root->SetKey(kKeyChrome, SerializeEvents(chrome_top_level_));

  // Duration.
  root->SetKey(kKeyDuration, base::Value(static_cast<double>(duration_)));

  return root;
}

std::string ArcTracingGraphicsModel::SerializeToJson() const {
  std::unique_ptr<base::DictionaryValue> root = Serialize();
  DCHECK(root);
  std::string output;
  if (!base::JSONWriter::WriteWithOptions(
          *root, base::JSONWriter::OPTIONS_PRETTY_PRINT, &output)) {
    LOG(ERROR) << "Failed to serialize model";
  }
  return output;
}

bool ArcTracingGraphicsModel::LoadFromJson(const std::string& json_data) {
  Reset();

  const std::unique_ptr<base::DictionaryValue> root =
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated(json_data));
  if (!root)
    return false;

  const base::Value* view_list =
      root->FindKeyOfType(kKeyViews, base::Value::Type::LIST);
  if (!view_list || view_list->GetList().empty())
    return false;

  for (const auto& view_entry : view_list->GetList()) {
    if (!view_entry.is_dict())
      return false;
    const base::Value* activity =
        view_entry.FindKeyOfType(kKeyActivity, base::Value::Type::STRING);
    const base::Value* task_id =
        view_entry.FindKeyOfType(kKeyTaskId, base::Value::Type::INTEGER);
    if (!activity || !task_id)
      return false;
    const ViewId view_id(task_id->GetInt(), activity->GetString());
    if (view_buffers_.find(view_id) != view_buffers_.end())
      return false;
    const base::Value* buffer_entries =
        view_entry.FindKeyOfType(kKeyBuffers, base::Value::Type::LIST);
    if (!buffer_entries)
      return false;
    for (const auto& buffer_entry : buffer_entries->GetList()) {
      BufferEvents events;
      if (!LoadEvents(&buffer_entry, &events))
        return false;
      view_buffers_[view_id].emplace_back(std::move(events));
    }
  }

  if (!LoadEvents(root->FindKey(kKeyAndroid), &android_top_level_))
    return false;

  if (!LoadEvents(root->FindKey(kKeyChrome), &chrome_top_level_))
    return false;

  const base::Value* duration = root->FindKey(kKeyDuration);
  if (!duration || (!duration->is_double() && !duration->is_int()))
    return false;

  duration_ = duration->GetDouble();
  if (duration_ < 0)
    return false;

  return true;
}

std::ostream& operator<<(std::ostream& os,
                         ArcTracingGraphicsModel::BufferEventType event_type) {
  return os << static_cast<typename std::underlying_type<
             ArcTracingGraphicsModel::BufferEventType>::type>(event_type);
}

}  // namespace arc
