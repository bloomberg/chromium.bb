// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/tracing/tracing_controller_impl.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/string_escape.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/tracing/file_tracing_provider_impl.h"
#include "content/browser/tracing/power_tracing_agent.h"
#include "content/browser/tracing/trace_message_filter.h"
#include "content/browser/tracing/tracing_ui.h"
#include "content/common/child_process_messages.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#endif

#if defined(OS_WIN)
#include "content/browser/tracing/etw_system_event_consumer_win.h"
#endif

using base::trace_event::TraceLog;
using base::trace_event::TraceConfig;

namespace content {

namespace {

base::LazyInstance<TracingControllerImpl>::Leaky g_controller =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

TracingController* TracingController::GetInstance() {
  return TracingControllerImpl::GetInstance();
}

TracingControllerImpl::TracingControllerImpl()
    : pending_disable_recording_ack_count_(0),
      pending_capture_monitoring_snapshot_ack_count_(0),
      pending_trace_log_status_ack_count_(0),
      maximum_trace_buffer_usage_(0),
      approximate_event_count_(0),
      pending_memory_dump_ack_count_(0),
      failed_memory_dump_count_(0),
    // Tracing may have been enabled by ContentMainRunner if kTraceStartup
    // is specified in command line.
#if defined(OS_CHROMEOS) || defined(OS_WIN)
      is_system_tracing_(false),
#endif
      is_recording_(TraceLog::GetInstance()->IsEnabled()),
      is_monitoring_(false),
      is_power_tracing_(false) {
  base::trace_event::MemoryDumpManager::GetInstance()->SetDelegate(this);

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
  if (!EnableRecording(TraceConfig("*", ""), EnableRecordingDoneCallback())) {
    pending_get_categories_done_callback_.Reset();
    return false;
  }

  bool ok = DisableRecording(NULL);
  DCHECK(ok);
  return true;
}

void TracingControllerImpl::SetEnabledOnFileThread(
    const TraceConfig& trace_config,
    int mode,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  TraceLog::GetInstance()->SetEnabled(
      trace_config, static_cast<TraceLog::Mode>(mode));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}

void TracingControllerImpl::SetDisabledOnFileThread(
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  TraceLog::GetInstance()->SetDisabled();
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}

bool TracingControllerImpl::EnableRecording(
    const TraceConfig& trace_config,
    const EnableRecordingDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!can_enable_recording())
    return false;
  is_recording_ = true;

#if defined(OS_ANDROID)
  if (pending_get_categories_done_callback_.is_null())
    TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif

  if (trace_config.IsSystraceEnabled()) {
#if defined(OS_CHROMEOS)
    DCHECK(!is_system_tracing_);
    chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->
      StartSystemTracing();
    is_system_tracing_ = true;
#elif defined(OS_WIN)
    DCHECK(!is_system_tracing_);
    is_system_tracing_ =
        EtwSystemEventConsumer::GetInstance()->StartSystemTracing();
#endif
  }

  base::Closure on_enable_recording_done_callback =
      base::Bind(&TracingControllerImpl::OnEnableRecordingDone,
                 base::Unretained(this),
                 trace_config, callback);
  if (!BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&TracingControllerImpl::SetEnabledOnFileThread,
                     base::Unretained(this), trace_config,
                     base::trace_event::TraceLog::RECORDING_MODE,
                     on_enable_recording_done_callback))) {
    // BrowserThread::PostTask fails if the threads haven't been created yet,
    // so it should be safe to just use TraceLog::SetEnabled directly.
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        trace_config, base::trace_event::TraceLog::RECORDING_MODE);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            on_enable_recording_done_callback);
  }

  return true;
}

void TracingControllerImpl::OnEnableRecordingDone(
    const TraceConfig& trace_config,
    const EnableRecordingDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Notify all child processes.
  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendBeginTracing(trace_config);
  }

  if (!callback.is_null())
    callback.Run();
}

bool TracingControllerImpl::DisableRecording(
    const scoped_refptr<TraceDataSink>& trace_data_sink) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!can_disable_recording())
    return false;

  trace_data_sink_ = trace_data_sink;
  // Disable local trace early to avoid traces during end-tracing process from
  // interfering with the process.
  base::Closure on_disable_recording_done_callback = base::Bind(
      &TracingControllerImpl::OnDisableRecordingDone, base::Unretained(this));
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&TracingControllerImpl::SetDisabledOnFileThread,
                 base::Unretained(this),
                 on_disable_recording_done_callback));
  return true;
}

void TracingControllerImpl::OnDisableRecordingDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(OS_ANDROID)
  if (pending_get_categories_done_callback_.is_null())
    TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif

  // Count myself (local trace) in pending_disable_recording_ack_count_,
  // acked below.
  pending_disable_recording_ack_count_ = trace_message_filters_.size() + 1;
  pending_disable_recording_filters_ = trace_message_filters_;

#if defined(OS_CHROMEOS) || defined(OS_WIN)
  if (is_system_tracing_) {
    // Disable system tracing.
    is_system_tracing_ = false;
    ++pending_disable_recording_ack_count_;

#if defined(OS_CHROMEOS)
    scoped_refptr<base::TaskRunner> task_runner =
        BrowserThread::GetBlockingPool();
    chromeos::DBusThreadManager::Get()
        ->GetDebugDaemonClient()
        ->RequestStopSystemTracing(
            task_runner,
            base::Bind(&TracingControllerImpl::OnEndSystemTracingAcked,
                       base::Unretained(this)));
#elif defined(OS_WIN)
    EtwSystemEventConsumer::GetInstance()->StopSystemTracing(
        base::Bind(&TracingControllerImpl::OnEndSystemTracingAcked,
                   base::Unretained(this)));
#endif
  }
#endif  // defined(OS_CHROMEOS) || defined(OS_WIN)

  if (is_power_tracing_) {
    is_power_tracing_ = false;
    ++pending_disable_recording_ack_count_;
    PowerTracingAgent::GetInstance()->StopTracing(
        base::Bind(&TracingControllerImpl::OnEndPowerTracingAcked,
                   base::Unretained(this)));
  }

  // Handle special case of zero child processes by immediately flushing the
  // trace log. Once the flush has completed the caller will be notified that
  // tracing has ended.
  if (pending_disable_recording_ack_count_ == 1) {
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
  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    if (trace_data_sink_)
      it->get()->SendEndTracing();
    else
      it->get()->SendCancelTracing();
  }
}

bool TracingControllerImpl::EnableMonitoring(
    const TraceConfig& trace_config,
    const EnableMonitoringDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!can_enable_monitoring())
    return false;
  OnMonitoringStateChanged(true);

#if defined(OS_ANDROID)
  TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif

  base::Closure on_enable_monitoring_done_callback =
      base::Bind(&TracingControllerImpl::OnEnableMonitoringDone,
                 base::Unretained(this),
                 trace_config, callback);
  if (!BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&TracingControllerImpl::SetEnabledOnFileThread,
                     base::Unretained(this), trace_config,
                     base::trace_event::TraceLog::MONITORING_MODE,
                     on_enable_monitoring_done_callback))) {
    // BrowserThread::PostTask fails if the threads haven't been created yet,
    // so it should be safe to just use TraceLog::SetEnabled directly.
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        trace_config, base::trace_event::TraceLog::MONITORING_MODE);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            on_enable_monitoring_done_callback);
  }
  return true;
}

void TracingControllerImpl::OnEnableMonitoringDone(
    const TraceConfig& trace_config,
    const EnableMonitoringDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Notify all child processes.
  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendEnableMonitoring(trace_config);
  }

  if (!callback.is_null())
    callback.Run();
}

bool TracingControllerImpl::DisableMonitoring(
    const DisableMonitoringDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!can_disable_monitoring())
    return false;

  base::Closure on_disable_monitoring_done_callback =
      base::Bind(&TracingControllerImpl::OnDisableMonitoringDone,
                 base::Unretained(this), callback);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&TracingControllerImpl::SetDisabledOnFileThread,
                 base::Unretained(this),
                 on_disable_monitoring_done_callback));
  return true;
}

void TracingControllerImpl::OnDisableMonitoringDone(
    const DisableMonitoringDoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  OnMonitoringStateChanged(false);

  // Notify all child processes.
  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendDisableMonitoring();
  }
  if (!callback.is_null())
    callback.Run();
}

void TracingControllerImpl::GetMonitoringStatus(
    bool* out_enabled,
    TraceConfig* out_trace_config) {
  *out_enabled = is_monitoring_;
  *out_trace_config = TraceLog::GetInstance()->GetCurrentTraceConfig();
}

bool TracingControllerImpl::CaptureMonitoringSnapshot(
    const scoped_refptr<TraceDataSink>& monitoring_data_sink) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!can_disable_monitoring())
    return false;

  if (!monitoring_data_sink.get())
    return false;

  monitoring_data_sink_ = monitoring_data_sink;

  // Count myself in pending_capture_monitoring_snapshot_ack_count_,
  // acked below.
  pending_capture_monitoring_snapshot_ack_count_ =
      trace_message_filters_.size() + 1;
  pending_capture_monitoring_filters_ = trace_message_filters_;

  // Handle special case of zero child processes by immediately flushing the
  // trace log. Once the flush has completed the caller will be notified that
  // the capture snapshot has ended.
  if (pending_capture_monitoring_snapshot_ack_count_ == 1) {
    // Flush asynchronously now, because we don't have any children to wait for.
    TraceLog::GetInstance()->FlushButLeaveBufferIntact(
        base::Bind(&TracingControllerImpl::OnLocalMonitoringTraceDataCollected,
                   base::Unretained(this)));
  }

  // Notify all child processes.
  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendCaptureMonitoringSnapshot();
  }

#if defined(OS_ANDROID)
  TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif

  return true;
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
      base::Bind(&TracingControllerImpl::OnTraceLogStatusReply,
                 base::Unretained(this), scoped_refptr<TraceMessageFilter>(),
                 status));

  // Notify all child processes.
  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendGetTraceLogStatus();
  }
  return true;
}

bool TracingControllerImpl::SetWatchEvent(
    const std::string& category_name,
    const std::string& event_name,
    const WatchEventCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (callback.is_null())
    return false;

  watch_category_name_ = category_name;
  watch_event_name_ = event_name;
  watch_event_callback_ = callback;

  TraceLog::GetInstance()->SetWatchEvent(
      category_name, event_name,
      base::Bind(&TracingControllerImpl::OnWatchEventMatched,
                 base::Unretained(this)));

  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendSetWatchEvent(category_name, event_name);
  }
  return true;
}

bool TracingControllerImpl::CancelWatchEvent() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!can_cancel_watch_event())
    return false;

  for (TraceMessageFilterSet::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendCancelWatchEvent();
  }

  watch_event_callback_.Reset();
  return true;
}

bool TracingControllerImpl::IsRecording() const {
  return is_recording_;
}

void TracingControllerImpl::AddTraceMessageFilter(
    TraceMessageFilter* trace_message_filter) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::AddTraceMessageFilter,
                   base::Unretained(this),
                   make_scoped_refptr(trace_message_filter)));
    return;
  }

  trace_message_filters_.insert(trace_message_filter);
  if (can_cancel_watch_event()) {
    trace_message_filter->SendSetWatchEvent(watch_category_name_,
                                            watch_event_name_);
  }
  if (can_disable_recording()) {
    trace_message_filter->SendBeginTracing(
        TraceLog::GetInstance()->GetCurrentTraceConfig());
  }
  if (can_disable_monitoring()) {
    trace_message_filter->SendEnableMonitoring(
        TraceLog::GetInstance()->GetCurrentTraceConfig());
  }
  if (!trace_message_filter_added_callback_.is_null())
    trace_message_filter_added_callback_.Run(trace_message_filter);
}

void TracingControllerImpl::RemoveTraceMessageFilter(
    TraceMessageFilter* trace_message_filter) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::RemoveTraceMessageFilter,
                   base::Unretained(this),
                   make_scoped_refptr(trace_message_filter)));
    return;
  }

  // If a filter is removed while a response from that filter is pending then
  // simulate the response. Otherwise the response count will be wrong and the
  // completion callback will never be executed.
  if (pending_disable_recording_ack_count_ > 0) {
    TraceMessageFilterSet::const_iterator it =
        pending_disable_recording_filters_.find(trace_message_filter);
    if (it != pending_disable_recording_filters_.end()) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::Bind(&TracingControllerImpl::OnDisableRecordingAcked,
                     base::Unretained(this),
                     make_scoped_refptr(trace_message_filter),
                     std::vector<std::string>()));
    }
  }
  if (pending_capture_monitoring_snapshot_ack_count_ > 0) {
    TraceMessageFilterSet::const_iterator it =
        pending_capture_monitoring_filters_.find(trace_message_filter);
    if (it != pending_capture_monitoring_filters_.end()) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::Bind(&TracingControllerImpl::OnCaptureMonitoringSnapshotAcked,
                     base::Unretained(this),
                     make_scoped_refptr(trace_message_filter)));
    }
  }
  if (pending_trace_log_status_ack_count_ > 0) {
    TraceMessageFilterSet::const_iterator it =
        pending_trace_log_status_filters_.find(trace_message_filter);
    if (it != pending_trace_log_status_filters_.end()) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&TracingControllerImpl::OnTraceLogStatusReply,
                     base::Unretained(this),
                     make_scoped_refptr(trace_message_filter),
                     base::trace_event::TraceLogStatus()));
    }
  }
  if (pending_memory_dump_ack_count_ > 0) {
    TraceMessageFilterSet::const_iterator it =
        pending_memory_dump_filters_.find(trace_message_filter);
    if (it != pending_memory_dump_filters_.end()) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&TracingControllerImpl::OnProcessMemoryDumpResponse,
                     base::Unretained(this),
                     make_scoped_refptr(trace_message_filter),
                     pending_memory_dump_guid_, false /* success */));
    }
  }
  trace_message_filters_.erase(trace_message_filter);
}

void TracingControllerImpl::OnDisableRecordingAcked(
    TraceMessageFilter* trace_message_filter,
    const std::vector<std::string>& known_category_groups) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnDisableRecordingAcked,
                   base::Unretained(this),
                   make_scoped_refptr(trace_message_filter),
                   known_category_groups));
    return;
  }

  // Merge known_category_groups with known_category_groups_
  known_category_groups_.insert(known_category_groups.begin(),
                                known_category_groups.end());

  if (pending_disable_recording_ack_count_ == 0)
    return;

  if (trace_message_filter &&
      !pending_disable_recording_filters_.erase(trace_message_filter)) {
    // The response from the specified message filter has already been received.
    return;
  }

  if (--pending_disable_recording_ack_count_ == 1) {
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

  if (pending_disable_recording_ack_count_ != 0)
    return;

  // All acks (including from the subprocesses and the local trace) have been
  // received.
  is_recording_ = false;

  // Trigger callback if one is set.
  if (!pending_get_categories_done_callback_.is_null()) {
    pending_get_categories_done_callback_.Run(known_category_groups_);
    pending_get_categories_done_callback_.Reset();
  } else if (trace_data_sink_.get()) {
    trace_data_sink_->Close();
    trace_data_sink_ = NULL;
  }
}

void TracingControllerImpl::OnEndPowerTracingAcked(
    const scoped_refptr<base::RefCountedString>& events_str_ptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (trace_data_sink_.get()) {
    std::string json_string = base::GetQuotedJSONString(events_str_ptr->data());
    trace_data_sink_->SetPowerTrace(json_string);
  }
  std::vector<std::string> category_groups;
  OnDisableRecordingAcked(NULL, category_groups);
}

#if defined(OS_CHROMEOS) || defined(OS_WIN)
void TracingControllerImpl::OnEndSystemTracingAcked(
    const scoped_refptr<base::RefCountedString>& events_str_ptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (trace_data_sink_.get()) {
#if defined(OS_WIN)
    // The Windows kernel events are kept into a JSon format stored as string
    // and must not be escaped.
    std::string json_string = events_str_ptr->data();
#else
    std::string json_string =
        base::GetQuotedJSONString(events_str_ptr->data());
#endif
    trace_data_sink_->SetSystemTrace(json_string);
  }
  DCHECK(!is_system_tracing_);
  std::vector<std::string> category_groups;
  OnDisableRecordingAcked(NULL, category_groups);
}
#endif

void TracingControllerImpl::OnCaptureMonitoringSnapshotAcked(
    TraceMessageFilter* trace_message_filter) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnCaptureMonitoringSnapshotAcked,
                   base::Unretained(this),
                   make_scoped_refptr(trace_message_filter)));
    return;
  }

  if (pending_capture_monitoring_snapshot_ack_count_ == 0)
    return;

  if (trace_message_filter &&
      !pending_capture_monitoring_filters_.erase(trace_message_filter)) {
    // The response from the specified message filter has already been received.
    return;
  }

  if (--pending_capture_monitoring_snapshot_ack_count_ == 1) {
    // All acks from subprocesses have been received. Now flush the local trace.
    // During or after this call, our OnLocalMonitoringTraceDataCollected
    // will be called with the last of the local trace data.
    TraceLog::GetInstance()->FlushButLeaveBufferIntact(
        base::Bind(&TracingControllerImpl::OnLocalMonitoringTraceDataCollected,
                   base::Unretained(this)));
    return;
  }

  if (pending_capture_monitoring_snapshot_ack_count_ != 0)
    return;

  if (monitoring_data_sink_.get()) {
    monitoring_data_sink_->Close();
    monitoring_data_sink_ = NULL;
  }
}

void TracingControllerImpl::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str_ptr) {
  // OnTraceDataCollected may be called from any browser thread, either by the
  // local event trace system or from child processes via TraceMessageFilter.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnTraceDataCollected,
                   base::Unretained(this), events_str_ptr));
    return;
  }

  if (trace_data_sink_.get())
    trace_data_sink_->AddTraceChunk(events_str_ptr->data());
}

void TracingControllerImpl::OnMonitoringTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str_ptr) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnMonitoringTraceDataCollected,
                   base::Unretained(this), events_str_ptr));
    return;
  }

  if (monitoring_data_sink_.get())
    monitoring_data_sink_->AddTraceChunk(events_str_ptr->data());
}

void TracingControllerImpl::OnLocalTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str_ptr,
    bool has_more_events) {
  if (events_str_ptr->data().size())
    OnTraceDataCollected(events_str_ptr);

  if (has_more_events)
    return;

  // Simulate an DisableRecordingAcked for the local trace.
  std::vector<std::string> category_groups;
  TraceLog::GetInstance()->GetKnownCategoryGroups(&category_groups);
  OnDisableRecordingAcked(NULL, category_groups);
}

void TracingControllerImpl::OnLocalMonitoringTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str_ptr,
    bool has_more_events) {
  if (events_str_ptr->data().size())
    OnMonitoringTraceDataCollected(events_str_ptr);

  if (has_more_events)
    return;

  // Simulate an CaptureMonitoringSnapshotAcked for the local trace.
  OnCaptureMonitoringSnapshotAcked(NULL);
}

void TracingControllerImpl::OnTraceLogStatusReply(
    TraceMessageFilter* trace_message_filter,
    const base::trace_event::TraceLogStatus& status) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnTraceLogStatusReply,
                   base::Unretained(this),
                   make_scoped_refptr(trace_message_filter), status));
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

void TracingControllerImpl::OnWatchEventMatched() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnWatchEventMatched,
                   base::Unretained(this)));
    return;
  }

  if (!watch_event_callback_.is_null())
    watch_event_callback_.Run();
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

void TracingControllerImpl::RequestGlobalMemoryDump(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const base::trace_event::MemoryDumpCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::RequestGlobalMemoryDump,
                   base::Unretained(this), args, callback));
    return;
  }
  // Abort if another dump is already in progress.
  if (pending_memory_dump_guid_) {
    DVLOG(1) << "Requested memory dump " << args.dump_guid
             << " while waiting for " << pending_memory_dump_guid_;
    if (!callback.is_null())
      callback.Run(args.dump_guid, false /* success */);
    return;
  }

  // Count myself (local trace) in pending_memory_dump_ack_count_, acked by
  // OnBrowserProcessMemoryDumpDone().
  pending_memory_dump_ack_count_ = trace_message_filters_.size() + 1;
  pending_memory_dump_filters_.clear();
  pending_memory_dump_guid_ = args.dump_guid;
  pending_memory_dump_callback_ = callback;
  failed_memory_dump_count_ = 0;

  MemoryDumpManagerDelegate::CreateProcessDump(
      args, base::Bind(&TracingControllerImpl::OnBrowserProcessMemoryDumpDone,
                       base::Unretained(this)));

  // If there are no child processes we are just done.
  if (pending_memory_dump_ack_count_ == 1)
    return;

  pending_memory_dump_filters_ = trace_message_filters_;

  for (const scoped_refptr<TraceMessageFilter>& tmf : trace_message_filters_)
    tmf->SendProcessMemoryDumpRequest(args);
}

bool TracingControllerImpl::IsCoordinatorProcess() const {
  return true;
}

uint64 TracingControllerImpl::GetTracingProcessId() const {
  return ChildProcessHost::kBrowserTracingProcessId;
}

void TracingControllerImpl::SetTraceMessageFilterAddedCallback(
    const TraceMessageFilterAddedCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  trace_message_filter_added_callback_ = callback;
}

void TracingControllerImpl::GetTraceMessageFilters(
    TraceMessageFilterSet* filters) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  filters->insert(trace_message_filters_.begin(), trace_message_filters_.end());
}

void TracingControllerImpl::OnProcessMemoryDumpResponse(
    TraceMessageFilter* trace_message_filter,
    uint64 dump_guid,
    bool success) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnProcessMemoryDumpResponse,
                   base::Unretained(this),
                   make_scoped_refptr(trace_message_filter), dump_guid,
                   success));
    return;
  }

  TraceMessageFilterSet::iterator it =
      pending_memory_dump_filters_.find(trace_message_filter);

  if (pending_memory_dump_guid_ != dump_guid ||
      it == pending_memory_dump_filters_.end()) {
    DLOG(WARNING) << "Received unexpected memory dump response: " << dump_guid;
    return;
  }

  DCHECK_GT(pending_memory_dump_ack_count_, 0);
  --pending_memory_dump_ack_count_;
  pending_memory_dump_filters_.erase(it);
  if (!success) {
    ++failed_memory_dump_count_;
    DLOG(WARNING) << "Global memory dump failed because of NACK from child "
                  << trace_message_filter->peer_pid();
  }
  FinalizeGlobalMemoryDumpIfAllProcessesReplied();
}

void TracingControllerImpl::OnBrowserProcessMemoryDumpDone(uint64 dump_guid,
                                                           bool success) {
  DCHECK_GT(pending_memory_dump_ack_count_, 0);
  --pending_memory_dump_ack_count_;
  if (!success) {
    ++failed_memory_dump_count_;
    DLOG(WARNING) << "Global memory dump aborted on the current process";
  }
  FinalizeGlobalMemoryDumpIfAllProcessesReplied();
}

void TracingControllerImpl::FinalizeGlobalMemoryDumpIfAllProcessesReplied() {
  if (pending_memory_dump_ack_count_ > 0)
    return;

  DCHECK_NE(0u, pending_memory_dump_guid_);
  const bool global_success = failed_memory_dump_count_ == 0;
  if (!pending_memory_dump_callback_.is_null()) {
    pending_memory_dump_callback_.Run(pending_memory_dump_guid_,
                                      global_success);
    pending_memory_dump_callback_.Reset();
  }
  pending_memory_dump_guid_ = 0;
}

void TracingControllerImpl::OnMonitoringStateChanged(bool is_monitoring) {
  if (is_monitoring_ == is_monitoring)
    return;

  is_monitoring_ = is_monitoring;
#if !defined(OS_ANDROID)
  for (std::set<TracingUI*>::iterator it = tracing_uis_.begin();
       it != tracing_uis_.end(); it++) {
    (*it)->OnMonitoringStateChanged(is_monitoring);
  }
#endif
}

}  // namespace content
