// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "content/public/browser/tracing_controller.h"

namespace base {
class RefCountedString;
class RefCountedMemory;
}

namespace content {

class TraceMessageFilter;
class TracingUI;

class TracingControllerImpl : public TracingController {
 public:
  static TracingControllerImpl* GetInstance();

  // TracingController implementation.
  bool GetCategories(const GetCategoriesDoneCallback& callback) override;
  bool EnableRecording(const base::trace_event::CategoryFilter& category_filter,
                       const base::trace_event::TraceOptions& trace_options,
                       const EnableRecordingDoneCallback& callback) override;
  bool DisableRecording(const scoped_refptr<TraceDataSink>& sink) override;
  bool EnableMonitoring(
      const base::trace_event::CategoryFilter& category_filter,
      const base::trace_event::TraceOptions& trace_options,
      const EnableMonitoringDoneCallback& callback) override;
  bool DisableMonitoring(
      const DisableMonitoringDoneCallback& callback) override;
  void GetMonitoringStatus(
      bool* out_enabled,
      base::trace_event::CategoryFilter* out_category_filter,
      base::trace_event::TraceOptions* out_trace_options) override;
  bool CaptureMonitoringSnapshot(
      const scoped_refptr<TraceDataSink>& sink) override;
  bool GetTraceBufferUsage(
      const GetTraceBufferUsageCallback& callback) override;
  bool SetWatchEvent(const std::string& category_name,
                     const std::string& event_name,
                     const WatchEventCallback& callback) override;
  bool CancelWatchEvent() override;

  void RegisterTracingUI(TracingUI* tracing_ui);
  void UnregisterTracingUI(TracingUI* tracing_ui);

 private:
  typedef std::set<scoped_refptr<TraceMessageFilter> > TraceMessageFilterSet;

  friend struct base::DefaultLazyInstanceTraits<TracingControllerImpl>;
  friend class TraceMessageFilter;

  TracingControllerImpl();
  ~TracingControllerImpl() override;

  bool can_enable_recording() const {
    return !is_recording_;
  }

  bool can_disable_recording() const {
    return is_recording_ && !trace_data_sink_.get();
  }

  bool can_enable_monitoring() const {
    return !is_monitoring_;
  }

  bool can_disable_monitoring() const {
    return is_monitoring_ && !monitoring_data_sink_.get();
  }

  bool can_get_trace_buffer_usage() const {
    return pending_trace_buffer_usage_callback_.is_null();
  }

  bool can_cancel_watch_event() const {
    return !watch_event_callback_.is_null();
  }

  // Methods for use by TraceMessageFilter.
  void AddTraceMessageFilter(TraceMessageFilter* trace_message_filter);
  void RemoveTraceMessageFilter(TraceMessageFilter* trace_message_filter);

  void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr);
  void OnMonitoringTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr);

  // Callback of TraceLog::Flush() for the local trace.
  void OnLocalTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr,
      bool has_more_events);
  // Callback of TraceLog::FlushMonitoring() for the local trace.
  void OnLocalMonitoringTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr,
      bool has_more_events);

  void OnDisableRecordingAcked(
      TraceMessageFilter* trace_message_filter,
      const std::vector<std::string>& known_category_groups);

#if defined(OS_CHROMEOS) || defined(OS_WIN)
  void OnEndSystemTracingAcked(
      const scoped_refptr<base::RefCountedString>& events_str_ptr);
#endif

  void OnCaptureMonitoringSnapshotAcked(
      TraceMessageFilter* trace_message_filter);

  void OnTraceLogStatusReply(TraceMessageFilter* trace_message_filter,
                             const base::trace_event::TraceLogStatus& status);

  void OnWatchEventMatched();

  void SetEnabledOnFileThread(
      const base::trace_event::CategoryFilter& category_filter,
      int mode,
      const base::trace_event::TraceOptions& trace_options,
      const base::Closure& callback);
  void SetDisabledOnFileThread(const base::Closure& callback);
  void OnEnableRecordingDone(
      const base::trace_event::CategoryFilter& category_filter,
      const base::trace_event::TraceOptions& trace_options,
      const EnableRecordingDoneCallback& callback);
  void OnDisableRecordingDone();
  void OnEnableMonitoringDone(
      const base::trace_event::CategoryFilter& category_filter,
      const base::trace_event::TraceOptions& trace_options,
      const EnableMonitoringDoneCallback& callback);
  void OnDisableMonitoringDone(const DisableMonitoringDoneCallback& callback);

  void OnMonitoringStateChanged(bool is_monitoring);

  TraceMessageFilterSet trace_message_filters_;

  // Pending acks for DisableRecording.
  int pending_disable_recording_ack_count_;
  TraceMessageFilterSet pending_disable_recording_filters_;
  // Pending acks for CaptureMonitoringSnapshot.
  int pending_capture_monitoring_snapshot_ack_count_;
  TraceMessageFilterSet pending_capture_monitoring_filters_;
  // Pending acks for GetTraceLogStatus.
  int pending_trace_log_status_ack_count_;
  TraceMessageFilterSet pending_trace_log_status_filters_;
  float maximum_trace_buffer_usage_;
  size_t approximate_event_count_;

#if defined(OS_CHROMEOS) || defined(OS_WIN)
  bool is_system_tracing_;
#endif
  bool is_recording_;
  bool is_monitoring_;
  base::trace_event::TraceOptions trace_options_;

  GetCategoriesDoneCallback pending_get_categories_done_callback_;
  GetTraceBufferUsageCallback pending_trace_buffer_usage_callback_;

  std::string watch_category_name_;
  std::string watch_event_name_;
  WatchEventCallback watch_event_callback_;

  std::set<std::string> known_category_groups_;
  std::set<TracingUI*> tracing_uis_;
  scoped_refptr<TraceDataSink> trace_data_sink_;
  scoped_refptr<TraceDataSink> monitoring_data_sink_;
  DISALLOW_COPY_AND_ASSIGN(TracingControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_
