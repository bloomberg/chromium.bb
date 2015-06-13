// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_resource_usage.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/common/resource_usage_reporter_type_converters.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"

class ProcessResourceUsage::ErrorHandler : public mojo::ErrorHandler {
 public:
  explicit ErrorHandler(ProcessResourceUsage* usage) : usage_(usage) {}

  // mojo::ErrorHandler implementation:
  void OnConnectionError() override;

 private:
  ProcessResourceUsage* usage_;  // Not owned.
};

void ProcessResourceUsage::ErrorHandler::OnConnectionError() {
  usage_->RunPendingRefreshCallbacks();
}

ProcessResourceUsage::ProcessResourceUsage(ResourceUsageReporterPtr service)
    : service_(service.Pass()),
      update_in_progress_(false),
      error_handler_(new ErrorHandler(this)) {
  service_.set_error_handler(error_handler_.get());
}

ProcessResourceUsage::~ProcessResourceUsage() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ProcessResourceUsage::RunPendingRefreshCallbacks() {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto task_runner = base::ThreadTaskRunnerHandle::Get();
  for (const auto& callback : refresh_callbacks_)
    task_runner->PostTask(FROM_HERE, callback);
  refresh_callbacks_.clear();
}

void ProcessResourceUsage::Refresh(const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!service_ || service_.encountered_error()) {
    if (!callback.is_null())
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
    return;
  }

  if (!callback.is_null())
    refresh_callbacks_.push_back(callback);

  if (!update_in_progress_) {
    update_in_progress_ = true;
    service_->GetUsageData(base::Bind(&ProcessResourceUsage::OnRefreshDone,
                                      base::Unretained(this)));
  }
}

void ProcessResourceUsage::OnRefreshDone(ResourceUsageDataPtr data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  update_in_progress_ = false;
  stats_ = data.Pass();
  RunPendingRefreshCallbacks();
}

bool ProcessResourceUsage::ReportsV8MemoryStats() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (stats_)
    return stats_->reports_v8_stats;
  return false;
}

size_t ProcessResourceUsage::GetV8MemoryAllocated() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (stats_ && stats_->reports_v8_stats)
    return stats_->v8_bytes_allocated;
  return 0;
}

size_t ProcessResourceUsage::GetV8MemoryUsed() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (stats_ && stats_->reports_v8_stats)
    return stats_->v8_bytes_used;
  return 0;
}

blink::WebCache::ResourceTypeStats ProcessResourceUsage::GetWebCoreCacheStats()
    const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (stats_ && stats_->web_cache_stats)
    return stats_->web_cache_stats->To<blink::WebCache::ResourceTypeStats>();
  return {};
}
