// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/tracing_controller_impl.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
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

class TracingControllerImpl::ResultFile {
 public:
  explicit ResultFile(const base::FilePath& path);
  void Write(const scoped_refptr<base::RefCountedString>& events_str_ptr) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&TracingControllerImpl::ResultFile::WriteTask,
                   base::Unretained(this), events_str_ptr));
  }
  void Close(const base::Closure& callback) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&TracingControllerImpl::ResultFile::CloseTask,
                   base::Unretained(this), callback));
  }
  const base::FilePath& path() const { return path_; }

 private:
  void OpenTask();
  void WriteTask(const scoped_refptr<base::RefCountedString>& events_str_ptr);
  void CloseTask(const base::Closure& callback);

  FILE* file_;
  base::FilePath path_;
  bool has_at_least_one_result_;

  DISALLOW_COPY_AND_ASSIGN(ResultFile);
};

TracingControllerImpl::ResultFile::ResultFile(const base::FilePath& path)
    : file_(NULL),
      path_(path),
      has_at_least_one_result_(false) {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&TracingControllerImpl::ResultFile::OpenTask,
                 base::Unretained(this)));
}

void TracingControllerImpl::ResultFile::OpenTask() {
  if (path_.empty())
    base::CreateTemporaryFile(&path_);
  file_ = base::OpenFile(path_, "w");
  if (!file_) {
    LOG(ERROR) << "Failed to open " << path_.value();
    return;
  }
  const char* preamble = "{\"traceEvents\": [";
  size_t written = fwrite(preamble, strlen(preamble), 1, file_);
  DCHECK(written == 1);
}

void TracingControllerImpl::ResultFile::WriteTask(
    const scoped_refptr<base::RefCountedString>& events_str_ptr) {
  if (!file_)
    return;

  // If there is already a result in the file, then put a commma
  // before the next batch of results.
  if (has_at_least_one_result_) {
    size_t written = fwrite(",", 1, 1, file_);
    DCHECK(written == 1);
  }
  has_at_least_one_result_ = true;
  size_t written = fwrite(events_str_ptr->data().c_str(),
                          events_str_ptr->data().size(), 1,
                          file_);
  DCHECK(written == 1);
}

void TracingControllerImpl::ResultFile::CloseTask(
    const base::Closure& callback) {
  if (!file_)
    return;

  const char* trailout = "]}";
  size_t written = fwrite(trailout, strlen(trailout), 1, file_);
  DCHECK(written == 1);
  base::CloseFile(file_);
  file_ = NULL;

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}


TracingControllerImpl::TracingControllerImpl() :
    pending_disable_recording_ack_count_(0),
    pending_capture_monitoring_snapshot_ack_count_(0),
    pending_trace_buffer_percent_full_ack_count_(0),
    maximum_trace_buffer_percent_full_(0),
    // Tracing may have been enabled by ContentMainRunner if kTraceStartup
    // is specified in command line.
    is_recording_(TraceLog::GetInstance()->IsEnabled()),
    is_monitoring_(false) {
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Known categories come back from child processes with the EndTracingAck
  // message. So to get known categories, just begin and end tracing immediately
  // afterwards. This will ping all the child processes for categories.
  pending_get_categories_done_callback_ = callback;
  if (!EnableRecording("*", TracingController::Options(),
                       EnableRecordingDoneCallback())) {
    pending_get_categories_done_callback_.Reset();
    return false;
  }

  bool ok = DisableRecording(base::FilePath(), TracingFileResultCallback());
  DCHECK(ok);
  return true;
}

bool TracingControllerImpl::EnableRecording(
    const std::string& category_filter,
    TracingController::Options options,
    const EnableRecordingDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_enable_recording())
    return false;

#if defined(OS_ANDROID)
  if (pending_get_categories_done_callback_.is_null())
    TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif

  TraceLog::Options trace_options = (options & RECORD_CONTINUOUSLY) ?
      TraceLog::RECORD_CONTINUOUSLY : TraceLog::RECORD_UNTIL_FULL;
  if (options & ENABLE_SAMPLING) {
    trace_options = static_cast<TraceLog::Options>(
        trace_options | TraceLog::ENABLE_SAMPLING);
  }
  // TODO(haraken): How to handle ENABLE_SYSTRACE?

  TraceLog::GetInstance()->SetEnabled(
      base::debug::CategoryFilter(category_filter), trace_options);
  is_recording_ = true;

  // Notify all child processes.
  for (TraceMessageFilterMap::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendBeginTracing(category_filter, trace_options);
  }

  if (!callback.is_null())
    callback.Run();
  return true;
}

bool TracingControllerImpl::DisableRecording(
    const base::FilePath& result_file_path,
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

  if (!callback.is_null() || !result_file_path.empty())
    result_file_.reset(new ResultFile(result_file_path));

  // Count myself (local trace) in pending_disable_recording_ack_count_,
  // acked below.
  pending_disable_recording_ack_count_ = trace_message_filters_.size() + 1;

  // Handle special case of zero child processes by immediately telling the
  // caller that tracing has ended. Use asynchronous OnDisableRecordingAcked
  // to avoid recursive call back to the caller.
  if (pending_disable_recording_ack_count_ == 1) {
    // Ack asynchronously now, because we don't have any children to wait for.
    std::vector<std::string> category_groups;
    TraceLog::GetInstance()->GetKnownCategoryGroups(&category_groups);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnDisableRecordingAcked,
                   base::Unretained(this), category_groups));
  }

  // Notify all child processes.
  for (TraceMessageFilterMap::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendEndTracing();
  }
  return true;
}

bool TracingControllerImpl::EnableMonitoring(
    const std::string& category_filter,
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

  TraceLog::GetInstance()->SetEnabled(
      base::debug::CategoryFilter(category_filter),
      static_cast<TraceLog::Options>(monitoring_tracing_options));

  // Notify all child processes.
  for (TraceMessageFilterMap::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendEnableMonitoring(category_filter,
        static_cast<TraceLog::Options>(monitoring_tracing_options));
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

  TraceLog::GetInstance()->SetDisabled();

  // Notify all child processes.
  for (TraceMessageFilterMap::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendDisableMonitoring();
  }

  if (!callback.is_null())
    callback.Run();
  return true;
}

void TracingControllerImpl::GetMonitoringStatus(
    bool* out_enabled,
    std::string* out_category_filter,
    TracingController::Options* out_options) {
  NOTIMPLEMENTED();
}

bool TracingControllerImpl::CaptureMonitoringSnapshot(
    const base::FilePath& result_file_path,
    const TracingFileResultCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_disable_monitoring())
    return false;

  if (callback.is_null() && result_file_path.empty())
    return false;

  pending_capture_monitoring_snapshot_done_callback_ = callback;
  monitoring_snapshot_file_.reset(new ResultFile(result_file_path));

  // Count myself in pending_capture_monitoring_snapshot_ack_count_,
  // acked below.
  pending_capture_monitoring_snapshot_ack_count_ =
      trace_message_filters_.size() + 1;

  // Handle special case of zero child processes by immediately telling the
  // caller that capturing snapshot has ended. Use asynchronous
  // OnCaptureMonitoringSnapshotAcked to avoid recursive call back to the
  // caller.
  if (pending_capture_monitoring_snapshot_ack_count_ == 1) {
    // Ack asynchronously now, because we don't have any children to wait for.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnCaptureMonitoringSnapshotAcked,
                   base::Unretained(this)));
  }

  // Notify all child processes.
  for (TraceMessageFilterMap::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendCaptureMonitoringSnapshot();
  }

#if defined(OS_ANDROID)
  TraceLog::GetInstance()->AddClockSyncMetadataEvent();
#endif

  return true;
}

bool TracingControllerImpl::GetTraceBufferPercentFull(
    const GetTraceBufferPercentFullCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_get_trace_buffer_percent_full() || callback.is_null())
    return false;

  pending_trace_buffer_percent_full_callback_ = callback;

  // Count myself in pending_trace_buffer_percent_full_ack_count_, acked below.
  pending_trace_buffer_percent_full_ack_count_ =
      trace_message_filters_.size() + 1;
  maximum_trace_buffer_percent_full_ = 0;

  // Handle special case of zero child processes.
  if (pending_trace_buffer_percent_full_ack_count_ == 1) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnTraceBufferPercentFullReply,
                   base::Unretained(this),
                   TraceLog::GetInstance()->GetBufferPercentFull()));
  }

  // Notify all child processes.
  for (TraceMessageFilterMap::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendGetTraceBufferPercentFull();
  }
  return true;
}

bool TracingControllerImpl::SetWatchEvent(
    const std::string& category_name,
    const std::string& event_name,
    const WatchEventCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (callback.is_null())
    return false;

  watch_category_name_ = category_name;
  watch_event_name_ = event_name;
  watch_event_callback_ = callback;

  TraceLog::GetInstance()->SetWatchEvent(
      category_name, event_name,
      base::Bind(&TracingControllerImpl::OnWatchEventMatched,
                 base::Unretained(this)));

  for (TraceMessageFilterMap::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendSetWatchEvent(category_name, event_name);
  }
  return true;
}

bool TracingControllerImpl::CancelWatchEvent() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_cancel_watch_event())
    return false;

  for (TraceMessageFilterMap::iterator it = trace_message_filters_.begin();
      it != trace_message_filters_.end(); ++it) {
    it->get()->SendCancelWatchEvent();
  }

  watch_event_callback_.Reset();
  return true;
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
        TraceLog::GetInstance()->GetCurrentCategoryFilter().ToString(),
        TraceLog::GetInstance()->trace_options());
  }
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

  trace_message_filters_.erase(trace_message_filter);
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
  } else if (result_file_) {
    result_file_->Close(
        base::Bind(&TracingControllerImpl::OnResultFileClosed,
                   base::Unretained(this)));
  }
}

void TracingControllerImpl::OnResultFileClosed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!result_file_)
    return;

  if (!pending_disable_recording_done_callback_.is_null()) {
    pending_disable_recording_done_callback_.Run(result_file_->path());
    pending_disable_recording_done_callback_.Reset();
  }
  result_file_.reset();
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
    return;
  }

  if (pending_capture_monitoring_snapshot_ack_count_ != 0)
    return;

  if (monitoring_snapshot_file_) {
    monitoring_snapshot_file_->Close(
        base::Bind(&TracingControllerImpl::OnMonitoringSnapshotFileClosed,
                   base::Unretained(this)));
  }
}

void TracingControllerImpl::OnMonitoringSnapshotFileClosed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!monitoring_snapshot_file_)
    return;

  if (!pending_capture_monitoring_snapshot_done_callback_.is_null()) {
    pending_capture_monitoring_snapshot_done_callback_.Run(
        monitoring_snapshot_file_->path());
    pending_capture_monitoring_snapshot_done_callback_.Reset();
  }
  monitoring_snapshot_file_.reset();
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

  if (result_file_)
    result_file_->Write(events_str_ptr);
}

void TracingControllerImpl::OnMonitoringTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str_ptr) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnMonitoringTraceDataCollected,
                   base::Unretained(this), events_str_ptr));
    return;
  }

  if (monitoring_snapshot_file_)
    monitoring_snapshot_file_->Write(events_str_ptr);
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
    OnMonitoringTraceDataCollected(events_str_ptr);

  if (has_more_events)
    return;

  // Simulate an CaptureMonitoringSnapshotAcked for the local trace.
  OnCaptureMonitoringSnapshotAcked();
}

void TracingControllerImpl::OnTraceBufferPercentFullReply(float percent_full) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnTraceBufferPercentFullReply,
                   base::Unretained(this), percent_full));
    return;
  }

  if (pending_trace_buffer_percent_full_ack_count_ == 0)
    return;

  maximum_trace_buffer_percent_full_ =
      std::max(maximum_trace_buffer_percent_full_, percent_full);

  if (--pending_trace_buffer_percent_full_ack_count_ == 0) {
    // Trigger callback if one is set.
    pending_trace_buffer_percent_full_callback_.Run(
        maximum_trace_buffer_percent_full_);
    pending_trace_buffer_percent_full_callback_.Reset();
  }

  if (pending_trace_buffer_percent_full_ack_count_ == 1) {
    // The last ack represents local trace, so we need to ack it now. Note that
    // this code only executes if there were child processes.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::OnTraceBufferPercentFullReply,
                   base::Unretained(this),
                   TraceLog::GetInstance()->GetBufferPercentFull()));
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

}  // namespace content
