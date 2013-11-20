// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/tracing_controller_impl.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/json/string_escape.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/tracing/trace_message_filter.h"
#include "content/common/child_process_messages.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/common/content_switches.h"

using base::debug::TraceLog;

namespace content {

namespace {

base::LazyInstance<TracingControllerImpl>::Leaky g_controller =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

TracingController* TracingController::GetInstance() {
  return TracingControllerImpl::GetInstance();
}

TracingControllerImpl::TracingControllerImpl() :
    pending_disable_recording_ack_count_(0),
    pending_capture_monitoring_snapshot_ack_count_(0),
    is_recording_(false),
    is_monitoring_(false),
    category_filter_(
        base::debug::CategoryFilter::kDefaultCategoryFilterString),
    result_file_(0),
    result_file_has_at_least_one_result_(false) {
}

TracingControllerImpl::~TracingControllerImpl() {
  // This is a Leaky instance.
  NOTREACHED();
}

TracingControllerImpl* TracingControllerImpl::GetInstance() {
  return g_controller.Pointer();
}

void TracingControllerImpl::GetCategories(
    const GetCategoriesDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Known categories come back from child processes with the EndTracingAck
  // message. So to get known categories, just begin and end tracing immediately
  // afterwards. This will ping all the child processes for categories.
  pending_get_categories_done_callback_ = callback;
  EnableRecording(base::debug::CategoryFilter("*"),
                  TracingController::Options(),
                  EnableRecordingDoneCallback());
  DisableRecording(TracingFileResultCallback());
}

bool TracingControllerImpl::EnableRecording(
    const base::debug::CategoryFilter& filter,
    TracingController::Options options,
    const EnableRecordingDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_enable_recording())
    return false;

#if defined(OS_ANDROID)
  if (pending_get_categories_done_callback_.is_null())
    TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif

  trace_options_ = TraceLog::GetInstance()->trace_options();
  TraceLog::GetInstance()->SetEnabled(filter, trace_options_);

  is_recording_ = true;
  category_filter_ = TraceLog::GetInstance()->GetCurrentCategoryFilter();

  // Notify all child processes.
  for (FilterMap::iterator it = filters_.begin(); it != filters_.end(); ++it) {
    it->get()->SendBeginTracing(
        category_filter_.ToString(), trace_options_, false);
  }

  if (!callback.is_null())
    callback.Run();
  return true;
}

bool TracingControllerImpl::DisableRecording(
    const TracingFileResultCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_disable_recording())
    return false;

  pending_disable_recording_done_callback_ = callback;

  // Disable local trace early to avoid traces during end-tracing process from
  // interfering with the process.
  TraceLog::GetInstance()->SetDisabled();

#if defined(OS_ANDROID)
  if (pending_get_categories_done_callback_.is_null())
    TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif

  // We don't need to create a temporary file when getting categories.
  if (pending_get_categories_done_callback_.is_null()) {
    base::FilePath temporary_file;
    file_util::CreateTemporaryFile(&temporary_file);
    result_file_path_.reset(new base::FilePath(temporary_file));
    result_file_ = file_util::OpenFile(*result_file_path_, "w");
    result_file_has_at_least_one_result_ = false;
    const char* preamble = "{\"traceEvents\": [";
    size_t written = fwrite(preamble, strlen(preamble), 1, result_file_);
    DCHECK(written == 1);
  }

  // There could be a case where there are no child processes and filters_
  // is empty. In that case we can immediately tell the subscriber that tracing
  // has ended. To avoid recursive calls back to the subscriber, we will just
  // use the existing asynchronous OnDisableRecordingAcked code.
  // Count myself (local trace) in pending_disable_recording_ack_count_,
  // acked below.
  pending_disable_recording_ack_count_ = filters_.size() + 1;

  // Handle special case of zero child processes.
  if (pending_disable_recording_ack_count_ == 1) {
    // Ack asynchronously now, because we don't have any children to wait for.
    std::vector<std::string> category_groups;
    TraceLog::GetInstance()->GetKnownCategoryGroups(&category_groups);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnDisableRecordingAcked,
                   base::Unretained(this), category_groups));
  }

  // Notify all child processes.
  for (FilterMap::iterator it = filters_.begin(); it != filters_.end(); ++it) {
    it->get()->SendEndTracing();
  }
  return true;
}

bool TracingControllerImpl::EnableMonitoring(
    const base::debug::CategoryFilter& filter,
    TracingController::Options options,
    const EnableMonitoringDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_enable_monitoring())
    return false;
  is_monitoring_ = true;

#if defined(OS_ANDROID)
  TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif

  int monitoring_tracing_options = 0;
  if (options & ENABLE_SAMPLING)
    monitoring_tracing_options |= base::debug::TraceLog::MONITOR_SAMPLING;

  // Notify all child processes.
  for (FilterMap::iterator it = filters_.begin(); it != filters_.end(); ++it) {
    it->get()->SendEnableMonitoring(filter.ToString(),
        base::debug::TraceLog::Options(monitoring_tracing_options));
  }

  if (!callback.is_null())
    callback.Run();
  return true;
}

bool TracingControllerImpl::DisableMonitoring(
    const DisableMonitoringDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_disable_monitoring())
    return false;
  is_monitoring_ = false;

  // Notify all child processes.
  for (FilterMap::iterator it = filters_.begin(); it != filters_.end(); ++it) {
    it->get()->SendDisableMonitoring();
  }

  if (!callback.is_null())
    callback.Run();
  return true;
}

void TracingControllerImpl::GetMonitoringStatus(
    bool* out_enabled,
    base::debug::CategoryFilter* out_filter,
    TracingController::Options* out_options) {
  NOTIMPLEMENTED();
}

void TracingControllerImpl::CaptureMonitoringSnapshot(
    const TracingFileResultCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_disable_monitoring())
    return;

  pending_capture_monitoring_snapshot_done_callback_ = callback;

  base::FilePath temporary_file;
  file_util::CreateTemporaryFile(&temporary_file);
  result_file_path_.reset(new base::FilePath(temporary_file));
  result_file_ = file_util::OpenFile(*result_file_path_, "w");
  result_file_has_at_least_one_result_ = false;
  const char* preamble = "{\"traceEvents\": [";
  size_t written = fwrite(preamble, strlen(preamble), 1, result_file_);
  DCHECK(written == 1);

  // There could be a case where there are no child processes and filters_
  // is empty. In that case we can immediately tell the subscriber that tracing
  // has ended. To avoid recursive calls back to the subscriber, we will just
  // use the existing asynchronous OnCaptureMonitoringSnapshotAcked code.
  // Count myself in pending_capture_monitoring_snapshot_ack_count_,
  // acked below.
  pending_capture_monitoring_snapshot_ack_count_ = filters_.size() + 1;

  // Handle special case of zero child processes.
  if (pending_capture_monitoring_snapshot_ack_count_ == 1) {
    // Ack asynchronously now, because we don't have any children to wait for.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnCaptureMonitoringSnapshotAcked,
                   base::Unretained(this)));
  }

  // Notify all child processes.
  for (FilterMap::iterator it = filters_.begin(); it != filters_.end(); ++it) {
    it->get()->SendCaptureMonitoringSnapshot();
  }

#if defined(OS_ANDROID)
  TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif
}

void TracingControllerImpl::AddFilter(TraceMessageFilter* filter) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::AddFilter, base::Unretained(this),
                   make_scoped_refptr(filter)));
    return;
  }

  filters_.insert(filter);
  if (can_disable_recording()) {
    std::string cf_str = category_filter_.ToString();
    filter->SendBeginTracing(cf_str, trace_options_, false);
  }
}

void TracingControllerImpl::RemoveFilter(TraceMessageFilter* filter) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::RemoveFilter, base::Unretained(this),
                   make_scoped_refptr(filter)));
    return;
  }

  filters_.erase(filter);
}

void TracingControllerImpl::OnDisableRecordingAcked(
    const std::vector<std::string>& known_category_groups) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnDisableRecordingAcked,
                   base::Unretained(this), known_category_groups));
    return;
  }

  // Merge known_category_groups with known_category_groups_
  known_category_groups_.insert(known_category_groups.begin(),
                                known_category_groups.end());

  if (pending_disable_recording_ack_count_ == 0)
    return;

  if (--pending_disable_recording_ack_count_ == 1) {
    // All acks from subprocesses have been received. Now flush the local trace.
    // During or after this call, our OnLocalTraceDataCollected will be
    // called with the last of the local trace data.
    TraceLog::GetInstance()->Flush(
        base::Bind(&TracingControllerImpl::OnLocalTraceDataCollected,
                   base::Unretained(this)));
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
  } else if (!pending_disable_recording_done_callback_.is_null()) {
    const char* trailout = "]}";
    size_t written = fwrite(trailout, strlen(trailout), 1, result_file_);
    DCHECK(written == 1);
    file_util::CloseFile(result_file_);
    result_file_ = 0;
    pending_disable_recording_done_callback_.Run(result_file_path_.Pass());
    pending_disable_recording_done_callback_.Reset();
  }
}

void TracingControllerImpl::OnCaptureMonitoringSnapshotAcked() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnCaptureMonitoringSnapshotAcked,
                   base::Unretained(this)));
    return;
  }

  if (pending_capture_monitoring_snapshot_ack_count_ == 0)
    return;

  if (--pending_capture_monitoring_snapshot_ack_count_ == 1) {
    // All acks from subprocesses have been received. Now flush the local trace.
    // During or after this call, our OnLocalMonitoringTraceDataCollected
    // will be called with the last of the local trace data.
    TraceLog::GetInstance()->FlushButLeaveBufferIntact(
        base::Bind(&TracingControllerImpl::OnLocalMonitoringTraceDataCollected,
                   base::Unretained(this)));
  }

  if (pending_capture_monitoring_snapshot_ack_count_ != 0)
    return;

  if (!pending_capture_monitoring_snapshot_done_callback_.is_null()) {
    const char* trailout = "]}";
    size_t written = fwrite(trailout, strlen(trailout), 1, result_file_);
    DCHECK(written == 1);
    file_util::CloseFile(result_file_);
    result_file_ = 0;
    pending_capture_monitoring_snapshot_done_callback_.Run(
        result_file_path_.Pass());
    pending_capture_monitoring_snapshot_done_callback_.Reset();
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

  // Drop trace events if we are just getting categories.
  if (!pending_get_categories_done_callback_.is_null())
    return;

  // If there is already a result in the file, then put a commma
  // before the next batch of results.
  if (result_file_has_at_least_one_result_) {
    size_t written = fwrite(",", 1, 1, result_file_);
    DCHECK(written == 1);
  } else {
    result_file_has_at_least_one_result_ = true;
  }
  size_t written = fwrite(events_str_ptr->data().c_str(),
                          events_str_ptr->data().size(), 1,
                          result_file_);
  DCHECK(written == 1);
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
  OnDisableRecordingAcked(category_groups);
}

void TracingControllerImpl::OnLocalMonitoringTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str_ptr,
    bool has_more_events) {
  if (events_str_ptr->data().size())
    OnTraceDataCollected(events_str_ptr);

  if (has_more_events)
    return;

  // Simulate an CaptureMonitoringSnapshotAcked for the local trace.
  OnCaptureMonitoringSnapshotAcked();
}

}  // namespace content
