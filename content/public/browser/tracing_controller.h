// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TRACING_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_TRACING_CONTROLLER_H_

#include "base/debug/trace_event.h"
#include "content/common/content_export.h"

namespace base {
class FilePath;
};

namespace content {

class TracingController;

// TracingController is used on the browser processes to enable/disable
// trace status and collect trace data. Only the browser UI thread is allowed
// to interact with the TracingController object. All callbacks are called on
// the UI thread.
class TracingController {
 public:
  enum Options {
    ENABLE_SYSTRACE = 1 << 0,
    ENABLE_SAMPLING = 1 << 1,
  };

  CONTENT_EXPORT static TracingController* GetInstance();

  // Get a set of category groups. The category groups can change as
  // new code paths are reached.
  //
  // Once all child processes have acked to the GetCategories request,
  // GetCategoriesDoneCallback is called back with a set of category
  // groups.
  typedef base::Callback<void(const std::set<std::string>&)>
      GetCategoriesDoneCallback;
  virtual void GetCategories(
      const GetCategoriesDoneCallback& callback) = 0;

  // Start recording on all processes.
  //
  // Recording begins immediately locally, and asynchronously on child processes
  // as soon as they receive the EnableRecording request.
  //
  // Once all child processes have acked to the EnableRecording request,
  // EnableRecordingDoneCallback will be called back.
  //
  // |filter| is a filter to control what category groups should be traced.
  // A filter can have an optional '-' prefix to exclude category groups
  // that contain a matching category. Having both included and excluded
  // category patterns in the same list would not be supported.
  //
  // Examples: "test_MyTest*",
  //           "test_MyTest*,test_OtherStuff",
  //           "-excluded_category1,-excluded_category2"
  //
  // |options| controls what kind of tracing is enabled.
  typedef base::Callback<void()> EnableRecordingDoneCallback;
  virtual bool EnableRecording(
      const base::debug::CategoryFilter& filter,
      TracingController::Options options,
      const EnableRecordingDoneCallback& callback) = 0;

  // Stop recording on all processes.
  //
  // Child processes typically are caching trace data and only rarely flush
  // and send trace data back to the browser process. That is because it may be
  // an expensive operation to send the trace data over IPC, and we would like
  // to avoid much runtime overhead of tracing. So, to end tracing, we must
  // asynchronously ask all child processes to flush any pending trace data.
  //
  // Once all child processes have acked to the DisableRecording request,
  // TracingFileResultCallback will be called back with a file that contains
  // the traced data.
  typedef base::Callback<void(scoped_ptr<base::FilePath>)>
      TracingFileResultCallback;
  virtual bool DisableRecording(const TracingFileResultCallback& callback) = 0;

  // Start monitoring on all processes.
  //
  // Monitoring begins immediately locally, and asynchronously on child
  // processes as soon as they receive the EnableMonitoring request.
  //
  // Once all child processes have acked to the EnableMonitoring request,
  // EnableMonitoringDoneCallback will be called back.
  //
  // |filter| is a filter to control what category groups should be traced.
  //
  // |options| controls what kind of tracing is enabled.
  typedef base::Callback<void()> EnableMonitoringDoneCallback;
  virtual bool EnableMonitoring(const base::debug::CategoryFilter& filter,
      TracingController::Options options,
      const EnableMonitoringDoneCallback& callback) = 0;

  // Stop monitoring on all processes.
  //
  // Once all child processes have acked to the DisableMonitoring request,
  // DisableMonitoringDoneCallback is called back.
  typedef base::Callback<void()> DisableMonitoringDoneCallback;
  virtual bool DisableMonitoring(
      const DisableMonitoringDoneCallback& callback) = 0;

  // Get the current monitoring configuration.
  virtual void GetMonitoringStatus(bool* out_enabled,
      base::debug::CategoryFilter* out_filter,
      TracingController::Options* out_options) = 0;

  // Get the current monitoring traced data.
  //
  // Child processes typically are caching trace data and only rarely flush
  // and send trace data back to the browser process. That is because it may be
  // an expensive operation to send the trace data over IPC, and we would like
  // to avoid much runtime overhead of tracing. So, to end tracing, we must
  // asynchronously ask all child processes to flush any pending trace data.
  //
  // Once all child processes have acked to the CaptureMonitoringSnapshot
  // request, TracingFileResultCallback will be called back with a file that
  // contains the traced data.
  virtual void CaptureMonitoringSnapshot(
      const TracingFileResultCallback& callback) = 0;

 protected:
  virtual ~TracingController() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TRACING_CONTROLLER_H_
