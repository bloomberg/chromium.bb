// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/tracing/tracing_controller_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/json/string_escape.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/tracing/file_tracing_provider_impl.h"
#include "content/browser/tracing/trace_message_filter.h"
#include "content/browser/tracing/tracing_ui.h"
#include "content/common/child_process_messages.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "gpu/config/gpu_info.h"
#include "net/base/network_change_notifier.h"
#include "v8/include/v8-version-string.h"

#if (defined(OS_POSIX) && defined(USE_UDEV)) || defined(OS_WIN) || \
    defined(OS_MACOSX)
#define ENABLE_POWER_TRACING
#endif

#if defined(ENABLE_POWER_TRACING)
#include "content/browser/tracing/power_tracing_agent.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "content/public/browser/arc_tracing_agent.h"
#endif

#if defined(OS_WIN)
#include "content/browser/tracing/etw_tracing_agent_win.h"
#endif

using base::trace_event::TraceLog;
using base::trace_event::TraceConfig;

namespace content {

namespace {

base::LazyInstance<TracingControllerImpl>::Leaky g_controller =
    LAZY_INSTANCE_INITIALIZER;

const char kChromeTracingAgentName[] = "chrome";
const char kETWTracingAgentName[] = "etw";
const char kArcTracingAgentName[] = "arc";
const char kChromeTraceLabel[] = "traceEvents";

const int kStartTracingTimeoutSeconds = 30;
const int kIssueClockSyncTimeoutSeconds = 30;
const int kStopTracingRetryTimeMilliseconds = 100;

std::string GetNetworkTypeString() {
  switch (net::NetworkChangeNotifier::GetConnectionType()) {
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      return "Ethernet";
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return "WiFi";
    case net::NetworkChangeNotifier::CONNECTION_2G:
      return "2G";
    case net::NetworkChangeNotifier::CONNECTION_3G:
      return "3G";
    case net::NetworkChangeNotifier::CONNECTION_4G:
      return "4G";
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      return "None";
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return "Bluetooth";
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
    default:
      break;
  }
  return "Unknown";
}

std::string GetClockString() {
  switch (base::TimeTicks::GetClock()) {
    case base::TimeTicks::Clock::FUCHSIA_MX_CLOCK_MONOTONIC:
      return "FUCHSIA_MX_CLOCK_MONOTONIC";
    case base::TimeTicks::Clock::LINUX_CLOCK_MONOTONIC:
      return "LINUX_CLOCK_MONOTONIC";
    case base::TimeTicks::Clock::IOS_CF_ABSOLUTE_TIME_MINUS_KERN_BOOTTIME:
      return "IOS_CF_ABSOLUTE_TIME_MINUS_KERN_BOOTTIME";
    case base::TimeTicks::Clock::MAC_MACH_ABSOLUTE_TIME:
      return "MAC_MACH_ABSOLUTE_TIME";
    case base::TimeTicks::Clock::WIN_QPC:
      return "WIN_QPC";
    case base::TimeTicks::Clock::WIN_ROLLOVER_PROTECTED_TIME_GET_TIME:
      return "WIN_ROLLOVER_PROTECTED_TIME_GET_TIME";
  }

  NOTREACHED();
  return std::string();
}

}  // namespace

TracingController* TracingController::GetInstance() {
  return TracingControllerImpl::GetInstance();
}

TracingControllerImpl::TracingControllerImpl()
    : pending_start_tracing_ack_count_(0),
      pending_stop_tracing_ack_count_(0),
      pending_trace_log_status_ack_count_(0),
      maximum_trace_buffer_usage_(0),
      approximate_event_count_(0),
      pending_clock_sync_ack_count_(0),
      enabled_tracing_modes_(0) {
  // Deliberately leaked, like this class.
  base::FileTracing::SetProvider(new FileTracingProviderImpl);
}

TracingControllerImpl::~TracingControllerImpl() {
  // This is a Leaky instance.
  NOTREACHED();
}

TracingControllerImpl* TracingControllerImpl::GetInstance() {
  return g_controller.Pointer();
}

bool TracingControllerImpl::GetCategories(
    const GetCategoriesDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Known categories come back from child processes with the EndTracingAck
  // message. So to get known categories, just begin and end tracing immediately
  // afterwards. This will ping all the child processes for categories.
  pending_get_categories_done_callback_ = callback;
  if (!StartTracing(TraceConfig("*", ""), StartTracingDoneCallback())) {
    pending_get_categories_done_callback_.Reset();
    return false;
  }

  bool ok = StopTracing(NULL);
  DCHECK(ok);
  return true;
}

void TracingControllerImpl::SetEnabledOnFileThread(
    const TraceConfig& trace_config,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  TraceLog::GetInstance()->SetEnabled(trace_config, enabled_tracing_modes_);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}

void TracingControllerImpl::SetDisabledOnFileThread(
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  DCHECK(enabled_tracing_modes_);
  TraceLog::GetInstance()->SetDisabled(enabled_tracing_modes_);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}

bool TracingControllerImpl::StartTracing(
    const TraceConfig& trace_config,
    const StartTracingDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(additional_tracing_agents_.empty());

  // TODO(ssid): Introduce a priority for tracing agents to handle multiple
  // start and stop requests, crbug.com/705087.
  if (!can_start_tracing())
    return false;
  start_tracing_done_callback_ = callback;
  trace_config_.reset(new base::trace_event::TraceConfig(trace_config));
  enabled_tracing_modes_ = TraceLog::RECORDING_MODE;
  if (!trace_config_->event_filters().empty())
    enabled_tracing_modes_ |= TraceLog::FILTERING_MODE;
  metadata_.reset(new base::DictionaryValue());
  pending_start_tracing_ack_count_ = 0;

#if defined(OS_ANDROID)
  if (pending_get_categories_done_callback_.is_null())
    TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif

  if (trace_config.IsSystraceEnabled()) {
#if defined(ENABLE_POWER_TRACING)
    PowerTracingAgent::GetInstance()->StartAgentTracing(
        trace_config,
        base::Bind(&TracingControllerImpl::OnStartAgentTracingAcked,
                   base::Unretained(this)));
    ++pending_start_tracing_ack_count_;
#endif

#if defined(OS_CHROMEOS)
    chromeos::DebugDaemonClient* debug_daemon =
        chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
    if (debug_daemon) {
      debug_daemon->StartAgentTracing(
          trace_config,
          base::Bind(&TracingControllerImpl::OnStartAgentTracingAcked,
                     base::Unretained(this)));
      ++pending_start_tracing_ack_count_;
    }

    ArcTracingAgent::GetInstance()->StartAgentTracing(
        trace_config,
        base::Bind(&TracingControllerImpl::OnStartAgentTracingAcked,
                   base::Unretained(this)));
    ++pending_start_tracing_ack_count_;
#elif defined(OS_WIN)
    EtwTracingAgent::GetInstance()->StartAgentTracing(
        trace_config,
        base::Bind(&TracingControllerImpl::OnStartAgentTracingAcked,
                   base::Unretained(this)));
    ++pending_start_tracing_ack_count_;
#endif
  }

  // TraceLog may have been enabled in startup tracing before threads are ready.
  if (TraceLog::GetInstance()->IsEnabled())
    return true;

  StartAgentTracing(trace_config,
                    base::Bind(&TracingControllerImpl::OnStartAgentTracingAcked,
                               base::Unretained(this)));
  ++pending_start_tracing_ack_count_;

  // Set a deadline to ensure all agents ack within a reasonable time frame.
  start_tracing_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kStartTracingTimeoutSeconds),
      base::Bind(&TracingControllerImpl::OnAllTracingAgentsStarted,
                 base::Unretained(this)));

  return true;
}

void TracingControllerImpl::OnAllTracingAgentsStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  TRACE_EVENT_API_ADD_METADATA_EVENT(
      TraceLog::GetCategoryGroupEnabled("__metadata"),
      "IsTimeTicksHighResolution", "value",
      base::TimeTicks::IsHighResolution());

  // Notify all child processes.
  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendBeginTracing(*trace_config_);
  }

  if (!start_tracing_done_callback_.is_null())
    start_tracing_done_callback_.Run();

  start_tracing_done_callback_.Reset();
}

void TracingControllerImpl::AddMetadata(const base::DictionaryValue& data) {
  if (metadata_)
    metadata_->MergeDictionary(&data);
}

bool TracingControllerImpl::StopTracing(
    const scoped_refptr<TraceDataSink>& trace_data_sink) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!can_stop_tracing())
    return false;

  // If we're still waiting to start tracing, try again after a delay.
  if (start_tracing_timer_.IsRunning()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&TracingControllerImpl::StopTracing),
                       base::Unretained(this), trace_data_sink),
        base::TimeDelta::FromMilliseconds(kStopTracingRetryTimeMilliseconds));
    return true;
  }

  if (trace_data_sink) {
    MetadataFilterPredicate metadata_filter;
    if (TraceLog::GetInstance()->GetCurrentTraceConfig()
        .IsArgumentFilterEnabled()) {
      std::unique_ptr<TracingDelegate> delegate(
          GetContentClient()->browser()->GetTracingDelegate());
      if (delegate)
        metadata_filter = delegate->GetMetadataFilterPredicate();
    }
    AddFilteredMetadata(trace_data_sink.get(), GenerateTracingMetadataDict(),
                        metadata_filter);
    AddFilteredMetadata(trace_data_sink.get(), std::move(metadata_),
                        metadata_filter);
  } else {
    metadata_.reset();
  }

  trace_data_sink_ = trace_data_sink;
  trace_config_.reset();

  // Issue clock sync marker before actually stopping tracing.
  // StopTracingAfterClockSync() will be called after clock sync is done.
  IssueClockSyncMarker();

  return true;
}

void TracingControllerImpl::StopTracingAfterClockSync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // |pending_clock_sync_ack_count_| could be non-zero if clock sync times out.
  pending_clock_sync_ack_count_ = 0;

  // Disable local trace early to avoid traces during end-tracing process from
  // interfering with the process.
  base::Closure on_stop_tracing_done_callback = base::Bind(
      &TracingControllerImpl::OnStopTracingDone, base::Unretained(this));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::BindOnce(&TracingControllerImpl::SetDisabledOnFileThread,
                     base::Unretained(this), on_stop_tracing_done_callback));
}

void TracingControllerImpl::OnStopTracingDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(OS_ANDROID)
  if (pending_get_categories_done_callback_.is_null())
    TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif

  // Count myself (local trace) in pending_stop_tracing_ack_count_,
  // acked below.
  pending_stop_tracing_ack_count_ = trace_message_filters_.size() + 1;
  pending_stop_tracing_filters_ = trace_message_filters_;

  pending_stop_tracing_ack_count_ += additional_tracing_agents_.size();
  for (auto* it : additional_tracing_agents_) {
    it->StopAgentTracing(
        base::Bind(&TracingControllerImpl::OnEndAgentTracingAcked,
                   base::Unretained(this)));
  }
  additional_tracing_agents_.clear();

  StopAgentTracing(StopAgentTracingCallback());
}

bool TracingControllerImpl::GetTraceBufferUsage(
    const GetTraceBufferUsageCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!can_get_trace_buffer_usage() || callback.is_null())
    return false;

  pending_trace_buffer_usage_callback_ = callback;

  // Count myself in pending_trace_log_status_ack_count_, acked below.
  pending_trace_log_status_ack_count_ = trace_message_filters_.size() + 1;
  pending_trace_log_status_filters_ = trace_message_filters_;
  maximum_trace_buffer_usage_ = 0;
  approximate_event_count_ = 0;

  base::trace_event::TraceLogStatus status =
      TraceLog::GetInstance()->GetStatus();
  // Call OnTraceLogStatusReply unconditionally for the browser process.
  // This will result in immediate execution of the callback if there are no
  // child processes.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&TracingControllerImpl::OnTraceLogStatusReply,
                     base::Unretained(this), nullptr, status));

  // Notify all child processes.
  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendGetTraceLogStatus();
  }
  return true;
}

bool TracingControllerImpl::IsTracing() const {
  return !!enabled_tracing_modes_;
}

void TracingControllerImpl::AddTraceMessageFilter(
    TraceMessageFilter* trace_message_filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  trace_message_filters_.insert(trace_message_filter);
  if (can_stop_tracing()) {
    trace_message_filter->SendBeginTracing(
        TraceLog::GetInstance()->GetCurrentTraceConfig());
  }
}

void TracingControllerImpl::RemoveTraceMessageFilter(
    TraceMessageFilter* trace_message_filter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If a filter is removed while a response from that filter is pending then
  // simulate the response. Otherwise the response count will be wrong and the
  // completion callback will never be executed.
  if (pending_stop_tracing_ack_count_ > 0) {
    TraceMessageFilterSet::const_iterator it =
        pending_stop_tracing_filters_.find(trace_message_filter);
    if (it != pending_stop_tracing_filters_.end()) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::BindOnce(&TracingControllerImpl::OnStopTracingAcked,
                         base::Unretained(this),
                         base::RetainedRef(trace_message_filter),
                         std::vector<std::string>()));
    }
  }
  if (pending_trace_log_status_ack_count_ > 0) {
    TraceMessageFilterSet::const_iterator it =
        pending_trace_log_status_filters_.find(trace_message_filter);
    if (it != pending_trace_log_status_filters_.end()) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::BindOnce(&TracingControllerImpl::OnTraceLogStatusReply,
                         base::Unretained(this),
                         base::RetainedRef(trace_message_filter),
                         base::trace_event::TraceLogStatus()));
    }
  }
  trace_message_filters_.erase(trace_message_filter);
}

void TracingControllerImpl::AddTracingAgent(const std::string& agent_name) {
#if defined(OS_CHROMEOS)
  auto* debug_daemon =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  if (agent_name == debug_daemon->GetTracingAgentName()) {
    additional_tracing_agents_.push_back(debug_daemon);
    debug_daemon->SetStopAgentTracingTaskRunner(
        BrowserThread::GetBlockingPool());
    return;
  }

  if (agent_name == kArcTracingAgentName) {
    additional_tracing_agents_.push_back(ArcTracingAgent::GetInstance());
    return;
  }
#elif defined(OS_WIN)
  auto* etw_agent = EtwTracingAgent::GetInstance();
  if (agent_name == etw_agent->GetTracingAgentName()) {
    additional_tracing_agents_.push_back(etw_agent);
    return;
  }
#endif

#if defined(ENABLE_POWER_TRACING)
  auto* power_agent = PowerTracingAgent::GetInstance();
  if (agent_name == power_agent->GetTracingAgentName()) {
    additional_tracing_agents_.push_back(power_agent);
    return;
  }
#endif

  DCHECK(agent_name == kChromeTracingAgentName);
}

void TracingControllerImpl::OnStartAgentTracingAcked(
    const std::string& agent_name,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Don't taken any further action if the ack came after the deadline.
  if (!start_tracing_timer_.IsRunning())
    return;

  if (success)
    AddTracingAgent(agent_name);

  if (--pending_start_tracing_ack_count_ == 0) {
    start_tracing_timer_.Stop();
    OnAllTracingAgentsStarted();
  }
}

void TracingControllerImpl::OnStopTracingAcked(
    TraceMessageFilter* trace_message_filter,
    const std::vector<std::string>& known_category_groups) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(
            &TracingControllerImpl::OnStopTracingAcked, base::Unretained(this),
            base::RetainedRef(trace_message_filter), known_category_groups));
    return;
  }

  // Merge known_category_groups with known_category_groups_
  known_category_groups_.insert(known_category_groups.begin(),
                                known_category_groups.end());

  if (pending_stop_tracing_ack_count_ == 0)
    return;

  if (trace_message_filter &&
      !pending_stop_tracing_filters_.erase(trace_message_filter)) {
    // The response from the specified message filter has already been received.
    return;
  }

  if (--pending_stop_tracing_ack_count_ == 1) {
    // All acks from subprocesses have been received. Now flush the local trace.
    // During or after this call, our OnLocalTraceDataCollected will be
    // called with the last of the local trace data.
    if (trace_data_sink_) {
    TraceLog::GetInstance()->Flush(
        base::Bind(&TracingControllerImpl::OnLocalTraceDataCollected,
                   base::Unretained(this)),
        true);
    } else {
      TraceLog::GetInstance()->CancelTracing(
          base::Bind(&TracingControllerImpl::OnLocalTraceDataCollected,
                     base::Unretained(this)));
    }
    return;
  }

  if (pending_stop_tracing_ack_count_ != 0)
    return;

  // All acks (including from the subprocesses and the local trace) have been
  // received.
  enabled_tracing_modes_ = 0;

  // Trigger callback if one is set.
  if (!pending_get_categories_done_callback_.is_null()) {
    pending_get_categories_done_callback_.Run(known_category_groups_);
    pending_get_categories_done_callback_.Reset();
  } else if (trace_data_sink_.get()) {
    trace_data_sink_->Close();
    trace_data_sink_ = NULL;
  }
}

void TracingControllerImpl::OnEndAgentTracingAcked(
    const std::string& agent_name,
    const std::string& events_label,
    const scoped_refptr<base::RefCountedString>& events_str_ptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (trace_data_sink_.get() && events_str_ptr &&
      !events_str_ptr->data().empty()) {
    if (agent_name == kETWTracingAgentName) {
      // The Windows kernel events are kept into a JSON format stored as string
      // and must not be escaped.
      trace_data_sink_->AddAgentTrace(events_label, events_str_ptr->data());
    } else if (agent_name == kArcTracingAgentName) {
      // The ARC events are kept into a JSON format stored as string
      // and must not be escaped.
      trace_data_sink_->AddTraceChunk(events_str_ptr->data());
    } else {
      trace_data_sink_->AddAgentTrace(
          events_label, base::GetQuotedJSONString(events_str_ptr->data()));
    }
  }
  std::vector<std::string> category_groups;
  OnStopTracingAcked(NULL, category_groups);
}

void TracingControllerImpl::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str_ptr) {
  // OnTraceDataCollected may be called from any browser thread, either by the
  // local event trace system or from child processes via TraceMessageFilter.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&TracingControllerImpl::OnTraceDataCollected,
                       base::Unretained(this), events_str_ptr));
    return;
  }

  if (trace_data_sink_.get())
    trace_data_sink_->AddTraceChunk(events_str_ptr->data());
}

void TracingControllerImpl::OnLocalTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str_ptr,
    bool has_more_events) {
  if (events_str_ptr->data().size())
    OnTraceDataCollected(events_str_ptr);

  if (has_more_events)
    return;

  // Simulate an StopTracingAcked for the local trace.
  std::vector<std::string> category_groups;
  TraceLog::GetInstance()->GetKnownCategoryGroups(&category_groups);
  OnStopTracingAcked(NULL, category_groups);
}

void TracingControllerImpl::OnTraceLogStatusReply(
    TraceMessageFilter* trace_message_filter,
    const base::trace_event::TraceLogStatus& status) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&TracingControllerImpl::OnTraceLogStatusReply,
                       base::Unretained(this),
                       base::RetainedRef(trace_message_filter), status));
    return;
  }

  if (pending_trace_log_status_ack_count_ == 0)
    return;

  if (trace_message_filter &&
      !pending_trace_log_status_filters_.erase(trace_message_filter)) {
    // The response from the specified message filter has already been received.
    return;
  }

  float percent_full = static_cast<float>(
      static_cast<double>(status.event_count) / status.event_capacity);
  maximum_trace_buffer_usage_ =
      std::max(maximum_trace_buffer_usage_, percent_full);
  approximate_event_count_ += status.event_count;

  if (--pending_trace_log_status_ack_count_ == 0) {
    // Trigger callback if one is set.
    pending_trace_buffer_usage_callback_.Run(maximum_trace_buffer_usage_,
                                             approximate_event_count_);
    pending_trace_buffer_usage_callback_.Reset();
  }
}

void TracingControllerImpl::RegisterTracingUI(TracingUI* tracing_ui) {
  DCHECK(tracing_uis_.find(tracing_ui) == tracing_uis_.end());
  tracing_uis_.insert(tracing_ui);
}

void TracingControllerImpl::UnregisterTracingUI(TracingUI* tracing_ui) {
  std::set<TracingUI*>::iterator it = tracing_uis_.find(tracing_ui);
  DCHECK(it != tracing_uis_.end());
  tracing_uis_.erase(it);
}

std::string TracingControllerImpl::GetTracingAgentName() {
  return kChromeTracingAgentName;
}

std::string TracingControllerImpl::GetTraceEventLabel() {
  return kChromeTraceLabel;
}

void TracingControllerImpl::StartAgentTracing(
    const base::trace_event::TraceConfig& trace_config,
    const StartAgentTracingCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::Closure on_agent_started =
      base::Bind(callback, kChromeTracingAgentName, true);
  if (!BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::BindOnce(&TracingControllerImpl::SetEnabledOnFileThread,
                         base::Unretained(this), trace_config,
                         on_agent_started))) {
    // BrowserThread::PostTask fails if the threads haven't been created yet,
    // so it should be safe to just use TraceLog::SetEnabled directly.
    TraceLog::GetInstance()->SetEnabled(trace_config, enabled_tracing_modes_);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, on_agent_started);
  }
}

void TracingControllerImpl::StopAgentTracing(
    const StopAgentTracingCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Handle special case of zero child processes by immediately flushing the
  // trace log. Once the flush has completed the caller will be notified that
  // tracing has ended.
  if (pending_stop_tracing_ack_count_ == 1) {
    // Flush/cancel asynchronously now, because we don't have any children to
    // wait for.
    if (trace_data_sink_) {
      TraceLog::GetInstance()->Flush(
          base::Bind(&TracingControllerImpl::OnLocalTraceDataCollected,
                     base::Unretained(this)),
          true);
    } else {
      TraceLog::GetInstance()->CancelTracing(
          base::Bind(&TracingControllerImpl::OnLocalTraceDataCollected,
                     base::Unretained(this)));
    }
  }

  // Notify all child processes.
  for (auto it : trace_message_filters_) {
    if (trace_data_sink_)
      it->SendEndTracing();
    else
      it->SendCancelTracing();
  }
}

bool TracingControllerImpl::SupportsExplicitClockSync() {
  return true;
}

void TracingControllerImpl::RecordClockSyncMarker(
    const std::string& sync_id,
    const RecordClockSyncMarkerCallback& callback) {
  DCHECK(SupportsExplicitClockSync());

  TRACE_EVENT_CLOCK_SYNC_RECEIVER(sync_id);
}

void TracingControllerImpl::IssueClockSyncMarker() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(0, pending_clock_sync_ack_count_);

  for (auto* it : additional_tracing_agents_) {
    if (it->SupportsExplicitClockSync()) {
      it->RecordClockSyncMarker(
          base::GenerateGUID(),
          base::Bind(&TracingControllerImpl::OnClockSyncMarkerRecordedByAgent,
                     base::Unretained(this)));
      pending_clock_sync_ack_count_++;
    }
  }

  // If no clock sync is needed, stop tracing right away. Otherwise, schedule
  // to stop tracing after timeout.
  if (pending_clock_sync_ack_count_ == 0) {
    StopTracingAfterClockSync();
  } else {
    clock_sync_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kIssueClockSyncTimeoutSeconds),
        this, &TracingControllerImpl::StopTracingAfterClockSync);
  }
}

void TracingControllerImpl::OnClockSyncMarkerRecordedByAgent(
    const std::string& sync_id,
    const base::TimeTicks& issue_ts,
    const base::TimeTicks& issue_end_ts) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(charliea): Change this function so that it can accept a boolean
  // success indicator instead of having to rely on sentinel issue_ts and
  // issue_end_ts values to signal failure.
  if (!(issue_ts == base::TimeTicks() || issue_end_ts == base::TimeTicks()))
    TRACE_EVENT_CLOCK_SYNC_ISSUER(sync_id, issue_ts, issue_end_ts);

  // Timer is not running means that clock sync already timed out.
  if (!clock_sync_timer_.IsRunning())
    return;

  // Stop tracing only if all agents report back.
  if (--pending_clock_sync_ack_count_ == 0) {
    clock_sync_timer_.Stop();
    StopTracingAfterClockSync();
  }
}

void TracingControllerImpl::AddFilteredMetadata(
    TracingController::TraceDataSink* sink,
    std::unique_ptr<base::DictionaryValue> metadata,
    const MetadataFilterPredicate& filter) {
  if (filter.is_null()) {
    sink->AddMetadata(std::move(metadata));
    return;
  }
  std::unique_ptr<base::DictionaryValue> filtered_metadata(
      new base::DictionaryValue);
  for (base::DictionaryValue::Iterator it(*metadata); !it.IsAtEnd();
       it.Advance()) {
    if (filter.Run(it.key()))
      filtered_metadata->Set(it.key(),
                             base::MakeUnique<base::Value>(it.value().Clone()));
    else
      filtered_metadata->SetString(it.key(), "__stripped__");
  }
  sink->AddMetadata(std::move(filtered_metadata));
}

std::unique_ptr<base::DictionaryValue>
TracingControllerImpl::GenerateTracingMetadataDict() const {
  // It's important that this function creates a new metadata dict and returns
  // it rather than directly populating the metadata_ member, as the data may
  // need filtering in some cases.
  std::unique_ptr<base::DictionaryValue> metadata_dict(
      new base::DictionaryValue());

  metadata_dict->SetString("network-type", GetNetworkTypeString());
  metadata_dict->SetString("product-version", GetContentClient()->GetProduct());
  metadata_dict->SetString("v8-version", V8_VERSION_STRING);
  metadata_dict->SetString("user-agent", GetContentClient()->GetUserAgent());

  // OS
#if defined(OS_CHROMEOS)
  metadata_dict->SetString("os-name", "CrOS");
  int32_t major_version;
  int32_t minor_version;
  int32_t bugfix_version;
  // OperatingSystemVersion only has a POSIX implementation which returns the
  // wrong versions for CrOS.
  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                               &bugfix_version);
  metadata_dict->SetString(
      "os-version", base::StringPrintf("%d.%d.%d", major_version, minor_version,
                                       bugfix_version));
#else
  metadata_dict->SetString("os-name", base::SysInfo::OperatingSystemName());
  metadata_dict->SetString("os-version",
                           base::SysInfo::OperatingSystemVersion());
#endif
  metadata_dict->SetString("os-arch",
                           base::SysInfo::OperatingSystemArchitecture());

  // CPU
  base::CPU cpu;
  metadata_dict->SetInteger("cpu-family", cpu.family());
  metadata_dict->SetInteger("cpu-model", cpu.model());
  metadata_dict->SetInteger("cpu-stepping", cpu.stepping());
  metadata_dict->SetInteger("num-cpus", base::SysInfo::NumberOfProcessors());
  metadata_dict->SetInteger("physical-memory",
                            base::SysInfo::AmountOfPhysicalMemoryMB());

  std::string cpu_brand = cpu.cpu_brand();
  // Workaround for crbug.com/249713.
  // TODO(oysteine): Remove workaround when bug is fixed.
  size_t null_pos = cpu_brand.find('\0');
  if (null_pos != std::string::npos)
    cpu_brand.erase(null_pos);
  metadata_dict->SetString("cpu-brand", cpu_brand);

  // GPU
  gpu::GPUInfo gpu_info = content::GpuDataManager::GetInstance()->GetGPUInfo();

#if !defined(OS_ANDROID)
  metadata_dict->SetInteger("gpu-venid", gpu_info.gpu.vendor_id);
  metadata_dict->SetInteger("gpu-devid", gpu_info.gpu.device_id);
#endif

  metadata_dict->SetString("gpu-driver", gpu_info.driver_version);
  metadata_dict->SetString("gpu-psver", gpu_info.pixel_shader_version);
  metadata_dict->SetString("gpu-vsver", gpu_info.vertex_shader_version);

#if defined(OS_MACOSX)
  metadata_dict->SetString("gpu-glver", gpu_info.gl_version);
#elif defined(OS_POSIX)
  metadata_dict->SetString("gpu-gl-vendor", gpu_info.gl_vendor);
  metadata_dict->SetString("gpu-gl-renderer", gpu_info.gl_renderer);
#endif

  std::unique_ptr<TracingDelegate> delegate(
      GetContentClient()->browser()->GetTracingDelegate());
  if (delegate)
    delegate->GenerateMetadataDict(metadata_dict.get());

  metadata_dict->SetString("clock-domain", GetClockString());
  metadata_dict->SetBoolean("highres-ticks",
                            base::TimeTicks::IsHighResolution());

  metadata_dict->SetString("trace-config", trace_config_->ToString());

  metadata_dict->SetString(
      "command_line",
      base::CommandLine::ForCurrentProcess()->GetCommandLineString());

  base::Time::Exploded ctime;
  base::Time::Now().UTCExplode(&ctime);
  std::string time_string = base::StringPrintf(
      "%u-%u-%u %d:%d:%d", ctime.year, ctime.month, ctime.day_of_month,
      ctime.hour, ctime.minute, ctime.second);
  metadata_dict->SetString("trace-capture-datetime", time_string);

  return metadata_dict;
}

}  // namespace content
