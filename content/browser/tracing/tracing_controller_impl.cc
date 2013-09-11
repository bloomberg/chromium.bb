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
    pending_end_ack_count_(0),
    is_recording_(false),
    category_filter_(
        base::debug::CategoryFilter::kDefaultCategoryFilterString) {
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

void TracingControllerImpl::EnableRecording(
    const base::debug::CategoryFilter& filter,
    TracingController::Options options,
    const EnableRecordingDoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_enable_recording())
    return;

  trace_options_ = TraceLog::GetInstance()->trace_options();
  TraceLog::GetInstance()->SetEnabled(filter, trace_options_);

  is_recording_ = true;
  category_filter_ = TraceLog::GetInstance()->GetCurrentCategoryFilter();

  // Notify all child processes.
  for (FilterMap::iterator it = filters_.begin(); it != filters_.end(); ++it) {
    it->get()->SendBeginTracing(category_filter_.ToString(), trace_options_);
  }

  if (!callback.is_null())
    callback.Run();
}

void TracingControllerImpl::DisableRecording(
    const TracingFileResultCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!can_end_recording())
    return;

  pending_disable_recording_done_callback_ = callback;

  // Disable local trace early to avoid traces during end-tracing process from
  // interfering with the process.
  TraceLog::GetInstance()->SetDisabled();

  // We don't need to create a temporary file when getting categories.
  if (pending_get_categories_done_callback_.is_null()) {
    base::FilePath temporary_file;
    file_util::CreateTemporaryFile(&temporary_file);
    recording_result_file_.reset(new base::FilePath(temporary_file));
  }

  // There could be a case where there are no child processes and filters_
  // is empty. In that case we can immediately tell the subscriber that tracing
  // has ended. To avoid recursive calls back to the subscriber, we will just
  // use the existing asynchronous OnDisableRecordingAcked code.
  // Count myself (local trace) in pending_end_ack_count_, acked below.
  pending_end_ack_count_ = filters_.size() + 1;

  // Handle special case of zero child processes.
  if (pending_end_ack_count_ == 1) {
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
}

void TracingControllerImpl::EnableMonitoring(
    const base::debug::CategoryFilter& filter,
    TracingController::Options options,
    const EnableMonitoringDoneCallback& callback) {
  NOTIMPLEMENTED();
}

void TracingControllerImpl::DisableMonitoring(
    const DisableMonitoringDoneCallback& callback) {
  NOTIMPLEMENTED();
}

void TracingControllerImpl::GetMonitoringStatus(
    bool* out_enabled,
    base::debug::CategoryFilter* out_filter,
    TracingController::Options* out_options) {
  NOTIMPLEMENTED();
}

void TracingControllerImpl::CaptureCurrentMonitoringSnapshot(
    const TracingFileResultCallback& callback) {
  NOTIMPLEMENTED();
}

void TracingControllerImpl::AddFilter(TraceMessageFilter* filter) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&TracingControllerImpl::AddFilter, base::Unretained(this),
                   make_scoped_refptr(filter)));
    return;
  }

  filters_.insert(filter);
  if (is_recording_enabled()) {
    std::string cf_str = category_filter_.ToString();
    filter->SendBeginTracing(cf_str, trace_options_);
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

  if (pending_end_ack_count_ == 0)
    return;

  if (--pending_end_ack_count_ == 1) {
    // All acks from subprocesses have been received. Now flush the local trace.
    // During or after this call, our OnLocalTraceDataCollected will be
    // called with the last of the local trace data.
    TraceLog::GetInstance()->Flush(
        base::Bind(&TracingControllerImpl::OnLocalTraceDataCollected,
                   base::Unretained(this)));
  }

  if (pending_end_ack_count_ != 0)
    return;

  // All acks (including from the subprocesses and the local trace) have been
  // received.
  is_recording_ = false;

  // Trigger callback if one is set.
  if (!pending_get_categories_done_callback_.is_null()) {
    pending_get_categories_done_callback_.Run(known_category_groups_);
    pending_get_categories_done_callback_.Reset();
  } else {
    OnEndTracingComplete();
  }
}

void TracingControllerImpl::OnEndTracingComplete() {
  if (pending_disable_recording_done_callback_.is_null())
    return;

  pending_disable_recording_done_callback_.Run(recording_result_file_.Pass());
  pending_disable_recording_done_callback_.Reset();
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

  std::string javascript;
  javascript.reserve(events_str_ptr->size() * 2);
  base::JsonDoubleQuote(events_str_ptr->data(), false, &javascript);

  // Intentionally append a , to the traceData. This technically causes all
  // traceData that we pass back to JS to end with a comma, but that is
  // actually something the JS side strips away anyway
  javascript.append(",");

  file_util::WriteFile(*recording_result_file_,
                       javascript.c_str(), javascript.length());
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

}  // namespace content
