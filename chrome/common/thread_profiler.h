// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_THREAD_PROFILER_H_
#define CHROME_COMMON_THREAD_PROFILER_H_

#include <memory>

#include "base/macros.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/single_thread_task_runner.h"
#include "components/metrics/call_stack_profile_params.h"

namespace service_manager {
class Connector;
}

// ThreadProfiler performs startup and periodic profiling of Chrome
// threads.
class ThreadProfiler {
 public:
  ~ThreadProfiler();

  // Creates a profiler for a main thread and immediately starts it. This
  // function should only be used when profiling the main thread of a
  // process. The returned profiler must be destroyed prior to thread exit to
  // stop the profiling. SetMainThreadTaskRunner() should be called after the
  // message loop has been started on the thread.
  static std::unique_ptr<ThreadProfiler> CreateAndStartOnMainThread(
      metrics::CallStackProfileParams::Thread thread);

  // Creates a profiler for a child thread and immediately starts it. This
  // should be called from a task posted on the child thread immediately after
  // thread start. The thread will be profiled until exit.
  static void StartOnChildThread(
      metrics::CallStackProfileParams::Thread thread);

  // This function must be called within child processes to supply the Service
  // Manager's connector, to bind the interface through which profiles are sent
  // back to the browser process.
  //
  // Note that the metrics::CallStackProfileCollector interface also must be
  // exposed to the child process, and metrics::mojom::CallStackProfileCollector
  // declared in chrome_content_browser_manifest_overlay.json, for the binding
  // to succeed.
  static void SetServiceManagerConnectorForChildProcess(
      service_manager::Connector* connector);

 private:
  // Creates the profiler.
  explicit ThreadProfiler(metrics::CallStackProfileParams::Thread thread);

  // Gets the completed callback for the ultimate receiver of the profiles.
  base::StackSamplingProfiler::CompletedCallback GetReceiverCallback(
      metrics::CallStackProfileParams* profile_params);

  // Receives |profiles| from the StackSamplingProfiler and forwards them on to
  // the original |receiver_callback|.  Note that we must obtain and bind the
  // original receiver callback prior to the start of collection because the
  // collection start time is currently recorded when obtaining the callback in
  // some collection scenarios. The implementation contains a TODO to fix this.
  static base::Optional<base::StackSamplingProfiler::SamplingParams>
  ReceiveStartupProfiles(
      const base::StackSamplingProfiler::CompletedCallback& receiver_callback,
      base::StackSamplingProfiler::CallStackProfiles profiles);

  // TODO(wittman): Make const after cleaning up the existing continuous
  // collection support.
  metrics::CallStackProfileParams startup_profile_params_;

  std::unique_ptr<base::StackSamplingProfiler> startup_profiler_;

  DISALLOW_COPY_AND_ASSIGN(ThreadProfiler);
};

#endif  // CHROME_COMMON_THREAD_PROFILER_H_
