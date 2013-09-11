// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "content/public/browser/trace_subscriber.h"
#include "content/public/browser/tracing_controller.h"

namespace content {

class TraceMessageFilter;

class TracingControllerImpl :
   public TracingController, public TraceSubscriber {
 public:
  static TracingControllerImpl* GetInstance();

  // TracingController implementation.
  virtual void GetCategories(
      const GetCategoriesDoneCallback& callback) OVERRIDE;
  virtual void EnableRecording(
      const base::debug::CategoryFilter& filter,
      TracingController::Options options,
      const EnableRecordingDoneCallback& callback) OVERRIDE;
  virtual void DisableRecording(
      const TracingFileResultCallback& callback) OVERRIDE;
  virtual void EnableMonitoring(const base::debug::CategoryFilter& filter,
      TracingController::Options options,
      const EnableMonitoringDoneCallback& callback) OVERRIDE;
  virtual void DisableMonitoring(
      const DisableMonitoringDoneCallback& callback) OVERRIDE;
  virtual void GetMonitoringStatus(
      bool* out_enabled,
      base::debug::CategoryFilter* out_filter,
      TracingController::Options* out_options) OVERRIDE;
  virtual void CaptureCurrentMonitoringSnapshot(
      const TracingFileResultCallback& callback) OVERRIDE;

 private:
  typedef std::set<scoped_refptr<TraceMessageFilter> > FilterMap;

  friend struct base::DefaultLazyInstanceTraits<TracingControllerImpl>;
  friend class TraceMessageFilter;

  TracingControllerImpl();
  virtual ~TracingControllerImpl();

  // TraceSubscriber implementation.
  virtual void OnEndTracingComplete() OVERRIDE;
  virtual void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr) OVERRIDE;

  bool can_enable_recording() const {
    return !is_recording_;
  }

  bool can_end_recording() const {
    return is_recording_ && pending_end_ack_count_ == 0;
  }

  bool is_recording_enabled() const {
    return can_end_recording();
  }

  // Methods for use by TraceMessageFilter.
  void AddFilter(TraceMessageFilter* filter);
  void RemoveFilter(TraceMessageFilter* filter);

  // Callback of TraceLog::Flush() for the local trace.
  void OnLocalTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr,
      bool has_more_events);

  void OnDisableRecordingAcked(
      const std::vector<std::string>& known_category_groups);

  FilterMap filters_;
  // Pending acks for DisableRecording.
  int pending_end_ack_count_;
  bool is_recording_;
  GetCategoriesDoneCallback pending_get_categories_done_callback_;
  TracingFileResultCallback pending_disable_recording_done_callback_;
  std::set<std::string> known_category_groups_;
  base::debug::TraceLog::Options trace_options_;
  base::debug::CategoryFilter category_filter_;
  scoped_ptr<base::FilePath> recording_result_file_;

  DISALLOW_COPY_AND_ASSIGN(TracingControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_
