// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROCESS_RESOURCE_USAGE_H_
#define CHROME_BROWSER_PROCESS_RESOURCE_USAGE_H_

#include <deque>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/common/resource_usage_reporter.mojom.h"

// Provides resource usage information about a child process.
//
// This is a wrapper around the ResourceUsageReporter Mojo service that exposes
// information about resources used by a child process. Currently, this is only
// V8 memory usage, but could be expanded to include other resources such as web
// cache. This is intended for status viewers such as the task manager and
// about://memory-internals.
//
// To create:
// 1. Obtain a ResourceUsageReporter connection using the child process's
//    service registry. i.e:
//    ResourceUsageReporterPtr service;
//    process->GetServiceRegistry()->ConnectToRemoteService(&service);
// 2. If needed, the connection can be passed to another thread using
//    ResourceUsageReporterPtr::PassInterface().
// 3. Pass the service to the constructor.
//
// Note: ProcessResourceUsage is thread-hostile and must live on a single
// thread.
class ProcessResourceUsage {
 public:
  // Must be called from the same thread that created |service|.
  explicit ProcessResourceUsage(ResourceUsageReporterPtr service);
  ~ProcessResourceUsage();

  // Refresh the resource usage information. |callback| is invoked when the
  // usage data is updated, or when the IPC connection is lost.
  void Refresh(const base::Closure& callback);

  // Get V8 memory usage information.
  bool ReportsV8MemoryStats() const;
  size_t GetV8MemoryAllocated() const;
  size_t GetV8MemoryUsed() const;

 private:
  class ErrorHandler;

  // Mojo IPC callback.
  void OnRefreshDone(ResourceUsageDataPtr data);

  void RunPendingRefreshCallbacks();

  ResourceUsageReporterPtr service_;
  bool update_in_progress_;
  std::deque<base::Closure> refresh_callbacks_;

  ResourceUsageDataPtr stats_;

  scoped_ptr<ErrorHandler> error_handler_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ProcessResourceUsage);
};

#endif  // CHROME_BROWSER_PROCESS_RESOURCE_USAGE_H_
